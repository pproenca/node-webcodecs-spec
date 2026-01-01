#pragma once
#include <napi.h>
#include "shared/Utils.h"

namespace webcodecs {

/**
 * EncodedVideoChunk - W3C WebCodecs EncodedVideoChunk implementation
 * @see spec/context/EncodedVideoChunk.md
 */
class EncodedVideoChunk : public Napi::ObjectWrap<EncodedVideoChunk> {
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  EncodedVideoChunk(const Napi::CallbackInfo& info);
  ~EncodedVideoChunk() override;

  // RAII Release
  void Release();

private:
  static Napi::FunctionReference constructor;

  // Internal Native Handle
  // TODO(impl): Define strict handle type (e.g., AVCodecContext*)
  void* handle_ = nullptr;

  // Attributes
  Napi::Value GetType(const Napi::CallbackInfo& info);
  Napi::Value GetTimestamp(const Napi::CallbackInfo& info);
  Napi::Value GetDuration(const Napi::CallbackInfo& info);
  Napi::Value GetByteLength(const Napi::CallbackInfo& info);


  // Methods
  Napi::Value CopyTo(const Napi::CallbackInfo& info);
};

}  // namespace webcodecs
