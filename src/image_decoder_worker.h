#pragma once
/**
 * image_decoder_worker.h - Async Worker for ImageDecoder
 *
 * Runs FFmpeg image decoding operations on a dedicated worker thread.
 * Unlike VideoDecoderWorker, this handles both demuxing and decoding since
 * image formats combine container and codec.
 *
 * Thread Safety:
 * - Main thread: Enqueue messages to ImageControlQueue
 * - Worker thread: Process messages, FFmpeg operations
 * - Output via callbacks (invoked from worker, dispatched via TSFN)
 */

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "ffmpeg_raii.h"
#include "shared/control_message_queue.h"

namespace webcodecs {

// Forward declaration
class ImageDecoder;

/**
 * Track information extracted from image file.
 */
struct ImageTrackInfo {
  bool animated = false;
  uint32_t frame_count = 1;
  float repetition_count = 0.0f;
  int stream_index = 0;
};

/**
 * Decoded frame result.
 */
struct ImageDecodeResult {
  raii::AVFramePtr frame;
  int64_t timestamp = 0;
  int64_t duration = 0;
  bool complete = true;  // Always true for non-progressive decoding
};

/**
 * ImageDecoderWorker - Worker thread for image decoding operations.
 *
 * Processes ImageControlQueue messages:
 * - Configure: Parse image data, establish tracks
 * - Decode: Decode specific frame by index
 * - Reset: Abort pending decodes
 * - Close: Release resources
 */
class ImageDecoderWorker {
 public:
  // Callback types for communicating results back to ImageDecoder
  using TrackInfoCallback =
      std::function<void(const std::vector<ImageTrackInfo>&, int32_t)>;
  using DecodeResultCallback =
      std::function<void(uint32_t, ImageDecodeResult)>;
  using ErrorCallback =
      std::function<void(uint32_t, int, const std::string&)>;
  using CompletedCallback = std::function<void()>;

  /**
   * Construct worker with reference to message queue.
   * Does NOT start the worker thread - call Start() explicitly.
   */
  explicit ImageDecoderWorker(ImageControlQueue& queue);
  ~ImageDecoderWorker();

  // Non-copyable, non-movable (owns thread)
  ImageDecoderWorker(const ImageDecoderWorker&) = delete;
  ImageDecoderWorker& operator=(const ImageDecoderWorker&) = delete;
  ImageDecoderWorker(ImageDecoderWorker&&) = delete;
  ImageDecoderWorker& operator=(ImageDecoderWorker&&) = delete;

  // =========================================================================
  // LIFECYCLE
  // =========================================================================

  /**
   * Start the worker thread.
   * Safe to call multiple times (idempotent).
   */
  bool Start();

  /**
   * Stop the worker thread.
   * Signals shutdown, waits for thread to exit, then joins.
   */
  void Stop();

  /**
   * Check if worker is currently running.
   */
  [[nodiscard]] bool IsRunning() const;

  /**
   * Check if worker should exit (for use in long operations).
   */
  [[nodiscard]] bool ShouldExit() const;

  // =========================================================================
  // CALLBACKS (set by ImageDecoder)
  // =========================================================================

  void SetTrackInfoCallback(TrackInfoCallback cb) {
    track_info_callback_ = std::move(cb);
  }

  void SetDecodeResultCallback(DecodeResultCallback cb) {
    decode_result_callback_ = std::move(cb);
  }

  void SetErrorCallback(ErrorCallback cb) {
    error_callback_ = std::move(cb);
  }

  void SetCompletedCallback(CompletedCallback cb) {
    completed_callback_ = std::move(cb);
  }

 private:
  /**
   * Main worker loop. Dequeues and processes messages.
   */
  void WorkerLoop();

  // =========================================================================
  // MESSAGE HANDLERS
  // =========================================================================

  /**
   * Handle Configure message.
   * Opens format context from memory, parses tracks, opens codec.
   */
  bool OnConfigure(const ImageConfigureMessage& msg);

  /**
   * Handle Decode message.
   * Seeks to and decodes the specified frame.
   */
  void OnDecode(const ImageDecodeMessage& msg);

  /**
   * Handle Reset message.
   * Flushes codec buffers and resets seek position.
   */
  void OnReset();

  /**
   * Handle Close message.
   * Releases all FFmpeg resources.
   */
  void OnClose();

  /**
   * Handle UpdateTrack message.
   * Changes the selected stream for decoding.
   */
  void OnUpdateTrack(const ImageUpdateTrackMessage& msg);

  /**
   * Handle StreamData message (streaming mode).
   * Appends data chunk to accumulated buffer.
   */
  void OnStreamData(const ImageStreamDataMessage& msg);

  /**
   * Handle StreamEnd message (streaming mode).
   * Signals that all data has been received.
   */
  void OnStreamEnd();

  /**
   * Handle StreamError message (streaming mode).
   * Signals that an error occurred reading the stream.
   */
  void OnStreamError(const ImageStreamErrorMessage& msg);

  // =========================================================================
  // HELPERS
  // =========================================================================

  /**
   * Signal track info to ImageDecoder.
   */
  void OutputTrackInfo(const std::vector<ImageTrackInfo>& tracks,
                       int32_t selected_index);

  /**
   * Signal decoded frame to ImageDecoder.
   */
  void OutputDecodeResult(uint32_t promise_id, ImageDecodeResult result);

  /**
   * Signal error to ImageDecoder.
   */
  void OutputError(uint32_t promise_id, int error_code,
                   const std::string& message);

  /**
   * Signal that all data has been read (completed).
   */
  void OutputCompleted();

  /**
   * Seek to a specific frame index.
   * Returns true if successful.
   */
  bool SeekToFrame(uint32_t frame_index);

  /**
   * Decode the next frame from the current position.
   */
  raii::AVFramePtr DecodeNextFrame(int64_t* timestamp, int64_t* duration);

  // Message queue reference (owned by ImageDecoder)
  ImageControlQueue& queue_;

  // Worker thread
  std::thread worker_thread_;
  std::mutex lifecycle_mutex_;
  std::atomic<bool> running_{false};
  std::atomic<bool> should_exit_{false};

  // Callbacks to ImageDecoder
  TrackInfoCallback track_info_callback_;
  DecodeResultCallback decode_result_callback_;
  ErrorCallback error_callback_;
  CompletedCallback completed_callback_;

  // =========================================================================
  // FFMPEG RESOURCES (owned by worker thread)
  // =========================================================================

  // Custom I/O context for reading from memory
  // Using unique_ptr with custom deleter since we allocate buffer and opaque
  struct CustomIODeleter {
    void operator()(AVIOContext* ctx) const noexcept;
  };
  using CustomAVIOContextPtr = std::unique_ptr<AVIOContext, CustomIODeleter>;
  CustomAVIOContextPtr io_ctx_;

  // Format context (demuxer)
  raii::AVFormatContextPtr fmt_ctx_;

  // Codec context (decoder)
  raii::AVCodecContextPtr codec_ctx_;

  // Image data buffer (copied from configure message)
  std::vector<uint8_t> image_data_;
  size_t read_position_ = 0;

  // Track information
  std::vector<ImageTrackInfo> tracks_;
  int32_t selected_stream_index_ = -1;

  // Frame tracking for animated images
  uint32_t current_frame_index_ = 0;
  uint32_t total_frame_count_ = 1;

  // Configuration
  std::string type_;
  std::optional<uint32_t> desired_width_;
  std::optional<uint32_t> desired_height_;

  // Streaming mode state
  bool is_streaming_ = false;       // true if data comes from ReadableStream
  bool stream_complete_ = false;    // true when stream has closed
  bool configured_ = false;         // true when codec is initialized (after enough data received)
  std::optional<bool> prefer_animation_;  // Stored for deferred configuration
  std::string color_space_conversion_;    // Stored for deferred configuration

  /**
   * Try to configure the decoder with accumulated data.
   * For streaming mode, called when enough data has accumulated.
   * Returns true if configuration succeeded.
   */
  bool TryConfigureFromBuffer();
};

}  // namespace webcodecs
