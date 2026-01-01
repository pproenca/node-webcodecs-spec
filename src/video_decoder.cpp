#include "video_decoder.h"

namespace webcodecs {

Napi::Object VideoDecoder::Init(Napi::Env env, Napi::Object exports) {
  Napi::Function func = DefineClass(env, "VideoDecoder", {
    InstanceMethod<&VideoDecoder::Configure>("configure"),
    InstanceMethod<&VideoDecoder::Decode>("decode"),
    InstanceMethod<&VideoDecoder::Flush>("flush"),
    InstanceMethod<&VideoDecoder::Reset>("reset"),
    InstanceMethod<&VideoDecoder::Close>("close"),
    InstanceAccessor<&VideoDecoder::GetState>("state"),
    InstanceAccessor<&VideoDecoder::GetDecodeQueueSize>("decodeQueueSize"),
    StaticMethod<&VideoDecoder::IsConfigSupported>("isConfigSupported"),
  });

  Napi::FunctionReference* constructor = new Napi::FunctionReference();
  *constructor = Napi::Persistent(func);
  env.SetInstanceData(constructor);

  exports.Set("VideoDecoder", func);
  return exports;
}

VideoDecoder::VideoDecoder(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<VideoDecoder>(info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1 || !info[0].IsObject()) {
    Napi::TypeError::New(env, "VideoDecoderInit object required")
        .ThrowAsJavaScriptException();
    return;
  }

  // TODO: Extract output and error callbacks from init object
  state_ = State::Unconfigured;
}

VideoDecoder::~VideoDecoder() {
  // RAII cleanup handled by destructor
}

Napi::Value VideoDecoder::Configure(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (state_ == State::Closed) {
    Napi::Error::New(env, "VideoDecoder is closed").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  if (info.Length() < 1 || !info[0].IsObject()) {
    Napi::TypeError::New(env, "VideoDecoderConfig object required")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }

  // TODO: Parse config, initialize AVCodecContext
  state_ = State::Configured;
  return env.Undefined();
}

Napi::Value VideoDecoder::Decode(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (state_ != State::Configured) {
    Napi::Error::New(env, "VideoDecoder not configured")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }

  // TODO: Queue EncodedVideoChunk for async decode
  return env.Undefined();
}

Napi::Value VideoDecoder::Flush(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (state_ != State::Configured) {
    Napi::Error::New(env, "VideoDecoder not configured")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }

  // TODO: Return Promise that resolves when decode queue is drained
  auto deferred = Napi::Promise::Deferred::New(env);
  deferred.Resolve(env.Undefined());
  return deferred.Promise();
}

Napi::Value VideoDecoder::Reset(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (state_ == State::Closed) {
    Napi::Error::New(env, "VideoDecoder is closed").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  // TODO: Abort pending decodes, reset to Unconfigured
  state_ = State::Unconfigured;
  return env.Undefined();
}

void VideoDecoder::Close(const Napi::CallbackInfo& info) {
  state_ = State::Closed;
  // TODO: Release FFmpeg resources
}

Napi::Value VideoDecoder::GetState(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  switch (state_) {
    case State::Unconfigured:
      return Napi::String::New(env, "unconfigured");
    case State::Configured:
      return Napi::String::New(env, "configured");
    case State::Closed:
      return Napi::String::New(env, "closed");
  }

  return Napi::String::New(env, "unconfigured");
}

Napi::Value VideoDecoder::GetDecodeQueueSize(const Napi::CallbackInfo& info) {
  // TODO: Return actual queue size
  return Napi::Number::New(info.Env(), 0);
}

Napi::Value VideoDecoder::IsConfigSupported(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // TODO: Check if config.codec is supported by FFmpeg
  auto deferred = Napi::Promise::Deferred::New(env);

  Napi::Object result = Napi::Object::New(env);
  result.Set("supported", Napi::Boolean::New(env, false));

  deferred.Resolve(result);
  return deferred.Promise();
}

}  // namespace webcodecs
