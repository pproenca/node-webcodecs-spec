#include "VideoColorSpace.h"

namespace webcodecs {

Napi::FunctionReference VideoColorSpace::constructor;

Napi::Object VideoColorSpace::Init(Napi::Env env, Napi::Object exports) {
  Napi::Function func = DefineClass(env, "VideoColorSpace", {
    InstanceAccessor<&VideoColorSpace::GetPrimaries>("primaries"),
    InstanceAccessor<&VideoColorSpace::GetTransfer>("transfer"),
    InstanceAccessor<&VideoColorSpace::GetMatrix>("matrix"),
    InstanceAccessor<&VideoColorSpace::GetFullRange>("fullRange"),
    InstanceMethod<&VideoColorSpace::ToJSON>("toJSON"),

  });

  constructor = Napi::Persistent(func);
  constructor.SuppressDestruct();
  exports.Set("VideoColorSpace", func);
  return exports;
}

VideoColorSpace::VideoColorSpace(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<VideoColorSpace>(info) {
  Napi::Env env = info.Env();

  // [SPEC] Constructor Algorithm
  /*
   * Initialize internal slots.
   */

  // TODO(impl): Implement Constructor & Resource Allocation
}

VideoColorSpace::~VideoColorSpace() {
  Release();
}

void VideoColorSpace::Release() {
  // TODO(impl): Free handle_ and native resources
  handle_ = nullptr;
}

// --- Attributes ---

Napi::Value VideoColorSpace::GetPrimaries(const Napi::CallbackInfo& info) {
  // TODO(impl): Return primaries
  return info.Env().Null();
}

Napi::Value VideoColorSpace::GetTransfer(const Napi::CallbackInfo& info) {
  // TODO(impl): Return transfer
  return info.Env().Null();
}

Napi::Value VideoColorSpace::GetMatrix(const Napi::CallbackInfo& info) {
  // TODO(impl): Return matrix
  return info.Env().Null();
}

Napi::Value VideoColorSpace::GetFullRange(const Napi::CallbackInfo& info) {
  // TODO(impl): Return fullRange
  return info.Env().Null();
}


// --- Methods ---

Napi::Value VideoColorSpace::ToJSON(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Algorithm
  /*
   * See spec/context file.
   */

  // TODO(impl): Implement method logic
  return env.Undefined();
}

}  // namespace webcodecs
