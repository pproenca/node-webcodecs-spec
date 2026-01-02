#pragma once
#include <napi.h>
#include <memory>
#include <queue>
#include <unordered_map>
#include "shared/utils.h"
#include "shared/control_message_queue.h"
#include "shared/codec_worker.h"
#include "shared/safe_tsfn.h"
#include "ffmpeg_raii.h"

namespace webcodecs {

// Forward declarations
class AudioDecoderWorker;

/**
 * AudioDecoder - W3C WebCodecs AudioDecoder implementation
 * @see spec/context/AudioDecoder.md
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
class AudioDecoder : public Napi::ObjectWrap<AudioDecoder> {
 public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  explicit AudioDecoder(const Napi::CallbackInfo& info);
  ~AudioDecoder() override;

  // Non-copyable, non-movable (Google C++ Style Guide)
  AudioDecoder(const AudioDecoder&) = delete;
  AudioDecoder& operator=(const AudioDecoder&) = delete;
  AudioDecoder(AudioDecoder&&) = delete;
  AudioDecoder& operator=(AudioDecoder&&) = delete;

  // Access constructor for static methods and AudioData creation
  static Napi::FunctionReference constructor;

  // RAII Release - cleans up all resources
  void Release();

 private:
  // --- Message Queue ---
  AudioControlQueue queue_;

  // --- Worker Thread ---
  std::unique_ptr<AudioDecoderWorker> worker_;

  // --- Thread-Safe State ---
  raii::AtomicCodecState state_;

  // --- Decode Queue Size (atomic for JS access) ---
  std::atomic<uint32_t> decode_queue_size_{0};

  // --- Key Chunk Tracking ---
  std::atomic<bool> key_chunk_required_{true};

  // --- [SPEC] [[dequeue event scheduled]] - coalesces multiple dequeue events ---
  std::atomic<bool> dequeue_event_scheduled_{false};

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
                            AudioDecoder* context, AVFrame** data);
  static void OnError(Napi::Env env, Napi::Function jsCallback,
                      AudioDecoder* context, ErrorData* data);
  static void OnFlushComplete(Napi::Env env, Napi::Function jsCallback,
                              AudioDecoder* context, FlushCompleteData* data);
  static void OnDequeue(Napi::Env env, Napi::Function jsCallback,
                        AudioDecoder* context, DequeueData* data);

  // --- TSFN types with CallJs template parameter ---
  using OutputTSFN = SafeThreadSafeFunction<AudioDecoder, AVFrame*, &AudioDecoder::OnOutputFrame>;
  OutputTSFN output_tsfn_;

  using ErrorTSFN = SafeThreadSafeFunction<AudioDecoder, ErrorData, &AudioDecoder::OnError>;
  ErrorTSFN error_tsfn_;

  using FlushTSFN = SafeThreadSafeFunction<AudioDecoder, FlushCompleteData, &AudioDecoder::OnFlushComplete>;
  FlushTSFN flush_tsfn_;

  using DequeueTSFN = SafeThreadSafeFunction<AudioDecoder, DequeueData, &AudioDecoder::OnDequeue>;
  DequeueTSFN dequeue_tsfn_;

  // --- JS Callbacks (stored for TSFN delivery) ---
  Napi::FunctionReference output_callback_;
  Napi::FunctionReference error_callback_;
  Napi::FunctionReference ondequeue_callback_;

  // --- Codec Configuration (deep copy for async processing) ---
  struct DecoderConfig {
    std::string codec;
    int sample_rate = 0;
    int number_of_channels = 0;
    std::vector<uint8_t> description;
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
  friend class AudioDecoderWorker;
};

/**
 * AudioDecoderWorker - Worker thread implementation for AudioDecoder
 *
 * Processes messages from AudioControlQueue:
 * - Configure: avcodec_open2
 * - Decode: avcodec_send_packet + receive_frame loop
 * - Flush: drain codec
 * - Reset: avcodec_flush_buffers
 */
class AudioDecoderWorker : public CodecWorker<AudioControlQueue> {
 public:
  explicit AudioDecoderWorker(AudioControlQueue& queue, AudioDecoder* decoder);
  ~AudioDecoderWorker() override;

  // Non-copyable, non-movable
  AudioDecoderWorker(const AudioDecoderWorker&) = delete;
  AudioDecoderWorker& operator=(const AudioDecoderWorker&) = delete;

 protected:
  bool OnConfigure(const ConfigureMessage& msg) override;
  void OnDecode(const DecodeMessage& msg) override;
  void OnFlush(const FlushMessage& msg) override;
  void OnReset() override;
  void OnClose() override;

 private:
  AudioDecoder* decoder_;  // Parent decoder (for callbacks)

  // --- FFmpeg Resources (owned by worker thread) ---
  raii::AVCodecContextPtr codec_ctx_;

  // --- Audio Parameters (copied from codec context after open) ---
  int sample_rate_ = 0;
  int channels_ = 0;
  AVSampleFormat sample_fmt_ = AV_SAMPLE_FMT_NONE;
};

}  // namespace webcodecs
