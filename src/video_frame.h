#pragma once
#include <napi.h>
#include <atomic>
#include "shared/utils.h"
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
  explicit VideoFrame(const Napi::CallbackInfo& info);
  ~VideoFrame() override;

  // RAII Release - unrefs the AVFrame buffers
  void Release();

  // Factory method to create a VideoFrame from an existing AVFrame
  // The AVFrame is cloned (refcounted), so caller retains ownership
  static Napi::Object CreateFromAVFrame(Napi::Env env, const AVFrame* frame);

  // Factory method to clone a VideoFrame, copying all internal slots
  // Used by clone() and VideoFrame(VideoFrame, init) constructor
  static Napi::Object CloneFrom(Napi::Env env, VideoFrame* source);

  // Get the underlying AVFrame (for internal use by decoder/encoder)
  AVFrame* GetAVFrame() const { return frame_.get(); }

  // Public for InstanceOf checks in encoder
  static Napi::FunctionReference constructor;

 private:

  // --- FFmpeg Resource (RAII managed) ---
  // Uses av_frame_ref for cloning, av_frame_unref on release
  raii::AVFramePtr frame_;

  // --- Closed State ---
  // Once closed, all accessors return null/throw
  std::atomic<bool> closed_{false};

  // --- WebCodecs Internal Slots ---
  // These store WebCodecs-specific properties not directly in AVFrame

  // [SPEC] [[rotation]] - 0, 90, 180, or 270 degrees
  int rotation_{0};

  // [SPEC] [[flip]] - whether the frame is flipped vertically
  bool flip_{false};

  // [SPEC] [[visible left/top/width/height]] - visible rect (may differ from coded)
  // We store these explicitly instead of using AVFrame crop fields for spec compliance
  int visible_left_{0};
  int visible_top_{0};
  int visible_width_{0};   // 0 means use coded width
  int visible_height_{0};  // 0 means use coded height

  // [SPEC] [[display width/height]] - display dimensions (may differ from visible)
  // 0 means use visible dimensions
  int display_width_{0};
  int display_height_{0};

  // --- Metadata Internal Slot ---
  // [SPEC] [[metadata]] - VideoFrameMetadata dictionary
  // Stored as persistent reference to allow structured cloning on access
  Napi::Reference<Napi::Object> metadata_;

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

  // --- Transfer/Serialization Support ---
  // W3C WebCodecs spec 9.4.7: Transfer and Serialization
  // serializeForTransfer(transfer: boolean) - creates a transferable clone
  // If transfer=true, this frame becomes detached (closed)
  Napi::Value SerializeForTransfer(const Napi::CallbackInfo& info);
};

}  // namespace webcodecs
