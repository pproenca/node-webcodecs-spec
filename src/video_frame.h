#ifndef WEBCODECS_VIDEO_FRAME_H
#define WEBCODECS_VIDEO_FRAME_H

#include <napi.h>

namespace webcodecs {

/**
 * VideoFrame - WebCodecs VideoFrame implementation.
 *
 * Represents a raw video frame with pixel data in a specific format.
 * Wraps FFmpeg AVFrame for internal storage.
 *
 * Memory Model:
 * - VideoFrame owns its buffer data
 * - close() releases the buffer immediately
 * - Garbage collection triggers close() if not called
 */
class VideoFrame : public Napi::ObjectWrap<VideoFrame> {
 public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  explicit VideoFrame(const Napi::CallbackInfo& info);
  ~VideoFrame() override;

 private:
  // WebCodecs API methods
  Napi::Value Clone(const Napi::CallbackInfo& info);
  void Close(const Napi::CallbackInfo& info);
  Napi::Value CopyTo(const Napi::CallbackInfo& info);
  Napi::Value AllocationSize(const Napi::CallbackInfo& info);

  // Properties
  Napi::Value GetFormat(const Napi::CallbackInfo& info);
  Napi::Value GetCodedWidth(const Napi::CallbackInfo& info);
  Napi::Value GetCodedHeight(const Napi::CallbackInfo& info);
  Napi::Value GetDisplayWidth(const Napi::CallbackInfo& info);
  Napi::Value GetDisplayHeight(const Napi::CallbackInfo& info);
  Napi::Value GetTimestamp(const Napi::CallbackInfo& info);
  Napi::Value GetDuration(const Napi::CallbackInfo& info);
  Napi::Value GetColorSpace(const Napi::CallbackInfo& info);

  // Instance state
  bool closed_ = false;
};

}  // namespace webcodecs

#endif  // WEBCODECS_VIDEO_FRAME_H
