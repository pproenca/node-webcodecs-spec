#pragma once
#include <napi.h>
#include "shared/utils.h"

namespace webcodecs {

/**
 * ImageDecoder - W3C WebCodecs ImageDecoder implementation
 * @see spec/context/ImageDecoder.md
 */
class ImageDecoder : public Napi::ObjectWrap<ImageDecoder> {
 public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  explicit ImageDecoder(const Napi::CallbackInfo& info);
  ~ImageDecoder() override;

  // RAII Release
  void Release();

 private:
  static Napi::FunctionReference constructor;

  // Internal Native Handle
  // TODO(impl): Define strict handle type (e.g., AVCodecContext*)
  void* handle_ = nullptr;

  // Attributes
  Napi::Value GetType(const Napi::CallbackInfo& info);
  Napi::Value GetComplete(const Napi::CallbackInfo& info);
  Napi::Value GetCompleted(const Napi::CallbackInfo& info);
  Napi::Value GetTracks(const Napi::CallbackInfo& info);

  // Methods
  Napi::Value Decode(const Napi::CallbackInfo& info);
  Napi::Value Reset(const Napi::CallbackInfo& info);
  Napi::Value Close(const Napi::CallbackInfo& info);
  static Napi::Value IsTypeSupported(const Napi::CallbackInfo& info);
};

}  // namespace webcodecs
