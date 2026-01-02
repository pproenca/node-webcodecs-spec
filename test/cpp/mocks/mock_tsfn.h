#pragma once
/**
 * mock_tsfn.h - Mock ThreadSafeFunction for unit testing
 *
 * Simulates N-API ThreadSafeFunction behavior without requiring Node.js runtime.
 * Enables testing SafeThreadSafeFunction wrapper in isolation.
 */

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <vector>

// Define napi_status for testing (N-API not available in unit tests)
enum napi_status { napi_ok = 0, napi_invalid_arg = 1, napi_closing = 4, napi_queue_full = 8, napi_generic_failure = 9 };

namespace webcodecs::testing {

/**
 * Mock ThreadSafeFunction that tracks calls and lifecycle.
 *
 * Unlike real TSFN, this doesn't invoke JavaScript callbacks.
 * Instead, it captures calls for verification in tests.
 */
template <typename DataType>
class MockThreadSafeFunction {
 public:
  enum class Status { kOk = 0, kClosing, kInvalidArg, kQueueFull, kUnknown };

  MockThreadSafeFunction() = default;

  // Simulate TSFN initialization
  void Initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    initialized_ = true;
    released_ = false;
    call_count_ = 0;
    pending_calls_.clear();
  }

  // Simulate NonBlockingCall - returns status like real TSFN
  Status NonBlockingCall(DataType* data) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
      return Status::kInvalidArg;
    }

    if (released_) {
      return Status::kClosing;
    }

    call_count_++;
    if (data) {
      pending_calls_.push_back(data);
    }

    return Status::kOk;
  }

  // Simulate BlockingCall
  Status BlockingCall(DataType* data) {
    // For testing purposes, behaves same as NonBlockingCall
    return NonBlockingCall(data);
  }

  // Simulate Release
  void Release() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_ && !released_) {
      released_ = true;
      release_count_++;
    }
  }

  // Simulate Abort
  void Abort() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) {
      released_ = true;
      aborted_ = true;
    }
  }

  // Test inspection methods
  [[nodiscard]] bool is_initialized() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return initialized_;
  }

  [[nodiscard]] bool is_released() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return released_;
  }

  [[nodiscard]] bool is_aborted() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return aborted_;
  }

  [[nodiscard]] size_t call_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return call_count_;
  }

  [[nodiscard]] size_t release_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return release_count_;
  }

  [[nodiscard]] std::vector<DataType*> pending_calls() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pending_calls_;
  }

  // Clear recorded state for test isolation
  void Reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    initialized_ = false;
    released_ = false;
    aborted_ = false;
    call_count_ = 0;
    release_count_ = 0;
    pending_calls_.clear();
  }

  // Inject failure for testing error paths
  void SetNextCallStatus(Status status) {
    std::lock_guard<std::mutex> lock(mutex_);
    injected_status_ = status;
    inject_failure_ = true;
  }

 private:
  mutable std::mutex mutex_;
  bool initialized_{false};
  bool released_{false};
  bool aborted_{false};
  size_t call_count_{0};
  size_t release_count_{0};
  std::vector<DataType*> pending_calls_;

  // Failure injection
  bool inject_failure_{false};
  Status injected_status_{Status::kOk};
};

/**
 * Mock context for SafeThreadSafeFunction tests.
 */
struct MockContext {
  std::atomic<int> callback_count{0};
  std::atomic<bool> finalized{false};

  void OnCallback() { callback_count.fetch_add(1, std::memory_order_relaxed); }

  void OnFinalize() { finalized.store(true, std::memory_order_release); }
};

/**
 * Shared state for MockTypedThreadSafeFunction.
 * Allows the mock to be movable by sharing state via shared_ptr.
 */
template <typename Context, typename DataType>
struct MockTSFNState {
  std::mutex mutex;
  Context* context{nullptr};
  std::function<void(Context*, DataType*)> callback;
  std::queue<DataType*> queue;
  size_t max_queue_size{0};
  size_t call_count{0};
  bool initialized{false};
  bool released{false};
  bool aborted{false};
};

/**
 * Simulates the TSFN wrapper interface for testing.
 * Matches the interface expected by SafeThreadSafeFunction.
 */
template <typename Context, typename DataType>
class MockTypedThreadSafeFunction {
 public:
  using CallbackType = std::function<void(Context*, DataType*)>;
  using StatePtr = std::shared_ptr<MockTSFNState<Context, DataType>>;

  MockTypedThreadSafeFunction() : state_(std::make_shared<MockTSFNState<Context, DataType>>()) {}

  // Factory method matching Napi::TypedThreadSafeFunction::New
  static MockTypedThreadSafeFunction New(Context* context, CallbackType callback, size_t max_queue_size = 0) {
    MockTypedThreadSafeFunction tsfn;
    tsfn.state_->context = context;
    tsfn.state_->callback = callback;
    tsfn.state_->max_queue_size = max_queue_size;
    tsfn.state_->initialized = true;
    return tsfn;
  }

  // Check if TSFN is valid
  explicit operator bool() const { return state_ && state_->initialized && !state_->released; }

  // NonBlockingCall
  napi_status NonBlockingCall(DataType* data = nullptr) {
    if (!state_) return napi_invalid_arg;
    std::lock_guard<std::mutex> lock(state_->mutex);

    if (!state_->initialized) {
      return napi_invalid_arg;
    }

    if (state_->released) {
      return napi_closing;
    }

    if (state_->max_queue_size > 0 && state_->queue.size() >= state_->max_queue_size) {
      return napi_queue_full;
    }

    state_->queue.push(data);
    state_->call_count++;
    return napi_ok;
  }

  // BlockingCall
  napi_status BlockingCall(DataType* data = nullptr) { return NonBlockingCall(data); }

  // Release
  void Release() {
    if (!state_) return;
    std::lock_guard<std::mutex> lock(state_->mutex);
    if (state_->initialized && !state_->released) {
      state_->released = true;
    }
  }

  // Abort
  void Abort() {
    if (!state_) return;
    std::lock_guard<std::mutex> lock(state_->mutex);
    if (state_->initialized) {
      state_->released = true;
      state_->aborted = true;
    }
  }

  // Process pending calls (for testing)
  void ProcessPending() {
    if (!state_) return;
    std::lock_guard<std::mutex> lock(state_->mutex);
    while (!state_->queue.empty()) {
      DataType* data = state_->queue.front();
      state_->queue.pop();
      if (state_->callback && state_->context) {
        state_->callback(state_->context, data);
      }
    }
  }

  // Test inspection
  [[nodiscard]] size_t call_count() const {
    if (!state_) return 0;
    std::lock_guard<std::mutex> lock(state_->mutex);
    return state_->call_count;
  }

  [[nodiscard]] size_t queue_size() const {
    if (!state_) return 0;
    std::lock_guard<std::mutex> lock(state_->mutex);
    return state_->queue.size();
  }

  [[nodiscard]] bool is_released() const {
    if (!state_) return true;
    std::lock_guard<std::mutex> lock(state_->mutex);
    return state_->released;
  }

 private:
  StatePtr state_;
};

}  // namespace webcodecs::testing
