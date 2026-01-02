#include "ImageTrack.h"

namespace webcodecs {

Napi::FunctionReference ImageTrack::constructor;

Napi::Object ImageTrack::Init(Napi::Env env, Napi::Object exports) {
  Napi::Function func =
      DefineClass(env, "ImageTrack",
                  {
                      InstanceAccessor<&ImageTrack::GetAnimated>("animated"),
                      InstanceAccessor<&ImageTrack::GetFrameCount>("frameCount"),
                      InstanceAccessor<&ImageTrack::GetRepetitionCount>("repetitionCount"),
                      InstanceAccessor<&ImageTrack::GetSelected, &ImageTrack::SetSelected>("selected"),
                  });

  constructor = Napi::Persistent(func);
  constructor.SuppressDestruct();
  exports.Set("ImageTrack", func);
  return exports;
}

ImageTrack::ImageTrack(const Napi::CallbackInfo& info) : Napi::ObjectWrap<ImageTrack>(info) {
  Napi::Env env = info.Env();

  // [SPEC] Constructor Algorithm
  /*
   * Refer to spec context.
   */

  // TODO(impl): Implement Constructor & Resource Allocation
}

ImageTrack::~ImageTrack() { Release(); }

void ImageTrack::Release() {
  // TODO(impl): Free handle_ and native resources
  handle_ = nullptr;
}

// --- Attributes ---

Napi::Value ImageTrack::GetAnimated(const Napi::CallbackInfo& info) {
  // TODO(impl): Return animated
  return info.Env().Null();
}

Napi::Value ImageTrack::GetFrameCount(const Napi::CallbackInfo& info) {
  // TODO(impl): Return frameCount
  return info.Env().Null();
}

Napi::Value ImageTrack::GetRepetitionCount(const Napi::CallbackInfo& info) {
  // TODO(impl): Return repetitionCount
  return info.Env().Null();
}

Napi::Value ImageTrack::GetSelected(const Napi::CallbackInfo& info) {
  // TODO(impl): Return selected
  return info.Env().Null();
}

void ImageTrack::SetSelected(const Napi::CallbackInfo& info, const Napi::Value& value) {
  // TODO(impl): Set selected
}

// --- Methods ---

}  // namespace webcodecs
