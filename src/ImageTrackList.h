#pragma once
#include <napi.h>
#include "shared/Utils.h"

namespace webcodecs {

/**
 * ImageTrackList - W3C WebCodecs ImageTrackList implementation
 * @see spec/context/ImageTrackList.md
 */
class ImageTrackList : public Napi::ObjectWrap<ImageTrackList> {
 public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  explicit ImageTrackList(const Napi::CallbackInfo& info);
  ~ImageTrackList() override;

  // RAII Release
  void Release();

 private:
  static Napi::FunctionReference constructor;

  // Internal Native Handle
  // TODO(impl): Define strict handle type (e.g., AVCodecContext*)
  void* handle_ = nullptr;

  // Attributes
  Napi::Value GetReady(const Napi::CallbackInfo& info);
  Napi::Value GetLength(const Napi::CallbackInfo& info);
  Napi::Value GetSelectedIndex(const Napi::CallbackInfo& info);
  Napi::Value GetSelectedTrack(const Napi::CallbackInfo& info);

  // Methods
};

}  // namespace webcodecs
