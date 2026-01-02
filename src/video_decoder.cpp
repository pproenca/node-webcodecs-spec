#include "video_decoder.h"

#include <string>

#include "video_frame.h"
#include "encoded_video_chunk.h"
#include "shared/codec_registry.h"
#include "error_builder.h"
#include "shared/buffer_utils.h"

namespace webcodecs {

Napi::FunctionReference VideoDecoder::constructor;

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
}

VideoDecoder::~VideoDecoder() { Release(); }

void VideoDecoder::Release() {
  // Thread-safe close: transition to Closed state
  state_.Close();

  // Lock to safely clear resources
  std::lock_guard<std::mutex> lock(mutex_);

  // Clear decode queue (RAII handles packet cleanup)
  while (!decode_queue_.empty()) {
    decode_queue_.pop();
  }
  decode_queue_size_.store(0, std::memory_order_release);

  // Release codec context (RAII handles avcodec_free_context)
  codec_ctx_.reset();

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

  Napi::Object config = info[0].As<Napi::Object>();

  // Required: codec string
  if (!config.Has("codec") || !config.Get("codec").IsString()) {
    errors::ThrowTypeError(env, "codec is required and must be a string");
    return env.Undefined();
  }

  std::string codec_string = config.Get("codec").As<Napi::String>().Utf8Value();

  // Parse codec string to get FFmpeg codec ID (can be done without lock)
  auto codec_info = ParseCodecString(codec_string);
  if (!codec_info) {
    errors::ThrowNotSupportedError(env, "Unsupported codec: " + codec_string);
    return env.Undefined();
  }

  // Find FFmpeg decoder (can be done without lock)
  const AVCodec* decoder = avcodec_find_decoder(codec_info->codec_id);
  if (!decoder) {
    errors::ThrowNotSupportedError(env, "No decoder available for: " + codec_string);
    return env.Undefined();
  }

  // Lock for codec operations - state check MUST be inside lock to prevent TOCTOU
  std::lock_guard<std::mutex> lock(mutex_);

  // [SPEC] 2. If state is "closed", throw InvalidStateError
  // Check inside lock to prevent race condition with close()
  if (state_.IsClosed()) {
    errors::ThrowInvalidStateError(env, "configure called on closed decoder");
    return env.Undefined();
  }

  // Allocate codec context (RAII)
  codec_ctx_ = raii::MakeAvCodecContext(decoder);
  if (!codec_ctx_) {
    errors::ThrowEncodingError(env, "Failed to allocate codec context");
    return env.Undefined();
  }

  // Set codec parameters from config
  if (config.Has("codedWidth") && config.Get("codedWidth").IsNumber()) {
    codec_ctx_->width = config.Get("codedWidth").As<Napi::Number>().Int32Value();
  }
  if (config.Has("codedHeight") && config.Get("codedHeight").IsNumber()) {
    codec_ctx_->height = config.Get("codedHeight").As<Napi::Number>().Int32Value();
  }

  // Handle description (extradata) if provided - e.g., for H.264 avcC
  if (config.Has("description")) {
    Napi::Value desc = config.Get("description");
    const uint8_t* data = nullptr;
    size_t size = 0;

    if (buffer_utils::ExtractBufferData(desc, &data, &size) && data && size > 0) {
      codec_ctx_->extradata = static_cast<uint8_t*>(av_mallocz(size + AV_INPUT_BUFFER_PADDING_SIZE));
      if (codec_ctx_->extradata) {
        std::memcpy(codec_ctx_->extradata, data, size);
        codec_ctx_->extradata_size = static_cast<int>(size);
      }
    }
  }

  // Set threading model
  codec_ctx_->thread_count = 0;  // Auto-detect
  codec_ctx_->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;

  // Open codec
  int ret = avcodec_open2(codec_ctx_.get(), decoder, nullptr);
  if (ret < 0) {
    codec_ctx_.reset();
    errors::ThrowEncodingError(env, ret, "Failed to open decoder");
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

  // [SPEC] 3. Increment decodeQueueSize
  decode_queue_size_.fetch_add(1, std::memory_order_relaxed);

  // Create AVPacket from data
  raii::AVPacketPtr packet = buffer_utils::CreatePacketFromBuffer(data, size);
  if (!packet) {
    decode_queue_size_.fetch_sub(1, std::memory_order_relaxed);
    errors::ThrowEncodingError(env, "Failed to create packet");
    return env.Undefined();
  }

  // Set packet timestamp (convert microseconds to codec timebase)
  packet->pts = timestamp;
  packet->dts = timestamp;

  // Set key frame flag
  if (chunk_type == "key") {
    packet->flags |= AV_PKT_FLAG_KEY;
  }

  // Lock for codec operations
  std::lock_guard<std::mutex> lock(mutex_);

  if (!codec_ctx_) {
    decode_queue_size_.fetch_sub(1, std::memory_order_relaxed);
    errors::ThrowInvalidStateError(env, "Decoder not configured");
    return env.Undefined();
  }

  // Send packet to decoder
  int ret = avcodec_send_packet(codec_ctx_.get(), packet.get());
  if (ret < 0 && ret != AVERROR(EAGAIN)) {
    decode_queue_size_.fetch_sub(1, std::memory_order_relaxed);
    errors::ThrowEncodingError(env, ret, "Failed to send packet to decoder");
    return env.Undefined();
  }

  // Receive all available frames
  raii::AVFramePtr frame = raii::MakeAvFrame();
  if (!frame) {
    decode_queue_size_.fetch_sub(1, std::memory_order_relaxed);
    errors::ThrowEncodingError(env, "Failed to allocate frame");
    return env.Undefined();
  }

  while (true) {
    ret = avcodec_receive_frame(codec_ctx_.get(), frame.get());
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      break;  // Need more input or end of stream
    }
    if (ret < 0) {
      decode_queue_size_.fetch_sub(1, std::memory_order_relaxed);
      errors::ThrowEncodingError(env, ret, "Error receiving frame");
      return env.Undefined();
    }

    // Output the frame via callback
    if (!output_callback_.IsEmpty()) {
      Napi::Object jsFrame = VideoFrame::CreateFromAVFrame(env, frame.get());
      if (!jsFrame.IsEmpty()) {
        output_callback_.Call({jsFrame});
      }
    }

    // Reset frame for next iteration
    av_frame_unref(frame.get());
  }

  // [SPEC] Decrement decodeQueueSize and schedule dequeue event
  decode_queue_size_.fetch_sub(1, std::memory_order_relaxed);

  // Call ondequeue if set
  if (!ondequeue_callback_.IsEmpty()) {
    ondequeue_callback_.Call({});
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

  // Lock for codec operations
  std::lock_guard<std::mutex> lock(mutex_);

  if (!codec_ctx_) {
    deferred.Resolve(env.Undefined());
    return deferred.Promise();
  }

  // Drain the decoder by sending a null packet
  int ret = avcodec_send_packet(codec_ctx_.get(), nullptr);
  if (ret < 0 && ret != AVERROR_EOF) {
    Napi::Error error = Napi::Error::New(env, "EncodingError: Failed to flush decoder");
    error.Set("name", Napi::String::New(env, "EncodingError"));
    deferred.Reject(error.Value());
    return deferred.Promise();
  }

  // Receive all remaining frames
  raii::AVFramePtr frame = raii::MakeAvFrame();
  if (!frame) {
    Napi::Error error = Napi::Error::New(env, "EncodingError: Failed to allocate frame");
    error.Set("name", Napi::String::New(env, "EncodingError"));
    deferred.Reject(error.Value());
    return deferred.Promise();
  }

  while (true) {
    ret = avcodec_receive_frame(codec_ctx_.get(), frame.get());
    if (ret == AVERROR_EOF) {
      break;  // All frames drained
    }
    if (ret == AVERROR(EAGAIN)) {
      break;  // Should not happen after null packet, but handle it
    }
    if (ret < 0) {
      Napi::Error error = Napi::Error::New(env, "EncodingError: Error receiving frame during flush");
      error.Set("name", Napi::String::New(env, "EncodingError"));
      deferred.Reject(error.Value());
      return deferred.Promise();
    }

    // Output the frame via callback
    if (!output_callback_.IsEmpty()) {
      Napi::Object jsFrame = VideoFrame::CreateFromAVFrame(env, frame.get());
      if (!jsFrame.IsEmpty()) {
        output_callback_.Call({jsFrame});
      }
    }

    // Reset frame for next iteration
    av_frame_unref(frame.get());
  }

  // Resolve the promise
  deferred.Resolve(env.Undefined());
  return deferred.Promise();
}

Napi::Value VideoDecoder::Reset(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] If state is "closed", throw InvalidStateError
  if (state_.IsClosed()) {
    errors::ThrowInvalidStateError(env, "reset called on closed decoder");
    return env.Undefined();
  }

  // Lock for codec operations
  std::lock_guard<std::mutex> lock(mutex_);

  // Clear decode queue
  while (!decode_queue_.empty()) {
    decode_queue_.pop();
  }
  decode_queue_size_.store(0, std::memory_order_release);

  // Flush the codec buffers (discard all pending frames)
  if (codec_ctx_) {
    avcodec_flush_buffers(codec_ctx_.get());
  }

  // Reset key chunk requirement
  key_chunk_required_.store(true, std::memory_order_release);

  // Transition back to unconfigured state
  // Release codec context so configure() must be called again
  codec_ctx_.reset();

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

}  // namespace webcodecs
