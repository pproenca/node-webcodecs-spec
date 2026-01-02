/**
 * test_packet_pool.cpp - Unit tests for GlobalPacketPool
 *
 * Tests packet pooling, thread-safety, statistics, and RAII semantics.
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

#include "../../src/shared/packet_pool.h"

extern "C" {
#include <libavutil/avutil.h>
}

using webcodecs::GlobalPacketPool;
using webcodecs::PacketPoolHandle;
using std::chrono_literals::operator""ms;

class PacketPoolTest : public ::testing::Test {
 protected:
  void SetUp() override {
    GlobalPacketPool::instance().clear();
    GlobalPacketPool::instance().ResetStats();
    GlobalPacketPool::instance().SetMaxPoolSize(64);
  }

  void TearDown() override { GlobalPacketPool::instance().clear(); }
};

// =============================================================================
// BASIC OPERATIONS
// =============================================================================

TEST_F(PacketPoolTest, AcquireReturnsValidPacket) {
  auto packet = GlobalPacketPool::instance().acquire();
  ASSERT_NE(packet, nullptr);
}

TEST_F(PacketPoolTest, PacketIsReturnedToPoolOnDestruction) {
  {
    auto packet = GlobalPacketPool::instance().acquire();
    ASSERT_NE(packet, nullptr);
  }

  auto& stats = GlobalPacketPool::instance().stats();
  EXPECT_EQ(stats.current_pooled.load(), 1);
  EXPECT_EQ(stats.current_in_flight.load(), 0);
}

TEST_F(PacketPoolTest, PoolHitOnSecondAcquire) {
  {
    auto packet = GlobalPacketPool::instance().acquire();
    ASSERT_NE(packet, nullptr);
  }

  auto& stats = GlobalPacketPool::instance().stats();
  EXPECT_EQ(stats.pool_misses.load(), 1);
  EXPECT_EQ(stats.pool_hits.load(), 0);

  {
    auto packet = GlobalPacketPool::instance().acquire();
    ASSERT_NE(packet, nullptr);
  }

  EXPECT_EQ(stats.pool_misses.load(), 1);
  EXPECT_EQ(stats.pool_hits.load(), 1);
}

// =============================================================================
// ACQUIRE VARIANTS
// =============================================================================

TEST_F(PacketPoolTest, AcquireWithBuffer) {
  auto packet = GlobalPacketPool::instance().acquire_with_buffer(1024);
  ASSERT_NE(packet, nullptr);
  EXPECT_EQ(packet->size, 1024);
  EXPECT_NE(packet->data, nullptr);

  auto& stats = GlobalPacketPool::instance().stats();
  EXPECT_EQ(stats.total_bytes_allocated.load(), 1024);
}

TEST_F(PacketPoolTest, AcquireRef) {
  // Create source packet with data
  AVPacket* src = av_packet_alloc();
  ASSERT_NE(src, nullptr);
  ASSERT_EQ(av_new_packet(src, 512), 0);
  memset(src->data, 42, 512);
  src->pts = 1000;
  src->dts = 900;

  auto packet = GlobalPacketPool::instance().acquire_ref(src);
  ASSERT_NE(packet, nullptr);

  // Should have same data (reference)
  EXPECT_EQ(packet->size, 512);
  EXPECT_EQ(packet->data[0], 42);
  EXPECT_EQ(packet->pts, 1000);
  EXPECT_EQ(packet->dts, 900);

  auto& stats = GlobalPacketPool::instance().stats();
  EXPECT_EQ(stats.total_bytes_allocated.load(), 512);

  av_packet_free(&src);
}

TEST_F(PacketPoolTest, AcquireRefNullptr) {
  auto packet = GlobalPacketPool::instance().acquire_ref(nullptr);
  EXPECT_EQ(packet, nullptr);
}

// =============================================================================
// STATISTICS
// =============================================================================

TEST_F(PacketPoolTest, StatsTrackTotalAllocated) {
  // Hold onto all packets to force 5 separate allocations
  std::vector<GlobalPacketPool::PooledPacket> packets;
  for (int i = 0; i < 5; ++i) {
    packets.push_back(GlobalPacketPool::instance().acquire());
  }

  auto& stats = GlobalPacketPool::instance().stats();
  EXPECT_EQ(stats.total_allocated.load(), 5);
}

TEST_F(PacketPoolTest, StatsTrackPeakInFlight) {
  std::vector<GlobalPacketPool::PooledPacket> packets;
  for (int i = 0; i < 10; ++i) {
    packets.push_back(GlobalPacketPool::instance().acquire());
  }

  auto& stats = GlobalPacketPool::instance().stats();
  EXPECT_EQ(stats.peak_in_flight.load(), 10);
  EXPECT_EQ(stats.current_in_flight.load(), 10);

  packets.clear();

  EXPECT_EQ(stats.current_in_flight.load(), 0);
  EXPECT_EQ(stats.peak_in_flight.load(), 10);
}

TEST_F(PacketPoolTest, HitRateCalculation) {
  // Hold 4 packets simultaneously to force 4 separate allocations (misses)
  {
    std::vector<GlobalPacketPool::PooledPacket> packets;
    for (int i = 0; i < 4; ++i) {
      packets.push_back(GlobalPacketPool::instance().acquire());
    }
    // All 4 packets returned to pool here
  }

  // Next 4 acquires are hits (reusing pooled packets)
  {
    std::vector<GlobalPacketPool::PooledPacket> packets;
    for (int i = 0; i < 4; ++i) {
      packets.push_back(GlobalPacketPool::instance().acquire());
    }
  }

  auto& stats = GlobalPacketPool::instance().stats();
  double hit_rate = stats.hit_rate();
  EXPECT_DOUBLE_EQ(hit_rate, 0.5);  // 4 hits / 8 total
}

TEST_F(PacketPoolTest, ResetStats) {
  auto packet = GlobalPacketPool::instance().acquire_with_buffer(100);
  GlobalPacketPool::instance().ResetStats();

  auto& stats = GlobalPacketPool::instance().stats();
  EXPECT_EQ(stats.total_allocated.load(), 0);
  EXPECT_EQ(stats.pool_hits.load(), 0);
  EXPECT_EQ(stats.pool_misses.load(), 0);
  EXPECT_EQ(stats.total_bytes_allocated.load(), 0);
}

// =============================================================================
// POOL SIZE LIMITS
// =============================================================================

TEST_F(PacketPoolTest, MaxPoolSizeEnforced) {
  GlobalPacketPool::instance().SetMaxPoolSize(3);

  // Acquire 5 packets simultaneously, then return them all
  {
    std::vector<GlobalPacketPool::PooledPacket> packets;
    for (int i = 0; i < 5; ++i) {
      packets.push_back(GlobalPacketPool::instance().acquire());
    }
    // All 5 returned here, but only 3 kept due to max size
  }

  EXPECT_EQ(GlobalPacketPool::instance().pooled_count(), 3);
}

TEST_F(PacketPoolTest, TrimReducesPoolSize) {
  // Acquire 10 packets simultaneously, then return them all to pool
  {
    std::vector<GlobalPacketPool::PooledPacket> packets;
    for (int i = 0; i < 10; ++i) {
      packets.push_back(GlobalPacketPool::instance().acquire());
    }
  }
  EXPECT_EQ(GlobalPacketPool::instance().pooled_count(), 10);

  GlobalPacketPool::instance().trim(3);
  EXPECT_EQ(GlobalPacketPool::instance().pooled_count(), 3);
}

TEST_F(PacketPoolTest, ClearRemovesAllPooledPackets) {
  // Acquire 5 packets simultaneously, then return them all to pool
  {
    std::vector<GlobalPacketPool::PooledPacket> packets;
    for (int i = 0; i < 5; ++i) {
      packets.push_back(GlobalPacketPool::instance().acquire());
    }
  }
  EXPECT_EQ(GlobalPacketPool::instance().pooled_count(), 5);

  GlobalPacketPool::instance().clear();
  EXPECT_EQ(GlobalPacketPool::instance().pooled_count(), 0);
}

// =============================================================================
// RAII SEMANTICS
// =============================================================================

TEST_F(PacketPoolTest, MoveSemantics) {
  auto packet1 = GlobalPacketPool::instance().acquire();
  ASSERT_NE(packet1, nullptr);

  GlobalPacketPool::PooledPacket packet2 = std::move(packet1);
  EXPECT_EQ(packet1, nullptr);
  EXPECT_NE(packet2, nullptr);

  auto& stats = GlobalPacketPool::instance().stats();
  EXPECT_EQ(stats.current_in_flight.load(), 1);
}

TEST_F(PacketPoolTest, NullptrDeleterIsSafe) {
  GlobalPacketPool::PooledPacket null_packet = nullptr;
  // Destructor should not crash
}

TEST_F(PacketPoolTest, PacketUnrefOnReturn) {
  // Acquire and fill with data
  auto packet = GlobalPacketPool::instance().acquire_with_buffer(256);
  ASSERT_NE(packet, nullptr);
  memset(packet->data, 42, 256);
  packet->pts = 12345;

  // Return to pool
  packet.reset();

  // Acquire again - should be unref'd
  auto packet2 = GlobalPacketPool::instance().acquire();
  EXPECT_EQ(packet2->data, nullptr);  // Unref'd
  EXPECT_EQ(packet2->size, 0);
  EXPECT_EQ(packet2->pts, AV_NOPTS_VALUE);
}

// =============================================================================
// THREAD SAFETY
// =============================================================================

TEST_F(PacketPoolTest, ConcurrentAcquireRelease) {
  constexpr int kThreads = 8;
  constexpr int kIterations = 100;
  std::vector<std::thread> threads;
  SimpleLatch start_latch(kThreads);

  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([&start_latch]() {
      start_latch.arrive_and_wait();
      for (int i = 0; i < kIterations; ++i) {
        auto packet = GlobalPacketPool::instance().acquire();
        EXPECT_NE(packet, nullptr);
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  auto& stats = GlobalPacketPool::instance().stats();
  EXPECT_EQ(stats.current_in_flight.load(), 0);
  uint64_t total = stats.pool_hits.load() + stats.pool_misses.load();
  EXPECT_EQ(total, kThreads * kIterations);
}

TEST_F(PacketPoolTest, ConcurrentAcquireWithBuffer) {
  constexpr int kThreads = 4;
  constexpr int kIterations = 50;
  std::vector<std::thread> threads;
  SimpleLatch start_latch(kThreads);

  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([t, &start_latch]() {
      start_latch.arrive_and_wait();
      for (int i = 0; i < kIterations; ++i) {
        int size = 100 + (t * 100) + (i % 10);  // Varying sizes
        auto packet = GlobalPacketPool::instance().acquire_with_buffer(size);
        EXPECT_NE(packet, nullptr);
        EXPECT_EQ(packet->size, size);
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  auto& stats = GlobalPacketPool::instance().stats();
  EXPECT_EQ(stats.current_in_flight.load(), 0);
}

TEST_F(PacketPoolTest, ConcurrentClearDuringAcquire) {
  constexpr int kAcquireThreads = 4;
  std::atomic<bool> stop{false};
  std::vector<std::thread> acquirers;
  SimpleLatch start_latch(kAcquireThreads + 1);

  for (int t = 0; t < kAcquireThreads; ++t) {
    acquirers.emplace_back([&stop, &start_latch]() {
      start_latch.arrive_and_wait();
      while (!stop.load(std::memory_order_acquire)) {
        auto packet = GlobalPacketPool::instance().acquire();
      }
    });
  }

  std::thread clearer([&stop, &start_latch]() {
    start_latch.arrive_and_wait();
    for (int i = 0; i < 10; ++i) {
      std::this_thread::sleep_for(1ms);
      GlobalPacketPool::instance().clear();
    }
    stop.store(true, std::memory_order_release);
  });

  for (auto& t : acquirers) {
    t.join();
  }
  clearer.join();

  EXPECT_EQ(GlobalPacketPool::instance().stats().current_in_flight.load(), 0);
}

// =============================================================================
// PACKET POOL HANDLE
// =============================================================================

TEST_F(PacketPoolTest, PacketPoolHandleAcquire) {
  PacketPoolHandle handle;
  auto packet = handle.acquire();
  ASSERT_NE(packet, nullptr);
}

TEST_F(PacketPoolTest, PacketPoolHandleAcquireRef) {
  AVPacket* src = av_packet_alloc();
  ASSERT_NE(src, nullptr);
  ASSERT_EQ(av_new_packet(src, 128), 0);
  src->pts = 500;

  PacketPoolHandle handle;
  auto packet = handle.acquire_ref(src);
  ASSERT_NE(packet, nullptr);
  EXPECT_EQ(packet->pts, 500);

  av_packet_free(&src);
}

TEST_F(PacketPoolTest, PacketPoolHandleAcquireWithBuffer) {
  PacketPoolHandle handle;
  auto packet = handle.acquire_with_buffer(512);
  ASSERT_NE(packet, nullptr);
  EXPECT_EQ(packet->size, 512);
}

TEST_F(PacketPoolTest, PacketPoolHandleStats) {
  PacketPoolHandle handle;
  auto packet = handle.acquire_with_buffer(100);

  const auto& stats = handle.stats();
  EXPECT_GT(stats.pool_misses.load() + stats.pool_hits.load(), 0);
  EXPECT_EQ(stats.total_bytes_allocated.load(), 100);
}

// =============================================================================
// EDGE CASES
// =============================================================================

TEST_F(PacketPoolTest, ZeroSizeBuffer) {
  auto packet = GlobalPacketPool::instance().acquire_with_buffer(0);
  // av_new_packet with size 0 succeeds in modern FFmpeg (allocates empty buffer)
  ASSERT_NE(packet, nullptr);
  EXPECT_EQ(packet->size, 0);
}

TEST_F(PacketPoolTest, LargeBuffer) {
  // 1MB buffer
  auto packet = GlobalPacketPool::instance().acquire_with_buffer(1024 * 1024);
  ASSERT_NE(packet, nullptr);
  EXPECT_EQ(packet->size, 1024 * 1024);

  auto& stats = GlobalPacketPool::instance().stats();
  EXPECT_EQ(stats.total_bytes_allocated.load(), 1024 * 1024);
}

TEST_F(PacketPoolTest, RapidAcquireRelease) {
  // Stress test rapid acquire/release
  for (int i = 0; i < 1000; ++i) {
    auto packet = GlobalPacketPool::instance().acquire();
    EXPECT_NE(packet, nullptr);
  }

  auto& stats = GlobalPacketPool::instance().stats();
  // After warmup, most should be hits
  EXPECT_GT(stats.pool_hits.load(), stats.pool_misses.load());
}
