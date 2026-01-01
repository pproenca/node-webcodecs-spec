#pragma once
#include <napi.h>
#include "../shared/Utils.h"

namespace webcodecs {

/**
 * EncodedAudioChunk - W3C WebCodecs EncodedAudioChunk implementation
 * @see spec/context/EncodedAudioChunk.md
 */
class EncodedAudioChunk : public Napi::ObjectWrap<EncodedAudioChunk> {
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  EncodedAudioChunk(const Napi::CallbackInfo& info);
  ~EncodedAudioChunk() override;

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
