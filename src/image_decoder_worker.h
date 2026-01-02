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
// INCREMENTALLY ADDING MEMBERS TO FIND CRASH CAUSE
class ImageDecoderWorker {
 public:
  // Original callback types
  using TrackInfoCallback = std::function<void(const std::vector<ImageTrackInfo>&, int32_t)>;
  using DecodeResultCallback = std::function<void(uint32_t, ImageDecodeResult)>;
  using ErrorCallback = std::function<void(uint32_t, int, const std::string&)>;
  using CompletedCallback = std::function<void()>;

  ImageDecoderWorker() = default;
  ~ImageDecoderWorker() = default;

  // Stubs
  bool Start() { return true; }
  void Stop() {}
  bool IsRunning() const { return false; }
  bool ShouldExit() const { return true; }

  // Callback setters - now with proper types
  void SetTrackInfoCallback(TrackInfoCallback cb) { track_info_callback_ = std::move(cb); }
  void SetDecodeResultCallback(DecodeResultCallback cb) { decode_result_callback_ = std::move(cb); }
  void SetErrorCallback(ErrorCallback cb) { error_callback_ = std::move(cb); }
  void SetCompletedCallback(CompletedCallback cb) { completed_callback_ = std::move(cb); }

 private:
  // Callbacks - this is what we're testing
  TrackInfoCallback track_info_callback_;
  DecodeResultCallback decode_result_callback_;
  ErrorCallback error_callback_;
  CompletedCallback completed_callback_;
};

}  // namespace webcodecs
