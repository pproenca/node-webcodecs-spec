#pragma once
#include <napi.h>
#include "../shared/Utils.h"

namespace webcodecs {

/**
 * AudioData - W3C WebCodecs AudioData implementation
 * @see spec/context/AudioData.md
 */
class AudioData : public Napi::ObjectWrap<AudioData> {
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  AudioData(const Napi::CallbackInfo& info);
  ~AudioData() override;

  // RAII Release
  void Release();

private:
  static Napi::FunctionReference constructor;

  // Internal Native Handle
  // TODO(impl): Define strict handle type (e.g., AVCodecContext*)
  void* handle_ = nullptr;

  // Attributes
  Napi::Value GetFormat(const Napi::CallbackInfo& info);
  Napi::Value GetSampleRate(const Napi::CallbackInfo& info);
  Napi::Value GetNumberOfFrames(const Napi::CallbackInfo& info);
  Napi::Value GetNumberOfChannels(const Napi::CallbackInfo& info);
  Napi::Value GetDuration(const Napi::CallbackInfo& info);
  Napi::Value GetTimestamp(const Napi::CallbackInfo& info);


  // Methods
  Napi::Value AllocationSize(const Napi::CallbackInfo& info);
  Napi::Value CopyTo(const Napi::CallbackInfo& info);
  Napi::Value Clone(const Napi::CallbackInfo& info);
  Napi::Value Close(const Napi::CallbackInfo& info);
};

}  // namespace webcodecs
