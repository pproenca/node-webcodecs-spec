#pragma once
#include <napi.h>
#include <queue>
#include "shared/utils.h"
#include "ffmpeg_raii.h"

namespace webcodecs {

/**
 * VideoDecoder - W3C WebCodecs VideoDecoder implementation
 * @see spec/context/VideoDecoder.md
 *
 * Thread Safety:
 * - All public methods are called from the JS main thread
 * - Decode operations run on a worker thread via AsyncDecodeContext
 * - State transitions use atomic operations
 * - Codec operations are protected by mutex
 */
class VideoDecoder : public Napi::ObjectWrap<VideoDecoder> {
 public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  explicit VideoDecoder(const Napi::CallbackInfo& info);
  ~VideoDecoder() override;

  // Non-copyable, non-movable (Google C++ Style Guide)
  VideoDecoder(const VideoDecoder&) = delete;
  VideoDecoder& operator=(const VideoDecoder&) = delete;
  VideoDecoder(VideoDecoder&&) = delete;
  VideoDecoder& operator=(VideoDecoder&&) = delete;

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

  // --- Decode Queue ---
  std::queue<raii::AVPacketPtr> decode_queue_;
  std::atomic<uint32_t> decode_queue_size_{0};

  // --- Key Chunk Tracking ---
  std::atomic<bool> key_chunk_required_{true};

  // --- Callbacks ---
  Napi::FunctionReference output_callback_;
  Napi::FunctionReference error_callback_;
  Napi::FunctionReference ondequeue_callback_;

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
