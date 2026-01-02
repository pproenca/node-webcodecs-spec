#pragma once
#include <napi.h>
#include <queue>
#include "shared/utils.h"
#include "ffmpeg_raii.h"

namespace webcodecs {

/**
 * VideoEncoder - W3C WebCodecs VideoEncoder implementation
 * @see spec/context/VideoEncoder.md
 *
 * Thread Safety:
 * - All public methods are called from the JS main thread
 * - Encode operations run on a worker thread via AsyncEncodeContext
 * - State transitions use atomic operations
 * - Codec operations are protected by mutex
 */
class VideoEncoder : public Napi::ObjectWrap<VideoEncoder> {
 public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  explicit VideoEncoder(const Napi::CallbackInfo& info);
  ~VideoEncoder() override;

  // Non-copyable, non-movable (Google C++ Style Guide)
  VideoEncoder(const VideoEncoder&) = delete;
  VideoEncoder& operator=(const VideoEncoder&) = delete;
  VideoEncoder(VideoEncoder&&) = delete;
  VideoEncoder& operator=(VideoEncoder&&) = delete;

  // RAII Release - cleans up all resources
  void Release();

 private:
  static Napi::FunctionReference constructor;

  // --- FFmpeg Resources (RAII managed) ---
  raii::AVCodecContextPtr codec_ctx_;

  // --- Thread-Safe State ---
  raii::AtomicCodecState state_;

  // --- Synchronization ---
  mutable std::mutex mutex_;

  // --- Encode Queue ---
  std::queue<raii::AVFramePtr> encode_queue_;
  std::atomic<uint32_t> encode_queue_size_{0};

  // --- Callbacks ---
  Napi::FunctionReference output_callback_;
  Napi::FunctionReference error_callback_;
  Napi::FunctionReference ondequeue_callback_;

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
