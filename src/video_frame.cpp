#include "video_frame.h"

namespace webcodecs {

Napi::Object VideoFrame::Init(Napi::Env env, Napi::Object exports) {
  Napi::Function func = DefineClass(env, "VideoFrame", {
    InstanceMethod<&VideoFrame::Clone>("clone"),
    InstanceMethod<&VideoFrame::Close>("close"),
    InstanceMethod<&VideoFrame::CopyTo>("copyTo"),
    InstanceMethod<&VideoFrame::AllocationSize>("allocationSize"),
    InstanceAccessor<&VideoFrame::GetFormat>("format"),
    InstanceAccessor<&VideoFrame::GetCodedWidth>("codedWidth"),
    InstanceAccessor<&VideoFrame::GetCodedHeight>("codedHeight"),
    InstanceAccessor<&VideoFrame::GetDisplayWidth>("displayWidth"),
    InstanceAccessor<&VideoFrame::GetDisplayHeight>("displayHeight"),
    InstanceAccessor<&VideoFrame::GetTimestamp>("timestamp"),
    InstanceAccessor<&VideoFrame::GetDuration>("duration"),
    InstanceAccessor<&VideoFrame::GetColorSpace>("colorSpace"),
  });

  Napi::FunctionReference* constructor = new Napi::FunctionReference();
  *constructor = Napi::Persistent(func);
  env.SetInstanceData(constructor);

  exports.Set("VideoFrame", func);
  return exports;
}

VideoFrame::VideoFrame(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<VideoFrame>(info) {
  // TODO: Accept ImageBitmap, ImageData, HTMLVideoElement, etc.
  // For now, stub accepts any argument
}

VideoFrame::~VideoFrame() {
  // Release resources if not already closed
  if (!closed_) {
    closed_ = true;
    // TODO: Free AVFrame
  }
}

Napi::Value VideoFrame::Clone(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (closed_) {
    Napi::Error::New(env, "VideoFrame is closed").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  // TODO: Create new VideoFrame with copied data
  return env.Undefined();
}

void VideoFrame::Close(const Napi::CallbackInfo& info) {
  if (!closed_) {
    closed_ = true;
    // TODO: Free AVFrame buffer
  }
}

Napi::Value VideoFrame::CopyTo(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (closed_) {
    Napi::Error::New(env, "VideoFrame is closed").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  // TODO: Copy frame data to provided BufferSource
  auto deferred = Napi::Promise::Deferred::New(env);
  deferred.Resolve(env.Undefined());
  return deferred.Promise();
}

Napi::Value VideoFrame::AllocationSize(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (closed_) {
    Napi::Error::New(env, "VideoFrame is closed").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  // TODO: Calculate actual allocation size based on format and dimensions
  return Napi::Number::New(env, 0);
}

Napi::Value VideoFrame::GetFormat(const Napi::CallbackInfo& info) {
  // TODO: Return actual pixel format (I420, NV12, RGBA, etc.)
  return Napi::String::New(info.Env(), "I420");
}

Napi::Value VideoFrame::GetCodedWidth(const Napi::CallbackInfo& info) {
  return Napi::Number::New(info.Env(), 0);
}

Napi::Value VideoFrame::GetCodedHeight(const Napi::CallbackInfo& info) {
  return Napi::Number::New(info.Env(), 0);
}

Napi::Value VideoFrame::GetDisplayWidth(const Napi::CallbackInfo& info) {
  return Napi::Number::New(info.Env(), 0);
}

Napi::Value VideoFrame::GetDisplayHeight(const Napi::CallbackInfo& info) {
  return Napi::Number::New(info.Env(), 0);
}

Napi::Value VideoFrame::GetTimestamp(const Napi::CallbackInfo& info) {
  // Timestamp in microseconds
  return Napi::Number::New(info.Env(), 0);
}

Napi::Value VideoFrame::GetDuration(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  // Duration may be null
  return env.Null();
}

Napi::Value VideoFrame::GetColorSpace(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  // TODO: Return VideoColorSpace object
  Napi::Object colorSpace = Napi::Object::New(env);
  colorSpace.Set("primaries", env.Null());
  colorSpace.Set("transfer", env.Null());
  colorSpace.Set("matrix", env.Null());
  colorSpace.Set("fullRange", env.Null());
  return colorSpace;
}

}  // namespace webcodecs
