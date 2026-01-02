#pragma once
#include <napi.h>
#include "shared/Utils.h"

namespace webcodecs {

/**
 * ImageTrack - W3C WebCodecs ImageTrack implementation
 * @see spec/context/ImageTrack.md
 */
class ImageTrack : public Napi::ObjectWrap<ImageTrack> {
 public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  explicit ImageTrack(const Napi::CallbackInfo& info);
  ~ImageTrack() override;

  // RAII Release
  void Release();

 private:
  static Napi::FunctionReference constructor;

  // Internal Native Handle
  // TODO(impl): Define strict handle type (e.g., AVCodecContext*)
  void* handle_ = nullptr;

  // Attributes
  Napi::Value GetAnimated(const Napi::CallbackInfo& info);
  Napi::Value GetFrameCount(const Napi::CallbackInfo& info);
  Napi::Value GetRepetitionCount(const Napi::CallbackInfo& info);
  Napi::Value GetSelected(const Napi::CallbackInfo& info);
  void SetSelected(const Napi::CallbackInfo& info, const Napi::Value& value);

  // Methods
};

}  // namespace webcodecs
