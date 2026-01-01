#include "VideoFrame.h"

namespace webcodecs {

Napi::FunctionReference VideoFrame::constructor;

Napi::Object VideoFrame::Init(Napi::Env env, Napi::Object exports) {
  Napi::Function func = DefineClass(env, "VideoFrame", {
    InstanceAccessor<&VideoFrame::GetFormat>("format"),
    InstanceAccessor<&VideoFrame::GetCodedWidth>("codedWidth"),
    InstanceAccessor<&VideoFrame::GetCodedHeight>("codedHeight"),
    InstanceAccessor<&VideoFrame::GetCodedRect>("codedRect"),
    InstanceAccessor<&VideoFrame::GetVisibleRect>("visibleRect"),
    InstanceAccessor<&VideoFrame::GetRotation>("rotation"),
    InstanceAccessor<&VideoFrame::GetFlip>("flip"),
    InstanceAccessor<&VideoFrame::GetDisplayWidth>("displayWidth"),
    InstanceAccessor<&VideoFrame::GetDisplayHeight>("displayHeight"),
    InstanceAccessor<&VideoFrame::GetDuration>("duration"),
    InstanceAccessor<&VideoFrame::GetTimestamp>("timestamp"),
    InstanceAccessor<&VideoFrame::GetColorSpace>("colorSpace"),
    InstanceMethod<&VideoFrame::Metadata>("metadata"),
    InstanceMethod<&VideoFrame::AllocationSize>("allocationSize"),
    InstanceMethod<&VideoFrame::CopyTo>("copyTo"),
    InstanceMethod<&VideoFrame::Clone>("clone"),
    InstanceMethod<&VideoFrame::Close>("close"),

  });

  constructor = Napi::Persistent(func);
  constructor.SuppressDestruct();
  exports.Set("VideoFrame", func);
  return exports;
}

VideoFrame::VideoFrame(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<VideoFrame>(info) {
  Napi::Env env = info.Env();

  // [SPEC] Constructor Algorithm
  /*
   * Initialize internal slots.
   */

  // TODO(impl): Implement Constructor & Resource Allocation
}

VideoFrame::~VideoFrame() {
  Release();
}

void VideoFrame::Release() {
  // TODO(impl): Free handle_ and native resources
  handle_ = nullptr;
}

// --- Attributes ---

Napi::Value VideoFrame::GetFormat(const Napi::CallbackInfo& info) {
  // TODO(impl): Return format
  return info.Env().Null();
}

Napi::Value VideoFrame::GetCodedWidth(const Napi::CallbackInfo& info) {
  // TODO(impl): Return codedWidth
  return info.Env().Null();
}

Napi::Value VideoFrame::GetCodedHeight(const Napi::CallbackInfo& info) {
  // TODO(impl): Return codedHeight
  return info.Env().Null();
}

Napi::Value VideoFrame::GetCodedRect(const Napi::CallbackInfo& info) {
  // TODO(impl): Return codedRect
  return info.Env().Null();
}

Napi::Value VideoFrame::GetVisibleRect(const Napi::CallbackInfo& info) {
  // TODO(impl): Return visibleRect
  return info.Env().Null();
}

Napi::Value VideoFrame::GetRotation(const Napi::CallbackInfo& info) {
  // TODO(impl): Return rotation
  return info.Env().Null();
}

Napi::Value VideoFrame::GetFlip(const Napi::CallbackInfo& info) {
  // TODO(impl): Return flip
  return info.Env().Null();
}

Napi::Value VideoFrame::GetDisplayWidth(const Napi::CallbackInfo& info) {
  // TODO(impl): Return displayWidth
  return info.Env().Null();
}

Napi::Value VideoFrame::GetDisplayHeight(const Napi::CallbackInfo& info) {
  // TODO(impl): Return displayHeight
  return info.Env().Null();
}

Napi::Value VideoFrame::GetDuration(const Napi::CallbackInfo& info) {
  // TODO(impl): Return duration
  return info.Env().Null();
}

Napi::Value VideoFrame::GetTimestamp(const Napi::CallbackInfo& info) {
  // TODO(impl): Return timestamp
  return info.Env().Null();
}

Napi::Value VideoFrame::GetColorSpace(const Napi::CallbackInfo& info) {
  // TODO(impl): Return colorSpace
  return info.Env().Null();
}


// --- Methods ---

Napi::Value VideoFrame::Metadata(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Algorithm
  /*
   * See spec/context file.
   */

  // TODO(impl): Implement method logic
  return env.Undefined();
}

Napi::Value VideoFrame::AllocationSize(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Algorithm
  /*
   * See spec/context file.
   */

  // TODO(impl): Implement method logic
  return env.Undefined();
}

Napi::Value VideoFrame::CopyTo(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Algorithm
  /*
   * See spec/context file.
   */

  // TODO(impl): Implement method logic
  return env.Undefined();
}

Napi::Value VideoFrame::Clone(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Algorithm
  /*
   * See spec/context file.
   */

  // TODO(impl): Implement method logic
  return env.Undefined();
}

Napi::Value VideoFrame::Close(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Algorithm
  /*
   * See spec/context file.
   */

  // TODO(impl): Implement method logic
  return env.Undefined();
}

}  // namespace webcodecs
