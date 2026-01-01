#pragma once
#include <napi.h>
#include "shared/Utils.h"

namespace webcodecs {

/**
 * VideoFrame - W3C WebCodecs VideoFrame implementation
 * @see spec/context/VideoFrame.md
 */
class VideoFrame : public Napi::ObjectWrap<VideoFrame> {
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  VideoFrame(const Napi::CallbackInfo& info);
  ~VideoFrame() override;

  // RAII Release
  void Release();

private:
  static Napi::FunctionReference constructor;

  // Internal Native Handle
  // TODO(impl): Define strict handle type (e.g., AVCodecContext*)
  void* handle_ = nullptr;

  // Attributes
  Napi::Value GetFormat(const Napi::CallbackInfo& info);
  Napi::Value GetCodedWidth(const Napi::CallbackInfo& info);
  Napi::Value GetCodedHeight(const Napi::CallbackInfo& info);
  Napi::Value GetCodedRect(const Napi::CallbackInfo& info);
  Napi::Value GetVisibleRect(const Napi::CallbackInfo& info);
  Napi::Value GetRotation(const Napi::CallbackInfo& info);
  Napi::Value GetFlip(const Napi::CallbackInfo& info);
  Napi::Value GetDisplayWidth(const Napi::CallbackInfo& info);
  Napi::Value GetDisplayHeight(const Napi::CallbackInfo& info);
  Napi::Value GetDuration(const Napi::CallbackInfo& info);
  Napi::Value GetTimestamp(const Napi::CallbackInfo& info);
  Napi::Value GetColorSpace(const Napi::CallbackInfo& info);


  // Methods
  Napi::Value Metadata(const Napi::CallbackInfo& info);
  Napi::Value AllocationSize(const Napi::CallbackInfo& info);
  Napi::Value CopyTo(const Napi::CallbackInfo& info);
  Napi::Value Clone(const Napi::CallbackInfo& info);
  Napi::Value Close(const Napi::CallbackInfo& info);
};

}  // namespace webcodecs
