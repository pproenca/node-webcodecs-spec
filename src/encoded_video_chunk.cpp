#include "encoded_video_chunk.h"

namespace webcodecs {

Napi::Object EncodedVideoChunk::Init(Napi::Env env, Napi::Object exports) {
  Napi::Function func = DefineClass(env, "EncodedVideoChunk", {
    InstanceMethod<&EncodedVideoChunk::CopyTo>("copyTo"),
    InstanceAccessor<&EncodedVideoChunk::GetType>("type"),
    InstanceAccessor<&EncodedVideoChunk::GetTimestamp>("timestamp"),
    InstanceAccessor<&EncodedVideoChunk::GetDuration>("duration"),
    InstanceAccessor<&EncodedVideoChunk::GetByteLength>("byteLength"),
  });

  Napi::FunctionReference* constructor = new Napi::FunctionReference();
  *constructor = Napi::Persistent(func);
  env.SetInstanceData(constructor);

  exports.Set("EncodedVideoChunk", func);
  return exports;
}

EncodedVideoChunk::EncodedVideoChunk(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<EncodedVideoChunk>(info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1 || !info[0].IsObject()) {
    Napi::TypeError::New(env, "EncodedVideoChunkInit object required")
        .ThrowAsJavaScriptException();
    return;
  }

  Napi::Object init = info[0].As<Napi::Object>();

  // Required: type ("key" or "delta")
  if (!init.Has("type") || !init.Get("type").IsString()) {
    Napi::TypeError::New(env, "type is required and must be a string")
        .ThrowAsJavaScriptException();
    return;
  }

  // Required: timestamp (in microseconds)
  if (!init.Has("timestamp") || !init.Get("timestamp").IsNumber()) {
    Napi::TypeError::New(env, "timestamp is required and must be a number")
        .ThrowAsJavaScriptException();
    return;
  }

  // Required: data (BufferSource)
  if (!init.Has("data")) {
    Napi::TypeError::New(env, "data is required")
        .ThrowAsJavaScriptException();
    return;
  }

  // TODO: Store type, timestamp, duration, and copy data buffer
}

EncodedVideoChunk::~EncodedVideoChunk() {
  // TODO: Free copied data buffer
}

Napi::Value EncodedVideoChunk::CopyTo(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1) {
    Napi::TypeError::New(env, "destination BufferSource required")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }

  // TODO: Copy internal data to provided buffer
  return env.Undefined();
}

Napi::Value EncodedVideoChunk::GetType(const Napi::CallbackInfo& info) {
  // TODO: Return stored type
  return Napi::String::New(info.Env(), "key");
}

Napi::Value EncodedVideoChunk::GetTimestamp(const Napi::CallbackInfo& info) {
  // TODO: Return stored timestamp
  return Napi::Number::New(info.Env(), 0);
}

Napi::Value EncodedVideoChunk::GetDuration(const Napi::CallbackInfo& info) {
  // TODO: Return stored duration (may be null)
  return info.Env().Null();
}

Napi::Value EncodedVideoChunk::GetByteLength(const Napi::CallbackInfo& info) {
  // TODO: Return actual data size
  return Napi::Number::New(info.Env(), 0);
}

}  // namespace webcodecs
