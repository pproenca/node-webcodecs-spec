#pragma once
/**
 * promise_utils.h - Promise Tracking for WebCodecs Async Operations
 *
 * Provides utilities for tracking pending promises across async operations,
 * particularly for flush() operations that need to resolve when all
 * queued work is complete.
 *
 * Thread Safety:
 * - PromiseTracker is thread-safe for concurrent access
 * - Promises must be resolved/rejected from the JS main thread
 * - Use TSFN to schedule resolution from worker threads
 */

#include <napi.h>

#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace webcodecs {
namespace promise_utils {

/**
 * PromiseTracker - Track pending promises for async operations.
 *
 * WebCodecs flush() operations return a Promise that resolves when all
 * queued frames/chunks have been processed. This class tracks these
 * promises and provides thread-safe resolution.
 *
 * Usage:
 *   class VideoDecoder {
 *     PromiseTracker flushPromises_;
 *
 *     Napi::Value Flush(const Napi::CallbackInfo& info) {
 *       auto [id, promise] = flushPromises_.CreatePromise(info.Env());
 *       // Queue flush operation with promise ID
 *       return promise;
 *     }
 *
 *     void OnFlushComplete(uint32_t promiseId) {
 *       flushPromises_.Resolve(promiseId);
 *     }
 *   };
 */
class PromiseTracker {
 public:
  /**
   * Result of creating a tracked promise.
   */
  struct TrackedPromise {
    uint32_t id;
    Napi::Promise promise;
  };

  PromiseTracker() : nextId_(1) {}

  // Non-copyable, non-movable (owns mutex and references)
  PromiseTracker(const PromiseTracker&) = delete;
  PromiseTracker& operator=(const PromiseTracker&) = delete;
  PromiseTracker(PromiseTracker&&) = delete;
  PromiseTracker& operator=(PromiseTracker&&) = delete;

  ~PromiseTracker() {
    // Note: Pending promises will be left unresolved if not cleaned up
    // This is intentional - the caller should call RejectAll() before destruction
  }

  /**
   * Create a new tracked promise.
   *
   * @param env The Napi environment
   * @return Struct containing the promise ID and the Promise object
   */
  TrackedPromise CreatePromise(Napi::Env env) {
    std::lock_guard<std::mutex> lock(mutex_);

    uint32_t id = nextId_++;
    Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);

    pending_.emplace(id, std::move(deferred));

    return TrackedPromise{id, pending_[id].Promise()};
  }

  /**
   * Resolve a promise with undefined.
   *
   * MUST be called from the JS main thread.
   *
   * @param id The promise ID
   * @return true if the promise was found and resolved, false if not found
   */
  bool Resolve(uint32_t id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = pending_.find(id);
    if (it == pending_.end()) {
      return false;
    }

    Napi::Env env = it->second.Env();
    it->second.Resolve(env.Undefined());
    pending_.erase(it);
    return true;
  }

  /**
   * Resolve a promise with a specific value.
   *
   * MUST be called from the JS main thread.
   *
   * @param id The promise ID
   * @param value The value to resolve with
   * @return true if the promise was found and resolved, false if not found
   */
  bool Resolve(uint32_t id, Napi::Value value) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = pending_.find(id);
    if (it == pending_.end()) {
      return false;
    }

    it->second.Resolve(value);
    pending_.erase(it);
    return true;
  }

  /**
   * Reject a promise with an error.
   *
   * MUST be called from the JS main thread.
   *
   * @param id The promise ID
   * @param error The error to reject with
   * @return true if the promise was found and rejected, false if not found
   */
  bool Reject(uint32_t id, Napi::Error error) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = pending_.find(id);
    if (it == pending_.end()) {
      return false;
    }

    it->second.Reject(error.Value());
    pending_.erase(it);
    return true;
  }

  /**
   * Reject a promise with an error value.
   *
   * MUST be called from the JS main thread.
   *
   * @param id The promise ID
   * @param errorValue The error value to reject with
   * @return true if the promise was found and rejected, false if not found
   */
  bool Reject(uint32_t id, Napi::Value errorValue) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = pending_.find(id);
    if (it == pending_.end()) {
      return false;
    }

    it->second.Reject(errorValue);
    pending_.erase(it);
    return true;
  }

  /**
   * Reject all pending promises with an error message.
   *
   * Use this during reset() or close() to reject all pending flush promises.
   * MUST be called from the JS main thread.
   *
   * @param env The Napi environment
   * @param errorName The error name (e.g., "AbortError", "InvalidStateError")
   * @param message The error message
   */
  void RejectAll(Napi::Env env, const std::string& errorName, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& [id, deferred] : pending_) {
      Napi::Error error = Napi::Error::New(env, message);
      error.Set("name", Napi::String::New(env, errorName));
      deferred.Reject(error.Value());
    }

    pending_.clear();
  }

  /**
   * Get the count of pending promises.
   *
   * @return Number of unresolved promises
   */
  size_t PendingCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pending_.size();
  }

  /**
   * Check if there are any pending promises.
   *
   * @return true if there are pending promises
   */
  bool HasPending() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !pending_.empty();
  }

  /**
   * Get all pending promise IDs.
   *
   * Useful for debugging or for flushing operations.
   *
   * @return Vector of pending promise IDs
   */
  std::vector<uint32_t> GetPendingIds() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<uint32_t> ids;
    ids.reserve(pending_.size());
    for (const auto& [id, _] : pending_) {
      ids.push_back(id);
    }
    return ids;
  }

 private:
  mutable std::mutex mutex_;
  std::atomic<uint32_t> nextId_;
  std::unordered_map<uint32_t, Napi::Promise::Deferred> pending_;
};

/**
 * Data structure for passing promise resolution info through TSFN.
 *
 * Use this when you need to resolve a promise from a worker thread.
 * Create this on the worker thread, pass through TSFN, then process
 * on the main thread.
 */
struct PromiseResolution {
  uint32_t promiseId;
  bool success;
  std::string errorName;
  std::string errorMessage;

  static PromiseResolution Success(uint32_t id) { return PromiseResolution{id, true, "", ""}; }

  static PromiseResolution Failure(uint32_t id, const std::string& name, const std::string& msg) {
    return PromiseResolution{id, false, name, msg};
  }
};

/**
 * Process a promise resolution on the main thread.
 *
 * @param tracker The promise tracker
 * @param env The Napi environment
 * @param resolution The resolution info
 */
inline void ProcessPromiseResolution(PromiseTracker& tracker, Napi::Env env, const PromiseResolution& resolution) {
  if (resolution.success) {
    tracker.Resolve(resolution.promiseId);
  } else {
    Napi::Error error = Napi::Error::New(env, resolution.errorMessage);
    error.Set("name", Napi::String::New(env, resolution.errorName));
    tracker.Reject(resolution.promiseId, error);
  }
}

}  // namespace promise_utils
}  // namespace webcodecs
