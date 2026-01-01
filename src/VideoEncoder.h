#pragma once
#include <napi.h>
#include "shared/Utils.h"

namespace webcodecs {

/**
 * VideoEncoder - W3C WebCodecs VideoEncoder implementation
 * @see spec/context/VideoEncoder.md
 */
class VideoEncoder : public Napi::ObjectWrap<VideoEncoder> {
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  VideoEncoder(const Napi::CallbackInfo& info);
  ~VideoEncoder() override;

  // RAII Release
  void Release();

private:
  static Napi::FunctionReference constructor;

  // Internal Native Handle
  // TODO(impl): Define strict handle type (e.g., AVCodecContext*)
  void* handle_ = nullptr;

  // Attributes
  Napi::Value GetState(const Napi::CallbackInfo& info);
  Napi::Value GetEncodeQueueSize(const Napi::CallbackInfo& info);
  Napi::Value GetOndequeue(const Napi::CallbackInfo& info);
  void SetOndequeue(const Napi::CallbackInfo& info, const Napi::Value& value);

  // Methods
  Napi::Value Configure(const Napi::CallbackInfo& info);
  Napi::Value Encode(const Napi::CallbackInfo& info);
  Napi::Value Flush(const Napi::CallbackInfo& info);
  Napi::Value Reset(const Napi::CallbackInfo& info);
  Napi::Value Close(const Napi::CallbackInfo& info);
  static Napi::Value IsConfigSupported(const Napi::CallbackInfo& info);
};

}  // namespace webcodecs
