#include "image_track_list.h"

namespace webcodecs {

Napi::FunctionReference ImageTrackList::constructor;

Napi::Object ImageTrackList::Init(Napi::Env env, Napi::Object exports) {
  Napi::Function func = DefineClass(env, "ImageTrackList",
                                    {
                                        InstanceAccessor<&ImageTrackList::GetReady>("ready"),
                                        InstanceAccessor<&ImageTrackList::GetLength>("length"),
                                        InstanceAccessor<&ImageTrackList::GetSelectedIndex>("selectedIndex"),
                                        InstanceAccessor<&ImageTrackList::GetSelectedTrack>("selectedTrack"),
                                    });

  constructor = Napi::Persistent(func);
  constructor.SuppressDestruct();
  exports.Set("ImageTrackList", func);
  return exports;
}

ImageTrackList::ImageTrackList(const Napi::CallbackInfo& info) : Napi::ObjectWrap<ImageTrackList>(info) {
  Napi::Env env = info.Env();

  // [SPEC] Constructor Algorithm
  /*
   * Refer to spec context.
   */

  // TODO(impl): Implement Constructor & Resource Allocation
}

ImageTrackList::~ImageTrackList() { Release(); }

void ImageTrackList::Release() {
  // TODO(impl): Free handle_ and native resources
  handle_ = nullptr;
}

// --- Attributes ---

Napi::Value ImageTrackList::GetReady(const Napi::CallbackInfo& info) {
  // TODO(impl): Return ready
  return info.Env().Null();
}

Napi::Value ImageTrackList::GetLength(const Napi::CallbackInfo& info) {
  // TODO(impl): Return length
  return info.Env().Null();
}

Napi::Value ImageTrackList::GetSelectedIndex(const Napi::CallbackInfo& info) {
  // TODO(impl): Return selectedIndex
  return info.Env().Null();
}

Napi::Value ImageTrackList::GetSelectedTrack(const Napi::CallbackInfo& info) {
  // TODO(impl): Return selectedTrack
  return info.Env().Null();
}

// --- Methods ---

}  // namespace webcodecs
