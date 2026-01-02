#include "video_decoder.h"

#include <string>

#include "video_frame.h"
#include "encoded_video_chunk.h"
#include "shared/codec_registry.h"
#include "error_builder.h"
#include "shared/buffer_utils.h"

namespace webcodecs {

Napi::FunctionReference VideoDecoder::constructor;

// =============================================================================
// VIDEODECODER IMPLEMENTATION
// =============================================================================

Napi::Object VideoDecoder::Init(Napi::Env env, Napi::Object exports) {
  Napi::Function func =
      DefineClass(env, "VideoDecoder",
                  {
                      InstanceAccessor<&VideoDecoder::GetState>("state"),
                      InstanceAccessor<&VideoDecoder::GetDecodeQueueSize>("decodeQueueSize"),
                      InstanceAccessor<&VideoDecoder::GetOndequeue, &VideoDecoder::SetOndequeue>("ondequeue"),
                      InstanceMethod<&VideoDecoder::Configure>("configure"),
                      InstanceMethod<&VideoDecoder::Decode>("decode"),
                      InstanceMethod<&VideoDecoder::Flush>("flush"),
                      InstanceMethod<&VideoDecoder::Reset>("reset"),
                      InstanceMethod<&VideoDecoder::Close>("close"),
                      StaticMethod<&VideoDecoder::IsConfigSupported>("isConfigSupported"),
                  });

  constructor = Napi::Persistent(func);
  constructor.SuppressDestruct();
  exports.Set("VideoDecoder", func);
  return exports;
}

VideoDecoder::VideoDecoder(const Napi::CallbackInfo& info) : Napi::ObjectWrap<VideoDecoder>(info) {
  Napi::Env env = info.Env();

  // [SPEC] Constructor Algorithm
  // VideoDecoderInit requires: output (VideoFrameOutputCallback), error (WebCodecsErrorCallback)

  // Validate init object is provided
  if (info.Length() < 1 || !info[0].IsObject()) {
    Napi::TypeError::New(env, "VideoDecoderInit is required").ThrowAsJavaScriptException();
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
  worker_ = std::make_unique<VideoDecoderWorker>(queue_, this);
}

VideoDecoder::~VideoDecoder() {
  Release();
}

void VideoDecoder::Release() {
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
    // Note: We can't reject here as we may not have a valid Env
    // The promises will be abandoned when the decoder is garbage collected
    pending_flushes_.clear();
  }

  // Reset decode queue size
  decode_queue_size_.store(0, std::memory_order_release);

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

void VideoDecoder::InitializeTSFNs(Napi::Env env) {
  // Create TSFNs with CallJs as the third template parameter.
  // TypedThreadSafeFunction<Context, DataType, CallJsFunc>::New(...)

  // Output TSFN - delivers decoded frames to JS (uses raw AVFrame* pointer)
  using OutputTSFNType = Napi::TypedThreadSafeFunction<VideoDecoder, AVFrame*, &VideoDecoder::OnOutputFrame>;
  auto output_tsfn = OutputTSFNType::New(
      env,
      output_callback_.Value(),
      "VideoDecoder::output",
      0,      // Unlimited queue
      1,      // Initial thread count
      this);  // Context
  output_tsfn_.Init(std::move(output_tsfn));

  // Error TSFN - delivers errors to JS
  using ErrorTSFNType = Napi::TypedThreadSafeFunction<VideoDecoder, ErrorData, &VideoDecoder::OnError>;
  auto error_tsfn = ErrorTSFNType::New(
      env,
      error_callback_.Value(),
      "VideoDecoder::error",
      0, 1, this);
  error_tsfn_.Init(std::move(error_tsfn));

  // Flush and dequeue use dummy callbacks - we handle logic in the handler
  Napi::Function dummyFn = Napi::Function::New(env, [](const Napi::CallbackInfo&) {});

  using FlushTSFNType = Napi::TypedThreadSafeFunction<VideoDecoder, FlushCompleteData, &VideoDecoder::OnFlushComplete>;
  auto flush_tsfn = FlushTSFNType::New(
      env, dummyFn, "VideoDecoder::flush",
      0, 1, this);
  flush_tsfn_.Init(std::move(flush_tsfn));

  using DequeueTSFNType = Napi::TypedThreadSafeFunction<VideoDecoder, DequeueData, &VideoDecoder::OnDequeue>;
  auto dequeue_tsfn = DequeueTSFNType::New(
      env, dummyFn, "VideoDecoder::dequeue",
      0, 1, this);
  dequeue_tsfn_.Init(std::move(dequeue_tsfn));

  // Unref all TSFNs to allow Node.js to exit cleanly.
  // TSFNs acquire a reference by default that prevents process exit.
  // Since we control the decoder lifecycle, we can safely unref.
  output_tsfn_.Unref(env);
  error_tsfn_.Unref(env);
  flush_tsfn_.Unref(env);
  dequeue_tsfn_.Unref(env);
}

void VideoDecoder::ReleaseTSFNs() {
  // Note: We've already unref'd the TSFNs in InitializeTSFNs().
  // During normal operation (explicit close()), releasing is safe.
  // During GC cleanup after env is torn down, Release() may cause issues.
  // The SafeThreadSafeFunction::Release() is idempotent and checks if already released.
  output_tsfn_.Release();
  error_tsfn_.Release();
  flush_tsfn_.Release();
  dequeue_tsfn_.Release();
}

// --- TSFN Callback Handlers ---

void VideoDecoder::OnOutputFrame(Napi::Env env, Napi::Function jsCallback,
                                  VideoDecoder* context, AVFrame** data) {
  if (!data || !*data) return;

  // Take ownership of the raw frame pointer
  AVFrame* frame = *data;
  delete data;  // Delete the pointer-to-pointer wrapper

  if (!context || context->state_.IsClosed()) {
    av_frame_free(&frame);
    return;
  }

  // Create VideoFrame JS object from AVFrame
  if (frame && !context->output_callback_.IsEmpty()) {
    Napi::Object jsFrame = VideoFrame::CreateFromAVFrame(env, frame);
    if (!jsFrame.IsEmpty()) {
      context->output_callback_.Call({jsFrame});
    }
  }

  // Free the frame after use (VideoFrame::CreateFromAVFrame clones it)
  av_frame_free(&frame);
}

void VideoDecoder::OnError(Napi::Env env, Napi::Function jsCallback,
                           VideoDecoder* context, ErrorData* data) {
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

  // Close the decoder on error
  context->state_.Close();
}

void VideoDecoder::OnFlushComplete(Napi::Env env, Napi::Function jsCallback,
                                   VideoDecoder* context, FlushCompleteData* data) {
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

void VideoDecoder::OnDequeue(Napi::Env env, Napi::Function jsCallback,
                             VideoDecoder* context, DequeueData* data) {
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

Napi::Value VideoDecoder::GetState(const Napi::CallbackInfo& info) {
  return Napi::String::New(info.Env(), state_.ToString());
}

Napi::Value VideoDecoder::GetDecodeQueueSize(const Napi::CallbackInfo& info) {
  return Napi::Number::New(info.Env(), static_cast<double>(decode_queue_size_.load(std::memory_order_acquire)));
}

Napi::Value VideoDecoder::GetOndequeue(const Napi::CallbackInfo& info) {
  if (ondequeue_callback_.IsEmpty()) {
    return info.Env().Null();
  }
  return ondequeue_callback_.Value();
}

void VideoDecoder::SetOndequeue(const Napi::CallbackInfo& info, const Napi::Value& value) {
  if (value.IsNull() || value.IsUndefined()) {
    ondequeue_callback_.Reset();
  } else if (value.IsFunction()) {
    ondequeue_callback_ = Napi::Persistent(value.As<Napi::Function>());
  }
}

// --- Methods ---

Napi::Value VideoDecoder::Configure(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] 1. If config is not a valid VideoDecoderConfig, throw TypeError
  if (info.Length() < 1 || !info[0].IsObject()) {
    errors::ThrowTypeError(env, "VideoDecoderConfig is required");
    return env.Undefined();
  }

  // [SPEC] 2. If state is "closed", throw InvalidStateError
  if (state_.IsClosed()) {
    errors::ThrowInvalidStateError(env, "configure called on closed decoder");
    return env.Undefined();
  }

  Napi::Object config = info[0].As<Napi::Object>();

  // Required: codec string
  if (!config.Has("codec") || !config.Get("codec").IsString()) {
    errors::ThrowTypeError(env, "codec is required and must be a string");
    return env.Undefined();
  }

  // Deep copy configuration for async processing
  active_config_.codec = config.Get("codec").As<Napi::String>().Utf8Value();

  if (config.Has("codedWidth") && config.Get("codedWidth").IsNumber()) {
    active_config_.coded_width = config.Get("codedWidth").As<Napi::Number>().Int32Value();
  }
  if (config.Has("codedHeight") && config.Get("codedHeight").IsNumber()) {
    active_config_.coded_height = config.Get("codedHeight").As<Napi::Number>().Int32Value();
  }
  if (config.Has("description")) {
    Napi::Value desc = config.Get("description");
    const uint8_t* data = nullptr;
    size_t size = 0;
    if (buffer_utils::ExtractBufferData(desc, &data, &size) && data && size > 0) {
      active_config_.description.assign(data, data + size);
    }
  }
  if (config.Has("hardwareAcceleration") && config.Get("hardwareAcceleration").IsString()) {
    active_config_.hardware_acceleration = config.Get("hardwareAcceleration").As<Napi::String>().Utf8Value();
  }
  if (config.Has("optimizeForLatency") && config.Get("optimizeForLatency").IsBoolean()) {
    active_config_.optimize_for_latency = config.Get("optimizeForLatency").As<Napi::Boolean>().Value();
  }

  // Validate codec string before queuing (fail fast)
  auto codec_info = ParseCodecString(active_config_.codec);
  if (!codec_info) {
    errors::ThrowNotSupportedError(env, "Unsupported codec: " + active_config_.codec);
    return env.Undefined();
  }

  const AVCodec* decoder = avcodec_find_decoder(codec_info->codec_id);
  if (!decoder) {
    errors::ThrowNotSupportedError(env, "No decoder available for: " + active_config_.codec);
    return env.Undefined();
  }

  // Create configure message with a lambda that captures the config
  DecoderConfig config_copy = active_config_;
  VideoControlQueue::ConfigureMessage msg{
      [this, config_copy]() -> bool {
        // This runs on worker thread
        return worker_ != nullptr;  // Actual config happens via OnConfigure
      }};

  // Start worker if not running
  if (!worker_->IsRunning()) {
    worker_->Start();
  }

  // [SPEC] Set [[message queue blocked]] before configure
  queue_.SetBlocked(true);

  // Enqueue configure message
  if (!queue_.Enqueue(std::move(msg))) {
    queue_.SetBlocked(false);  // Reset on failure
    errors::ThrowInvalidStateError(env, "Failed to enqueue configure");
    return env.Undefined();
  }

  // [SPEC] 3. Set state to "configured"
  // [SPEC] 4. Set key chunk required to true
  state_.transition(raii::AtomicCodecState::State::Unconfigured, raii::AtomicCodecState::State::Configured);
  key_chunk_required_.store(true, std::memory_order_release);

  return env.Undefined();
}

Napi::Value VideoDecoder::Decode(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] 1. If state is not "configured", throw InvalidStateError
  if (!state_.IsConfigured()) {
    errors::ThrowInvalidStateError(env, "decode called on " + std::string(state_.ToString()) + " decoder");
    return env.Undefined();
  }

  // Validate chunk argument
  if (info.Length() < 1 || !info[0].IsObject()) {
    errors::ThrowTypeError(env, "EncodedVideoChunk is required");
    return env.Undefined();
  }

  Napi::Object chunk = info[0].As<Napi::Object>();

  // Get chunk type
  std::string chunk_type;
  if (chunk.Has("type") && chunk.Get("type").IsString()) {
    chunk_type = chunk.Get("type").As<Napi::String>().Utf8Value();
  }

  // [SPEC] 2. If key chunk required is true, check for key frame
  if (key_chunk_required_.load(std::memory_order_acquire)) {
    if (chunk_type != "key") {
      errors::ThrowDataError(env, "A key frame is required");
      return env.Undefined();
    }
    key_chunk_required_.store(false, std::memory_order_release);
  }

  // Get chunk data - support both EncodedVideoChunk wrapper and plain objects
  const uint8_t* data = nullptr;
  size_t size = 0;

  // Try to get data from EncodedVideoChunk wrapper
  if (chunk.InstanceOf(EncodedVideoChunk::constructor.Value())) {
    EncodedVideoChunk* enc = Napi::ObjectWrap<EncodedVideoChunk>::Unwrap(chunk);
    const AVPacket* pkt = enc->packet();
    if (pkt && pkt->data && pkt->size > 0) {
      data = pkt->data;
      size = static_cast<size_t>(pkt->size);
    }
  } else if (chunk.Has("data")) {
    // Plain object with data property
    buffer_utils::ExtractBufferData(chunk.Get("data"), &data, &size);
  }

  if (!data || size == 0) {
    errors::ThrowTypeError(env, "Chunk data is required");
    return env.Undefined();
  }

  // Get timestamp (microseconds)
  int64_t timestamp = 0;
  if (chunk.Has("timestamp") && chunk.Get("timestamp").IsNumber()) {
    timestamp = chunk.Get("timestamp").As<Napi::Number>().Int64Value();
  }

  // Create AVPacket from data (deep copy for async processing)
  raii::AVPacketPtr packet = buffer_utils::CreatePacketFromBuffer(data, size);
  if (!packet) {
    errors::ThrowEncodingError(env, "Failed to create packet");
    return env.Undefined();
  }

  // Set packet timestamp
  packet->pts = timestamp;
  packet->dts = timestamp;

  // Set key frame flag
  if (chunk_type == "key") {
    packet->flags |= AV_PKT_FLAG_KEY;
  }

  // [SPEC] 3. Increment decodeQueueSize
  decode_queue_size_.fetch_add(1, std::memory_order_relaxed);

  // Enqueue decode message
  VideoControlQueue::DecodeMessage msg{std::move(packet)};
  if (!queue_.Enqueue(std::move(msg))) {
    decode_queue_size_.fetch_sub(1, std::memory_order_relaxed);
    errors::ThrowInvalidStateError(env, "Failed to enqueue decode");
    return env.Undefined();
  }

  return env.Undefined();
}

Napi::Value VideoDecoder::Flush(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] 1. If state is not "configured", reject with InvalidStateError
  if (!state_.IsConfigured()) {
    Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
    Napi::Error error =
        Napi::Error::New(env, "InvalidStateError: flush called on " + std::string(state_.ToString()) + " decoder");
    error.Set("name", Napi::String::New(env, "InvalidStateError"));
    deferred.Reject(error.Value());
    return deferred.Promise();
  }

  // [SPEC] 2. Set key chunk required to true
  key_chunk_required_.store(true, std::memory_order_release);

  // [SPEC] 3. Create promise
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

  // Return the promise (will be resolved by worker via TSFN)
  {
    std::lock_guard<std::mutex> lock(flush_mutex_);
    auto it = pending_flushes_.find(promise_id);
    if (it != pending_flushes_.end()) {
      return it->second.Promise();
    }
  }
  return Napi::Promise::Deferred::New(env).Promise();
}

Napi::Value VideoDecoder::Reset(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] If state is "closed", throw InvalidStateError
  if (state_.IsClosed()) {
    errors::ThrowInvalidStateError(env, "reset called on closed decoder");
    return env.Undefined();
  }

  // Clear the queue (returns dropped packets for cleanup)
  auto dropped = queue_.Clear();
  // Packets are automatically cleaned up by RAII

  // Adjust queue size
  decode_queue_size_.store(0, std::memory_order_release);

  // Enqueue reset message (ignore return - queue might be closed during reset)
  VideoControlQueue::ResetMessage msg{};
  (void)queue_.Enqueue(std::move(msg));

  // Reset key chunk requirement
  key_chunk_required_.store(true, std::memory_order_release);

  // Reject all pending flush promises
  {
    std::lock_guard<std::mutex> lock(flush_mutex_);
    for (auto& [id, deferred] : pending_flushes_) {
      Napi::Error error = Napi::Error::New(env, "AbortError: Decoder was reset");
      error.Set("name", Napi::String::New(env, "AbortError"));
      deferred.Reject(error.Value());
    }
    pending_flushes_.clear();
  }

  // Transition state to unconfigured
  state_.transition(raii::AtomicCodecState::State::Configured, raii::AtomicCodecState::State::Unconfigured);

  return env.Undefined();
}

Napi::Value VideoDecoder::Close(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Immediately aborts all pending work and releases system resources.
  // Close is final - after close(), the decoder cannot be used again.

  // Release all resources and transition to closed state
  Release();

  return env.Undefined();
}

Napi::Value VideoDecoder::IsConfigSupported(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] 1. If config is not a valid VideoDecoderConfig, reject with TypeError
  if (info.Length() < 1 || !info[0].IsObject()) {
    Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
    deferred.Reject(Napi::TypeError::New(env, "VideoDecoderConfig is required").Value());
    return deferred.Promise();
  }

  Napi::Object config = info[0].As<Napi::Object>();

  // Required: codec string
  if (!config.Has("codec") || !config.Get("codec").IsString()) {
    Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
    deferred.Reject(Napi::TypeError::New(env, "codec is required and must be a string").Value());
    return deferred.Promise();
  }

  std::string codec_string = config.Get("codec").As<Napi::String>().Utf8Value();

  // [SPEC] 2. Create promise
  Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);

  // Check if codec is supported
  bool supported = IsCodecSupported(codec_string);

  // [SPEC] Create VideoDecoderSupport object
  Napi::Object result = Napi::Object::New(env);
  result.Set("supported", Napi::Boolean::New(env, supported));

  // Clone the config (only recognized members)
  Napi::Object clonedConfig = Napi::Object::New(env);
  clonedConfig.Set("codec", config.Get("codec"));

  if (config.Has("codedWidth") && config.Get("codedWidth").IsNumber()) {
    clonedConfig.Set("codedWidth", config.Get("codedWidth"));
  }
  if (config.Has("codedHeight") && config.Get("codedHeight").IsNumber()) {
    clonedConfig.Set("codedHeight", config.Get("codedHeight"));
  }
  if (config.Has("description")) {
    clonedConfig.Set("description", config.Get("description"));
  }
  if (config.Has("hardwareAcceleration") && config.Get("hardwareAcceleration").IsString()) {
    clonedConfig.Set("hardwareAcceleration", config.Get("hardwareAcceleration"));
  }
  if (config.Has("optimizeForLatency") && config.Get("optimizeForLatency").IsBoolean()) {
    clonedConfig.Set("optimizeForLatency", config.Get("optimizeForLatency"));
  }

  result.Set("config", clonedConfig);

  deferred.Resolve(result);
  return deferred.Promise();
}

// =============================================================================
// VIDEODECODERWORKER IMPLEMENTATION
// =============================================================================

VideoDecoderWorker::VideoDecoderWorker(VideoControlQueue& queue, VideoDecoder* decoder)
    : CodecWorker<VideoControlQueue>(queue), decoder_(decoder) {
  // Set up callbacks to deliver results via TSFN
  SetOutputFrameCallback([this](raii::AVFramePtr frame) {
    if (!decoder_ || decoder_->state_.IsClosed()) return;

    // Release unique_ptr ownership and wrap raw pointer for TSFN transfer
    // The OnOutputFrame handler takes ownership and calls av_frame_free
    AVFrame* raw_frame = frame.release();
    auto* data = new AVFrame*(raw_frame);
    if (!decoder_->output_tsfn_.Call(data)) {
      av_frame_free(&raw_frame);  // TSFN released, clean up
      delete data;
    }
  });

  SetOutputErrorCallback([this](int error_code, const std::string& message) {
    if (!decoder_ || decoder_->state_.IsClosed()) return;

    auto* data = new VideoDecoder::ErrorData{error_code, message};
    if (!decoder_->error_tsfn_.Call(data)) {
      delete data;
    }
  });

  SetFlushCompleteCallback([this](uint32_t promise_id, bool success, const std::string& error) {
    if (!decoder_) return;

    auto* data = new VideoDecoder::FlushCompleteData{promise_id, success, error};
    if (!decoder_->flush_tsfn_.Call(data)) {
      delete data;
    }
  });

  SetDequeueCallback([this](uint32_t new_queue_size) {
    if (!decoder_ || decoder_->state_.IsClosed()) return;

    // [SPEC] [[dequeue event scheduled]] - coalesce multiple dequeue events
    // Use compare_exchange to atomically check-and-set; if already scheduled, skip
    bool expected = false;
    if (!decoder_->dequeue_event_scheduled_.compare_exchange_strong(
            expected, true,
            std::memory_order_acq_rel,
            std::memory_order_acquire)) {
      return;  // Already scheduled, coalesce this event
    }

    auto* data = new VideoDecoder::DequeueData{new_queue_size};
    if (!decoder_->dequeue_tsfn_.Call(data)) {
      delete data;
      // Reset flag on TSFN failure
      decoder_->dequeue_event_scheduled_.store(false, std::memory_order_release);
    }
  });
}

VideoDecoderWorker::~VideoDecoderWorker() {
  Stop();
}

bool VideoDecoderWorker::OnConfigure(const ConfigureMessage& msg) {
  // Get config from parent decoder
  if (!decoder_) return false;

  // Helper to unblock queue on exit (RAII pattern)
  struct ScopeGuard {
    VideoDecoder* d;
    ~ScopeGuard() { if (d) d->queue_.SetBlocked(false); }
  } guard{decoder_};

  const VideoDecoder::DecoderConfig& config = decoder_->active_config_;

  // Parse codec string
  auto codec_info = ParseCodecString(config.codec);
  if (!codec_info) {
    OutputError(AVERROR_DECODER_NOT_FOUND, "Unsupported codec: " + config.codec);
    return false;
  }

  // Find FFmpeg decoder
  const AVCodec* decoder = avcodec_find_decoder(codec_info->codec_id);
  if (!decoder) {
    OutputError(AVERROR_DECODER_NOT_FOUND, "No decoder available for: " + config.codec);
    return false;
  }

  // Allocate codec context
  codec_ctx_ = raii::MakeAvCodecContext(decoder);
  if (!codec_ctx_) {
    OutputError(AVERROR(ENOMEM), "Failed to allocate codec context");
    return false;
  }

  // Set codec parameters
  if (config.coded_width > 0) {
    codec_ctx_->width = config.coded_width;
    width_ = config.coded_width;
  }
  if (config.coded_height > 0) {
    codec_ctx_->height = config.coded_height;
    height_ = config.coded_height;
  }

  // Handle description (extradata)
  if (!config.description.empty()) {
    codec_ctx_->extradata = static_cast<uint8_t*>(
        av_mallocz(config.description.size() + AV_INPUT_BUFFER_PADDING_SIZE));
    if (codec_ctx_->extradata) {
      std::memcpy(codec_ctx_->extradata, config.description.data(), config.description.size());
      codec_ctx_->extradata_size = static_cast<int>(config.description.size());
    }
  }

  // Set threading model
  codec_ctx_->thread_count = 0;  // Auto-detect
  codec_ctx_->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;

  // Open codec
  int ret = avcodec_open2(codec_ctx_.get(), decoder, nullptr);
  if (ret < 0) {
    OutputError(ret, "Failed to open decoder");
    codec_ctx_.reset();
    return false;
  }

  // Set key chunk required
  key_chunk_required_.store(true, std::memory_order_release);

  return true;
  // [SPEC] [[message queue blocked]] reset by ScopeGuard destructor
}

void VideoDecoderWorker::OnDecode(const DecodeMessage& msg) {
  if (!codec_ctx_ || ShouldExit()) return;

  // Send packet to decoder
  int ret = avcodec_send_packet(codec_ctx_.get(), msg.packet.get());

  // [SPEC] [[codec saturated]] - track when codec cannot accept more input
  if (ret == AVERROR(EAGAIN)) {
    // Codec input buffer full - set saturation flag
    if (decoder_) {
      decoder_->codec_saturated_.store(true, std::memory_order_release);
    }
  } else if (ret < 0) {
    OutputError(ret, "Failed to send packet to decoder");
    return;
  }

  // Receive all available frames (Fabrice's pull model)
  raii::AVFramePtr frame = raii::MakeAvFrame();
  if (!frame) {
    OutputError(AVERROR(ENOMEM), "Failed to allocate frame");
    return;
  }

  bool received_frame = false;
  while (!ShouldExit()) {
    ret = avcodec_receive_frame(codec_ctx_.get(), frame.get());

    if (ret == AVERROR(EAGAIN)) {
      break;  // Need more input
    }
    if (ret == AVERROR_EOF) {
      break;  // End of stream
    }
    if (ret < 0) {
      OutputError(ret, "Error receiving frame");
      return;
    }

    received_frame = true;

    // Update format info from first frame
    if (format_ == AV_PIX_FMT_NONE) {
      format_ = static_cast<AVPixelFormat>(frame->format);
      width_ = frame->width;
      height_ = frame->height;
    }

    // Clone frame for output (original stays in decoder)
    raii::AVFramePtr output_frame = raii::CloneAvFrame(frame.get());
    if (output_frame) {
      OutputFrame(std::move(output_frame));
    }

    // Reset frame for next iteration
    av_frame_unref(frame.get());
  }

  // [SPEC] Clear [[codec saturated]] after receiving output
  if (received_frame && decoder_) {
    decoder_->codec_saturated_.store(false, std::memory_order_release);
  }

  // Decrement queue size and signal dequeue
  if (decoder_) {
    uint32_t new_size = decoder_->decode_queue_size_.fetch_sub(1, std::memory_order_relaxed) - 1;
    SignalDequeue(new_size);
  }
}

void VideoDecoderWorker::OnFlush(const FlushMessage& msg) {
  if (!codec_ctx_) {
    FlushComplete(msg.promise_id, true, "");
    return;
  }

  // Send NULL packet to trigger drain
  int ret = avcodec_send_packet(codec_ctx_.get(), nullptr);
  if (ret < 0 && ret != AVERROR_EOF) {
    FlushComplete(msg.promise_id, false, errors::FfmpegErrorString(ret));
    return;
  }

  // Receive all remaining frames
  raii::AVFramePtr frame = raii::MakeAvFrame();
  if (!frame) {
    FlushComplete(msg.promise_id, false, "Failed to allocate frame");
    return;
  }

  while (!ShouldExit()) {
    ret = avcodec_receive_frame(codec_ctx_.get(), frame.get());

    if (ret == AVERROR_EOF) {
      break;  // All frames drained
    }
    if (ret == AVERROR(EAGAIN)) {
      break;  // Should not happen after null packet
    }
    if (ret < 0) {
      FlushComplete(msg.promise_id, false, errors::FfmpegErrorString(ret));
      return;
    }

    // Clone and output frame
    raii::AVFramePtr output_frame = raii::CloneAvFrame(frame.get());
    if (output_frame) {
      OutputFrame(std::move(output_frame));
    }

    av_frame_unref(frame.get());
  }

  // Set key chunk required
  key_chunk_required_.store(true, std::memory_order_release);

  // Signal flush complete
  FlushComplete(msg.promise_id, true, "");
}

void VideoDecoderWorker::OnReset() {
  if (codec_ctx_) {
    avcodec_flush_buffers(codec_ctx_.get());
  }
  key_chunk_required_.store(true, std::memory_order_release);
}

void VideoDecoderWorker::OnClose() {
  codec_ctx_.reset();
}

}  // namespace webcodecs
