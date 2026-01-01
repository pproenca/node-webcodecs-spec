#include "video_encoder.h"

namespace webcodecs {

Napi::Object VideoEncoder::Init(Napi::Env env, Napi::Object exports) {
  Napi::Function func = DefineClass(env, "VideoEncoder", {
    InstanceMethod<&VideoEncoder::Configure>("configure"),
    InstanceMethod<&VideoEncoder::Encode>("encode"),
    InstanceMethod<&VideoEncoder::Flush>("flush"),
    InstanceMethod<&VideoEncoder::Reset>("reset"),
    InstanceMethod<&VideoEncoder::Close>("close"),
    InstanceAccessor<&VideoEncoder::GetState>("state"),
    InstanceAccessor<&VideoEncoder::GetEncodeQueueSize>("encodeQueueSize"),
    StaticMethod<&VideoEncoder::IsConfigSupported>("isConfigSupported"),
  });

  Napi::FunctionReference* constructor = new Napi::FunctionReference();
  *constructor = Napi::Persistent(func);
  env.SetInstanceData(constructor);

  exports.Set("VideoEncoder", func);
  return exports;
}

VideoEncoder::VideoEncoder(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<VideoEncoder>(info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1 || !info[0].IsObject()) {
    Napi::TypeError::New(env, "VideoEncoderInit object required")
        .ThrowAsJavaScriptException();
    return;
  }

  state_ = State::Unconfigured;
}

VideoEncoder::~VideoEncoder() = default;

Napi::Value VideoEncoder::Configure(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (state_ == State::Closed) {
    Napi::Error::New(env, "VideoEncoder is closed").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  if (info.Length() < 1 || !info[0].IsObject()) {
    Napi::TypeError::New(env, "VideoEncoderConfig object required")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }

  // TODO: Parse config, initialize AVCodecContext for encoding
  state_ = State::Configured;
  return env.Undefined();
}

Napi::Value VideoEncoder::Encode(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (state_ != State::Configured) {
    Napi::Error::New(env, "VideoEncoder not configured")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }

  // TODO: Queue VideoFrame for async encode
  return env.Undefined();
}

Napi::Value VideoEncoder::Flush(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (state_ != State::Configured) {
    Napi::Error::New(env, "VideoEncoder not configured")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto deferred = Napi::Promise::Deferred::New(env);
  deferred.Resolve(env.Undefined());
  return deferred.Promise();
}

Napi::Value VideoEncoder::Reset(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (state_ == State::Closed) {
    Napi::Error::New(env, "VideoEncoder is closed").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  state_ = State::Unconfigured;
  return env.Undefined();
}

void VideoEncoder::Close(const Napi::CallbackInfo& info) {
  state_ = State::Closed;
}

Napi::Value VideoEncoder::GetState(const Napi::CallbackInfo& info) {
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

Napi::Value VideoEncoder::GetEncodeQueueSize(const Napi::CallbackInfo& info) {
  return Napi::Number::New(info.Env(), 0);
}

Napi::Value VideoEncoder::IsConfigSupported(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  auto deferred = Napi::Promise::Deferred::New(env);

  Napi::Object result = Napi::Object::New(env);
  result.Set("supported", Napi::Boolean::New(env, false));

  deferred.Resolve(result);
  return deferred.Promise();
}

}  // namespace webcodecs
