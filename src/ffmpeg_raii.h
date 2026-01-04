#pragma once
/**
 * ffmpeg_raii.h - RAII Wrappers for FFmpeg Resources
 *
 * Provides type-safe, leak-proof wrappers for all FFmpeg allocated types.
 * These wrappers guarantee cleanup on all code paths including exceptions.
 *
 * Usage:
 *   AVFramePtr frame = MakeAvFrame();
 *   if (!frame) { handle allocation failure }
 *   Use frame->data, frame->linesize, etc.
 *   Automatically freed when scope exits
 */

#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
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

struct SwrContextDeleter {
  void operator()(SwrContext* ctx) const noexcept {
    if (ctx) {
      swr_free(&ctx);
    }
  }
};

struct AVFilterGraphDeleter {
  void operator()(AVFilterGraph* graph) const noexcept {
    if (graph) {
      avfilter_graph_free(&graph);
    }
  }
};

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
using SwrContextPtr = std::unique_ptr<SwrContext, SwrContextDeleter>;
using AVFilterGraphPtr = std::unique_ptr<AVFilterGraph, AVFilterGraphDeleter>;
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
[[nodiscard]] inline AVFramePtr MakeAvFrame() { return AVFramePtr(av_frame_alloc()); }

/**
 * Creates a new AVPacket wrapped in RAII.
 * Returns nullptr on allocation failure.
 */
[[nodiscard]] inline AVPacketPtr MakeAvPacket() { return AVPacketPtr(av_packet_alloc()); }

/**
 * Creates a new AVCodecContext for the given codec.
 * Returns nullptr on allocation failure.
 */
[[nodiscard]] inline AVCodecContextPtr MakeAvCodecContext(const AVCodec* codec) {
  return AVCodecContextPtr(avcodec_alloc_context3(codec));
}

/**
 * Creates a deep copy (reference) of an AVFrame.
 * The new frame references the same underlying buffers (refcounted).
 * Returns nullptr on failure.
 */
[[nodiscard]] inline AVFramePtr CloneAvFrame(const AVFrame* src) {
  if (!src) return nullptr;

  AVFramePtr dst = MakeAvFrame();
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
[[nodiscard]] inline AVPacketPtr CloneAvPacket(const AVPacket* src) {
  if (!src) return nullptr;

  AVPacketPtr dst = MakeAvPacket();
  if (!dst) return nullptr;

  if (av_packet_ref(dst.get(), src) < 0) {
    return nullptr;
  }

  return dst;
}

/**
 * Creates a new SwrContext for audio resampling.
 * Returns nullptr on allocation failure.
 * Caller must call swr_init() after setting options via av_opt_set_*.
 */
[[nodiscard]] inline SwrContextPtr MakeSwrContext() { return SwrContextPtr(swr_alloc()); }

/**
 * Creates and initializes a SwrContext in one call.
 * Returns nullptr on failure.
 *
 * @param out_ch_layout Output channel layout
 * @param out_sample_fmt Output sample format
 * @param out_sample_rate Output sample rate
 * @param in_ch_layout Input channel layout
 * @param in_sample_fmt Input sample format
 * @param in_sample_rate Input sample rate
 */
[[nodiscard]] inline SwrContextPtr MakeSwrContextInitialized(const AVChannelLayout* out_ch_layout,
                                                             AVSampleFormat out_sample_fmt, int out_sample_rate,
                                                             const AVChannelLayout* in_ch_layout,
                                                             AVSampleFormat in_sample_fmt, int in_sample_rate) {
  SwrContext* ctx = nullptr;
  int ret = swr_alloc_set_opts2(&ctx, out_ch_layout, out_sample_fmt, out_sample_rate, in_ch_layout, in_sample_fmt,
                                in_sample_rate, 0, nullptr);
  if (ret < 0 || !ctx) return nullptr;

  if (swr_init(ctx) < 0) {
    swr_free(&ctx);
    return nullptr;
  }
  return SwrContextPtr(ctx);
}

/**
 * Creates a new AVFilterGraph for video/audio filtering.
 * Returns nullptr on allocation failure.
 */
[[nodiscard]] inline AVFilterGraphPtr MakeFilterGraph() { return AVFilterGraphPtr(avfilter_graph_alloc()); }

/**
 * Wraps an already-opened AVFormatContext in RAII.
 * Use after avformat_open_input() succeeds.
 *
 * NOTE: avformat_open_input() frees the context on failure, so only call
 * this wrapper after confirming success.
 */
[[nodiscard]] inline AVFormatContextPtr MakeAvFormatContext(AVFormatContext* ctx) {
  return AVFormatContextPtr(ctx);
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
template <typename TSFN>
struct SafeAsyncContext {
  // Thread synchronization
  mutable std::mutex mutex;
  std::condition_variable cv;
  std::atomic<bool> should_exit{false};

  // FFmpeg resources (RAII managed)
  AVCodecContextPtr codec_ctx;

  // Worker thread
  std::thread worker_thread;

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
    should_exit.store(true, std::memory_order_release);
    cv.notify_all();

    // 2. Join worker thread FIRST (before releasing any resources)
    if (worker_thread.joinable()) {
      worker_thread.join();
    }

    // 3. Release TSFN (after worker is done)
    if (tsfn) {
      tsfn.Release();
    }

    // 4. codec_ctx is freed automatically by RAII
  }

  /**
   * Thread-safe check if context should exit.
   */
  bool ShouldExit() const { return should_exit.load(std::memory_order_acquire); }

  /**
   * Lock the mutex for codec operations.
   * Use with std::lock_guard or std::unique_lock.
   */
  std::unique_lock<std::mutex> Lock() const { return std::unique_lock<std::mutex>(mutex); }
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
  enum class State : int { Unconfigured = 0, Configured = 1, Closed = 2 };

  AtomicCodecState() : state_(static_cast<int>(State::Unconfigured)) {}

  State get() const { return static_cast<State>(state_.load(std::memory_order_acquire)); }

  /**
   * Attempt to transition from expected state to new state.
   * Returns true if transition succeeded, false if current state didn't match.
   */
  bool transition(State expected, State desired) {
    int exp = static_cast<int>(expected);
    int des = static_cast<int>(desired);
    return state_.compare_exchange_strong(exp, des, std::memory_order_acq_rel, std::memory_order_acquire);
  }

  /**
   * Force transition to Closed state (always succeeds).
   */
  void Close() { state_.store(static_cast<int>(State::Closed), std::memory_order_release); }

  /**
   * Check if state is Configured (valid for decode/encode operations).
   */
  bool IsConfigured() const { return get() == State::Configured; }

  /**
   * Check if state is Closed (no operations allowed).
   */
  bool IsClosed() const { return get() == State::Closed; }

  /**
   * Get string representation of current state.
   */
  const char* ToString() const {
    switch (get()) {
      case State::Unconfigured:
        return "unconfigured";
      case State::Configured:
        return "configured";
      case State::Closed:
        return "closed";
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

  explicit AVMallocBuffer(size_t size) : data_(static_cast<uint8_t*>(av_malloc(size))), size_(size) {
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

  AVMallocBuffer(AVMallocBuffer&& other) noexcept : data_(other.data_), size_(other.size_) {
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
