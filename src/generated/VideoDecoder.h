#pragma once
#include <napi.h>
#include "../shared/Utils.h"

namespace webcodecs {

/**
 * VideoDecoder - W3C WebCodecs VideoDecoder implementation
 * @see spec/context/VideoDecoder.md
 */
class VideoDecoder : public Napi::ObjectWrap<VideoDecoder> {
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  VideoDecoder(const Napi::CallbackInfo& info);
  ~VideoDecoder() override;

  // RAII Release
  void Release();

private:
  static Napi::FunctionReference constructor;

  // Internal Native Handle
  // TODO(impl): Define strict handle type (e.g., AVCodecContext*)
  void* handle_ = nullptr;

  // Attributes
  Napi::Value GetState(const Napi::CallbackInfo& info);
  Napi::Value GetDecodeQueueSize(const Napi::CallbackInfo& info);
  Napi::Value GetOndequeue(const Napi::CallbackInfo& info);
  void SetOndequeue(const Napi::CallbackInfo& info, const Napi::Value& value);

  // Methods
  Napi::Value Configure(const Napi::CallbackInfo& info);
  Napi::Value Decode(const Napi::CallbackInfo& info);
  Napi::Value Flush(const Napi::CallbackInfo& info);
  Napi::Value Reset(const Napi::CallbackInfo& info);
  Napi::Value Close(const Napi::CallbackInfo& info);
  static Napi::Value IsConfigSupported(const Napi::CallbackInfo& info);
};

}  // namespace webcodecs
