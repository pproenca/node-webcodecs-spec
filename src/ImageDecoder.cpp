#include "ImageDecoder.h"

namespace webcodecs {

Napi::FunctionReference ImageDecoder::constructor;

Napi::Object ImageDecoder::Init(Napi::Env env, Napi::Object exports) {
  Napi::Function func = DefineClass(env, "ImageDecoder",
                                    {
                                        InstanceAccessor<&ImageDecoder::GetType>("type"),
                                        InstanceAccessor<&ImageDecoder::GetComplete>("complete"),
                                        InstanceAccessor<&ImageDecoder::GetCompleted>("completed"),
                                        InstanceAccessor<&ImageDecoder::GetTracks>("tracks"),
                                        InstanceMethod<&ImageDecoder::Decode>("decode"),
                                        InstanceMethod<&ImageDecoder::Reset>("reset"),
                                        InstanceMethod<&ImageDecoder::Close>("close"),
                                        StaticMethod<&ImageDecoder::IsTypeSupported>("isTypeSupported"),
                                    });

  constructor = Napi::Persistent(func);
  constructor.SuppressDestruct();
  exports.Set("ImageDecoder", func);
  return exports;
}

ImageDecoder::ImageDecoder(const Napi::CallbackInfo& info) : Napi::ObjectWrap<ImageDecoder>(info) {
  Napi::Env env = info.Env();

  // [SPEC] Constructor Algorithm
  /*
   * Initialize internal slots.
   */

  // TODO(impl): Implement Constructor & Resource Allocation
}

ImageDecoder::~ImageDecoder() { Release(); }

void ImageDecoder::Release() {
  // TODO(impl): Free handle_ and native resources
  handle_ = nullptr;
}

// --- Attributes ---

Napi::Value ImageDecoder::GetType(const Napi::CallbackInfo& info) {
  // TODO(impl): Return type
  return info.Env().Null();
}

Napi::Value ImageDecoder::GetComplete(const Napi::CallbackInfo& info) {
  // TODO(impl): Return complete
  return info.Env().Null();
}

Napi::Value ImageDecoder::GetCompleted(const Napi::CallbackInfo& info) {
  // TODO(impl): Return completed
  return info.Env().Null();
}

Napi::Value ImageDecoder::GetTracks(const Napi::CallbackInfo& info) {
  // TODO(impl): Return tracks
  return info.Env().Null();
}

// --- Methods ---

Napi::Value ImageDecoder::Decode(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Algorithm
  /*
   * See spec/context file.
   */

  // TODO(impl): Implement method logic
  return env.Undefined();
}

Napi::Value ImageDecoder::Reset(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Algorithm
  /*
   * See spec/context file.
   */

  // TODO(impl): Implement method logic
  return env.Undefined();
}

Napi::Value ImageDecoder::Close(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Algorithm
  /*
   * See spec/context file.
   */

  // TODO(impl): Implement method logic
  return env.Undefined();
}

Napi::Value ImageDecoder::IsTypeSupported(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Algorithm
  /*
   * See spec/context file.
   */

  // TODO(impl): Implement method logic
  return env.Undefined();
}

}  // namespace webcodecs
