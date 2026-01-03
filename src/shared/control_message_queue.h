#pragma once
/**
 * control_message_queue.h - Thread-Safe Control Message Queue
 *
 * Implements the WebCodecs spec "control message queue" abstraction for
 * VideoDecoder, AudioDecoder, VideoEncoder, and AudioEncoder.
 *
 * Per spec, messages are processed FIFO with specific semantics:
 * - Configure blocks until complete
 * - Decode/Encode are queued and processed by worker
 * - Flush completes when all pending work is done
 * - Reset clears pending work
 * - Close terminates the queue
 *
 * Thread model:
 * - JS thread: enqueue() messages
 * - Worker thread: dequeue() and process
 * - TSFN callbacks deliver results to JS thread
 *
 * @see https://www.w3.org/TR/webcodecs/#control-message-queue
 */

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <utility>
#include <variant>
#include <vector>

#include "../ffmpeg_raii.h"

namespace webcodecs {

/**
 * Thread-safe control message queue per WebCodecs spec.
 *
 * @tparam PacketType Type for encoded data (e.g., raii::AVPacketPtr)
 * @tparam FrameType Type for decoded data (e.g., raii::AVFramePtr)
 */
template <typename PacketType, typename FrameType>
class ControlMessageQueue {
 public:
  // =========================================================================
  // MESSAGE TYPES
  // =========================================================================

  /**
   * Configure message - blocks processing until codec is configured.
   * The configure_fn returns true on success, false on failure.
   */
  struct ConfigureMessage {
    std::function<bool()> configure_fn;
  };

  /**
   * Decode message - queued work item for decoders.
   * Contains encoded packet to decode.
   */
  struct DecodeMessage {
    PacketType packet;
  };

  /**
   * Encode message - queued work item for encoders.
   * Contains frame to encode with optional keyframe flag.
   */
  struct EncodeMessage {
    FrameType frame;
    bool key_frame = false;
  };

  /**
   * Flush message - complete all pending work before resolving.
   * promise_id is used to resolve the Promise on the JS side.
   */
  struct FlushMessage {
    uint32_t promise_id;
  };

  /**
   * Reset message - clear pending work and reset codec state.
   */
  struct ResetMessage {};

  /**
   * Close message - terminate the queue permanently.
   */
  struct CloseMessage {};

  using Message = std::variant<ConfigureMessage, DecodeMessage, EncodeMessage, FlushMessage, ResetMessage, CloseMessage>;

  // =========================================================================
  // CONSTRUCTORS / DESTRUCTOR
  // =========================================================================

  ControlMessageQueue() = default;

  ~ControlMessageQueue() { Shutdown(); }

  // Non-copyable, non-movable (owns synchronization primitives)
  ControlMessageQueue(const ControlMessageQueue&) = delete;
  ControlMessageQueue& operator=(const ControlMessageQueue&) = delete;
  ControlMessageQueue(ControlMessageQueue&&) = delete;
  ControlMessageQueue& operator=(ControlMessageQueue&&) = delete;

  // =========================================================================
  // PRODUCER API (JS Thread)
  // =========================================================================

  /**
   * Enqueue a message for processing.
   * Thread-safe, called from JS main thread.
   *
   * @param msg The message to enqueue
   * @return true if message was enqueued, false if queue is closed
   */
  [[nodiscard]] bool Enqueue(Message msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (closed_) {
      return false;
    }
    queue_.push(std::move(msg));
    cv_.notify_one();
    return true;
  }

  // =========================================================================
  // CONSUMER API (Worker Thread)
  // =========================================================================

  /**
   * Dequeue a message for processing.
   * Blocks until a message is available or queue is closed.
   * Thread-safe, called from worker thread.
   *
   * @return The next message, or std::nullopt if queue is closed and empty
   */
  [[nodiscard]] std::optional<Message> Dequeue() {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] { return !queue_.empty() || closed_; });

    if (closed_ && queue_.empty()) {
      return std::nullopt;
    }

    Message msg = std::move(queue_.front());
    queue_.pop();
    return msg;
  }

  /**
   * Dequeue with timeout.
   * Useful for checking shutdown conditions periodically.
   *
   * @param timeout Maximum time to wait
   * @return The next message, or std::nullopt on timeout or if closed
   */
  [[nodiscard]] std::optional<Message> DequeueFor(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!cv_.wait_for(lock, timeout, [this] { return !queue_.empty() || closed_; })) {
      return std::nullopt;  // Timeout
    }

    if (closed_ && queue_.empty()) {
      return std::nullopt;
    }

    Message msg = std::move(queue_.front());
    queue_.pop();
    return msg;
  }

  /**
   * Try to dequeue without blocking.
   *
   * @return The next message, or std::nullopt if queue is empty
   */
  [[nodiscard]] std::optional<Message> TryDequeue() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) {
      return std::nullopt;
    }

    Message msg = std::move(queue_.front());
    queue_.pop();
    return msg;
  }

  // =========================================================================
  // RESET / SHUTDOWN
  // =========================================================================

  /**
   * Clear all pending messages (for reset).
   * Returns any packets that were dropped so they can be properly unreferenced.
   *
   * @return Vector of packets that were dropped
   */
  std::vector<PacketType> Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<PacketType> dropped;

    while (!queue_.empty()) {
      auto& msg = queue_.front();
      if (auto* decode = std::get_if<DecodeMessage>(&msg)) {
        dropped.push_back(std::move(decode->packet));
      }
      queue_.pop();
    }

    return dropped;
  }

  /**
   * Clear all pending encode messages (for encoder reset).
   * Returns any frames that were dropped so they can be properly unreferenced.
   *
   * @return Vector of frames that were dropped
   */
  std::vector<FrameType> ClearFrames() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<FrameType> dropped;

    while (!queue_.empty()) {
      auto& msg = queue_.front();
      if (auto* encode = std::get_if<EncodeMessage>(&msg)) {
        dropped.push_back(std::move(encode->frame));
      }
      queue_.pop();
    }

    return dropped;
  }

  /**
   * Shutdown the queue permanently.
   * Any subsequent Enqueue() calls will return false.
   * Any blocked Dequeue() calls will return std::nullopt.
   */
  void Shutdown() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      closed_ = true;
    }
    cv_.notify_all();
  }

  // =========================================================================
  // QUERY
  // =========================================================================

  /**
   * Get the current queue size (for decodeQueueSize/encodeQueueSize attribute).
   * Thread-safe.
   */
  [[nodiscard]] size_t size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
  }

  /**
   * Check if the queue is empty.
   */
  [[nodiscard]] bool empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
  }

  /**
   * Check if the queue is closed.
   */
  [[nodiscard]] bool IsClosed() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return closed_;
  }

  /**
   * Check if queue is blocked (for configure).
   */
  [[nodiscard]] bool IsBlocked() const { return blocked_.load(std::memory_order_acquire); }

  /**
   * Set blocked state (during configure).
   */
  void SetBlocked(bool blocked) { blocked_.store(blocked, std::memory_order_release); }

 private:
  mutable std::mutex mutex_;
  std::condition_variable cv_;
  std::queue<Message> queue_;
  std::atomic<bool> blocked_{false};
  bool closed_{false};
};

// ===========================================================================
// TYPE ALIASES
// ===========================================================================

/**
 * Control queue for video codecs.
 * PacketType: encoded video data
 * FrameType: decoded video frames
 */
using VideoControlQueue = ControlMessageQueue<raii::AVPacketPtr, raii::AVFramePtr>;

/**
 * Control queue for audio codecs.
 * PacketType: encoded audio data
 * FrameType: decoded audio frames
 */
using AudioControlQueue = ControlMessageQueue<raii::AVPacketPtr, raii::AVFramePtr>;

// ===========================================================================
// IMAGE DECODER MESSAGE TYPES
// ===========================================================================

/**
 * ImageDecoder has a different architecture than Video/AudioDecoder:
 * - Image data is provided in constructor (BufferSource), not per-decode
 * - decode() specifies which frame to decode by index
 * - No flush() method (images are decoded on-demand)
 * - Supports multi-track images (animated GIF, WebP)
 */

/**
 * Configure message for ImageDecoder.
 * Contains the image data and configuration options.
 *
 * For BufferSource input: data contains the full encoded data, is_streaming = false
 * For ReadableStream input: data is empty, is_streaming = true
 *   Data chunks will arrive via ImageStreamDataMessage.
 */
struct ImageConfigureMessage {
  std::string type;  // MIME type (e.g., "image/jpeg")
  std::vector<uint8_t> data;  // Encoded image data (empty if streaming)
  bool is_streaming = false;  // true if data comes from ReadableStream
  std::string color_space_conversion;
  std::optional<uint32_t> desired_width;
  std::optional<uint32_t> desired_height;
  std::optional<bool> prefer_animation;
};

/**
 * Decode message for ImageDecoder.
 * Specifies which frame to decode.
 */
struct ImageDecodeMessage {
  uint32_t frame_index;
  bool complete_frames_only;
  uint32_t promise_id;  // For resolving the decode promise
};

/**
 * Reset message for ImageDecoder.
 * Aborts pending decodes with AbortError.
 */
struct ImageResetMessage {};

/**
 * Close message for ImageDecoder.
 * Permanently closes the decoder and releases resources.
 */
struct ImageCloseMessage {};

/**
 * Update selected track message for ImageDecoder.
 * Changes which track is selected for decoding.
 */
struct ImageUpdateTrackMessage {
  int32_t selected_index;
};

/**
 * Stream data message for ImageDecoder.
 * Contains a chunk of data received from a ReadableStream.
 * Per W3C spec "Fetch Stream Data Loop" algorithm.
 */
struct ImageStreamDataMessage {
  std::vector<uint8_t> chunk;  // Chunk of encoded image data
};

/**
 * Stream end message for ImageDecoder.
 * Signals that the ReadableStream has closed (no more data).
 * Per W3C spec, this triggers [[complete]] = true and resolves [[completed promise]].
 */
struct ImageStreamEndMessage {};

/**
 * Stream error message for ImageDecoder.
 * Signals that an error occurred reading the ReadableStream.
 * Per W3C spec, this triggers Close ImageDecoder with NotReadableError.
 */
struct ImageStreamErrorMessage {
  std::string message;
};

/**
 * Variant type for all ImageDecoder messages.
 */
using ImageMessage = std::variant<ImageConfigureMessage, ImageDecodeMessage, ImageResetMessage,
                                   ImageCloseMessage, ImageUpdateTrackMessage,
                                   ImageStreamDataMessage, ImageStreamEndMessage, ImageStreamErrorMessage>;

/**
 * Thread-safe control message queue for ImageDecoder.
 * Similar to ControlMessageQueue but with image-specific message types.
 */
class ImageControlQueue {
 public:
  using Message = ImageMessage;

  ImageControlQueue() = default;
  ~ImageControlQueue() { Shutdown(); }

  // Non-copyable, non-movable
  ImageControlQueue(const ImageControlQueue&) = delete;
  ImageControlQueue& operator=(const ImageControlQueue&) = delete;
  ImageControlQueue(ImageControlQueue&&) = delete;
  ImageControlQueue& operator=(ImageControlQueue&&) = delete;

  /**
   * Enqueue a message for processing.
   * Thread-safe, called from JS main thread.
   */
  [[nodiscard]] bool Enqueue(Message msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (closed_) {
      return false;
    }
    queue_.push(std::move(msg));
    cv_.notify_one();
    return true;
  }

  /**
   * Dequeue a message with timeout.
   * Thread-safe, called from worker thread.
   */
  [[nodiscard]] std::optional<Message> DequeueFor(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!cv_.wait_for(lock, timeout, [this] { return !queue_.empty() || closed_; })) {
      return std::nullopt;
    }

    if (closed_ && queue_.empty()) {
      return std::nullopt;
    }

    Message msg = std::move(queue_.front());
    queue_.pop();
    return msg;
  }

  /**
   * Clear all pending decode messages.
   * Returns promise IDs that need to be rejected.
   */
  std::vector<uint32_t> ClearDecodes() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<uint32_t> dropped_promises;

    std::queue<Message> remaining;
    while (!queue_.empty()) {
      auto& msg = queue_.front();
      if (auto* decode = std::get_if<ImageDecodeMessage>(&msg)) {
        dropped_promises.push_back(decode->promise_id);
      } else {
        // Keep non-decode messages (like close)
        remaining.push(std::move(queue_.front()));
      }
      queue_.pop();
    }
    queue_ = std::move(remaining);

    return dropped_promises;
  }

  /**
   * Shutdown the queue permanently.
   */
  void Shutdown() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      closed_ = true;
    }
    cv_.notify_all();
  }

  /**
   * Get the current queue size.
   */
  [[nodiscard]] size_t size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
  }

  /**
   * Check if the queue is closed.
   */
  [[nodiscard]] bool IsClosed() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return closed_;
  }

 private:
  mutable std::mutex mutex_;
  std::condition_variable cv_;
  std::queue<Message> queue_;
  bool closed_{false};
};

// ===========================================================================
// VISITOR HELPER
// ===========================================================================

/**
 * Helper for processing messages with std::visit.
 *
 * Usage:
 *   auto result = std::visit(MessageVisitor{
 *     [](ConfigureMessage& msg) { ... },
 *     [](DecodeMessage& msg) { ... },
 *     [](FlushMessage& msg) { ... },
 *     [](ResetMessage& msg) { ... },
 *     [](CloseMessage& msg) { ... }
 *   }, message);
 */
template <class... Ts>
struct MessageVisitor : Ts... {
  using Ts::operator()...;
};

// Deduction guide for C++17
template <class... Ts>
MessageVisitor(Ts...) -> MessageVisitor<Ts...>;

}  // namespace webcodecs
