#include "audio_decoder.h"

#include <string>

#include "audio_data.h"
#include "encoded_audio_chunk.h"
#include "shared/codec_registry.h"
#include "error_builder.h"
#include "shared/buffer_utils.h"

namespace webcodecs {

Napi::FunctionReference AudioDecoder::constructor;

// =============================================================================
// AUDIODECODER IMPLEMENTATION
// =============================================================================

Napi::Object AudioDecoder::Init(Napi::Env env, Napi::Object exports) {
  Napi::Function func =
      DefineClass(env, "AudioDecoder",
                  {
                      InstanceAccessor<&AudioDecoder::GetState>("state"),
                      InstanceAccessor<&AudioDecoder::GetDecodeQueueSize>("decodeQueueSize"),
                      InstanceAccessor<&AudioDecoder::GetOndequeue, &AudioDecoder::SetOndequeue>("ondequeue"),
                      InstanceMethod<&AudioDecoder::Configure>("configure"),
                      InstanceMethod<&AudioDecoder::Decode>("decode"),
                      InstanceMethod<&AudioDecoder::Flush>("flush"),
                      InstanceMethod<&AudioDecoder::Reset>("reset"),
                      InstanceMethod<&AudioDecoder::Close>("close"),
                      StaticMethod<&AudioDecoder::IsConfigSupported>("isConfigSupported"),
                  });

  constructor = Napi::Persistent(func);
  constructor.SuppressDestruct();
  exports.Set("AudioDecoder", func);
  return exports;
}

AudioDecoder::AudioDecoder(const Napi::CallbackInfo& info) : Napi::ObjectWrap<AudioDecoder>(info) {
  Napi::Env env = info.Env();

  // [SPEC] Constructor Algorithm
  // AudioDecoderInit requires: output (AudioDataOutputCallback), error (WebCodecsErrorCallback)

  // Validate init object is provided
  if (info.Length() < 1 || !info[0].IsObject()) {
    Napi::TypeError::New(env, "AudioDecoderInit is required").ThrowAsJavaScriptException();
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
  worker_ = std::make_unique<AudioDecoderWorker>(queue_, this);
}

AudioDecoder::~AudioDecoder() {
  Release();
}

void AudioDecoder::Release() {
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

void AudioDecoder::InitializeTSFNs(Napi::Env env) {
  // Create TSFNs with CallJs as the third template parameter.
  // TypedThreadSafeFunction<Context, DataType, CallJsFunc>::New(...)

  // Output TSFN - delivers decoded frames to JS (uses raw AVFrame* pointer)
  using OutputTSFNType = Napi::TypedThreadSafeFunction<AudioDecoder, AVFrame*, &AudioDecoder::OnOutputFrame>;
  auto output_tsfn = OutputTSFNType::New(
      env,
      output_callback_.Value(),
      "AudioDecoder::output",
      0,      // Unlimited queue
      1,      // Initial thread count
      this);  // Context
  output_tsfn_.Init(std::move(output_tsfn));

  // Error TSFN - delivers errors to JS
  using ErrorTSFNType = Napi::TypedThreadSafeFunction<AudioDecoder, ErrorData, &AudioDecoder::OnError>;
  auto error_tsfn = ErrorTSFNType::New(
      env,
      error_callback_.Value(),
      "AudioDecoder::error",
      0, 1, this);
  error_tsfn_.Init(std::move(error_tsfn));

  // Flush and dequeue use dummy callbacks - we handle logic in the handler
  Napi::Function dummyFn = Napi::Function::New(env, [](const Napi::CallbackInfo&) {});

  using FlushTSFNType = Napi::TypedThreadSafeFunction<AudioDecoder, FlushCompleteData, &AudioDecoder::OnFlushComplete>;
  auto flush_tsfn = FlushTSFNType::New(
      env, dummyFn, "AudioDecoder::flush",
      0, 1, this);
  flush_tsfn_.Init(std::move(flush_tsfn));

  using DequeueTSFNType = Napi::TypedThreadSafeFunction<AudioDecoder, DequeueData, &AudioDecoder::OnDequeue>;
  auto dequeue_tsfn = DequeueTSFNType::New(
      env, dummyFn, "AudioDecoder::dequeue",
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

void AudioDecoder::ReleaseTSFNs() {
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

void AudioDecoder::OnOutputFrame(Napi::Env env, Napi::Function jsCallback,
                                  AudioDecoder* context, AVFrame** data) {
  if (!data || !*data) return;

  // Take ownership of the raw frame pointer
  AVFrame* frame = *data;
  delete data;  // Delete the pointer-to-pointer wrapper

  if (!context || context->state_.IsClosed()) {
    av_frame_free(&frame);
    return;
  }

  // Create AudioData JS object from AVFrame
  if (frame && !context->output_callback_.IsEmpty()) {
    // Use frame's pts as timestamp (in time base units, convert to microseconds)
    int64_t timestamp_us = frame->pts;
    Napi::Object jsAudioData = AudioData::CreateFromFrame(env, frame, timestamp_us);
    if (!jsAudioData.IsEmpty()) {
      context->output_callback_.Call({jsAudioData});
    }
  }

  // Free the frame after use (AudioData::CreateFromFrame clones it)
  av_frame_free(&frame);
}

void AudioDecoder::OnError(Napi::Env env, Napi::Function jsCallback,
                           AudioDecoder* context, ErrorData* data) {
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

void AudioDecoder::OnFlushComplete(Napi::Env env, Napi::Function jsCallback,
                                   AudioDecoder* context, FlushCompleteData* data) {
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

void AudioDecoder::OnDequeue(Napi::Env env, Napi::Function jsCallback,
                             AudioDecoder* context, DequeueData* data) {
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

Napi::Value AudioDecoder::GetState(const Napi::CallbackInfo& info) {
  return Napi::String::New(info.Env(), state_.ToString());
}

Napi::Value AudioDecoder::GetDecodeQueueSize(const Napi::CallbackInfo& info) {
  return Napi::Number::New(info.Env(), static_cast<double>(decode_queue_size_.load(std::memory_order_acquire)));
}

Napi::Value AudioDecoder::GetOndequeue(const Napi::CallbackInfo& info) {
  if (ondequeue_callback_.IsEmpty()) {
    return info.Env().Null();
  }
  return ondequeue_callback_.Value();
}

void AudioDecoder::SetOndequeue(const Napi::CallbackInfo& info, const Napi::Value& value) {
  if (value.IsNull() || value.IsUndefined()) {
    ondequeue_callback_.Reset();
  } else if (value.IsFunction()) {
    ondequeue_callback_ = Napi::Persistent(value.As<Napi::Function>());
  }
}

// --- Methods ---

Napi::Value AudioDecoder::Configure(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] 1. If config is not a valid AudioDecoderConfig, throw TypeError
  if (info.Length() < 1 || !info[0].IsObject()) {
    errors::ThrowTypeError(env, "AudioDecoderConfig is required");
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

  if (config.Has("sampleRate") && config.Get("sampleRate").IsNumber()) {
    active_config_.sample_rate = config.Get("sampleRate").As<Napi::Number>().Int32Value();
  }
  if (config.Has("numberOfChannels") && config.Get("numberOfChannels").IsNumber()) {
    active_config_.number_of_channels = config.Get("numberOfChannels").As<Napi::Number>().Int32Value();
  }
  if (config.Has("description")) {
    Napi::Value desc = config.Get("description");
    const uint8_t* data = nullptr;
    size_t size = 0;
    if (buffer_utils::ExtractBufferData(desc, &data, &size) && data && size > 0) {
      active_config_.description.assign(data, data + size);
    }
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
  AudioControlQueue::ConfigureMessage msg{
      [this, config_copy]() -> bool {
        // This runs on worker thread
        return worker_ != nullptr;  // Actual config happens via OnConfigure
      }};

  // Start worker if not running
  if (!worker_->IsRunning()) {
    worker_->Start();
  }

  // Enqueue configure message
  if (!queue_.Enqueue(std::move(msg))) {
    errors::ThrowInvalidStateError(env, "Failed to enqueue configure");
    return env.Undefined();
  }

  // [SPEC] 3. Set state to "configured"
  // [SPEC] 4. Set key chunk required to true
  state_.transition(raii::AtomicCodecState::State::Unconfigured, raii::AtomicCodecState::State::Configured);
  key_chunk_required_.store(true, std::memory_order_release);

  return env.Undefined();
}

Napi::Value AudioDecoder::Decode(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] 1. If state is not "configured", throw InvalidStateError
  if (!state_.IsConfigured()) {
    errors::ThrowInvalidStateError(env, "decode called on " + std::string(state_.ToString()) + " decoder");
    return env.Undefined();
  }

  // Validate chunk argument
  if (info.Length() < 1 || !info[0].IsObject()) {
    errors::ThrowTypeError(env, "EncodedAudioChunk is required");
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

  // Get chunk data - support both EncodedAudioChunk wrapper and plain objects
  const uint8_t* data = nullptr;
  size_t size = 0;

  // Try to get data from EncodedAudioChunk wrapper
  if (chunk.InstanceOf(EncodedAudioChunk::constructor.Value())) {
    EncodedAudioChunk* enc = Napi::ObjectWrap<EncodedAudioChunk>::Unwrap(chunk);
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
  AudioControlQueue::DecodeMessage msg{std::move(packet)};
  if (!queue_.Enqueue(std::move(msg))) {
    decode_queue_size_.fetch_sub(1, std::memory_order_relaxed);
    errors::ThrowInvalidStateError(env, "Failed to enqueue decode");
    return env.Undefined();
  }

  return env.Undefined();
}

Napi::Value AudioDecoder::Flush(const Napi::CallbackInfo& info) {
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
  AudioControlQueue::FlushMessage msg{promise_id};
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

Napi::Value AudioDecoder::Reset(const Napi::CallbackInfo& info) {
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
  AudioControlQueue::ResetMessage msg{};
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

Napi::Value AudioDecoder::Close(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Immediately aborts all pending work and releases system resources.
  // Close is final - after close(), the decoder cannot be used again.

  // [SPEC] Reject all pending flush promises with AbortError before releasing resources
  // This must happen here (not in Release) because we have a valid Env from JS context.
  {
    std::lock_guard<std::mutex> lock(flush_mutex_);
    for (auto& [id, deferred] : pending_flushes_) {
      Napi::Error error = Napi::Error::New(env, "AbortError: Decoder was closed");
      error.Set("name", Napi::String::New(env, "AbortError"));
      deferred.Reject(error.Value());
    }
    pending_flushes_.clear();
  }

  // Release all resources and transition to closed state
  Release();

  return env.Undefined();
}

Napi::Value AudioDecoder::IsConfigSupported(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] 1. If config is not a valid AudioDecoderConfig, reject with TypeError
  if (info.Length() < 1 || !info[0].IsObject()) {
    Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
    deferred.Reject(Napi::TypeError::New(env, "AudioDecoderConfig is required").Value());
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

  // [SPEC] Create AudioDecoderSupport object
  Napi::Object result = Napi::Object::New(env);
  result.Set("supported", Napi::Boolean::New(env, supported));

  // Clone the config (only recognized members)
  Napi::Object clonedConfig = Napi::Object::New(env);
  clonedConfig.Set("codec", config.Get("codec"));

  if (config.Has("sampleRate") && config.Get("sampleRate").IsNumber()) {
    clonedConfig.Set("sampleRate", config.Get("sampleRate"));
  }
  if (config.Has("numberOfChannels") && config.Get("numberOfChannels").IsNumber()) {
    clonedConfig.Set("numberOfChannels", config.Get("numberOfChannels"));
  }
  if (config.Has("description")) {
    clonedConfig.Set("description", config.Get("description"));
  }

  result.Set("config", clonedConfig);

  deferred.Resolve(result);
  return deferred.Promise();
}

// =============================================================================
// AUDIODECODERWORKER IMPLEMENTATION
// =============================================================================

AudioDecoderWorker::AudioDecoderWorker(AudioControlQueue& queue, AudioDecoder* decoder)
    : CodecWorker<AudioControlQueue>(queue), decoder_(decoder) {
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

    auto* data = new AudioDecoder::ErrorData{error_code, message};
    if (!decoder_->error_tsfn_.Call(data)) {
      delete data;
    }
  });

  SetFlushCompleteCallback([this](uint32_t promise_id, bool success, const std::string& error) {
    if (!decoder_) return;

    auto* data = new AudioDecoder::FlushCompleteData{promise_id, success, error};
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

    auto* data = new AudioDecoder::DequeueData{new_queue_size};
    if (!decoder_->dequeue_tsfn_.Call(data)) {
      delete data;
      // Reset flag on TSFN failure
      decoder_->dequeue_event_scheduled_.store(false, std::memory_order_release);
    }
  });
}

AudioDecoderWorker::~AudioDecoderWorker() {
  Stop();
}

bool AudioDecoderWorker::OnConfigure(const ConfigureMessage& msg) {
  // Get config from parent decoder
  if (!decoder_) return false;

  const AudioDecoder::DecoderConfig& config = decoder_->active_config_;

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
  if (config.sample_rate > 0) {
    codec_ctx_->sample_rate = config.sample_rate;
    sample_rate_ = config.sample_rate;
  }
  if (config.number_of_channels > 0) {
    // FFmpeg 5.0+ uses ch_layout instead of channels
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(59, 37, 100)
    av_channel_layout_default(&codec_ctx_->ch_layout, config.number_of_channels);
#else
    codec_ctx_->channels = config.number_of_channels;
    codec_ctx_->channel_layout = av_get_default_channel_layout(config.number_of_channels);
#endif
    channels_ = config.number_of_channels;
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

  // Open codec
  int ret = avcodec_open2(codec_ctx_.get(), decoder, nullptr);
  if (ret < 0) {
    OutputError(ret, "Failed to open decoder");
    codec_ctx_.reset();
    return false;
  }

  // Update sample format from codec context
  sample_fmt_ = codec_ctx_->sample_fmt;
  sample_rate_ = codec_ctx_->sample_rate;
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(59, 37, 100)
  channels_ = codec_ctx_->ch_layout.nb_channels;
#else
  channels_ = codec_ctx_->channels;
#endif

  return true;
}

void AudioDecoderWorker::OnDecode(const DecodeMessage& msg) {
  // [SPEC] Always decrement queue size when decode work is processed, even on error
  // Use a lambda to ensure dequeue is signaled on all exit paths
  auto signal_dequeue_on_exit = [this]() {
    if (decoder_) {
      uint32_t new_size = decoder_->decode_queue_size_.fetch_sub(1, std::memory_order_relaxed) - 1;
      SignalDequeue(new_size);
    }
  };

  if (!codec_ctx_ || ShouldExit()) {
    signal_dequeue_on_exit();
    return;
  }

  // Send packet to decoder
  int ret = avcodec_send_packet(codec_ctx_.get(), msg.packet.get());
  if (ret < 0 && ret != AVERROR(EAGAIN)) {
    OutputError(ret, "Failed to send packet to decoder");
    signal_dequeue_on_exit();
    return;
  }

  // Receive all available frames (Fabrice's pull model)
  raii::AVFramePtr frame = raii::MakeAvFrame();
  if (!frame) {
    OutputError(AVERROR(ENOMEM), "Failed to allocate frame");
    signal_dequeue_on_exit();
    return;
  }

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
      signal_dequeue_on_exit();
      return;
    }

    // Clone frame for output (original stays in decoder)
    raii::AVFramePtr output_frame = raii::CloneAvFrame(frame.get());
    if (output_frame) {
      OutputFrame(std::move(output_frame));
    }

    // Reset frame for next iteration
    av_frame_unref(frame.get());
  }

  // Normal exit path
  signal_dequeue_on_exit();
}

void AudioDecoderWorker::OnFlush(const FlushMessage& msg) {
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

  // Signal flush complete
  FlushComplete(msg.promise_id, true, "");
}

void AudioDecoderWorker::OnReset() {
  if (codec_ctx_) {
    avcodec_flush_buffers(codec_ctx_.get());
  }
}

void AudioDecoderWorker::OnClose() {
  codec_ctx_.reset();
}

}  // namespace webcodecs
