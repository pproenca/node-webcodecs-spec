#pragma once
#include <napi.h>
#include "shared/utils.h"

namespace webcodecs {

/**
 * VideoColorSpace - W3C WebCodecs VideoColorSpace implementation
 * @see spec/context/VideoColorSpace.md
 */
class VideoColorSpace : public Napi::ObjectWrap<VideoColorSpace> {
 public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  explicit VideoColorSpace(const Napi::CallbackInfo& info);
  ~VideoColorSpace() override;

  // RAII Release
  void Release();

 private:
  static Napi::FunctionReference constructor;

  // Internal Native Handle
  // TODO(impl): Define strict handle type (e.g., AVCodecContext*)
  void* handle_ = nullptr;

  // Attributes
  Napi::Value GetPrimaries(const Napi::CallbackInfo& info);
  Napi::Value GetTransfer(const Napi::CallbackInfo& info);
  Napi::Value GetMatrix(const Napi::CallbackInfo& info);
  Napi::Value GetFullRange(const Napi::CallbackInfo& info);

  // Methods
  Napi::Value ToJSON(const Napi::CallbackInfo& info);
};

}  // namespace webcodecs
