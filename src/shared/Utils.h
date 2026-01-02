#pragma once

// Only include napi.h when not in pure C++ testing mode
#ifndef WEBCODECS_TESTING
#include <napi.h>
#endif

#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <vector>
#include <string>
#include <memory>
#include "../ffmpeg_raii.h"

// Note: FFmpeg headers are included via ffmpeg_raii.h

namespace webcodecs {

// --- ASYNC CONTEXT STRUCT ---
/**
 * AsyncDecodeContext - Thread-safe RAII container for async decode operations.
 *
 * Owns:
 * - TypedThreadSafeFunction for main thread callbacks
 * - AVCodecContext for FFmpeg decode state
 * - Worker thread for non-blocking decode
 *
 * Thread Safety:
 * - shouldExit is atomic for lock-free shutdown signaling
 * - codecMutex protects all codec operations
 * - Destructor ordering: join thread -> release TSFN -> free codec
 *
 * CRITICAL: The destructor order is essential:
 * 1. Signal worker to exit (atomic)
 * 2. Wake any waiting threads (condition variable)
 * 3. Join worker thread (wait for completion)
 * 4. Release TSFN (after worker is done using it)
 * 5. Free codec context (after worker is done using it)
 */
struct AsyncDecodeContext {
#ifndef WEBCODECS_TESTING
  using TSFN = Napi::TypedThreadSafeFunction<AsyncDecodeContext, AVFrame*>;
#else
  // Mock TSFN for testing
  struct MockTSFN {
    bool released = false;
    void Release() { released = true; }
    explicit operator bool() const { return !released; }
  };
  using TSFN = MockTSFN;
#endif

  // Thread synchronization
  mutable std::mutex codecMutex;
  std::condition_variable cv;
  std::atomic<bool> shouldExit{false};

  // TSFN for callbacks to JS main thread
  TSFN tsfn;

  // FFmpeg codec context (RAII managed, protected by codecMutex)
  raii::AVCodecContextPtr codecCtx;

  // Worker thread
  std::thread workerThread;

  AsyncDecodeContext() = default;

  // Non-copyable, non-movable (owns thread and mutex)
  AsyncDecodeContext(const AsyncDecodeContext&) = delete;
  AsyncDecodeContext& operator=(const AsyncDecodeContext&) = delete;
  AsyncDecodeContext(AsyncDecodeContext&&) = delete;
  AsyncDecodeContext& operator=(AsyncDecodeContext&&) = delete;

  ~AsyncDecodeContext() {
    // 1. Signal worker to exit (atomic, no lock needed)
    shouldExit.store(true, std::memory_order_release);

    // 2. Wake any threads waiting on the condition variable
    cv.notify_all();

    // 3. Join worker thread FIRST (before releasing any resources it uses)
    if (workerThread.joinable()) {
      workerThread.join();
    }

    // 4. Release TSFN (worker is done, safe to release)
    if (tsfn) {
      tsfn.Release();
    }

    // 5. codecCtx is freed automatically by RAII destructor
  }

  /**
   * Thread-safe check if context should exit.
   * Use this in worker loops.
   */
  bool should_exit() const {
    return shouldExit.load(std::memory_order_acquire);
  }

  /**
   * Lock the codec mutex for thread-safe operations.
   * Use with std::lock_guard or std::unique_lock.
   */
  std::unique_lock<std::mutex> lock() const {
    return std::unique_lock<std::mutex>(codecMutex);
  }
};

}  // namespace webcodecs

// --- ENGINEERING EXCELLENCE: Guardrails ---
// These macros and classes require NAPI and are not available in pure C++ tests

#ifndef WEBCODECS_TESTING

// Macro: Ensure condition is met or throw JS Error
#define ENSURE(env, condition, message) \
    if (!(condition)) { \
        Napi::Error::New(env, message).ThrowAsJavaScriptException(); \
        return env.Null(); \
    }

// Macro: Check argument count
#define CHECK_ARGS(env, info, count) \
    ENSURE(env, info.Length() >= count, "Invalid number of arguments")

// Macro: Assert state matches expected
#define ENSURE_STATE(env, actual, expected, message) \
    ENSURE(env, actual == expected, message)

// --- ASYNC WORKER TEMPLATE ---
// Use this for all heavy FFmpeg tasks to avoid blocking the Event Loop.
class BaseWorker : public Napi::AsyncWorker {
public:
    explicit BaseWorker(const Napi::Function& callback)
        : Napi::AsyncWorker(callback) {}

    // Subclasses must implement Execute()

    void OnOK() override {
        // Default: Call callback with (null, result)
        Callback().Call({Env().Null()});
    }

    void OnError(const Napi::Error& e) override {
        Callback().Call({e.Value()});
    }
};

#endif  // WEBCODECS_TESTING

// --- RAII HANDLE WRAPPER ---
// Template for automatic cleanup of native handles
template<typename T, void(*Deleter)(T**)>
class ScopedHandle {
public:
    ScopedHandle() : handle_(nullptr) {}
    explicit ScopedHandle(T* handle) : handle_(handle) {}

    ~ScopedHandle() {
        if (handle_) {
            Deleter(&handle_);
        }
    }

    // Move-only semantics
    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;

    ScopedHandle(ScopedHandle&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }

    ScopedHandle& operator=(ScopedHandle&& other) noexcept {
        if (this != &other) {
            if (handle_) Deleter(&handle_);
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    T* get() const { return handle_; }
    T* release() {
        T* tmp = handle_;
        handle_ = nullptr;
        return tmp;
    }
    void reset(T* handle = nullptr) {
        if (handle_) Deleter(&handle_);
        handle_ = handle;
    }

    explicit operator bool() const { return handle_ != nullptr; }
    T* operator->() const { return handle_; }
    T& operator*() const { return *handle_; }

private:
    T* handle_;
};

// --- TYPE CONVERSION HELPERS ---
// These require NAPI and are not available in pure C++ tests

#ifndef WEBCODECS_TESTING

// JS -> Native
inline std::string ToStdString(const Napi::Value& value) {
    if (!value.IsString()) return "";
    return value.As<Napi::String>().Utf8Value();
}

inline int64_t ToInt64(const Napi::Value& value) {
    if (!value.IsNumber()) return 0;
    return value.As<Napi::Number>().Int64Value();
}

inline double ToDouble(const Napi::Value& value) {
    if (!value.IsNumber()) return 0.0;
    return value.As<Napi::Number>().DoubleValue();
}

inline bool ToBool(const Napi::Value& value) {
    if (!value.IsBoolean()) return false;
    return value.As<Napi::Boolean>().Value();
}

// Native -> JS
inline Napi::String FromStdString(Napi::Env env, const std::string& str) {
    return Napi::String::New(env, str);
}

inline Napi::Number FromInt64(Napi::Env env, int64_t value) {
    return Napi::Number::New(env, static_cast<double>(value));
}

inline Napi::Number FromDouble(Napi::Env env, double value) {
    return Napi::Number::New(env, value);
}

inline Napi::Boolean FromBool(Napi::Env env, bool value) {
    return Napi::Boolean::New(env, value);
}

#endif  // WEBCODECS_TESTING

// --- COMMON TYPES ---

// WebCodecs state enum (shared across Decoder/Encoder)
enum class CodecState {
    Unconfigured,
    Configured,
    Closed
};

inline const char* CodecStateToString(CodecState state) {
    switch (state) {
        case CodecState::Unconfigured: return "unconfigured";
        case CodecState::Configured: return "configured";
        case CodecState::Closed: return "closed";
    }
    return "unknown";
}
