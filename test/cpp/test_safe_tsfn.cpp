/**
 * test_safe_tsfn.cpp - Unit tests for SafeThreadSafeFunction
 *
 * Tests thread-safety, lifecycle management, and race condition prevention.
 *
 * Note: These tests use a mock TSFN since real N-API is not available
 * in pure C++ unit tests. The mock simulates TSFN behavior for testing
 * the wrapper's thread-safety guarantees.
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <condition_variable>

// C++17-compatible latch implementation
class SimpleLatch {
 public:
  explicit SimpleLatch(int count) : count_(count) {}

  void arrive_and_wait() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (--count_ <= 0) {
      cv_.notify_all();
    } else {
      cv_.wait(lock, [this] { return count_ <= 0; });
    }
  }

 private:
  std::mutex mutex_;
  std::condition_variable cv_;
  int count_;
};

#include "mocks/mock_tsfn.h"

using namespace webcodecs::testing;
using namespace std::chrono_literals;

// =============================================================================
// TEST HARNESS
// =============================================================================

/**
 * Mock-based SafeThreadSafeFunction for testing.
 * Mirrors the real implementation but uses MockTypedThreadSafeFunction.
 */
template<typename Context, typename DataType>
class TestSafeThreadSafeFunction {
 public:
  using MockTSFN = MockTypedThreadSafeFunction<Context, DataType>;

  TestSafeThreadSafeFunction() = default;

  ~TestSafeThreadSafeFunction() {
    release();
  }

  TestSafeThreadSafeFunction(const TestSafeThreadSafeFunction&) = delete;
  TestSafeThreadSafeFunction& operator=(const TestSafeThreadSafeFunction&) = delete;

  void init(MockTSFN tsfn) {
    std::lock_guard<std::mutex> lock(mutex_);
    tsfn_ = std::move(tsfn);
    released_ = false;
    initialized_ = true;
  }

  [[nodiscard]] bool call(DataType* data) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || released_) {
      return false;
    }
    napi_status status = tsfn_.NonBlockingCall(data);
    return status == napi_ok;
  }

  [[nodiscard]] bool blocking_call(DataType* data) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || released_) {
      return false;
    }
    napi_status status = tsfn_.BlockingCall(data);
    return status == napi_ok;
  }

  void release() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_ && !released_) {
      tsfn_.Release();
      released_ = true;
    }
  }

  [[nodiscard]] bool is_released() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return released_;
  }

  [[nodiscard]] bool is_active() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return initialized_ && !released_;
  }

  // Test helpers
  [[nodiscard]] size_t call_count() const {
    return tsfn_.call_count();
  }

 private:
  mutable std::mutex mutex_;
  MockTSFN tsfn_;
  bool initialized_{false};
  bool released_{true};
};

struct TestContext {
  std::atomic<int> callback_count{0};
};

struct TestData {
  int value;
};

class SafeTSFNTest : public ::testing::Test {
 protected:
  void SetUp() override {
    context_ = std::make_unique<TestContext>();
    tsfn_ = std::make_unique<TestSafeThreadSafeFunction<TestContext, TestData>>();
  }

  void TearDown() override {
    tsfn_.reset();
    context_.reset();
  }

  void InitTSFN() {
    auto mock = MockTypedThreadSafeFunction<TestContext, TestData>::New(
        context_.get(),
        [](TestContext* ctx, TestData*) {
          ctx->callback_count.fetch_add(1, std::memory_order_relaxed);
        }
    );
    tsfn_->init(std::move(mock));
  }

  std::unique_ptr<TestContext> context_;
  std::unique_ptr<TestSafeThreadSafeFunction<TestContext, TestData>> tsfn_;
};

// =============================================================================
// BASIC LIFECYCLE TESTS
// =============================================================================

TEST_F(SafeTSFNTest, InitialStateIsNotActive) {
  EXPECT_FALSE(tsfn_->is_active());
  EXPECT_TRUE(tsfn_->is_released());
}

TEST_F(SafeTSFNTest, InitMakesActive) {
  InitTSFN();
  EXPECT_TRUE(tsfn_->is_active());
  EXPECT_FALSE(tsfn_->is_released());
}

TEST_F(SafeTSFNTest, ReleaseDeactivates) {
  InitTSFN();
  EXPECT_TRUE(tsfn_->is_active());

  tsfn_->release();

  EXPECT_FALSE(tsfn_->is_active());
  EXPECT_TRUE(tsfn_->is_released());
}

TEST_F(SafeTSFNTest, DoubleReleaseIsSafe) {
  InitTSFN();
  tsfn_->release();
  tsfn_->release();  // Should not crash or throw
  EXPECT_TRUE(tsfn_->is_released());
}

TEST_F(SafeTSFNTest, DestructorReleasesAutomatically) {
  {
    TestSafeThreadSafeFunction<TestContext, TestData> local_tsfn;
    auto mock = MockTypedThreadSafeFunction<TestContext, TestData>::New(
        context_.get(), [](TestContext*, TestData*) {});
    local_tsfn.init(std::move(mock));
    EXPECT_TRUE(local_tsfn.is_active());
  }
  // Destructor should have released without issue
}

// =============================================================================
// CALL BEHAVIOR TESTS
// =============================================================================

TEST_F(SafeTSFNTest, CallFailsBeforeInit) {
  TestData data{42};
  EXPECT_FALSE(tsfn_->call(&data));
}

TEST_F(SafeTSFNTest, CallSucceedsAfterInit) {
  InitTSFN();
  TestData data{42};
  EXPECT_TRUE(tsfn_->call(&data));
  EXPECT_EQ(tsfn_->call_count(), 1);
}

TEST_F(SafeTSFNTest, CallFailsAfterRelease) {
  InitTSFN();
  tsfn_->release();

  TestData data{42};
  EXPECT_FALSE(tsfn_->call(&data));
}

TEST_F(SafeTSFNTest, BlockingCallBehavior) {
  InitTSFN();
  TestData data{42};
  EXPECT_TRUE(tsfn_->blocking_call(&data));
  EXPECT_EQ(tsfn_->call_count(), 1);
}

TEST_F(SafeTSFNTest, MultipleCalls) {
  InitTSFN();

  for (int i = 0; i < 10; ++i) {
    TestData data{i};
    EXPECT_TRUE(tsfn_->call(&data));
  }

  EXPECT_EQ(tsfn_->call_count(), 10);
}

// =============================================================================
// THREAD SAFETY TESTS (ThreadSanitizer will detect races)
// =============================================================================

TEST_F(SafeTSFNTest, ConcurrentCalls) {
  InitTSFN();

  constexpr int kThreads = 8;
  constexpr int kCallsPerThread = 100;
  std::vector<std::thread> threads;
  SimpleLatch start_latch(kThreads);

  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([this, &start_latch]() {
      start_latch.arrive_and_wait();
      for (int i = 0; i < kCallsPerThread; ++i) {
        TestData data{i};
        tsfn_->call(&data);
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  EXPECT_EQ(tsfn_->call_count(), kThreads * kCallsPerThread);
}

TEST_F(SafeTSFNTest, ConcurrentCallAndRelease) {
  InitTSFN();

  constexpr int kCallers = 4;
  std::atomic<int> successful_calls{0};
  std::atomic<int> failed_calls{0};
  SimpleLatch start_latch(kCallers + 1);  // +1 for releaser

  std::vector<std::thread> callers;
  for (int t = 0; t < kCallers; ++t) {
    callers.emplace_back([this, &start_latch, &successful_calls, &failed_calls]() {
      start_latch.arrive_and_wait();
      for (int i = 0; i < 100; ++i) {
        TestData data{i};
        if (tsfn_->call(&data)) {
          successful_calls.fetch_add(1, std::memory_order_relaxed);
        } else {
          failed_calls.fetch_add(1, std::memory_order_relaxed);
        }
      }
    });
  }

  std::thread releaser([this, &start_latch]() {
    start_latch.arrive_and_wait();
    std::this_thread::sleep_for(1ms);  // Let some calls through
    tsfn_->release();
  });

  for (auto& t : callers) {
    t.join();
  }
  releaser.join();

  // Some calls should succeed, some should fail after release
  EXPECT_GT(successful_calls.load(), 0);
  // Not asserting on failed_calls - timing dependent
  EXPECT_TRUE(tsfn_->is_released());
}

TEST_F(SafeTSFNTest, ConcurrentRelease) {
  InitTSFN();

  constexpr int kReleasers = 4;
  std::vector<std::thread> releasers;
  SimpleLatch start_latch(kReleasers);

  for (int t = 0; t < kReleasers; ++t) {
    releasers.emplace_back([this, &start_latch]() {
      start_latch.arrive_and_wait();
      tsfn_->release();
    });
  }

  for (auto& t : releasers) {
    t.join();
  }

  // All releases should have completed without error
  EXPECT_TRUE(tsfn_->is_released());
}

// =============================================================================
// EDGE CASES
// =============================================================================

TEST_F(SafeTSFNTest, NullDataCall) {
  InitTSFN();
  EXPECT_TRUE(tsfn_->call(nullptr));  // Should handle null data
}

TEST_F(SafeTSFNTest, CallAfterDestruction) {
  // Ensure no use-after-free when calling on destroyed TSFN
  auto local_tsfn = std::make_unique<TestSafeThreadSafeFunction<TestContext, TestData>>();
  auto mock = MockTypedThreadSafeFunction<TestContext, TestData>::New(
      context_.get(), [](TestContext*, TestData*) {});
  local_tsfn->init(std::move(mock));
  local_tsfn->release();
  local_tsfn.reset();  // Destroy

  // Cannot test calling after destruction as that would be UB
  // This test just verifies the cleanup path doesn't crash
}

TEST_F(SafeTSFNTest, ReinitAfterRelease) {
  InitTSFN();
  tsfn_->release();
  EXPECT_TRUE(tsfn_->is_released());

  // Reinitialize with new mock
  auto mock = MockTypedThreadSafeFunction<TestContext, TestData>::New(
      context_.get(), [](TestContext*, TestData*) {});
  tsfn_->init(std::move(mock));

  EXPECT_TRUE(tsfn_->is_active());
  TestData data{42};
  EXPECT_TRUE(tsfn_->call(&data));
}

// =============================================================================
// STATE CONSISTENCY UNDER CONTENTION
// =============================================================================

TEST_F(SafeTSFNTest, StateConsistencyUnderContention) {
  InitTSFN();

  constexpr int kIterations = 1000;
  std::atomic<bool> stop{false};
  std::atomic<int> state_checks{0};

  // Thread that continuously checks state consistency
  std::thread checker([this, &stop, &state_checks]() {
    while (!stop.load(std::memory_order_acquire)) {
      bool active = tsfn_->is_active();
      bool released = tsfn_->is_released();
      // active and released should be mutually exclusive
      EXPECT_NE(active, released);
      state_checks.fetch_add(1, std::memory_order_relaxed);
    }
  });

  // Thread that calls
  std::thread caller([this]() {
    for (int i = 0; i < 500; ++i) {
      TestData data{i};
      tsfn_->call(&data);
      std::this_thread::yield();
    }
  });

  caller.join();
  stop.store(true, std::memory_order_release);
  checker.join();

  EXPECT_GT(state_checks.load(), 100);  // Ensure we did meaningful checking
}
