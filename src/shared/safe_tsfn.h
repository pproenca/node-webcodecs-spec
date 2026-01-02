#pragma once
/**
 * safe_tsfn.h - Thread-Safe Function Lifecycle Wrapper
 *
 * Provides a safe wrapper around Napi::TypedThreadSafeFunction that prevents:
 * - Calling after Release() (undefined behavior)
 * - Double Release() calls
 * - Race conditions between call and release
 *
 * Thread Safety:
 * - call() can be called from any thread
 * - release() can be called from any thread
 * - All operations are mutex-protected
 *
 * Usage:
 *   SafeThreadSafeFunction<Context, DataType> tsfn;
 *   tsfn.init(napi_tsfn);
 *
 *   // From worker thread:
 *   if (!tsfn.call(data)) {
 *     // TSFN was released, clean up data yourself
 *     delete data;
 *   }
 *
 *   // From any thread:
 *   tsfn.release();  // Idempotent
 */

#include <napi.h>

#include <atomic>
#include <functional>
#include <mutex>
#include <utility>

namespace webcodecs {

/**
 * Thread-safe wrapper for Napi::TypedThreadSafeFunction.
 *
 * Guarantees:
 * - No calls after Release()
 * - No double Release()
 * - Thread-safe call from any thread
 *
 * @tparam Context The context type passed to the TSFN callback
 * @tparam DataType The data type passed through the TSFN
 */
template <typename Context, typename DataType>
class SafeThreadSafeFunction {
 public:
  using TSFN = Napi::TypedThreadSafeFunction<Context, DataType>;

  SafeThreadSafeFunction() = default;

  ~SafeThreadSafeFunction() { release(); }

  // Non-copyable, non-movable (wraps non-copyable TSFN)
  SafeThreadSafeFunction(const SafeThreadSafeFunction&) = delete;
  SafeThreadSafeFunction& operator=(const SafeThreadSafeFunction&) = delete;
  SafeThreadSafeFunction(SafeThreadSafeFunction&&) = delete;
  SafeThreadSafeFunction& operator=(SafeThreadSafeFunction&&) = delete;

  /**
   * Initialize with a TSFN.
   * Must be called before any other operations.
   *
   * @param tsfn The TypedThreadSafeFunction to wrap
   */
  void init(TSFN tsfn) {
    std::lock_guard<std::mutex> lock(mutex_);
    tsfn_ = std::move(tsfn);
    released_ = false;
    initialized_ = true;
  }

  /**
   * Thread-safe call to the TSFN.
   *
   * @param data Pointer to data to pass to the callback.
   *             If this returns false, caller is responsible for cleanup.
   * @param mode Blocking or non-blocking call (default: non-blocking)
   * @return true if call succeeded, false if TSFN was released or not initialized
   */
  [[nodiscard]] bool call(DataType* data, napi_threadsafe_function_call_mode mode = napi_tsfn_nonblocking) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_ || released_) {
      // Caller must clean up data
      return false;
    }

    napi_status status = tsfn_.NonBlockingCall(data);
    return status == napi_ok;
  }

  /**
   * Thread-safe blocking call to the TSFN.
   * Blocks until the call is processed.
   *
   * @param data Pointer to data to pass to the callback
   * @return true if call succeeded, false if TSFN was released or not initialized
   */
  [[nodiscard]] bool blocking_call(DataType* data) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_ || released_) {
      return false;
    }

    napi_status status = tsfn_.BlockingCall(data);
    return status == napi_ok;
  }

  /**
   * Release the TSFN.
   * Idempotent - safe to call multiple times.
   * After this call, all future call() operations will return false.
   */
  void release() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_ && !released_) {
      tsfn_.Release();
      released_ = true;
    }
  }

  /**
   * Check if the TSFN has been released.
   */
  [[nodiscard]] bool is_released() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return released_;
  }

  /**
   * Check if the TSFN is initialized and not released.
   */
  [[nodiscard]] bool is_active() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return initialized_ && !released_;
  }

  /**
   * Acquire a reference to prevent GC collection.
   * Must be balanced with a later release or unref.
   */
  void acquire() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_ && !released_) {
      tsfn_.Acquire();
    }
  }

  /**
   * Release a reference without fully releasing the TSFN.
   */
  void unref() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_ && !released_) {
      tsfn_.Unref();
    }
  }

 private:
  mutable std::mutex mutex_;
  TSFN tsfn_;
  bool initialized_{false};
  bool released_{true};  // Start as released (not initialized)
};

/**
 * Scoped TSFN reference acquisition.
 * Acquires a reference on construction, releases on destruction.
 * Useful for ensuring TSFN stays alive during async operations.
 */
template <typename Context, typename DataType>
class ScopedTSFNRef {
 public:
  explicit ScopedTSFNRef(SafeThreadSafeFunction<Context, DataType>& tsfn) : tsfn_(tsfn) { tsfn_.acquire(); }

  ~ScopedTSFNRef() { tsfn_.unref(); }

  // Non-copyable, non-movable
  ScopedTSFNRef(const ScopedTSFNRef&) = delete;
  ScopedTSFNRef& operator=(const ScopedTSFNRef&) = delete;

 private:
  SafeThreadSafeFunction<Context, DataType>& tsfn_;
};

/**
 * Helper to create a TSFN with proper error handling.
 *
 * @param env N-API environment
 * @param name Name for debugging
 * @param callback The JS function to call
 * @param context Context pointer passed to callbacks
 * @param callJs The C++ callback function
 * @return Initialized SafeThreadSafeFunction, or throws on error
 */
template <typename Context, typename DataType, typename CallbackType>
SafeThreadSafeFunction<Context, DataType> make_safe_tsfn(Napi::Env env, const char* name, Napi::Function callback,
                                                         Context* context, CallbackType callJs) {
  SafeThreadSafeFunction<Context, DataType> safe_tsfn;

  auto tsfn = Napi::TypedThreadSafeFunction<Context, DataType>::New(
      env, callback, name,
      0,                                           // Unlimited queue
      1,                                           // Initial thread count
      context, [](Napi::Env, void*, Context*) {},  // Destructor callback
      callJs);

  safe_tsfn.init(std::move(tsfn));
  return safe_tsfn;
}

}  // namespace webcodecs
