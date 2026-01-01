#pragma once
/**
 * ffmpeg_raii.h - RAII Wrappers for FFmpeg Resources
 *
 * Provides type-safe, leak-proof wrappers for all FFmpeg allocated types.
 * These wrappers guarantee cleanup on all code paths including exceptions.
 *
 * Usage:
 *   AVFramePtr frame = make_av_frame();
 *   if (!frame) { handle allocation failure }
 *   Use frame->data, frame->linesize, etc.
 *   Automatically freed when scope exits
 */

#include <memory>
#include <mutex>
#include <atomic>
#include <condition_variable>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace webcodecs {
namespace raii {

// =============================================================================
// DELETERS - Custom deleters for std::unique_ptr
// =============================================================================

struct AVFrameDeleter {
  void operator()(AVFrame* frame) const noexcept {
    if (frame) {
      av_frame_free(&frame);
    }
  }
};

struct AVPacketDeleter {
  void operator()(AVPacket* packet) const noexcept {
    if (packet) {
      av_packet_free(&packet);
    }
  }
};

struct AVCodecContextDeleter {
  void operator()(AVCodecContext* ctx) const noexcept {
    if (ctx) {
      avcodec_free_context(&ctx);
    }
  }
};

struct AVFormatContextDeleter {
  void operator()(AVFormatContext* ctx) const noexcept {
    if (ctx) {
      avformat_close_input(&ctx);
    }
  }
};

struct AVFormatContextOutputDeleter {
  void operator()(AVFormatContext* ctx) const noexcept {
    if (ctx) {
      if (ctx->pb) {
        avio_closep(&ctx->pb);
      }
      avformat_free_context(ctx);
    }
  }
};

struct SwsContextDeleter {
  void operator()(SwsContext* ctx) const noexcept {
    if (ctx) {
      sws_freeContext(ctx);
    }
  }
};

// Note: SwrContext (audio resampling) wrappers can be added when needed
// by including <libswresample/swresample.h>

struct AVBufferRefDeleter {
  void operator()(AVBufferRef* buf) const noexcept {
    if (buf) {
      av_buffer_unref(&buf);
    }
  }
};

struct AVIOContextDeleter {
  void operator()(AVIOContext* ctx) const noexcept {
    if (ctx) {
      avio_closep(&ctx);
    }
  }
};

struct AVDictionaryDeleter {
  void operator()(AVDictionary* dict) const noexcept {
    if (dict) {
      av_dict_free(&dict);
    }
  }
};

// =============================================================================
// TYPE ALIASES - Smart pointer types for FFmpeg resources
// =============================================================================

using AVFramePtr = std::unique_ptr<AVFrame, AVFrameDeleter>;
using AVPacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;
using AVCodecContextPtr = std::unique_ptr<AVCodecContext, AVCodecContextDeleter>;
using AVFormatContextPtr = std::unique_ptr<AVFormatContext, AVFormatContextDeleter>;
using AVFormatContextOutputPtr = std::unique_ptr<AVFormatContext, AVFormatContextOutputDeleter>;
using SwsContextPtr = std::unique_ptr<SwsContext, SwsContextDeleter>;
using AVBufferRefPtr = std::unique_ptr<AVBufferRef, AVBufferRefDeleter>;
using AVIOContextPtr = std::unique_ptr<AVIOContext, AVIOContextDeleter>;
using AVDictionaryPtr = std::unique_ptr<AVDictionary, AVDictionaryDeleter>;

// =============================================================================
// FACTORY FUNCTIONS - Safe allocation with automatic cleanup
// =============================================================================

/**
 * Creates a new AVFrame wrapped in RAII.
 * Returns nullptr on allocation failure.
 */
inline AVFramePtr make_av_frame() {
  return AVFramePtr(av_frame_alloc());
}

/**
 * Creates a new AVPacket wrapped in RAII.
 * Returns nullptr on allocation failure.
 */
inline AVPacketPtr make_av_packet() {
  return AVPacketPtr(av_packet_alloc());
}

/**
 * Creates a new AVCodecContext for the given codec.
 * Returns nullptr on allocation failure.
 */
inline AVCodecContextPtr make_av_codec_context(const AVCodec* codec) {
  return AVCodecContextPtr(avcodec_alloc_context3(codec));
}

/**
 * Creates a deep copy (reference) of an AVFrame.
 * The new frame references the same underlying buffers (refcounted).
 * Returns nullptr on failure.
 */
inline AVFramePtr clone_av_frame(const AVFrame* src) {
  if (!src) return nullptr;

  AVFramePtr dst = make_av_frame();
  if (!dst) return nullptr;

  if (av_frame_ref(dst.get(), src) < 0) {
    return nullptr;
  }

  return dst;
}

/**
 * Creates a deep copy of an AVPacket.
 * The new packet references the same underlying data (refcounted).
 * Returns nullptr on failure.
 */
inline AVPacketPtr clone_av_packet(const AVPacket* src) {
  if (!src) return nullptr;

  AVPacketPtr dst = make_av_packet();
  if (!dst) return nullptr;

  if (av_packet_ref(dst.get(), src) < 0) {
    return nullptr;
  }

  return dst;
}

// =============================================================================
// THREAD-SAFE ASYNC DECODE CONTEXT
// =============================================================================

/**
 * Thread-safe context for async decode operations.
 *
 * Provides proper synchronization between:
 * - JavaScript main thread (configure, decode, close)
 * - Worker thread (FFmpeg decode loop)
 *
 * Destructor ordering is correct:
 * 1. Signal worker to exit
 * 2. Join worker thread
 * 3. Release TSFN
 * 4. Free codec context (via RAII)
 */
template<typename TSFN>
struct SafeAsyncContext {
  // Thread synchronization
  mutable std::mutex mutex;
  std::condition_variable cv;
  std::atomic<bool> shouldExit{false};

  // FFmpeg resources (RAII managed)
  AVCodecContextPtr codecCtx;

  // Worker thread
  std::thread workerThread;

  // Thread-safe function for JS callbacks
  TSFN tsfn;

  SafeAsyncContext() = default;

  // Non-copyable, non-movable (owns thread)
  SafeAsyncContext(const SafeAsyncContext&) = delete;
  SafeAsyncContext& operator=(const SafeAsyncContext&) = delete;
  SafeAsyncContext(SafeAsyncContext&&) = delete;
  SafeAsyncContext& operator=(SafeAsyncContext&&) = delete;

  ~SafeAsyncContext() {
    // 1. Signal worker to exit
    shouldExit.store(true, std::memory_order_release);
    cv.notify_all();

    // 2. Join worker thread FIRST (before releasing any resources)
    if (workerThread.joinable()) {
      workerThread.join();
    }

    // 3. Release TSFN (after worker is done)
    if (tsfn) {
      tsfn.Release();
    }

    // 4. codecCtx is freed automatically by RAII
  }

  /**
   * Thread-safe check if context should exit.
   */
  bool should_exit() const {
    return shouldExit.load(std::memory_order_acquire);
  }

  /**
   * Lock the mutex for codec operations.
   * Use with std::lock_guard or std::unique_lock.
   */
  std::unique_lock<std::mutex> lock() const {
    return std::unique_lock<std::mutex>(mutex);
  }
};

// =============================================================================
// CODEC STATE MACHINE (Thread-Safe)
// =============================================================================

/**
 * Atomic codec state with safe transitions.
 * Prevents invalid state changes from concurrent calls.
 */
class AtomicCodecState {
 public:
  enum class State : int {
    Unconfigured = 0,
    Configured = 1,
    Closed = 2
  };

  AtomicCodecState() : state_(static_cast<int>(State::Unconfigured)) {}

  State get() const {
    return static_cast<State>(state_.load(std::memory_order_acquire));
  }

  /**
   * Attempt to transition from expected state to new state.
   * Returns true if transition succeeded, false if current state didn't match.
   */
  bool transition(State expected, State desired) {
    int exp = static_cast<int>(expected);
    int des = static_cast<int>(desired);
    return state_.compare_exchange_strong(exp, des,
                                          std::memory_order_acq_rel,
                                          std::memory_order_acquire);
  }

  /**
   * Force transition to Closed state (always succeeds).
   */
  void close() {
    state_.store(static_cast<int>(State::Closed), std::memory_order_release);
  }

  /**
   * Check if state is Configured (valid for decode/encode operations).
   */
  bool is_configured() const {
    return get() == State::Configured;
  }

  /**
   * Check if state is Closed (no operations allowed).
   */
  bool is_closed() const {
    return get() == State::Closed;
  }

  const char* to_string() const {
    switch (get()) {
      case State::Unconfigured: return "unconfigured";
      case State::Configured: return "configured";
      case State::Closed: return "closed";
    }
    return "unknown";
  }

 private:
  std::atomic<int> state_;
};

// =============================================================================
// BUFFER UTILITIES
// =============================================================================

/**
 * RAII wrapper for av_malloc'd buffers.
 */
class AVMallocBuffer {
 public:
  AVMallocBuffer() : data_(nullptr), size_(0) {}

  explicit AVMallocBuffer(size_t size)
      : data_(static_cast<uint8_t*>(av_malloc(size))), size_(size) {
    if (!data_) size_ = 0;
  }

  ~AVMallocBuffer() {
    if (data_) {
      av_free(data_);
    }
  }

  // Move-only
  AVMallocBuffer(const AVMallocBuffer&) = delete;
  AVMallocBuffer& operator=(const AVMallocBuffer&) = delete;

  AVMallocBuffer(AVMallocBuffer&& other) noexcept
      : data_(other.data_), size_(other.size_) {
    other.data_ = nullptr;
    other.size_ = 0;
  }

  AVMallocBuffer& operator=(AVMallocBuffer&& other) noexcept {
    if (this != &other) {
      if (data_) av_free(data_);
      data_ = other.data_;
      size_ = other.size_;
      other.data_ = nullptr;
      other.size_ = 0;
    }
    return *this;
  }

  uint8_t* data() const { return data_; }
  size_t size() const { return size_; }
  explicit operator bool() const { return data_ != nullptr; }

  /**
   * Release ownership and return raw pointer.
   */
  uint8_t* release() {
    uint8_t* tmp = data_;
    data_ = nullptr;
    size_ = 0;
    return tmp;
  }

 private:
  uint8_t* data_;
  size_t size_;
};

}  // namespace raii
}  // namespace webcodecs
