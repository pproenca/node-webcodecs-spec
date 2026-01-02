#include "audio_encoder.h"

#include <string>

#include "audio_data.h"
#include "encoded_audio_chunk.h"
#include "shared/codec_registry.h"
#include "error_builder.h"
#include "shared/buffer_utils.h"

namespace webcodecs {

Napi::FunctionReference AudioEncoder::constructor;

// =============================================================================
// AUDIOENCODER IMPLEMENTATION
// =============================================================================

Napi::Object AudioEncoder::Init(Napi::Env env, Napi::Object exports) {
  Napi::Function func =
      DefineClass(env, "AudioEncoder",
                  {
                      InstanceAccessor<&AudioEncoder::GetState>("state"),
                      InstanceAccessor<&AudioEncoder::GetEncodeQueueSize>("encodeQueueSize"),
                      InstanceAccessor<&AudioEncoder::GetOndequeue, &AudioEncoder::SetOndequeue>("ondequeue"),
                      InstanceMethod<&AudioEncoder::Configure>("configure"),
                      InstanceMethod<&AudioEncoder::Encode>("encode"),
                      InstanceMethod<&AudioEncoder::Flush>("flush"),
                      InstanceMethod<&AudioEncoder::Reset>("reset"),
                      InstanceMethod<&AudioEncoder::Close>("close"),
                      StaticMethod<&AudioEncoder::IsConfigSupported>("isConfigSupported"),
                  });

  constructor = Napi::Persistent(func);
  constructor.SuppressDestruct();
  exports.Set("AudioEncoder", func);
  return exports;
}

AudioEncoder::AudioEncoder(const Napi::CallbackInfo& info) : Napi::ObjectWrap<AudioEncoder>(info) {
  Napi::Env env = info.Env();

  // [SPEC] Constructor Algorithm
  // AudioEncoderInit requires: output (EncodedAudioChunkOutputCallback), error (WebCodecsErrorCallback)

  // Validate init object is provided
  if (info.Length() < 1 || !info[0].IsObject()) {
    Napi::TypeError::New(env, "AudioEncoderInit is required").ThrowAsJavaScriptException();
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
  worker_ = std::make_unique<AudioEncoderWorker>(queue_, this);
}

AudioEncoder::~AudioEncoder() {
  Release();
}

void AudioEncoder::Release() {
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

void AudioEncoder::InitializeTSFNs(Napi::Env env) {
  // Create TSFNs with CallJs as the third template parameter.

  // Output TSFN - delivers encoded chunks to JS
  using OutputTSFNType = Napi::TypedThreadSafeFunction<AudioEncoder, OutputData, &AudioEncoder::OnOutputChunk>;
  auto output_tsfn = OutputTSFNType::New(
      env,
      output_callback_.Value(),
      "AudioEncoder::output",
      0,      // Unlimited queue
      1,      // Initial thread count
      this);  // Context
  output_tsfn_.Init(std::move(output_tsfn));

  // Error TSFN - delivers errors to JS
  using ErrorTSFNType = Napi::TypedThreadSafeFunction<AudioEncoder, ErrorData, &AudioEncoder::OnError>;
  auto error_tsfn = ErrorTSFNType::New(
      env,
      error_callback_.Value(),
      "AudioEncoder::error",
      0, 1, this);
  error_tsfn_.Init(std::move(error_tsfn));

  // Flush and dequeue use dummy callbacks - we handle logic in the handler
  Napi::Function dummyFn = Napi::Function::New(env, [](const Napi::CallbackInfo&) {});

  using FlushTSFNType = Napi::TypedThreadSafeFunction<AudioEncoder, FlushCompleteData, &AudioEncoder::OnFlushComplete>;
  auto flush_tsfn = FlushTSFNType::New(
      env, dummyFn, "AudioEncoder::flush",
      0, 1, this);
  flush_tsfn_.Init(std::move(flush_tsfn));

  using DequeueTSFNType = Napi::TypedThreadSafeFunction<AudioEncoder, DequeueData, &AudioEncoder::OnDequeue>;
  auto dequeue_tsfn = DequeueTSFNType::New(
      env, dummyFn, "AudioEncoder::dequeue",
      0, 1, this);
  dequeue_tsfn_.Init(std::move(dequeue_tsfn));

  // Unref all TSFNs to allow Node.js to exit cleanly
  output_tsfn_.Unref(env);
  error_tsfn_.Unref(env);
  flush_tsfn_.Unref(env);
  dequeue_tsfn_.Unref(env);
}

void AudioEncoder::ReleaseTSFNs() {
  output_tsfn_.Release();
  error_tsfn_.Release();
  flush_tsfn_.Release();
  dequeue_tsfn_.Release();
}

// =============================================================================
// TSFN CALLBACK HANDLERS
// =============================================================================

void AudioEncoder::OnOutputChunk(Napi::Env env, Napi::Function jsCallback,
                                  AudioEncoder* context, OutputData* data) {
  if (!data) return;

  // Take ownership of the data
  std::unique_ptr<OutputData> output(data);

  if (!context || context->state_.IsClosed()) {
    return;
  }

  if (!output->packet || context->output_callback_.IsEmpty()) {
    return;
  }

  // Create EncodedAudioChunk JS object from packet
  Napi::Object chunk = EncodedAudioChunk::CreateFromPacket(
      env,
      output->packet.get(),
      output->is_key_frame,
      output->timestamp);

  if (chunk.IsEmpty()) {
    return;
  }

  // Create metadata if needed (first output after configure)
  if (output->include_decoder_config && context->worker_) {
    Napi::Object metadata = Napi::Object::New(env);

    // Build decoderConfig
    Napi::Object decoderConfig = Napi::Object::New(env);
    decoderConfig.Set("codec", Napi::String::New(env, context->active_config_.codec));
    decoderConfig.Set("sampleRate", Napi::Number::New(env, context->active_config_.sample_rate));
    decoderConfig.Set("numberOfChannels", Napi::Number::New(env, context->active_config_.number_of_channels));

    // Include extradata as description if available
    AVCodecContext* codec_ctx = context->worker_->GetCodecContext();
    if (codec_ctx && codec_ctx->extradata && codec_ctx->extradata_size > 0) {
      Napi::ArrayBuffer desc = Napi::ArrayBuffer::New(env, codec_ctx->extradata_size);
      std::memcpy(desc.Data(), codec_ctx->extradata, codec_ctx->extradata_size);
      decoderConfig.Set("description", Napi::Uint8Array::New(env, codec_ctx->extradata_size, desc, 0));
    }

    metadata.Set("decoderConfig", decoderConfig);

    // Call output with metadata
    context->output_callback_.Call({chunk, metadata});
  } else {
    // Call output without metadata
    context->output_callback_.Call({chunk});
  }
}

void AudioEncoder::OnError(Napi::Env env, Napi::Function jsCallback,
                           AudioEncoder* context, ErrorData* data) {
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

void AudioEncoder::OnFlushComplete(Napi::Env env, Napi::Function jsCallback,
                                   AudioEncoder* context, FlushCompleteData* data) {
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

void AudioEncoder::OnDequeue(Napi::Env env, Napi::Function jsCallback,
                             AudioEncoder* context, DequeueData* data) {
  if (!data) return;

  delete data;

  if (!context || context->state_.IsClosed()) {
    return;
  }

  // Fire ondequeue event
  if (!context->ondequeue_callback_.IsEmpty()) {
    context->ondequeue_callback_.Call({});
  }
}

// =============================================================================
// ATTRIBUTE ACCESSORS
// =============================================================================

Napi::Value AudioEncoder::GetState(const Napi::CallbackInfo& info) {
  return Napi::String::New(info.Env(), state_.ToString());
}

Napi::Value AudioEncoder::GetEncodeQueueSize(const Napi::CallbackInfo& info) {
  return Napi::Number::New(info.Env(), static_cast<double>(encode_queue_size_.load(std::memory_order_acquire)));
}

Napi::Value AudioEncoder::GetOndequeue(const Napi::CallbackInfo& info) {
  if (ondequeue_callback_.IsEmpty()) {
    return info.Env().Null();
  }
  return ondequeue_callback_.Value();
}

void AudioEncoder::SetOndequeue(const Napi::CallbackInfo& info, const Napi::Value& value) {
  if (value.IsNull() || value.IsUndefined()) {
    ondequeue_callback_.Reset();
  } else if (value.IsFunction()) {
    ondequeue_callback_ = Napi::Persistent(value.As<Napi::Function>());
  }
}

// =============================================================================
// METHODS
// =============================================================================

Napi::Value AudioEncoder::Configure(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] 1. If config is not a valid AudioEncoderConfig, throw TypeError
  if (info.Length() < 1 || !info[0].IsObject()) {
    errors::ThrowTypeError(env, "AudioEncoderConfig is required");
    return env.Undefined();
  }

  // [SPEC] 2. If state is "closed", throw InvalidStateError
  if (state_.IsClosed()) {
    errors::ThrowInvalidStateError(env, "configure called on closed encoder");
    return env.Undefined();
  }

  Napi::Object config = info[0].As<Napi::Object>();

  // Required: codec string
  if (!config.Has("codec") || !config.Get("codec").IsString()) {
    errors::ThrowTypeError(env, "codec is required and must be a string");
    return env.Undefined();
  }

  // Required: sampleRate
  if (!config.Has("sampleRate") || !config.Get("sampleRate").IsNumber()) {
    errors::ThrowTypeError(env, "sampleRate is required and must be a number");
    return env.Undefined();
  }

  // Required: numberOfChannels
  if (!config.Has("numberOfChannels") || !config.Get("numberOfChannels").IsNumber()) {
    errors::ThrowTypeError(env, "numberOfChannels is required and must be a number");
    return env.Undefined();
  }

  // Deep copy configuration for async processing
  active_config_.codec = config.Get("codec").As<Napi::String>().Utf8Value();
  active_config_.sample_rate = config.Get("sampleRate").As<Napi::Number>().Int32Value();
  active_config_.number_of_channels = config.Get("numberOfChannels").As<Napi::Number>().Int32Value();

  // Validate positive values
  if (active_config_.sample_rate <= 0) {
    errors::ThrowTypeError(env, "sampleRate must be positive");
    return env.Undefined();
  }

  if (active_config_.number_of_channels <= 0) {
    errors::ThrowTypeError(env, "numberOfChannels must be positive");
    return env.Undefined();
  }

  if (config.Has("bitrate") && config.Get("bitrate").IsNumber()) {
    active_config_.bitrate = config.Get("bitrate").As<Napi::Number>().Int64Value();
  } else {
    active_config_.bitrate = 128000;  // Default 128kbps
  }

  if (config.Has("bitrateMode") && config.Get("bitrateMode").IsString()) {
    active_config_.bitrate_mode = config.Get("bitrateMode").As<Napi::String>().Utf8Value();
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

  // Start worker if not running
  if (!worker_->IsRunning()) {
    worker_->Start();
  }

  // Create configure message
  AudioControlQueue::ConfigureMessage msg{
      [this]() -> bool {
        return worker_ != nullptr;
      }};

  // Enqueue configure message
  if (!queue_.Enqueue(std::move(msg))) {
    errors::ThrowInvalidStateError(env, "Failed to enqueue configure");
    return env.Undefined();
  }

  // [SPEC] 3. Set state to "configured"
  state_.transition(raii::AtomicCodecState::State::Unconfigured, raii::AtomicCodecState::State::Configured);

  return env.Undefined();
}

Napi::Value AudioEncoder::Encode(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] 1. If state is not "configured", throw InvalidStateError
  if (!state_.IsConfigured()) {
    errors::ThrowInvalidStateError(env, "encode called on " + std::string(state_.ToString()) + " encoder");
    return env.Undefined();
  }

  // Validate AudioData argument
  if (info.Length() < 1 || !info[0].IsObject()) {
    errors::ThrowTypeError(env, "AudioData is required");
    return env.Undefined();
  }

  Napi::Object dataObj = info[0].As<Napi::Object>();

  // Get AudioData wrapper - handle both native objects and TypeScript wrappers
  AudioData* audioDataWrapper = nullptr;

  // First, try direct native AudioData
  if (dataObj.InstanceOf(AudioData::constructor.Value())) {
    audioDataWrapper = Napi::ObjectWrap<AudioData>::Unwrap(dataObj);
  }

  // If not native, check for TypeScript wrapper with 'native' property
  if (!audioDataWrapper && dataObj.Has("native")) {
    Napi::Value nativeVal = dataObj.Get("native");
    if (nativeVal.IsObject()) {
      Napi::Object nativeObj = nativeVal.As<Napi::Object>();
      if (nativeObj.InstanceOf(AudioData::constructor.Value())) {
        audioDataWrapper = Napi::ObjectWrap<AudioData>::Unwrap(nativeObj);
      }
    }
  }

  if (!audioDataWrapper) {
    errors::ThrowTypeError(env, "AudioData is required");
    return env.Undefined();
  }

  const AVFrame* srcFrame = audioDataWrapper->frame();
  if (!srcFrame) {
    errors::ThrowTypeError(env, "AudioData is closed or invalid");
    return env.Undefined();
  }

  // [SPEC] Clone frame for async processing
  raii::AVFramePtr frameClone = raii::CloneAvFrame(srcFrame);
  if (!frameClone) {
    errors::ThrowEncodingError(env, "Failed to clone audio data");
    return env.Undefined();
  }

  // [SPEC] Increment encodeQueueSize
  encode_queue_size_.fetch_add(1, std::memory_order_relaxed);

  // Enqueue encode message (audio has no keyFrame option)
  AudioControlQueue::EncodeMessage msg{std::move(frameClone), false};
  if (!queue_.Enqueue(std::move(msg))) {
    encode_queue_size_.fetch_sub(1, std::memory_order_relaxed);
    errors::ThrowInvalidStateError(env, "Failed to enqueue encode");
    return env.Undefined();
  }

  return env.Undefined();
}

Napi::Value AudioEncoder::Flush(const Napi::CallbackInfo& info) {
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

Napi::Value AudioEncoder::Reset(const Napi::CallbackInfo& info) {
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
  AudioControlQueue::ResetMessage msg{};
  (void)queue_.Enqueue(std::move(msg));

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

Napi::Value AudioEncoder::Close(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Immediately aborts all pending work and releases system resources.
  Release();

  return env.Undefined();
}

Napi::Value AudioEncoder::IsConfigSupported(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] 1. If config is not a valid AudioEncoderConfig, reject with TypeError
  if (info.Length() < 1 || !info[0].IsObject()) {
    Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
    deferred.Reject(Napi::TypeError::New(env, "AudioEncoderConfig is required").Value());
    return deferred.Promise();
  }

  Napi::Object config = info[0].As<Napi::Object>();

  // Required: codec string
  if (!config.Has("codec") || !config.Get("codec").IsString()) {
    Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
    deferred.Reject(Napi::TypeError::New(env, "codec is required and must be a string").Value());
    return deferred.Promise();
  }

  // Required: sampleRate
  if (!config.Has("sampleRate") || !config.Get("sampleRate").IsNumber()) {
    Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
    deferred.Reject(Napi::TypeError::New(env, "sampleRate is required and must be a number").Value());
    return deferred.Promise();
  }

  // Required: numberOfChannels
  if (!config.Has("numberOfChannels") || !config.Get("numberOfChannels").IsNumber()) {
    Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
    deferred.Reject(Napi::TypeError::New(env, "numberOfChannels is required and must be a number").Value());
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

  // [SPEC] Create AudioEncoderSupport object
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
  if (config.Has("bitrate") && config.Get("bitrate").IsNumber()) {
    clonedConfig.Set("bitrate", config.Get("bitrate"));
  }
  if (config.Has("bitrateMode") && config.Get("bitrateMode").IsString()) {
    clonedConfig.Set("bitrateMode", config.Get("bitrateMode"));
  }

  result.Set("config", clonedConfig);

  deferred.Resolve(result);
  return deferred.Promise();
}

// =============================================================================
// AUDIOENCODERWORKER IMPLEMENTATION
// =============================================================================

AudioEncoderWorker::AudioEncoderWorker(AudioControlQueue& queue, AudioEncoder* encoder)
    : CodecWorker<AudioControlQueue>(queue), encoder_(encoder) {
  // Callbacks are set up in output helpers
}

AudioEncoderWorker::~AudioEncoderWorker() {
  Stop();
}

bool AudioEncoderWorker::OnConfigure(const ConfigureMessage& msg) {
  // Get config from parent encoder
  if (!encoder_) return false;

  const AudioEncoder::EncoderConfig& config = encoder_->active_config_;

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
  codec_ctx_->sample_rate = config.sample_rate;
  sample_rate_ = config.sample_rate;

  // Set channel layout (FFmpeg version compatibility)
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(59, 37, 100)
  av_channel_layout_default(&codec_ctx_->ch_layout, config.number_of_channels);
  channels_ = codec_ctx_->ch_layout.nb_channels;
#else
  codec_ctx_->channels = config.number_of_channels;
  codec_ctx_->channel_layout = av_get_default_channel_layout(config.number_of_channels);
  channels_ = codec_ctx_->channels;
#endif

  // Time base (use sample rate)
  codec_ctx_->time_base = AVRational{1, config.sample_rate};

  // Set sample format (prefer encoder's first supported format)
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(61, 0, 0)
  // FFmpeg 7.0+ uses avcodec_get_supported_config
  const AVSampleFormat* sample_fmts = nullptr;
  int num_fmts = 0;
  if (avcodec_get_supported_config(codec_ctx_.get(), encoder, AV_CODEC_CONFIG_SAMPLE_FORMAT,
                                    0, reinterpret_cast<const void**>(&sample_fmts), &num_fmts) >= 0) {
    if (sample_fmts && num_fmts > 0) {
      codec_ctx_->sample_fmt = sample_fmts[0];
    }
  }
#else
  // Older FFmpeg - use deprecated pix_fmts field
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  if (encoder->sample_fmts) {
    codec_ctx_->sample_fmt = encoder->sample_fmts[0];
  }
  #pragma GCC diagnostic pop
#endif

  // Default to float planar if not set
  if (codec_ctx_->sample_fmt == AV_SAMPLE_FMT_NONE) {
    codec_ctx_->sample_fmt = AV_SAMPLE_FMT_FLTP;
  }
  sample_fmt_ = codec_ctx_->sample_fmt;

  // Bitrate
  if (config.bitrate > 0) {
    codec_ctx_->bit_rate = config.bitrate;
  }

  // Bitrate mode
  if (config.bitrate_mode == "constant") {
    codec_ctx_->rc_max_rate = codec_ctx_->bit_rate;
    codec_ctx_->rc_min_rate = codec_ctx_->bit_rate;
  }

  // Threading
  codec_ctx_->thread_count = 0;  // Auto-detect

  // Open encoder
  int ret = avcodec_open2(codec_ctx_.get(), encoder, nullptr);
  if (ret < 0) {
    OutputError(ret, "Failed to open encoder");
    codec_ctx_.reset();
    return false;
  }

  // Reset state for new configuration
  first_output_after_configure_ = true;
  sample_count_ = 0;

  return true;
}

void AudioEncoderWorker::OnEncode(const EncodeMessage& msg) {
  if (!codec_ctx_ || ShouldExit()) return;

  AVFrame* frame = msg.frame.get();
  if (!frame) {
    OutputError(AVERROR(EINVAL), "Invalid frame");
    return;
  }

  // Set frame PTS (sample-based for audio)
  if (frame->pts == AV_NOPTS_VALUE) {
    frame->pts = sample_count_;
  }
  sample_count_ += frame->nb_samples;

  // Send frame to encoder
  int ret = avcodec_send_frame(codec_ctx_.get(), frame);
  if (ret < 0 && ret != AVERROR(EAGAIN)) {
    OutputError(ret, "Failed to send frame to encoder");
    return;
  }

  // Receive all available packets
  raii::AVPacketPtr packet = raii::MakeAvPacket();
  if (!packet) {
    OutputError(AVERROR(ENOMEM), "Failed to allocate packet");
    return;
  }

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

    // Audio packets are typically all key frames
    bool is_key = (packet->flags & AV_PKT_FLAG_KEY) != 0 || true;

    // Include decoder config on first packet after configure
    bool include_config = first_output_after_configure_;
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

  // Decrement queue size and signal dequeue
  if (encoder_) {
    uint32_t new_size = encoder_->encode_queue_size_.fetch_sub(1, std::memory_order_relaxed) - 1;
    SignalDequeue(new_size);
  }
}

void AudioEncoderWorker::OnFlush(const FlushMessage& msg) {
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

    bool is_key = true;
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

void AudioEncoderWorker::OnReset() {
  if (codec_ctx_) {
    avcodec_flush_buffers(codec_ctx_.get());
  }
  first_output_after_configure_ = true;
  sample_count_ = 0;
}

void AudioEncoderWorker::OnClose() {
  codec_ctx_.reset();
}

void AudioEncoderWorker::OutputChunk(raii::AVPacketPtr packet, bool is_key,
                                      int64_t ts, int64_t dur, bool include_config) {
  if (!encoder_ || encoder_->state_.IsClosed()) return;

  auto* data = new AudioEncoder::OutputData{
      std::move(packet),
      is_key,
      ts,
      dur,
      include_config};

  if (!encoder_->output_tsfn_.Call(data)) {
    delete data;
  }
}

void AudioEncoderWorker::OutputError(int code, const std::string& message) {
  if (!encoder_ || encoder_->state_.IsClosed()) return;

  auto* data = new AudioEncoder::ErrorData{code, message};
  if (!encoder_->error_tsfn_.Call(data)) {
    delete data;
  }
}

void AudioEncoderWorker::FlushComplete(uint32_t promise_id, bool success, const std::string& error) {
  if (!encoder_) return;

  auto* data = new AudioEncoder::FlushCompleteData{promise_id, success, error};
  if (!encoder_->flush_tsfn_.Call(data)) {
    delete data;
  }
}

void AudioEncoderWorker::SignalDequeue(uint32_t new_size) {
  if (!encoder_ || encoder_->state_.IsClosed()) return;

  auto* data = new AudioEncoder::DequeueData{new_size};
  if (!encoder_->dequeue_tsfn_.Call(data)) {
    delete data;
  }
}

}  // namespace webcodecs
