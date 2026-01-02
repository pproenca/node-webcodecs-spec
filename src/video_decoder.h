#pragma once
#include <napi.h>
#include <memory>
#include <queue>
#include <unordered_map>
#include "shared/utils.h"
#include "shared/control_message_queue.h"
#include "shared/codec_worker.h"
#include "shared/safe_tsfn.h"
#include "shared/frame_pool.h"
#include "ffmpeg_raii.h"

namespace webcodecs {

// Forward declarations
class VideoDecoderWorker;

/**
 * VideoDecoder - W3C WebCodecs VideoDecoder implementation
 * @see spec/context/VideoDecoder.md
 *
 * Architecture:
 * - JS main thread: handles API calls, enqueues messages to worker
 * - Worker thread: processes FFmpeg codec operations
 * - Output via SafeThreadSafeFunction back to JS thread
 *
 * Thread Safety:
 * - All public methods are called from the JS main thread
 * - Decode operations run on dedicated worker thread
 * - State transitions use atomic operations
 * - Output ordering guaranteed by single worker thread (FIFO)
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

  // Access constructor for static methods and VideoFrame creation
  static Napi::FunctionReference constructor;

  // RAII Release - cleans up all resources
  void Release();

 private:
  // --- Message Queue ---
  VideoControlQueue queue_;

  // --- Worker Thread ---
  std::unique_ptr<VideoDecoderWorker> worker_;

  // --- Thread-Safe State ---
  raii::AtomicCodecState state_;

  // --- Decode Queue Size (atomic for JS access) ---
  std::atomic<uint32_t> decode_queue_size_{0};

  // --- [[dequeue event scheduled]] per spec ---
  // Prevents duplicate ondequeue events within the same task
  // Coalesces multiple queue size changes into one event
  std::atomic<bool> dequeue_event_scheduled_{false};

  // --- [[codec saturated]] per spec ---
  // True when codec cannot accept more work (EAGAIN from FFmpeg)
  // Cleared after successfully receiving output frames
  std::atomic<bool> codec_saturated_{false};

  // --- Key Chunk Tracking ---
  std::atomic<bool> key_chunk_required_{true};

  // --- Pending Flush Promises ---
  std::unordered_map<uint32_t, Napi::Promise::Deferred> pending_flushes_;
  uint32_t next_flush_id_{0};
  std::mutex flush_mutex_;

  // --- Data structs for TSFN callbacks (must be defined before TSFN types) ---
  struct ErrorData {
    int error_code;
    std::string message;
  };

  struct FlushCompleteData {
    uint32_t promise_id;
    bool success;
    std::string error_message;
  };

  struct DequeueData {
    uint32_t new_queue_size;
  };

  // --- TSFN Callback Handlers (called on JS thread) ---
  static void OnOutputFrame(Napi::Env env, Napi::Function jsCallback,
                            VideoDecoder* context, AVFrame** data);
  static void OnError(Napi::Env env, Napi::Function jsCallback,
                      VideoDecoder* context, ErrorData* data);
  static void OnFlushComplete(Napi::Env env, Napi::Function jsCallback,
                              VideoDecoder* context, FlushCompleteData* data);
  static void OnDequeue(Napi::Env env, Napi::Function jsCallback,
                        VideoDecoder* context, DequeueData* data);

  // --- TSFN types with CallJs template parameter ---
  using OutputTSFN = SafeThreadSafeFunction<VideoDecoder, AVFrame*, &VideoDecoder::OnOutputFrame>;
  OutputTSFN output_tsfn_;

  using ErrorTSFN = SafeThreadSafeFunction<VideoDecoder, ErrorData, &VideoDecoder::OnError>;
  ErrorTSFN error_tsfn_;

  using FlushTSFN = SafeThreadSafeFunction<VideoDecoder, FlushCompleteData, &VideoDecoder::OnFlushComplete>;
  FlushTSFN flush_tsfn_;

  using DequeueTSFN = SafeThreadSafeFunction<VideoDecoder, DequeueData, &VideoDecoder::OnDequeue>;
  DequeueTSFN dequeue_tsfn_;

  // --- JS Callbacks (stored for TSFN delivery) ---
  Napi::FunctionReference output_callback_;
  Napi::FunctionReference error_callback_;
  Napi::FunctionReference ondequeue_callback_;

  // --- Codec Configuration (deep copy for async processing) ---
  struct DecoderConfig {
    std::string codec;
    int coded_width = 0;
    int coded_height = 0;
    std::vector<uint8_t> description;
    std::string hardware_acceleration;
    bool optimize_for_latency = false;
  };
  DecoderConfig active_config_;

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

  // --- Internal Helpers ---
  void InitializeTSFNs(Napi::Env env);
  void ReleaseTSFNs();

  // Friend class for worker access
  friend class VideoDecoderWorker;
};

/**
 * VideoDecoderWorker - Worker thread implementation for VideoDecoder
 *
 * Processes messages from VideoControlQueue:
 * - Configure: avcodec_open2
 * - Decode: avcodec_send_packet + receive_frame loop
 * - Flush: drain codec
 * - Reset: avcodec_flush_buffers
 */
class VideoDecoderWorker : public CodecWorker<VideoControlQueue> {
 public:
  explicit VideoDecoderWorker(VideoControlQueue& queue, VideoDecoder* decoder);
  ~VideoDecoderWorker() override;

  // Non-copyable, non-movable
  VideoDecoderWorker(const VideoDecoderWorker&) = delete;
  VideoDecoderWorker& operator=(const VideoDecoderWorker&) = delete;

 protected:
  bool OnConfigure(const ConfigureMessage& msg) override;
  void OnDecode(const DecodeMessage& msg) override;
  void OnFlush(const FlushMessage& msg) override;
  void OnReset() override;
  void OnClose() override;

 private:
  VideoDecoder* decoder_;  // Parent decoder (for callbacks)

  // --- FFmpeg Resources (owned by worker thread) ---
  raii::AVCodecContextPtr codec_ctx_;

  // --- Frame Pool Handle ---
  FramePoolHandle frame_pool_;

  // --- Key Frame Tracking ---
  std::atomic<bool> key_chunk_required_{true};

  // --- Codec Parameters (copied from config) ---
  int width_ = 0;
  int height_ = 0;
  AVPixelFormat format_ = AV_PIX_FMT_NONE;
};

}  // namespace webcodecs
