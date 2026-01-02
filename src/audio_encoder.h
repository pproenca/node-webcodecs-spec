#pragma once
/**
 * audio_encoder.h - W3C WebCodecs AudioEncoder Implementation
 *
 * Encodes AudioData into EncodedAudioChunk using FFmpeg.
 * Uses async worker thread for non-blocking codec operations.
 *
 * Architecture:
 * - JS Main Thread: API surface, callback management, state machine
 * - Worker Thread: FFmpeg operations (AVCodecContext exclusive ownership)
 * - TSFN: Thread-safe delivery of results to JS thread
 *
 * @see https://www.w3.org/TR/webcodecs/#audioencoder-interface
 */

#include <napi.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "ffmpeg_raii.h"
#include "shared/control_message_queue.h"
#include "shared/codec_worker.h"
#include "shared/safe_tsfn.h"

namespace webcodecs {

// Forward declaration
class AudioEncoderWorker;

/**
 * AudioEncoder - W3C WebCodecs AudioEncoder implementation
 *
 * Encodes raw AudioData into compressed EncodedAudioChunk.
 * Supports Opus, AAC, MP3, FLAC, and other FFmpeg-supported codecs.
 */
class AudioEncoder : public Napi::ObjectWrap<AudioEncoder> {
 public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  explicit AudioEncoder(const Napi::CallbackInfo& info);
  ~AudioEncoder() override;

  // Non-copyable, non-movable
  AudioEncoder(const AudioEncoder&) = delete;
  AudioEncoder& operator=(const AudioEncoder&) = delete;
  AudioEncoder(AudioEncoder&&) = delete;
  AudioEncoder& operator=(AudioEncoder&&) = delete;

  // RAII Release - cleans up all resources
  void Release();

  // ==========================================================================
  // TSFN CALLBACK DATA STRUCTURES
  // ==========================================================================

  /**
   * Output data delivered via TSFN to JS thread.
   */
  struct OutputData {
    raii::AVPacketPtr packet;        // Encoded audio data (ownership transferred)
    bool is_key_frame;               // Key frame indicator
    int64_t timestamp;               // Microseconds
    int64_t duration;                // Microseconds
    bool include_decoder_config;     // Include metadata on first output
  };

  /**
   * Error notification data.
   */
  struct ErrorData {
    int error_code;
    std::string message;
  };

  /**
   * Flush completion notification.
   */
  struct FlushCompleteData {
    uint32_t promise_id;
    bool success;
    std::string error_message;
  };

  /**
   * Dequeue event notification.
   */
  struct DequeueData {
    uint32_t new_queue_size;
  };

  // ==========================================================================
  // ENCODER CONFIGURATION
  // ==========================================================================

  /**
   * Deep-copied configuration for async processing.
   */
  struct EncoderConfig {
    std::string codec;
    int sample_rate = 0;
    int number_of_channels = 0;
    int64_t bitrate = 0;
    std::string bitrate_mode;  // "constant" or "variable"
  };

  EncoderConfig active_config_;

  // ==========================================================================
  // STATIC MEMBERS
  // ==========================================================================

  static Napi::FunctionReference constructor;

 private:
  // ==========================================================================
  // TSFN CALLBACK HANDLERS (static - called on JS thread)
  // ==========================================================================

  static void OnOutputChunk(Napi::Env env, Napi::Function jsCallback,
                            AudioEncoder* context, OutputData* data);
  static void OnError(Napi::Env env, Napi::Function jsCallback,
                      AudioEncoder* context, ErrorData* data);
  static void OnFlushComplete(Napi::Env env, Napi::Function jsCallback,
                              AudioEncoder* context, FlushCompleteData* data);
  static void OnDequeue(Napi::Env env, Napi::Function jsCallback,
                        AudioEncoder* context, DequeueData* data);

  // ==========================================================================
  // TSFN TYPES AND INSTANCES
  // ==========================================================================

  using OutputTSFN = SafeThreadSafeFunction<AudioEncoder, OutputData, &AudioEncoder::OnOutputChunk>;
  using ErrorTSFN = SafeThreadSafeFunction<AudioEncoder, ErrorData, &AudioEncoder::OnError>;
  using FlushTSFN = SafeThreadSafeFunction<AudioEncoder, FlushCompleteData, &AudioEncoder::OnFlushComplete>;
  using DequeueTSFN = SafeThreadSafeFunction<AudioEncoder, DequeueData, &AudioEncoder::OnDequeue>;

  OutputTSFN output_tsfn_;
  ErrorTSFN error_tsfn_;
  FlushTSFN flush_tsfn_;
  DequeueTSFN dequeue_tsfn_;

  void InitializeTSFNs(Napi::Env env);
  void ReleaseTSFNs();

  // ==========================================================================
  // CORE COMPONENTS
  // ==========================================================================

  // Message queue (JS thread → Worker thread)
  AudioControlQueue queue_;

  // Worker thread (owns AVCodecContext exclusively)
  std::unique_ptr<AudioEncoderWorker> worker_;

  // Thread-safe atomic state
  raii::AtomicCodecState state_;

  // Encode queue size (atomic for JS reads without locking)
  std::atomic<uint32_t> encode_queue_size_{0};

  // [SPEC] [[dequeue event scheduled]] - coalesces multiple dequeue events
  std::atomic<bool> dequeue_event_scheduled_{false};

  // Pending flush promises (protected by flush_mutex_)
  std::unordered_map<uint32_t, Napi::Promise::Deferred> pending_flushes_;
  uint32_t next_flush_id_{0};
  std::mutex flush_mutex_;

  // ==========================================================================
  // JS CALLBACK REFERENCES
  // ==========================================================================

  Napi::FunctionReference output_callback_;
  Napi::FunctionReference error_callback_;
  Napi::FunctionReference ondequeue_callback_;

  // ==========================================================================
  // ATTRIBUTE ACCESSORS
  // ==========================================================================

  Napi::Value GetState(const Napi::CallbackInfo& info);
  Napi::Value GetEncodeQueueSize(const Napi::CallbackInfo& info);
  Napi::Value GetOndequeue(const Napi::CallbackInfo& info);
  void SetOndequeue(const Napi::CallbackInfo& info, const Napi::Value& value);

  // ==========================================================================
  // METHODS
  // ==========================================================================

  Napi::Value Configure(const Napi::CallbackInfo& info);
  Napi::Value Encode(const Napi::CallbackInfo& info);
  Napi::Value Flush(const Napi::CallbackInfo& info);
  Napi::Value Reset(const Napi::CallbackInfo& info);
  Napi::Value Close(const Napi::CallbackInfo& info);
  static Napi::Value IsConfigSupported(const Napi::CallbackInfo& info);

  // Friend declaration for worker access
  friend class AudioEncoderWorker;
};

// =============================================================================
// AUDIOENCODERWORKER - WORKER THREAD FOR FFMPEG OPERATIONS
// =============================================================================

/**
 * Worker thread that owns AVCodecContext and processes encode messages.
 *
 * Thread Safety:
 * - codec_ctx_ is exclusively owned by worker thread (no mutex needed)
 * - Communicates with JS thread via TSFN callbacks
 */
class AudioEncoderWorker : public CodecWorker<AudioControlQueue> {
 public:
  explicit AudioEncoderWorker(AudioControlQueue& queue, AudioEncoder* encoder);
  ~AudioEncoderWorker() override;

  // Non-copyable, non-movable
  AudioEncoderWorker(const AudioEncoderWorker&) = delete;
  AudioEncoderWorker& operator=(const AudioEncoderWorker&) = delete;
  AudioEncoderWorker(AudioEncoderWorker&&) = delete;
  AudioEncoderWorker& operator=(AudioEncoderWorker&&) = delete;

  /**
   * Get codec context (for metadata extraction).
   * Only call from JS thread after worker is stopped.
   */
  AVCodecContext* GetCodecContext() const { return codec_ctx_.get(); }

 protected:
  // ==========================================================================
  // VIRTUAL HANDLERS (called on worker thread)
  // ==========================================================================

  bool OnConfigure(const ConfigureMessage& msg) override;
  void OnEncode(const EncodeMessage& msg) override;
  void OnFlush(const FlushMessage& msg) override;
  void OnReset() override;
  void OnClose() override;

 private:
  // Parent encoder (for TSFN callbacks and config access)
  AudioEncoder* encoder_;

  // FFmpeg codec context (worker-thread-exclusive)
  raii::AVCodecContextPtr codec_ctx_;

  // Encoder state tracking
  bool first_output_after_configure_{true};
  int64_t sample_count_{0};  // Running sample position for PTS

  // Codec parameters (copied from context after open)
  int sample_rate_{0};
  int channels_{0};
  AVSampleFormat sample_fmt_{AV_SAMPLE_FMT_NONE};

  // ==========================================================================
  // OUTPUT HELPERS
  // ==========================================================================

  void OutputChunk(raii::AVPacketPtr packet, bool is_key, int64_t ts,
                   int64_t dur, bool include_config);
  void OutputError(int code, const std::string& message);
  void FlushComplete(uint32_t promise_id, bool success, const std::string& error);
  void SignalDequeue(uint32_t new_size);
};

}  // namespace webcodecs
