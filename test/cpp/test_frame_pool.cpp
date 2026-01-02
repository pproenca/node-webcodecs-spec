/**
 * test_frame_pool.cpp - Unit tests for GlobalFramePool
 *
 * Tests dimension-keyed pooling, thread-safety, statistics, and RAII semantics.
 */

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <thread>
#include <utility>
#include <vector>

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

#include "../../src/shared/frame_pool.h"

using webcodecs::FramePoolHandle;
using webcodecs::GlobalFramePool;
using std::chrono_literals::operator""ms;

class FramePoolTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Reset pool state before each test
    GlobalFramePool::instance().clear();
    GlobalFramePool::instance().ResetStats();
    GlobalFramePool::instance().SetMaxPoolSize(32);
  }

  void TearDown() override { GlobalFramePool::instance().clear(); }
};

// =============================================================================
// BASIC OPERATIONS
// =============================================================================

TEST_F(FramePoolTest, AcquireReturnsValidFrame) {
  auto frame = GlobalFramePool::instance().acquire(1920, 1080, AV_PIX_FMT_YUV420P);
  ASSERT_NE(frame, nullptr);
  // Frame is allocated but not filled with buffer data yet
}

TEST_F(FramePoolTest, AcquireWithBufferReturnsAllocatedFrame) {
  auto frame = GlobalFramePool::instance().acquire_with_buffer(1920, 1080, AV_PIX_FMT_YUV420P);
  ASSERT_NE(frame, nullptr);
  EXPECT_EQ(frame->width, 1920);
  EXPECT_EQ(frame->height, 1080);
  EXPECT_EQ(frame->format, AV_PIX_FMT_YUV420P);
  // Buffer should be allocated
  EXPECT_NE(frame->data[0], nullptr);
}

TEST_F(FramePoolTest, FrameIsReturnedToPoolOnDestruction) {
  {
    auto frame = GlobalFramePool::instance().acquire(1920, 1080, AV_PIX_FMT_YUV420P);
    ASSERT_NE(frame, nullptr);
  }
  // Frame should be returned to pool

  auto& stats = GlobalFramePool::instance().stats();
  EXPECT_EQ(stats.current_pooled.load(), 1);
  EXPECT_EQ(stats.current_in_flight.load(), 0);
}

TEST_F(FramePoolTest, PoolHitOnSecondAcquire) {
  // First acquire - pool miss
  {
    auto frame = GlobalFramePool::instance().acquire(1920, 1080, AV_PIX_FMT_YUV420P);
    ASSERT_NE(frame, nullptr);
  }

  auto& stats = GlobalFramePool::instance().stats();
  EXPECT_EQ(stats.pool_misses.load(), 1);
  EXPECT_EQ(stats.pool_hits.load(), 0);

  // Second acquire - pool hit
  {
    auto frame = GlobalFramePool::instance().acquire(1920, 1080, AV_PIX_FMT_YUV420P);
    ASSERT_NE(frame, nullptr);
  }

  EXPECT_EQ(stats.pool_misses.load(), 1);
  EXPECT_EQ(stats.pool_hits.load(), 1);
}

// =============================================================================
// DIMENSION-KEYED POOLS
// =============================================================================

TEST_F(FramePoolTest, DifferentDimensionsSeparatePools) {
  // Acquire frames of different dimensions
  {
    auto frame_1080p = GlobalFramePool::instance().acquire(1920, 1080, AV_PIX_FMT_YUV420P);
    auto frame_720p = GlobalFramePool::instance().acquire(1280, 720, AV_PIX_FMT_YUV420P);
    ASSERT_NE(frame_1080p, nullptr);
    ASSERT_NE(frame_720p, nullptr);
  }

  auto& pool = GlobalFramePool::instance();
  EXPECT_EQ(pool.pool_count(), 2);
  EXPECT_EQ(pool.total_pooled(), 2);
}

TEST_F(FramePoolTest, DifferentFormatsSeparatePools) {
  {
    auto frame_yuv = GlobalFramePool::instance().acquire(1920, 1080, AV_PIX_FMT_YUV420P);
    auto frame_rgb = GlobalFramePool::instance().acquire(1920, 1080, AV_PIX_FMT_RGB24);
    ASSERT_NE(frame_yuv, nullptr);
    ASSERT_NE(frame_rgb, nullptr);
  }

  auto& pool = GlobalFramePool::instance();
  EXPECT_EQ(pool.pool_count(), 2);
}

TEST_F(FramePoolTest, SameDimensionsSharePool) {
  // Acquire 3 frames simultaneously, then return them to pool
  std::vector<GlobalFramePool::PooledFrame> frames;
  for (int i = 0; i < 3; ++i) {
    auto frame = GlobalFramePool::instance().acquire(1920, 1080, AV_PIX_FMT_YUV420P);
    ASSERT_NE(frame, nullptr);
    frames.push_back(std::move(frame));
  }

  // Now return all frames to pool
  frames.clear();

  EXPECT_EQ(GlobalFramePool::instance().pool_count(), 1);
  EXPECT_EQ(GlobalFramePool::instance().total_pooled(), 3);
}

// =============================================================================
// STATISTICS
// =============================================================================

TEST_F(FramePoolTest, StatsTrackTotalAllocated) {
  // Hold onto all frames to force 5 separate allocations
  std::vector<GlobalFramePool::PooledFrame> frames;
  for (int i = 0; i < 5; ++i) {
    frames.push_back(GlobalFramePool::instance().acquire(1920, 1080, AV_PIX_FMT_YUV420P));
  }

  auto& stats = GlobalFramePool::instance().stats();
  EXPECT_EQ(stats.total_allocated.load(), 5);
}

TEST_F(FramePoolTest, StatsTrackPeakInFlight) {
  std::vector<GlobalFramePool::PooledFrame> frames;
  for (int i = 0; i < 10; ++i) {
    frames.push_back(GlobalFramePool::instance().acquire(1920, 1080, AV_PIX_FMT_YUV420P));
  }

  auto& stats = GlobalFramePool::instance().stats();
  EXPECT_EQ(stats.peak_in_flight.load(), 10);
  EXPECT_EQ(stats.current_in_flight.load(), 10);

  frames.clear();  // Return all to pool

  EXPECT_EQ(stats.current_in_flight.load(), 0);
  EXPECT_EQ(stats.peak_in_flight.load(), 10);  // Peak unchanged
}

TEST_F(FramePoolTest, HitRateCalculation) {
  // Hold 5 frames simultaneously to force 5 separate allocations (misses)
  {
    std::vector<GlobalFramePool::PooledFrame> frames;
    for (int i = 0; i < 5; ++i) {
      frames.push_back(GlobalFramePool::instance().acquire(1920, 1080, AV_PIX_FMT_YUV420P));
    }
    // All 5 frames returned to pool here
  }

  // Next 5 acquires are hits (reusing pooled frames)
  {
    std::vector<GlobalFramePool::PooledFrame> frames;
    for (int i = 0; i < 5; ++i) {
      frames.push_back(GlobalFramePool::instance().acquire(1920, 1080, AV_PIX_FMT_YUV420P));
    }
  }

  auto& stats = GlobalFramePool::instance().stats();
  double hit_rate = stats.hit_rate();
  EXPECT_DOUBLE_EQ(hit_rate, 0.5);  // 5 hits / 10 total
}

TEST_F(FramePoolTest, ResetStats) {
  auto frame = GlobalFramePool::instance().acquire(1920, 1080, AV_PIX_FMT_YUV420P);
  GlobalFramePool::instance().ResetStats();

  auto& stats = GlobalFramePool::instance().stats();
  EXPECT_EQ(stats.total_allocated.load(), 0);
  EXPECT_EQ(stats.pool_hits.load(), 0);
  EXPECT_EQ(stats.pool_misses.load(), 0);
}

// =============================================================================
// POOL SIZE LIMITS
// =============================================================================

TEST_F(FramePoolTest, MaxPoolSizeEnforced) {
  GlobalFramePool::instance().SetMaxPoolSize(3);

  // Acquire 5 frames simultaneously, then return them all
  {
    std::vector<GlobalFramePool::PooledFrame> frames;
    for (int i = 0; i < 5; ++i) {
      frames.push_back(GlobalFramePool::instance().acquire(1920, 1080, AV_PIX_FMT_YUV420P));
    }
    // All 5 returned here, but only 3 kept due to max size
  }

  EXPECT_EQ(GlobalFramePool::instance().total_pooled(), 3);
}

TEST_F(FramePoolTest, TrimReducesPoolSize) {
  // Acquire 10 frames simultaneously, then return them all to pool
  {
    std::vector<GlobalFramePool::PooledFrame> frames;
    for (int i = 0; i < 10; ++i) {
      frames.push_back(GlobalFramePool::instance().acquire(1920, 1080, AV_PIX_FMT_YUV420P));
    }
  }
  EXPECT_EQ(GlobalFramePool::instance().total_pooled(), 10);

  GlobalFramePool::instance().trim(3);
  EXPECT_EQ(GlobalFramePool::instance().total_pooled(), 3);
}

TEST_F(FramePoolTest, ClearRemovesAllPooledFrames) {
  // Acquire 5 frames simultaneously, then return them all to pool
  {
    std::vector<GlobalFramePool::PooledFrame> frames;
    for (int i = 0; i < 5; ++i) {
      frames.push_back(GlobalFramePool::instance().acquire(1920, 1080, AV_PIX_FMT_YUV420P));
    }
  }
  EXPECT_EQ(GlobalFramePool::instance().total_pooled(), 5);

  GlobalFramePool::instance().clear();
  EXPECT_EQ(GlobalFramePool::instance().total_pooled(), 0);
}

// =============================================================================
// RAII SEMANTICS
// =============================================================================

TEST_F(FramePoolTest, MoveSemantics) {
  auto frame1 = GlobalFramePool::instance().acquire(1920, 1080, AV_PIX_FMT_YUV420P);
  ASSERT_NE(frame1, nullptr);

  GlobalFramePool::PooledFrame frame2 = std::move(frame1);
  EXPECT_EQ(frame1, nullptr);
  EXPECT_NE(frame2, nullptr);

  auto& stats = GlobalFramePool::instance().stats();
  EXPECT_EQ(stats.current_in_flight.load(), 1);  // Only one in flight
}

TEST_F(FramePoolTest, NullptrDeleterIsSafe) {
  GlobalFramePool::PooledFrame null_frame = nullptr;
  // Destructor should not crash
}

// =============================================================================
// THREAD SAFETY (ThreadSanitizer will detect races)
// =============================================================================

TEST_F(FramePoolTest, ConcurrentAcquireRelease) {
  constexpr int kThreads = 8;
  constexpr int kIterations = 100;
  std::vector<std::thread> threads;
  SimpleLatch start_latch(kThreads);

  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([&start_latch]() {
      start_latch.arrive_and_wait();
      for (int i = 0; i < kIterations; ++i) {
        auto frame = GlobalFramePool::instance().acquire(1920, 1080, AV_PIX_FMT_YUV420P);
        EXPECT_NE(frame, nullptr);
        // Frame returned on scope exit
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  auto& stats = GlobalFramePool::instance().stats();
  EXPECT_EQ(stats.current_in_flight.load(), 0);
  // Total operations = threads * iterations
  uint64_t total = stats.pool_hits.load() + stats.pool_misses.load();
  EXPECT_EQ(total, kThreads * kIterations);
}

TEST_F(FramePoolTest, ConcurrentDifferentDimensions) {
  constexpr int kThreads = 4;
  constexpr int kIterations = 50;
  std::vector<std::thread> threads;
  SimpleLatch start_latch(kThreads);

  // Each thread uses different dimensions
  int dimensions[][2] = {{1920, 1080}, {1280, 720}, {3840, 2160}, {640, 480}};

  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([t, &dimensions, &start_latch]() {
      start_latch.arrive_and_wait();
      for (int i = 0; i < kIterations; ++i) {
        auto frame = GlobalFramePool::instance().acquire(dimensions[t][0], dimensions[t][1], AV_PIX_FMT_YUV420P);
        EXPECT_NE(frame, nullptr);
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  EXPECT_EQ(GlobalFramePool::instance().pool_count(), 4);
}

TEST_F(FramePoolTest, ConcurrentClearDuringAcquire) {
  constexpr int kAcquireThreads = 4;
  std::atomic<bool> stop{false};
  std::vector<std::thread> acquirers;
  SimpleLatch start_latch(kAcquireThreads + 1);

  for (int t = 0; t < kAcquireThreads; ++t) {
    acquirers.emplace_back([&stop, &start_latch]() {
      start_latch.arrive_and_wait();
      while (!stop.load(std::memory_order_acquire)) {
        auto frame = GlobalFramePool::instance().acquire(1920, 1080, AV_PIX_FMT_YUV420P);
        // Release quickly
      }
    });
  }

  std::thread clearer([&stop, &start_latch]() {
    start_latch.arrive_and_wait();
    for (int i = 0; i < 10; ++i) {
      std::this_thread::sleep_for(1ms);
      GlobalFramePool::instance().clear();
    }
    stop.store(true, std::memory_order_release);
  });

  for (auto& t : acquirers) {
    t.join();
  }
  clearer.join();

  // Should complete without crashes or data races
  EXPECT_EQ(GlobalFramePool::instance().stats().current_in_flight.load(), 0);
}

// =============================================================================
// FRAME POOL HANDLE
// =============================================================================

TEST_F(FramePoolTest, FramePoolHandleAcquire) {
  FramePoolHandle handle;
  auto frame = handle.acquire(1920, 1080, AV_PIX_FMT_YUV420P);
  ASSERT_NE(frame, nullptr);
}

TEST_F(FramePoolTest, FramePoolHandleAcquireWithBuffer) {
  FramePoolHandle handle;
  auto frame = handle.acquire_with_buffer(1920, 1080, AV_PIX_FMT_YUV420P);
  ASSERT_NE(frame, nullptr);
  EXPECT_EQ(frame->width, 1920);
  EXPECT_NE(frame->data[0], nullptr);
}

TEST_F(FramePoolTest, FramePoolHandleStats) {
  FramePoolHandle handle;
  auto frame = handle.acquire(1920, 1080, AV_PIX_FMT_YUV420P);

  const auto& stats = handle.stats();
  EXPECT_GT(stats.pool_misses.load() + stats.pool_hits.load(), 0);
}

// =============================================================================
// EDGE CASES
// =============================================================================

TEST_F(FramePoolTest, ZeroDimensionsHandled) {
  // Edge case: zero dimensions should still work (FFmpeg allows it)
  auto frame = GlobalFramePool::instance().acquire(0, 0, AV_PIX_FMT_YUV420P);
  EXPECT_NE(frame, nullptr);
}

TEST_F(FramePoolTest, LargeDimensions) {
  // 8K resolution
  auto frame = GlobalFramePool::instance().acquire(7680, 4320, AV_PIX_FMT_YUV420P);
  EXPECT_NE(frame, nullptr);
}

TEST_F(FramePoolTest, PooledFrameUnrefOnReturn) {
  // Verify frame is unref'd when returned to pool
  auto frame = GlobalFramePool::instance().acquire_with_buffer(1920, 1080, AV_PIX_FMT_YUV420P);
  ASSERT_NE(frame, nullptr);

  // Fill with data
  memset(frame->data[0], 42, frame->linesize[0] * frame->height);

  // Return to pool
  frame.reset();

  // Acquire again - should be unref'd (data pointers null until alloc)
  auto frame2 = GlobalFramePool::instance().acquire(1920, 1080, AV_PIX_FMT_YUV420P);
  EXPECT_EQ(frame2->data[0], nullptr);  // Unref'd, no buffer
}
