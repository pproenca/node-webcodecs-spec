#pragma once
#include <napi.h>
#include <atomic>
#include "shared/Utils.h"
#include "ffmpeg_raii.h"

namespace webcodecs {

/**
 * VideoFrame - W3C WebCodecs VideoFrame implementation
 * @see spec/context/VideoFrame.md
 *
 * Resource Management:
 * - Wraps an AVFrame using RAII (AVFramePtr)
 * - Uses av_frame_ref for clone() to share underlying buffer memory
 * - close() releases the frame immediately
 * - Destructor ensures cleanup even if close() not called
 *
 * Thread Safety:
 * - VideoFrame instances should only be accessed from JS main thread
 * - The underlying AVFrame buffers are refcounted and thread-safe
 */
class VideoFrame : public Napi::ObjectWrap<VideoFrame> {
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  VideoFrame(const Napi::CallbackInfo& info);
  ~VideoFrame() override;

  // RAII Release - unrefs the AVFrame buffers
  void Release();

  // Factory method to create a VideoFrame from an existing AVFrame
  // The AVFrame is cloned (refcounted), so caller retains ownership
  static Napi::Object CreateFromAVFrame(Napi::Env env, const AVFrame* frame);

  // Get the underlying AVFrame (for internal use by decoder/encoder)
  AVFrame* GetAVFrame() const { return frame_.get(); }

private:
  static Napi::FunctionReference constructor;

  // --- FFmpeg Resource (RAII managed) ---
  // Uses av_frame_ref for cloning, av_frame_unref on release
  raii::AVFramePtr frame_;

  // --- Closed State ---
  // Once closed, all accessors return null/throw
  std::atomic<bool> closed_{false};

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
