#pragma once
/**
 * video_encoder.h - W3C WebCodecs VideoEncoder implementation
 *
 * Architecture:
 * - JS main thread: handles API calls, enqueues messages to worker
 * - Worker thread: processes FFmpeg codec operations
 * - Output via SafeThreadSafeFunction back to JS thread
 *
 * Thread Safety:
 * - All public methods are called from the JS main thread
 * - Encode operations run on dedicated worker thread
 * - State transitions use atomic operations
 * - Output ordering guaranteed by single worker thread (FIFO)
 *
 * @see https://www.w3.org/TR/webcodecs/#videoencoder-interface
 */

#include <napi.h>
#include <memory>
#include <optional>
#include <queue>
#include <unordered_map>
#include "shared/utils.h"
#include "shared/control_message_queue.h"
#include "shared/codec_worker.h"
#include "shared/safe_tsfn.h"
#include "ffmpeg_raii.h"

namespace webcodecs {

// Forward declarations
class VideoEncoderWorker;

/**
 * VideoEncoder - W3C WebCodecs VideoEncoder implementation
 *
 * Encodes VideoFrame objects into EncodedVideoChunk outputs.
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

  // Access constructor for static methods
  static Napi::FunctionReference constructor;

  // RAII Release - cleans up all resources
  void Release();

 private:
  // --- Message Queue ---
  VideoControlQueue queue_;

  // --- Worker Thread ---
  std::unique_ptr<VideoEncoderWorker> worker_;

  // --- Thread-Safe State ---
  raii::AtomicCodecState state_;

  // --- Encode Queue Size (atomic for JS access) ---
  std::atomic<uint32_t> encode_queue_size_{0};

  // --- [[dequeue event scheduled]] per spec ---
  // Prevents duplicate ondequeue events within the same task
  // Coalesces multiple queue size changes into one event
  std::atomic<bool> dequeue_event_scheduled_{false};

  // --- [[codec saturated]] per spec ---
  // True when codec cannot accept more work (EAGAIN from FFmpeg)
  // Cleared after successfully receiving output packets
  std::atomic<bool> codec_saturated_{false};

  // --- Active Orientation Tracking (per spec) ---
  struct Orientation {
    int rotation = 0;    // 0, 90, 180, 270 degrees
    bool flip = false;   // Horizontal flip
  };
  std::optional<Orientation> active_orientation_;
  std::mutex orientation_mutex_;

  // --- Pending Flush Promises ---
  std::unordered_map<uint32_t, Napi::Promise::Deferred> pending_flushes_;
  uint32_t next_flush_id_{0};
  std::mutex flush_mutex_;

  // --- Data structs for TSFN callbacks (must be defined before TSFN types) ---

  /**
   * Output data passed through TSFN for encoded chunks.
   *
   * Thread Safety: All fields are populated on the worker thread before
   * being passed through TSFN. The JS thread only reads from this struct,
   * never accesses codec_ctx_ directly.
   */
  struct OutputData {
    raii::AVPacketPtr packet;
    bool is_key_frame;
    int64_t timestamp;       // microseconds
    int64_t duration;        // microseconds
    bool include_decoder_config;  // First keyframe after configure
    std::vector<uint8_t> extradata;  // Codec extradata (copied from worker thread)
    std::string codec;       // Codec string for decoderConfig
    int coded_width;         // Coded width for decoderConfig
    int coded_height;        // Coded height for decoderConfig
  };

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
  static void OnOutputChunk(Napi::Env env, Napi::Function jsCallback,
                            VideoEncoder* context, OutputData* data);
  static void OnError(Napi::Env env, Napi::Function jsCallback,
                      VideoEncoder* context, ErrorData* data);
  static void OnFlushComplete(Napi::Env env, Napi::Function jsCallback,
                              VideoEncoder* context, FlushCompleteData* data);
  static void OnDequeue(Napi::Env env, Napi::Function jsCallback,
                        VideoEncoder* context, DequeueData* data);

  // --- TSFN types with CallJs template parameter ---
  using OutputTSFN = SafeThreadSafeFunction<VideoEncoder, OutputData, &VideoEncoder::OnOutputChunk>;
  OutputTSFN output_tsfn_;

  using ErrorTSFN = SafeThreadSafeFunction<VideoEncoder, ErrorData, &VideoEncoder::OnError>;
  ErrorTSFN error_tsfn_;

  using FlushTSFN = SafeThreadSafeFunction<VideoEncoder, FlushCompleteData, &VideoEncoder::OnFlushComplete>;
  FlushTSFN flush_tsfn_;

  using DequeueTSFN = SafeThreadSafeFunction<VideoEncoder, DequeueData, &VideoEncoder::OnDequeue>;
  DequeueTSFN dequeue_tsfn_;

  // --- JS Callbacks (stored for TSFN delivery) ---
  Napi::FunctionReference output_callback_;
  Napi::FunctionReference error_callback_;
  Napi::FunctionReference ondequeue_callback_;

  // --- Encoder Configuration (deep copy for async processing) ---
  struct EncoderConfig {
    std::string codec;
    int width = 0;
    int height = 0;
    int display_width = 0;
    int display_height = 0;
    int64_t bitrate = 0;
    double framerate = 0;
    std::string hardware_acceleration;  // "no-preference", "prefer-hardware", "prefer-software"
    std::string alpha;                  // "keep" or "discard"
    std::string scalability_mode;       // SVC mode (e.g., "L1T1")
    std::string bitrate_mode;           // "constant", "variable", "quantizer"
    std::string latency_mode;           // "quality" or "realtime"
  };
  EncoderConfig active_config_;

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

  // --- Internal Helpers ---
  void InitializeTSFNs(Napi::Env env);
  void ReleaseTSFNs();

  // Friend class for worker access
  friend class VideoEncoderWorker;
};

/**
 * VideoEncoderWorker - Worker thread implementation for VideoEncoder
 *
 * Processes messages from VideoControlQueue:
 * - Configure: avcodec_open2 with encoder
 * - Encode: avcodec_send_frame + receive_packet loop
 * - Flush: drain encoder
 * - Reset: avcodec_flush_buffers
 */
class VideoEncoderWorker : public CodecWorker<VideoControlQueue> {
 public:
  explicit VideoEncoderWorker(VideoControlQueue& queue, VideoEncoder* encoder);
  ~VideoEncoderWorker() override;

  // Non-copyable, non-movable
  VideoEncoderWorker(const VideoEncoderWorker&) = delete;
  VideoEncoderWorker& operator=(const VideoEncoderWorker&) = delete;

  // Access to codec context for metadata generation
  AVCodecContext* GetCodecContext() const { return codec_ctx_.get(); }

 protected:
  bool OnConfigure(const ConfigureMessage& msg) override;
  void OnEncode(const EncodeMessage& msg) override;
  void OnFlush(const FlushMessage& msg) override;
  void OnReset() override;
  void OnClose() override;

 private:
  VideoEncoder* encoder_;  // Parent encoder (for callbacks)

  // --- FFmpeg Resources (owned by worker thread) ---
  raii::AVCodecContextPtr codec_ctx_;

  // --- Encoder State ---
  bool first_output_after_configure_{true};  // For decoderConfig metadata
  int64_t frame_count_{0};

  // --- Codec Parameters (thread-local copies from config) ---
  // These are copied from active_config_ during OnConfigure to avoid cross-thread access
  std::string codec_;  // Codec string for decoderConfig metadata
  int width_ = 0;
  int height_ = 0;
  AVPixelFormat format_ = AV_PIX_FMT_NONE;

  // --- Output Helpers ---
  void OutputChunk(raii::AVPacketPtr packet, bool is_key, int64_t ts, int64_t dur, bool include_config);
  void OutputError(int code, const std::string& message);
  void FlushComplete(uint32_t promise_id, bool success, const std::string& error);
  void SignalDequeue(uint32_t new_size);
};

}  // namespace webcodecs
