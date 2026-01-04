#include "video_encoder.h"

#include <cstring>
#include <string>

#include "video_frame.h"
#include "encoded_video_chunk.h"
#include "shared/codec_registry.h"
#include "error_builder.h"
#include "shared/buffer_utils.h"

namespace webcodecs {

Napi::FunctionReference VideoEncoder::constructor;

// =============================================================================
// VIDEOENCODER IMPLEMENTATION
// =============================================================================

Napi::Object VideoEncoder::Init(Napi::Env env, Napi::Object exports) {
  Napi::Function func =
      DefineClass(env, "VideoEncoder",
                  {
                      InstanceAccessor<&VideoEncoder::GetState>("state"),
                      InstanceAccessor<&VideoEncoder::GetEncodeQueueSize>("encodeQueueSize"),
                      InstanceAccessor<&VideoEncoder::GetOndequeue, &VideoEncoder::SetOndequeue>("ondequeue"),
                      InstanceMethod<&VideoEncoder::Configure>("configure"),
                      InstanceMethod<&VideoEncoder::Encode>("encode"),
                      InstanceMethod<&VideoEncoder::Flush>("flush"),
                      InstanceMethod<&VideoEncoder::Reset>("reset"),
                      InstanceMethod<&VideoEncoder::Close>("close"),
                      StaticMethod<&VideoEncoder::IsConfigSupported>("isConfigSupported"),
                  });

  constructor = Napi::Persistent(func);
  constructor.SuppressDestruct();
  exports.Set("VideoEncoder", func);
  return exports;
}

VideoEncoder::VideoEncoder(const Napi::CallbackInfo& info) : Napi::ObjectWrap<VideoEncoder>(info) {
  Napi::Env env = info.Env();

  // [SPEC] Constructor Algorithm
  // VideoEncoderInit requires: output (EncodedVideoChunkOutputCallback), error (WebCodecsErrorCallback)

  // Validate init object is provided
  if (info.Length() < 1 || !info[0].IsObject()) {
    Napi::TypeError::New(env, "VideoEncoderInit is required").ThrowAsJavaScriptException();
    return;
  }

  Napi::Object init = info[0].As<Napi::Object>();

  // Validate output callback is provided and is a function
  if (!init.Has("output") || !init.Get("output").IsFunction()) {
    Napi::TypeError::New(env, "output callback is required").ThrowAsJavaScriptException();
    return;
  }

  // Validate error callback is provided and is a function
  if (!init.Has("error") || !init.Get("error").IsFunction()) {
    Napi::TypeError::New(env, "error callback is required").ThrowAsJavaScriptException();
    return;
  }

  // Store callbacks for later use
  output_callback_ = Napi::Persistent(init.Get("output").As<Napi::Function>());
  error_callback_ = Napi::Persistent(init.Get("error").As<Napi::Function>());

  // Initialize TSFNs for async callbacks
  InitializeTSFNs(env);

  // Create worker (but don't start until configure)
  worker_ = std::make_unique<VideoEncoderWorker>(queue_, this);
}

VideoEncoder::~VideoEncoder() {
  Release();
}

void VideoEncoder::Release() {
  // Thread-safe close: transition to Closed state
  state_.Close();

  // Stop the worker thread first
  if (worker_) {
    worker_->Stop();
  }

  // Shutdown the queue
  queue_.Shutdown();

  // Release TSFNs
  ReleaseTSFNs();

  // Reject all pending flush promises
  {
    std::lock_guard<std::mutex> lock(flush_mutex_);
    pending_flushes_.clear();
  }

  // Reset encode queue size
  encode_queue_size_.store(0, std::memory_order_release);

  // Clear active orientation
  {
    std::lock_guard<std::mutex> lock(orientation_mutex_);
    active_orientation_.reset();
  }

  // Release JS callbacks
  if (!output_callback_.IsEmpty()) {
    output_callback_.Reset();
  }
  if (!error_callback_.IsEmpty()) {
    error_callback_.Reset();
  }
  if (!ondequeue_callback_.IsEmpty()) {
    ondequeue_callback_.Reset();
  }
}

void VideoEncoder::InitializeTSFNs(Napi::Env env) {
  // Output TSFN - delivers encoded chunks to JS
  using OutputTSFNType = Napi::TypedThreadSafeFunction<VideoEncoder, OutputData, &VideoEncoder::OnOutputChunk>;
  auto output_tsfn = OutputTSFNType::New(
      env,
      output_callback_.Value(),
      "VideoEncoder::output",
      0,      // Unlimited queue
      1,      // Initial thread count
      this);  // Context
  output_tsfn_.Init(std::move(output_tsfn));

  // Error TSFN - delivers errors to JS
  using ErrorTSFNType = Napi::TypedThreadSafeFunction<VideoEncoder, ErrorData, &VideoEncoder::OnError>;
  auto error_tsfn = ErrorTSFNType::New(
      env,
      error_callback_.Value(),
      "VideoEncoder::error",
      0, 1, this);
  error_tsfn_.Init(std::move(error_tsfn));

  // Flush and dequeue use dummy callbacks - we handle logic in the handler
  Napi::Function dummyFn = Napi::Function::New(env, [](const Napi::CallbackInfo&) {});

  using FlushTSFNType = Napi::TypedThreadSafeFunction<VideoEncoder, FlushCompleteData, &VideoEncoder::OnFlushComplete>;
  auto flush_tsfn = FlushTSFNType::New(
      env, dummyFn, "VideoEncoder::flush",
      0, 1, this);
  flush_tsfn_.Init(std::move(flush_tsfn));

  using DequeueTSFNType = Napi::TypedThreadSafeFunction<VideoEncoder, DequeueData, &VideoEncoder::OnDequeue>;
  auto dequeue_tsfn = DequeueTSFNType::New(
      env, dummyFn, "VideoEncoder::dequeue",
      0, 1, this);
  dequeue_tsfn_.Init(std::move(dequeue_tsfn));

  // Unref all TSFNs to allow Node.js to exit cleanly
  output_tsfn_.Unref(env);
  error_tsfn_.Unref(env);
  flush_tsfn_.Unref(env);
  dequeue_tsfn_.Unref(env);
}

void VideoEncoder::ReleaseTSFNs() {
  output_tsfn_.Release();
  error_tsfn_.Release();
  flush_tsfn_.Release();
  dequeue_tsfn_.Release();
}

// --- TSFN Callback Handlers ---

void VideoEncoder::OnOutputChunk(Napi::Env env, Napi::Function jsCallback,
                                  VideoEncoder* context, OutputData* data) {
  if (!data) return;

  // Take ownership of the data
  std::unique_ptr<OutputData> output(data);

  if (!context || context->state_.IsClosed()) {
    return;
  }

  if (!output->packet || context->output_callback_.IsEmpty()) {
    return;
  }

  // Create EncodedVideoChunk JS object from packet
  Napi::Object chunk = EncodedVideoChunk::CreateFromPacket(
      env,
      output->packet.get(),
      output->is_key_frame,
      output->timestamp);

  if (chunk.IsEmpty()) {
    return;
  }

  // Create metadata if needed (first keyframe after configure)
  if (output->include_decoder_config) {
    Napi::Object metadata = Napi::Object::New(env);

    // Build decoderConfig from OutputData (thread-safe - no codec_ctx_ access)
    Napi::Object decoderConfig = Napi::Object::New(env);
    decoderConfig.Set("codec", Napi::String::New(env, output->codec));
    decoderConfig.Set("codedWidth", Napi::Number::New(env, output->coded_width));
    decoderConfig.Set("codedHeight", Napi::Number::New(env, output->coded_height));

    // Include extradata as description if available (copied from worker thread)
    if (!output->extradata.empty()) {
      Napi::ArrayBuffer desc = Napi::ArrayBuffer::New(env, output->extradata.size());
      std::memcpy(desc.Data(), output->extradata.data(), output->extradata.size());
      decoderConfig.Set("description", Napi::Uint8Array::New(env, output->extradata.size(), desc, 0));
    }

    metadata.Set("decoderConfig", decoderConfig);

    // Call output with metadata
    context->output_callback_.Call({chunk, metadata});
  } else {
    // Call output without metadata
    context->output_callback_.Call({chunk});
  }
}

void VideoEncoder::OnError(Napi::Env env, Napi::Function jsCallback,
                           VideoEncoder* context, ErrorData* data) {
  if (!data) return;

  std::string message = std::move(data->message);
  delete data;

  if (!context || context->state_.IsClosed()) {
    return;
  }

  // Create DOMException and call error callback
  if (!context->error_callback_.IsEmpty()) {
    Napi::Error error = Napi::Error::New(env, "EncodingError: " + message);
    error.Set("name", Napi::String::New(env, "EncodingError"));
    context->error_callback_.Call({error.Value()});
  }

  // Close the encoder on error
  context->state_.Close();
}

void VideoEncoder::OnFlushComplete(Napi::Env env, Napi::Function jsCallback,
                                   VideoEncoder* context, FlushCompleteData* data) {
  if (!data) return;

  uint32_t promise_id = data->promise_id;
  bool success = data->success;
  std::string error_message = std::move(data->error_message);
  delete data;

  if (!context) return;

  // Find and resolve/reject the promise
  std::lock_guard<std::mutex> lock(context->flush_mutex_);
  auto it = context->pending_flushes_.find(promise_id);
  if (it != context->pending_flushes_.end()) {
    if (success) {
      it->second.Resolve(env.Undefined());
    } else {
      Napi::Error error = Napi::Error::New(env, "EncodingError: " + error_message);
      error.Set("name", Napi::String::New(env, "EncodingError"));
      it->second.Reject(error.Value());
    }
    context->pending_flushes_.erase(it);
  }
}

void VideoEncoder::OnDequeue(Napi::Env env, Napi::Function jsCallback,
                             VideoEncoder* context, DequeueData* data) {
  if (!data) return;

  delete data;

  if (!context || context->state_.IsClosed()) {
    return;
  }

  // Fire ondequeue event
  if (!context->ondequeue_callback_.IsEmpty()) {
    context->ondequeue_callback_.Call({});
  }

  // [SPEC] Reset [[dequeue event scheduled]] after firing
  context->dequeue_event_scheduled_.store(false, std::memory_order_release);
}

// --- Attributes ---

Napi::Value VideoEncoder::GetState(const Napi::CallbackInfo& info) {
  return Napi::String::New(info.Env(), state_.ToString());
}

Napi::Value VideoEncoder::GetEncodeQueueSize(const Napi::CallbackInfo& info) {
  return Napi::Number::New(info.Env(), static_cast<double>(encode_queue_size_.load(std::memory_order_acquire)));
}

Napi::Value VideoEncoder::GetOndequeue(const Napi::CallbackInfo& info) {
  if (ondequeue_callback_.IsEmpty()) {
    return info.Env().Null();
  }
  return ondequeue_callback_.Value();
}

void VideoEncoder::SetOndequeue(const Napi::CallbackInfo& info, const Napi::Value& value) {
  if (value.IsNull() || value.IsUndefined()) {
    ondequeue_callback_.Reset();
  } else if (value.IsFunction()) {
    ondequeue_callback_ = Napi::Persistent(value.As<Napi::Function>());
  }
}

// --- Methods ---

Napi::Value VideoEncoder::Configure(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] 1. If config is not a valid VideoEncoderConfig, throw TypeError
  if (info.Length() < 1 || !info[0].IsObject()) {
    errors::ThrowTypeError(env, "VideoEncoderConfig is required");
    return env.Undefined();
  }

  // [SPEC] 2. If state is "closed", throw InvalidStateError
  if (state_.IsClosed()) {
    errors::ThrowInvalidStateError(env, "configure called on closed encoder");
    return env.Undefined();
  }

  Napi::Object config = info[0].As<Napi::Object>();

  // Required: codec, width, height
  if (!config.Has("codec") || !config.Get("codec").IsString()) {
    errors::ThrowTypeError(env, "codec is required and must be a string");
    return env.Undefined();
  }
  if (!config.Has("width") || !config.Get("width").IsNumber()) {
    errors::ThrowTypeError(env, "width is required and must be a number");
    return env.Undefined();
  }
  if (!config.Has("height") || !config.Get("height").IsNumber()) {
    errors::ThrowTypeError(env, "height is required and must be a number");
    return env.Undefined();
  }

  // Deep copy configuration for async processing
  active_config_.codec = config.Get("codec").As<Napi::String>().Utf8Value();
  active_config_.width = config.Get("width").As<Napi::Number>().Int32Value();
  active_config_.height = config.Get("height").As<Napi::Number>().Int32Value();

  // Validate dimensions
  if (active_config_.width <= 0 || active_config_.height <= 0) {
    errors::ThrowTypeError(env, "width and height must be positive");
    return env.Undefined();
  }

  // Optional fields
  if (config.Has("displayWidth") && config.Get("displayWidth").IsNumber()) {
    active_config_.display_width = config.Get("displayWidth").As<Napi::Number>().Int32Value();
  } else {
    active_config_.display_width = active_config_.width;
  }
  if (config.Has("displayHeight") && config.Get("displayHeight").IsNumber()) {
    active_config_.display_height = config.Get("displayHeight").As<Napi::Number>().Int32Value();
  } else {
    active_config_.display_height = active_config_.height;
  }
  if (config.Has("bitrate") && config.Get("bitrate").IsNumber()) {
    active_config_.bitrate = config.Get("bitrate").As<Napi::Number>().Int64Value();
  }
  if (config.Has("framerate") && config.Get("framerate").IsNumber()) {
    active_config_.framerate = config.Get("framerate").As<Napi::Number>().DoubleValue();
  }
  if (config.Has("hardwareAcceleration") && config.Get("hardwareAcceleration").IsString()) {
    active_config_.hardware_acceleration = config.Get("hardwareAcceleration").As<Napi::String>().Utf8Value();
  }
  if (config.Has("alpha") && config.Get("alpha").IsString()) {
    active_config_.alpha = config.Get("alpha").As<Napi::String>().Utf8Value();
  }
  if (config.Has("scalabilityMode") && config.Get("scalabilityMode").IsString()) {
    active_config_.scalability_mode = config.Get("scalabilityMode").As<Napi::String>().Utf8Value();
  }
  if (config.Has("bitrateMode") && config.Get("bitrateMode").IsString()) {
    active_config_.bitrate_mode = config.Get("bitrateMode").As<Napi::String>().Utf8Value();
  }
  if (config.Has("latencyMode") && config.Get("latencyMode").IsString()) {
    active_config_.latency_mode = config.Get("latencyMode").As<Napi::String>().Utf8Value();
  }

  // Validate codec string before queuing (fail fast)
  auto codec_info = ParseCodecString(active_config_.codec);
  if (!codec_info) {
    errors::ThrowNotSupportedError(env, "Unsupported codec: " + active_config_.codec);
    return env.Undefined();
  }

  const AVCodec* encoder = avcodec_find_encoder(codec_info->codec_id);
  if (!encoder) {
    errors::ThrowNotSupportedError(env, "No encoder available for: " + active_config_.codec);
    return env.Undefined();
  }

  // [SPEC] 4. Set active orientation to null
  {
    std::lock_guard<std::mutex> lock(orientation_mutex_);
    active_orientation_.reset();
  }

  // Start worker if not running
  if (!worker_->IsRunning()) {
    worker_->Start();
  }

  // [SPEC] Set [[message queue blocked]] before configure
  queue_.SetBlocked(true);

  // Create configure message
  VideoControlQueue::ConfigureMessage msg{
      [this]() -> bool {
        return worker_ != nullptr;
      }};

  // Enqueue configure message
  if (!queue_.Enqueue(std::move(msg))) {
    queue_.SetBlocked(false);  // Reset on failure
    errors::ThrowInvalidStateError(env, "Failed to enqueue configure");
    return env.Undefined();
  }

  // [SPEC] 3. Set state to "configured"
  state_.transition(raii::AtomicCodecState::State::Unconfigured, raii::AtomicCodecState::State::Configured);

  return env.Undefined();
}

Napi::Value VideoEncoder::Encode(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] 2. If state is not "configured", throw InvalidStateError
  if (!state_.IsConfigured()) {
    errors::ThrowInvalidStateError(env, "encode called on " + std::string(state_.ToString()) + " encoder");
    return env.Undefined();
  }

  // [SPEC] 1. Validate frame is not detached
  if (info.Length() < 1 || !info[0].IsObject()) {
    errors::ThrowTypeError(env, "VideoFrame is required");
    return env.Undefined();
  }

  Napi::Object frameObj = info[0].As<Napi::Object>();

  // Get VideoFrame wrapper
  VideoFrame* frameWrapper = nullptr;
  if (frameObj.InstanceOf(VideoFrame::constructor.Value())) {
    frameWrapper = Napi::ObjectWrap<VideoFrame>::Unwrap(frameObj);
  }

  if (!frameWrapper) {
    errors::ThrowTypeError(env, "VideoFrame is required");
    return env.Undefined();
  }

  AVFrame* srcFrame = frameWrapper->GetAVFrame();
  if (!srcFrame) {
    errors::ThrowTypeError(env, "VideoFrame is closed or invalid");
    return env.Undefined();
  }

  // [SPEC] 3-4. Check and set active orientation
  {
    std::lock_guard<std::mutex> lock(orientation_mutex_);

    // Get rotation/flip from frame (default to 0/false)
    Orientation frameOrientation{0, false};

    if (active_orientation_.has_value()) {
      if (active_orientation_->rotation != frameOrientation.rotation ||
          active_orientation_->flip != frameOrientation.flip) {
        errors::ThrowDataError(env, "Frame orientation does not match active orientation");
        return env.Undefined();
      }
    } else {
      active_orientation_ = frameOrientation;
    }
  }

  // Parse encode options
  bool keyFrame = false;
  if (info.Length() >= 2 && info[1].IsObject()) {
    Napi::Object options = info[1].As<Napi::Object>();
    if (options.Has("keyFrame") && options.Get("keyFrame").IsBoolean()) {
      keyFrame = options.Get("keyFrame").As<Napi::Boolean>().Value();
    }
  }

  // [SPEC] 5. Clone frame for async processing
  raii::AVFramePtr frameClone = raii::CloneAvFrame(srcFrame);
  if (!frameClone) {
    errors::ThrowEncodingError(env, "Failed to clone frame");
    return env.Undefined();
  }

  // [SPEC] 6. Increment encodeQueueSize
  encode_queue_size_.fetch_add(1, std::memory_order_relaxed);

  // [SPEC] 7. Queue encode message
  VideoControlQueue::EncodeMessage msg{std::move(frameClone), keyFrame};
  if (!queue_.Enqueue(std::move(msg))) {
    encode_queue_size_.fetch_sub(1, std::memory_order_relaxed);
    errors::ThrowInvalidStateError(env, "Failed to enqueue encode");
    return env.Undefined();
  }

  return env.Undefined();
}

Napi::Value VideoEncoder::Flush(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] 1. If state is not "configured", reject with InvalidStateError
  if (!state_.IsConfigured()) {
    Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
    Napi::Error error =
        Napi::Error::New(env, "InvalidStateError: flush called on " + std::string(state_.ToString()) + " encoder");
    error.Set("name", Napi::String::New(env, "InvalidStateError"));
    deferred.Reject(error.Value());
    return deferred.Promise();
  }

  // [SPEC] 2. Create promise
  Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);

  // Store promise for later resolution
  uint32_t promise_id;
  {
    std::lock_guard<std::mutex> lock(flush_mutex_);
    promise_id = next_flush_id_++;
    pending_flushes_.emplace(promise_id, std::move(deferred));
  }

  // Enqueue flush message
  VideoControlQueue::FlushMessage msg{promise_id};
  if (!queue_.Enqueue(std::move(msg))) {
    std::lock_guard<std::mutex> lock(flush_mutex_);
    auto it = pending_flushes_.find(promise_id);
    if (it != pending_flushes_.end()) {
      Napi::Error error = Napi::Error::New(env, "InvalidStateError: Failed to enqueue flush");
      error.Set("name", Napi::String::New(env, "InvalidStateError"));
      it->second.Reject(error.Value());
      pending_flushes_.erase(it);
    }
    return Napi::Promise::Deferred::New(env).Promise();
  }

  // Return the promise
  {
    std::lock_guard<std::mutex> lock(flush_mutex_);
    auto it = pending_flushes_.find(promise_id);
    if (it != pending_flushes_.end()) {
      return it->second.Promise();
    }
  }
  return Napi::Promise::Deferred::New(env).Promise();
}

Napi::Value VideoEncoder::Reset(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] If state is "closed", throw InvalidStateError
  if (state_.IsClosed()) {
    errors::ThrowInvalidStateError(env, "reset called on closed encoder");
    return env.Undefined();
  }

  // Clear the queue (returns dropped frames for cleanup)
  auto dropped = queue_.ClearFrames();
  // Frames are automatically cleaned up by RAII

  // Reset queue size
  encode_queue_size_.store(0, std::memory_order_release);

  // Enqueue reset message
  VideoControlQueue::ResetMessage msg{};
  (void)queue_.Enqueue(std::move(msg));

  // Reset active orientation
  {
    std::lock_guard<std::mutex> lock(orientation_mutex_);
    active_orientation_.reset();
  }

  // Reject all pending flush promises
  {
    std::lock_guard<std::mutex> lock(flush_mutex_);
    for (auto& [id, deferred] : pending_flushes_) {
      Napi::Error error = Napi::Error::New(env, "AbortError: Encoder was reset");
      error.Set("name", Napi::String::New(env, "AbortError"));
      deferred.Reject(error.Value());
    }
    pending_flushes_.clear();
  }

  // Transition state to unconfigured
  state_.transition(raii::AtomicCodecState::State::Configured, raii::AtomicCodecState::State::Unconfigured);

  return env.Undefined();
}

Napi::Value VideoEncoder::Close(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Immediately aborts all pending work and releases system resources.
  Release();

  return env.Undefined();
}

Napi::Value VideoEncoder::IsConfigSupported(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] 1. If config is not a valid VideoEncoderConfig, reject with TypeError
  if (info.Length() < 1 || !info[0].IsObject()) {
    Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
    deferred.Reject(Napi::TypeError::New(env, "VideoEncoderConfig is required").Value());
    return deferred.Promise();
  }

  Napi::Object config = info[0].As<Napi::Object>();

  // Required: codec, width, height
  if (!config.Has("codec") || !config.Get("codec").IsString()) {
    Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
    deferred.Reject(Napi::TypeError::New(env, "codec is required and must be a string").Value());
    return deferred.Promise();
  }
  if (!config.Has("width") || !config.Get("width").IsNumber()) {
    Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
    deferred.Reject(Napi::TypeError::New(env, "width is required and must be a number").Value());
    return deferred.Promise();
  }
  if (!config.Has("height") || !config.Get("height").IsNumber()) {
    Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
    deferred.Reject(Napi::TypeError::New(env, "height is required and must be a number").Value());
    return deferred.Promise();
  }

  std::string codec_string = config.Get("codec").As<Napi::String>().Utf8Value();

  // [SPEC] 2. Create promise
  Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);

  // Check if codec is supported for encoding
  auto codec_info = ParseCodecString(codec_string);
  bool supported = false;
  if (codec_info) {
    const AVCodec* encoder = avcodec_find_encoder(codec_info->codec_id);
    supported = (encoder != nullptr);
  }

  // [SPEC] Create VideoEncoderSupport object
  Napi::Object result = Napi::Object::New(env);
  result.Set("supported", Napi::Boolean::New(env, supported));

  // Clone the config (only recognized members)
  Napi::Object clonedConfig = Napi::Object::New(env);
  clonedConfig.Set("codec", config.Get("codec"));
  clonedConfig.Set("width", config.Get("width"));
  clonedConfig.Set("height", config.Get("height"));

  if (config.Has("displayWidth") && config.Get("displayWidth").IsNumber()) {
    clonedConfig.Set("displayWidth", config.Get("displayWidth"));
  }
  if (config.Has("displayHeight") && config.Get("displayHeight").IsNumber()) {
    clonedConfig.Set("displayHeight", config.Get("displayHeight"));
  }
  if (config.Has("bitrate") && config.Get("bitrate").IsNumber()) {
    clonedConfig.Set("bitrate", config.Get("bitrate"));
  }
  if (config.Has("framerate") && config.Get("framerate").IsNumber()) {
    clonedConfig.Set("framerate", config.Get("framerate"));
  }
  if (config.Has("hardwareAcceleration") && config.Get("hardwareAcceleration").IsString()) {
    clonedConfig.Set("hardwareAcceleration", config.Get("hardwareAcceleration"));
  }
  if (config.Has("alpha") && config.Get("alpha").IsString()) {
    clonedConfig.Set("alpha", config.Get("alpha"));
  }
  if (config.Has("scalabilityMode") && config.Get("scalabilityMode").IsString()) {
    clonedConfig.Set("scalabilityMode", config.Get("scalabilityMode"));
  }
  if (config.Has("bitrateMode") && config.Get("bitrateMode").IsString()) {
    clonedConfig.Set("bitrateMode", config.Get("bitrateMode"));
  }
  if (config.Has("latencyMode") && config.Get("latencyMode").IsString()) {
    clonedConfig.Set("latencyMode", config.Get("latencyMode"));
  }

  result.Set("config", clonedConfig);

  deferred.Resolve(result);
  return deferred.Promise();
}

// =============================================================================
// SCALABILITY MODE HELPERS
// =============================================================================

/**
 * Apply scalabilityMode (SVC) settings to VP9/AV1 encoder context.
 *
 * Scalability mode format: LxTy where:
 * - x = number of spatial layers (1-3)
 * - y = number of temporal layers (1-3)
 *
 * For VP9 (libvpx-vp9), temporal layers are supported via ts-parameters.
 * Spatial layers require SVC mode which is more complex.
 *
 * @param ctx AVCodecContext to configure
 * @param mode Scalability mode string (e.g., "L1T2", "L1T3")
 * @param[out] error_msg Optional output for detailed error message
 * @return true if mode was applied, false if unsupported
 */
static bool ApplyScalabilityMode(AVCodecContext* ctx, const std::string& mode,
                                  std::string* error_msg = nullptr) {
  if (mode.empty() || mode == "L1T1") {
    // L1T1 = no SVC, default behavior
    return true;
  }

  // Parse mode: LxTy format
  if (mode.length() != 4 || mode[0] != 'L' || mode[2] != 'T') {
    if (error_msg) *error_msg = "Invalid format: expected LxTy (e.g., L1T2)";
    return false;
  }

  int spatial_layers = mode[1] - '0';
  int temporal_layers = mode[3] - '0';

  if (spatial_layers < 1 || spatial_layers > 3 ||
      temporal_layers < 1 || temporal_layers > 3) {
    if (error_msg) *error_msg = "Layer count out of range (must be 1-3)";
    return false;
  }

  // Check if this is a VP9 encoder (libvpx-vp9)
  const char* codec_name = ctx->codec ? ctx->codec->name : nullptr;
  bool is_vp9 = codec_name && (strcmp(codec_name, "libvpx-vp9") == 0 ||
                                strcmp(codec_name, "vp9") == 0);

  if (!is_vp9) {
    // For non-VP9 codecs, we only support L1T1 (no SVC)
    // AV1 SVC support in FFmpeg is limited
    if (spatial_layers != 1 || temporal_layers != 1) {
      if (error_msg) *error_msg = "SVC is only supported for VP9 (libvpx-vp9) codec";
    }
    return spatial_layers == 1 && temporal_layers == 1;
  }

  // VP9 only supports temporal layers via ts-parameters (no spatial in basic SVC)
  if (spatial_layers > 1) {
    // Full SVC with spatial layers requires special configuration
    // For now, only support L1Tx modes
    if (error_msg) *error_msg = "Spatial layers (L2+) not yet supported, only L1Tx modes";
    return false;
  }

  // Build ts-parameters for temporal layers
  // Format: ts_number_layers=N:ts_target_bitrate=B1,B2,...:ts_rate_decimator=D1,D2,...:
  //         ts_periodicity=P:ts_layer_id=0,1,...:ts_layering_mode=M
  std::string ts_params;

  if (temporal_layers == 2) {
    // L1T2: 2 temporal layers (15fps + 30fps at 30fps base)
    // Layer 0: base layer at 1/2 frame rate
    // Layer 1: enhancement layer at full frame rate
    int base_bitrate = ctx->bit_rate > 0 ? static_cast<int>(ctx->bit_rate * 0.6) : 500000;
    int total_bitrate = ctx->bit_rate > 0 ? static_cast<int>(ctx->bit_rate) : 1000000;

    ts_params = "ts_number_layers=2:"
                "ts_target_bitrate=" + std::to_string(base_bitrate) + "," +
                std::to_string(total_bitrate) + ":"
                "ts_rate_decimator=2,1:"
                "ts_periodicity=2:"
                "ts_layer_id=0,1:"
                "ts_layering_mode=2";
  } else if (temporal_layers == 3) {
    // L1T3: 3 temporal layers (7.5fps + 15fps + 30fps at 30fps base)
    int layer0_bitrate = ctx->bit_rate > 0 ? static_cast<int>(ctx->bit_rate * 0.4) : 400000;
    int layer1_bitrate = ctx->bit_rate > 0 ? static_cast<int>(ctx->bit_rate * 0.7) : 700000;
    int layer2_bitrate = ctx->bit_rate > 0 ? static_cast<int>(ctx->bit_rate) : 1000000;

    ts_params = "ts_number_layers=3:"
                "ts_target_bitrate=" + std::to_string(layer0_bitrate) + "," +
                std::to_string(layer1_bitrate) + "," +
                std::to_string(layer2_bitrate) + ":"
                "ts_rate_decimator=4,2,1:"
                "ts_periodicity=4:"
                "ts_layer_id=0,2,1,2:"
                "ts_layering_mode=3";
  } else {
    // L1T1 - no SVC needed
    return true;
  }

  // Apply ts-parameters to the codec context
  int ret = av_opt_set(ctx->priv_data, "ts-parameters", ts_params.c_str(), 0);
  if (ret < 0) {
    // ts-parameters not supported by this encoder
    if (error_msg) {
      char errbuf[AV_ERROR_MAX_STRING_SIZE];
      av_strerror(ret, errbuf, sizeof(errbuf));
      *error_msg = "Encoder does not support ts-parameters for SVC: " + std::string(errbuf);
    }
    // L1T1 doesn't need SVC, but L1T2/L1T3 require ts-parameters support
    return false;
  }

  return true;
}

// =============================================================================
// VIDEOENCODERWORKER IMPLEMENTATION
// =============================================================================

VideoEncoderWorker::VideoEncoderWorker(VideoControlQueue& queue, VideoEncoder* encoder)
    : CodecWorker<VideoControlQueue>(queue), encoder_(encoder) {
  // Callbacks are set up in output helpers
}

VideoEncoderWorker::~VideoEncoderWorker() {
  Stop();
}

bool VideoEncoderWorker::OnConfigure(const ConfigureMessage& msg) {
  // Get config from parent encoder
  if (!encoder_) return false;

  // Helper to unblock queue on exit (RAII pattern)
  struct ScopeGuard {
    VideoEncoder* e;
    ~ScopeGuard() { if (e) e->queue_.SetBlocked(false); }
  } guard{encoder_};

  // Thread-safe: copy config at start to avoid race with main thread
  // The main thread may call Configure() again while we're processing
  const VideoEncoder::EncoderConfig config = encoder_->active_config_;

  // Parse codec string
  auto codec_info = ParseCodecString(config.codec);
  if (!codec_info) {
    OutputError(AVERROR_ENCODER_NOT_FOUND, "Unsupported codec: " + config.codec);
    return false;
  }

  // Find FFmpeg encoder
  const AVCodec* encoder = avcodec_find_encoder(codec_info->codec_id);
  if (!encoder) {
    OutputError(AVERROR_ENCODER_NOT_FOUND, "No encoder available for: " + config.codec);
    return false;
  }

  // Allocate codec context
  codec_ctx_ = raii::MakeAvCodecContext(encoder);
  if (!codec_ctx_) {
    OutputError(AVERROR(ENOMEM), "Failed to allocate encoder context");
    return false;
  }

  // Set encoder parameters
  codec_ctx_->width = config.width;
  codec_ctx_->height = config.height;
  width_ = config.width;
  height_ = config.height;

  // Time base in microseconds (WebCodecs uses microseconds)
  codec_ctx_->time_base = AVRational{1, 1000000};

  // Set pixel format (default to YUV420P, most compatible)
  codec_ctx_->pix_fmt = AV_PIX_FMT_YUV420P;
  format_ = AV_PIX_FMT_YUV420P;

  // Use encoder's preferred format if available
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(61, 0, 0)
  // FFmpeg 7.0+ uses avcodec_get_supported_config
  const AVPixelFormat* pix_fmts = nullptr;
  int num_fmts = 0;
  if (avcodec_get_supported_config(codec_ctx_.get(), encoder, AV_CODEC_CONFIG_PIX_FORMAT,
                                    0, reinterpret_cast<const void**>(&pix_fmts), &num_fmts) >= 0) {
    if (pix_fmts && num_fmts > 0) {
      codec_ctx_->pix_fmt = pix_fmts[0];
      format_ = pix_fmts[0];
    }
  }
#else
  // Suppress deprecated warning for older FFmpeg
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  if (encoder->pix_fmts) {
    codec_ctx_->pix_fmt = encoder->pix_fmts[0];
    format_ = encoder->pix_fmts[0];
  }
  #pragma GCC diagnostic pop
#endif

  // Bitrate
  if (config.bitrate > 0) {
    codec_ctx_->bit_rate = config.bitrate;
  }

  // Framerate
  if (config.framerate > 0) {
    codec_ctx_->framerate = AVRational{static_cast<int>(config.framerate * 1000), 1000};
  }

  // GOP size (keyframe interval)
  codec_ctx_->gop_size = config.framerate > 0 ? static_cast<int>(config.framerate) : 30;

  // Bitrate mode
  if (config.bitrate_mode == "constant") {
    codec_ctx_->rc_max_rate = codec_ctx_->bit_rate;
    codec_ctx_->rc_buffer_size = static_cast<int>(codec_ctx_->bit_rate);
  }

  // Latency mode
  if (config.latency_mode == "realtime") {
    codec_ctx_->flags |= AV_CODEC_FLAG_LOW_DELAY;
    codec_ctx_->max_b_frames = 0;
  }

  // Threading
  codec_ctx_->thread_count = 0;  // Auto-detect
  codec_ctx_->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;

  // Apply scalability mode (SVC) for VP9 temporal layers
  if (!config.scalability_mode.empty()) {
    std::string svc_error;
    if (!ApplyScalabilityMode(codec_ctx_.get(), config.scalability_mode, &svc_error)) {
      std::string msg = "Unsupported scalabilityMode '" + config.scalability_mode + "'";
      if (!svc_error.empty()) {
        msg += ": " + svc_error;
      }
      OutputError(AVERROR(EINVAL), msg);
      codec_ctx_.reset();
      return false;
    }
  }

  // Open encoder
  int ret = avcodec_open2(codec_ctx_.get(), encoder, nullptr);
  if (ret < 0) {
    OutputError(ret, "Failed to open encoder");
    codec_ctx_.reset();
    return false;
  }

  // Store codec string for thread-safe access in OutputChunk
  codec_ = config.codec;

  // Reset state for new configuration
  first_output_after_configure_ = true;
  frame_count_ = 0;

  return true;
}

void VideoEncoderWorker::OnEncode(const EncodeMessage& msg) {
  if (!codec_ctx_ || ShouldExit()) return;

  AVFrame* frame = msg.frame.get();
  if (!frame) {
    OutputError(AVERROR(EINVAL), "Invalid frame");
    return;
  }

  // Force keyframe if requested
  if (msg.key_frame) {
    frame->pict_type = AV_PICTURE_TYPE_I;
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(58, 0, 0)
    frame->flags |= AV_FRAME_FLAG_KEY;
#endif
  } else {
    frame->pict_type = AV_PICTURE_TYPE_NONE;  // Let encoder decide
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(58, 0, 0)
    frame->flags &= ~AV_FRAME_FLAG_KEY;
#endif
  }

  // Set frame PTS
  if (frame->pts == AV_NOPTS_VALUE) {
    frame->pts = frame_count_;
  }
  frame_count_++;

  // Send frame to encoder
  int ret = avcodec_send_frame(codec_ctx_.get(), frame);

  // [SPEC] [[codec saturated]] - track when codec cannot accept more input
  if (ret == AVERROR(EAGAIN)) {
    // Codec input buffer full - set saturation flag
    if (encoder_) {
      encoder_->codec_saturated_.store(true, std::memory_order_release);
    }
  } else if (ret < 0) {
    OutputError(ret, "Failed to send frame to encoder");
    return;
  }

  // Receive all available packets
  raii::AVPacketPtr packet = raii::MakeAvPacket();
  if (!packet) {
    OutputError(AVERROR(ENOMEM), "Failed to allocate packet");
    return;
  }

  bool received_packet = false;
  while (!ShouldExit()) {
    ret = avcodec_receive_packet(codec_ctx_.get(), packet.get());

    if (ret == AVERROR(EAGAIN)) {
      break;  // Need more input
    }
    if (ret == AVERROR_EOF) {
      break;  // End of stream
    }
    if (ret < 0) {
      OutputError(ret, "Error receiving packet");
      return;
    }

    received_packet = true;

    // Determine if this is a keyframe
    bool is_key = (packet->flags & AV_PKT_FLAG_KEY) != 0;

    // Include decoder config on first keyframe after configure
    bool include_config = first_output_after_configure_ && is_key;
    if (include_config) {
      first_output_after_configure_ = false;
    }

    // Get timestamp and duration
    int64_t timestamp = packet->pts != AV_NOPTS_VALUE ? packet->pts : 0;
    int64_t duration = packet->duration > 0 ? packet->duration : 0;

    // Clone packet for output
    raii::AVPacketPtr outputPacket = raii::CloneAvPacket(packet.get());
    if (outputPacket) {
      OutputChunk(std::move(outputPacket), is_key, timestamp, duration, include_config);
    }

    av_packet_unref(packet.get());
  }

  // [SPEC] Clear [[codec saturated]] after receiving output
  if (received_packet && encoder_) {
    encoder_->codec_saturated_.store(false, std::memory_order_release);
  }

  // Decrement queue size and signal dequeue
  if (encoder_) {
    uint32_t new_size = encoder_->encode_queue_size_.fetch_sub(1, std::memory_order_relaxed) - 1;
    SignalDequeue(new_size);
  }
}

void VideoEncoderWorker::OnFlush(const FlushMessage& msg) {
  if (!codec_ctx_) {
    FlushComplete(msg.promise_id, true, "");
    return;
  }

  // Send NULL frame to trigger drain
  int ret = avcodec_send_frame(codec_ctx_.get(), nullptr);
  if (ret < 0 && ret != AVERROR_EOF) {
    FlushComplete(msg.promise_id, false, errors::FfmpegErrorString(ret));
    return;
  }

  // Receive all remaining packets
  raii::AVPacketPtr packet = raii::MakeAvPacket();
  if (!packet) {
    FlushComplete(msg.promise_id, false, "Failed to allocate packet");
    return;
  }

  while (!ShouldExit()) {
    ret = avcodec_receive_packet(codec_ctx_.get(), packet.get());

    if (ret == AVERROR_EOF) {
      break;  // All packets drained
    }
    if (ret == AVERROR(EAGAIN)) {
      break;  // Should not happen after null frame
    }
    if (ret < 0) {
      FlushComplete(msg.promise_id, false, errors::FfmpegErrorString(ret));
      return;
    }

    bool is_key = (packet->flags & AV_PKT_FLAG_KEY) != 0;
    int64_t timestamp = packet->pts != AV_NOPTS_VALUE ? packet->pts : 0;
    int64_t duration = packet->duration > 0 ? packet->duration : 0;

    raii::AVPacketPtr outputPacket = raii::CloneAvPacket(packet.get());
    if (outputPacket) {
      OutputChunk(std::move(outputPacket), is_key, timestamp, duration, false);
    }

    av_packet_unref(packet.get());
  }

  // Signal flush complete
  FlushComplete(msg.promise_id, true, "");
}

void VideoEncoderWorker::OnReset() {
  if (codec_ctx_) {
    avcodec_flush_buffers(codec_ctx_.get());
  }
  first_output_after_configure_ = true;
  frame_count_ = 0;
}

void VideoEncoderWorker::OnClose() {
  codec_ctx_.reset();
}

void VideoEncoderWorker::OutputChunk(raii::AVPacketPtr packet, bool is_key,
                                      int64_t ts, int64_t dur, bool include_config) {
  if (!encoder_ || encoder_->state_.IsClosed()) return;

  auto* data = new VideoEncoder::OutputData{
      std::move(packet),
      is_key,
      ts,
      dur,
      include_config,
      {},  // extradata - populated below
      {},  // codec - populated below
      0,   // coded_width - populated below
      0    // coded_height - populated below
  };

  // Copy decoder config data on worker thread (thread-safe)
  // All data is from worker's local copies, avoiding cross-thread access
  if (include_config && codec_ctx_) {
    if (codec_ctx_->extradata && codec_ctx_->extradata_size > 0) {
      data->extradata.assign(
          codec_ctx_->extradata,
          codec_ctx_->extradata + codec_ctx_->extradata_size);
    }
    data->codec = codec_;  // Use worker's local copy, not encoder_->active_config_
    data->coded_width = width_;
    data->coded_height = height_;
  }

  if (!encoder_->output_tsfn_.Call(data)) {
    delete data;
  }
}

void VideoEncoderWorker::OutputError(int code, const std::string& message) {
  if (!encoder_ || encoder_->state_.IsClosed()) return;

  auto* data = new VideoEncoder::ErrorData{code, message};
  if (!encoder_->error_tsfn_.Call(data)) {
    delete data;
  }
}

void VideoEncoderWorker::FlushComplete(uint32_t promise_id, bool success, const std::string& error) {
  if (!encoder_) return;

  auto* data = new VideoEncoder::FlushCompleteData{promise_id, success, error};
  if (!encoder_->flush_tsfn_.Call(data)) {
    delete data;
  }
}

void VideoEncoderWorker::SignalDequeue(uint32_t new_size) {
  if (!encoder_ || encoder_->state_.IsClosed()) return;

  // [SPEC] [[dequeue event scheduled]] - coalesce multiple dequeue events
  // Use compare_exchange to atomically check-and-set; if already scheduled, skip
  bool expected = false;
  if (!encoder_->dequeue_event_scheduled_.compare_exchange_strong(
          expected, true,
          std::memory_order_acq_rel,
          std::memory_order_acquire)) {
    return;  // Already scheduled, coalesce this event
  }

  auto* data = new VideoEncoder::DequeueData{new_size};
  if (!encoder_->dequeue_tsfn_.Call(data)) {
    delete data;
    // Reset flag on TSFN failure
    encoder_->dequeue_event_scheduled_.store(false, std::memory_order_release);
  }
}

}  // namespace webcodecs
