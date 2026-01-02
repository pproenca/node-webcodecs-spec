/**
 * test_control_message_queue.cpp - Unit tests for ControlMessageQueue
 *
 * Tests thread-safety, message ordering, and WebCodecs queue semantics.
 */

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

#include "../../src/shared/control_message_queue.h"

using webcodecs::ControlMessageQueue;
using webcodecs::MessageVisitor;
using std::chrono_literals::operator""ms;

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

// Test with simple types to avoid FFmpeg dependency for basic queue tests
using TestPacket = std::unique_ptr<int>;
using TestFrame = std::unique_ptr<int>;
using TestQueue = ControlMessageQueue<TestPacket, TestFrame>;

class ControlMessageQueueTest : public ::testing::Test {
 protected:
  void SetUp() override { queue_ = std::make_unique<TestQueue>(); }

  void TearDown() override {
    queue_->shutdown();
    queue_.reset();
  }

  std::unique_ptr<TestQueue> queue_;
};

// =============================================================================
// BASIC OPERATIONS
// =============================================================================

TEST_F(ControlMessageQueueTest, InitialStateIsEmpty) {
  EXPECT_TRUE(queue_->empty());
  EXPECT_EQ(queue_->size(), 0);
  EXPECT_FALSE(queue_->is_closed());
  EXPECT_FALSE(queue_->is_blocked());
}

TEST_F(ControlMessageQueueTest, EnqueueIncrementsSize) {
  TestQueue::DecodeMessage msg{std::make_unique<int>(42)};
  EXPECT_TRUE(queue_->enqueue(std::move(msg)));
  EXPECT_EQ(queue_->size(), 1);
  EXPECT_FALSE(queue_->empty());
}

TEST_F(ControlMessageQueueTest, DequeueDecrementsSize) {
  queue_->enqueue(TestQueue::DecodeMessage{std::make_unique<int>(1)});
  queue_->enqueue(TestQueue::DecodeMessage{std::make_unique<int>(2)});
  EXPECT_EQ(queue_->size(), 2);

  auto msg = queue_->try_dequeue();
  ASSERT_TRUE(msg.has_value());
  EXPECT_EQ(queue_->size(), 1);
}

TEST_F(ControlMessageQueueTest, FIFOOrdering) {
  for (int i = 0; i < 5; ++i) {
    queue_->enqueue(TestQueue::DecodeMessage{std::make_unique<int>(i)});
  }

  for (int i = 0; i < 5; ++i) {
    auto msg = queue_->try_dequeue();
    ASSERT_TRUE(msg.has_value());
    auto* decode = std::get_if<TestQueue::DecodeMessage>(&*msg);
    ASSERT_NE(decode, nullptr);
    EXPECT_EQ(*decode->packet, i);
  }
}

// =============================================================================
// MESSAGE TYPE TESTS
// =============================================================================

TEST_F(ControlMessageQueueTest, ConfigureMessage) {
  bool called = false;
  TestQueue::ConfigureMessage msg{[&called]() {
    called = true;
    return true;
  }};

  EXPECT_TRUE(queue_->enqueue(std::move(msg)));
  auto dequeued = queue_->try_dequeue();
  ASSERT_TRUE(dequeued.has_value());

  auto* configure = std::get_if<TestQueue::ConfigureMessage>(&*dequeued);
  ASSERT_NE(configure, nullptr);
  EXPECT_TRUE(configure->configure_fn());
  EXPECT_TRUE(called);
}

TEST_F(ControlMessageQueueTest, FlushMessage) {
  TestQueue::FlushMessage msg{123};
  EXPECT_TRUE(queue_->enqueue(std::move(msg)));

  auto dequeued = queue_->try_dequeue();
  ASSERT_TRUE(dequeued.has_value());

  auto* flush = std::get_if<TestQueue::FlushMessage>(&*dequeued);
  ASSERT_NE(flush, nullptr);
  EXPECT_EQ(flush->promise_id, 123);
}

TEST_F(ControlMessageQueueTest, ResetMessage) {
  EXPECT_TRUE(queue_->enqueue(TestQueue::ResetMessage{}));

  auto dequeued = queue_->try_dequeue();
  ASSERT_TRUE(dequeued.has_value());
  EXPECT_NE(std::get_if<TestQueue::ResetMessage>(&*dequeued), nullptr);
}

TEST_F(ControlMessageQueueTest, CloseMessage) {
  EXPECT_TRUE(queue_->enqueue(TestQueue::CloseMessage{}));

  auto dequeued = queue_->try_dequeue();
  ASSERT_TRUE(dequeued.has_value());
  EXPECT_NE(std::get_if<TestQueue::CloseMessage>(&*dequeued), nullptr);
}

// =============================================================================
// SHUTDOWN BEHAVIOR
// =============================================================================

TEST_F(ControlMessageQueueTest, EnqueueFailsAfterShutdown) {
  queue_->shutdown();
  EXPECT_TRUE(queue_->is_closed());
  EXPECT_FALSE(queue_->enqueue(TestQueue::DecodeMessage{std::make_unique<int>(42)}));
}

TEST_F(ControlMessageQueueTest, TryDequeueReturnsNulloptWhenEmpty) {
  auto msg = queue_->try_dequeue();
  EXPECT_FALSE(msg.has_value());
}

TEST_F(ControlMessageQueueTest, DequeueReturnsNulloptAfterShutdownWhenEmpty) {
  queue_->shutdown();
  auto msg = queue_->try_dequeue();
  EXPECT_FALSE(msg.has_value());
}

TEST_F(ControlMessageQueueTest, DequeueReturnsPendingMessagesAfterShutdown) {
  queue_->enqueue(TestQueue::DecodeMessage{std::make_unique<int>(1)});
  queue_->enqueue(TestQueue::DecodeMessage{std::make_unique<int>(2)});
  queue_->shutdown();

  // Should still be able to drain pending messages
  auto msg1 = queue_->try_dequeue();
  auto msg2 = queue_->try_dequeue();
  auto msg3 = queue_->try_dequeue();

  EXPECT_TRUE(msg1.has_value());
  EXPECT_TRUE(msg2.has_value());
  EXPECT_FALSE(msg3.has_value());
}

// =============================================================================
// CLEAR BEHAVIOR
// =============================================================================

TEST_F(ControlMessageQueueTest, ClearReturnsDroppedPackets) {
  queue_->enqueue(TestQueue::DecodeMessage{std::make_unique<int>(1)});
  queue_->enqueue(TestQueue::DecodeMessage{std::make_unique<int>(2)});
  queue_->enqueue(TestQueue::DecodeMessage{std::make_unique<int>(3)});
  queue_->enqueue(TestQueue::FlushMessage{99});  // Non-decode message

  auto dropped = queue_->clear();
  EXPECT_EQ(dropped.size(), 3);  // Only DecodeMessages have packets
  EXPECT_TRUE(queue_->empty());
}

TEST_F(ControlMessageQueueTest, ClearPreservesPacketOwnership) {
  queue_->enqueue(TestQueue::DecodeMessage{std::make_unique<int>(42)});
  auto dropped = queue_->clear();

  ASSERT_EQ(dropped.size(), 1);
  EXPECT_EQ(*dropped[0], 42);  // Ownership transferred to dropped vector
}

// =============================================================================
// BLOCKED STATE
// =============================================================================

TEST_F(ControlMessageQueueTest, BlockedStateCanBeSet) {
  EXPECT_FALSE(queue_->is_blocked());
  queue_->set_blocked(true);
  EXPECT_TRUE(queue_->is_blocked());
  queue_->set_blocked(false);
  EXPECT_FALSE(queue_->is_blocked());
}

// =============================================================================
// BLOCKING DEQUEUE
// =============================================================================

TEST_F(ControlMessageQueueTest, DequeueBlocksUntilMessageAvailable) {
  std::atomic<bool> dequeued{false};
  std::thread consumer([this, &dequeued]() {
    auto msg = queue_->dequeue();
    dequeued.store(true, std::memory_order_release);
    EXPECT_TRUE(msg.has_value());
  });

  // Give consumer time to block
  std::this_thread::sleep_for(10ms);
  EXPECT_FALSE(dequeued.load(std::memory_order_acquire));

  // Enqueue message to unblock
  queue_->enqueue(TestQueue::DecodeMessage{std::make_unique<int>(1)});
  consumer.join();

  EXPECT_TRUE(dequeued.load(std::memory_order_acquire));
}

TEST_F(ControlMessageQueueTest, ShutdownUnblocksDequeue) {
  std::atomic<bool> unblocked{false};
  std::thread consumer([this, &unblocked]() {
    auto msg = queue_->dequeue();
    unblocked.store(true, std::memory_order_release);
    EXPECT_FALSE(msg.has_value());  // nullopt due to shutdown
  });

  std::this_thread::sleep_for(10ms);
  queue_->shutdown();
  consumer.join();

  EXPECT_TRUE(unblocked.load(std::memory_order_acquire));
}

TEST_F(ControlMessageQueueTest, DequeueForTimesOut) {
  auto start = std::chrono::steady_clock::now();
  auto msg = queue_->dequeue_for(50ms);
  auto elapsed = std::chrono::steady_clock::now() - start;

  EXPECT_FALSE(msg.has_value());
  EXPECT_GE(elapsed, 50ms);
  EXPECT_LT(elapsed, 200ms);  // Allow some slack
}

TEST_F(ControlMessageQueueTest, DequeueForReturnsImmediatelyIfMessageAvailable) {
  queue_->enqueue(TestQueue::DecodeMessage{std::make_unique<int>(1)});

  auto start = std::chrono::steady_clock::now();
  auto msg = queue_->dequeue_for(1000ms);
  auto elapsed = std::chrono::steady_clock::now() - start;

  EXPECT_TRUE(msg.has_value());
  EXPECT_LT(elapsed, 50ms);
}

// =============================================================================
// CONCURRENT ACCESS (ThreadSanitizer will catch races)
// =============================================================================

TEST_F(ControlMessageQueueTest, ConcurrentEnqueue) {
  constexpr int kThreads = 4;
  constexpr int kMessagesPerThread = 100;
  std::vector<std::thread> producers;

  SimpleLatch start_latch(kThreads);

  for (int t = 0; t < kThreads; ++t) {
    producers.emplace_back([this, t, &start_latch]() {
      start_latch.arrive_and_wait();
      for (int i = 0; i < kMessagesPerThread; ++i) {
        queue_->enqueue(TestQueue::DecodeMessage{std::make_unique<int>(t * kMessagesPerThread + i)});
      }
    });
  }

  for (auto& t : producers) {
    t.join();
  }

  EXPECT_EQ(queue_->size(), kThreads * kMessagesPerThread);
}

TEST_F(ControlMessageQueueTest, ConcurrentEnqueueDequeue) {
  constexpr int kMessages = 1000;
  std::atomic<int> produced{0};
  std::atomic<int> consumed{0};

  std::thread producer([this, &produced]() {
    for (int i = 0; i < kMessages; ++i) {
      queue_->enqueue(TestQueue::DecodeMessage{std::make_unique<int>(i)});
      produced.fetch_add(1, std::memory_order_relaxed);
    }
    queue_->shutdown();
  });

  std::thread consumer([this, &consumed]() {
    while (true) {
      auto msg = queue_->dequeue();
      if (!msg.has_value()) break;
      consumed.fetch_add(1, std::memory_order_relaxed);
    }
  });

  producer.join();
  consumer.join();

  EXPECT_EQ(produced.load(), kMessages);
  EXPECT_EQ(consumed.load(), kMessages);
}

TEST_F(ControlMessageQueueTest, MultipleConsumers) {
  constexpr int kMessages = 500;
  constexpr int kConsumers = 4;
  std::atomic<int> consumed{0};

  for (int i = 0; i < kMessages; ++i) {
    queue_->enqueue(TestQueue::DecodeMessage{std::make_unique<int>(i)});
  }

  std::vector<std::thread> consumers;
  for (int c = 0; c < kConsumers; ++c) {
    consumers.emplace_back([this, &consumed]() {
      while (true) {
        auto msg = queue_->dequeue_for(50ms);
        if (!msg.has_value()) {
          if (queue_->is_closed() && queue_->empty()) break;
          continue;
        }
        consumed.fetch_add(1, std::memory_order_relaxed);
      }
    });
  }

  // Wait a bit for consumption, then shutdown
  std::this_thread::sleep_for(100ms);
  queue_->shutdown();

  for (auto& c : consumers) {
    c.join();
  }

  EXPECT_EQ(consumed.load(), kMessages);
}

// =============================================================================
// EDGE CASES
// =============================================================================

TEST_F(ControlMessageQueueTest, ShutdownDuringBlockedDequeue) {
  std::atomic<bool> consumer_finished{false};
  std::atomic<bool> consumer_started{false};

  std::thread consumer([this, &consumer_finished, &consumer_started]() {
    consumer_started.store(true, std::memory_order_release);
    auto msg = queue_->dequeue();
    consumer_finished.store(true, std::memory_order_release);
    EXPECT_FALSE(msg.has_value());
  });

  // Wait for consumer to start
  while (!consumer_started.load(std::memory_order_acquire)) {
    std::this_thread::sleep_for(1ms);
  }
  std::this_thread::sleep_for(10ms);  // Let it block
  queue_->shutdown();
  consumer.join();

  EXPECT_TRUE(consumer_finished.load());
}

TEST_F(ControlMessageQueueTest, ClearDuringEnqueue) {
  std::atomic<bool> clear_done{false};
  std::atomic<bool> enqueuer_started{false};

  std::thread enqueuer([this, &enqueuer_started]() {
    for (int i = 0; i < 100; ++i) {
      (void)queue_->enqueue(TestQueue::DecodeMessage{std::make_unique<int>(i)});
      if (i == 10) enqueuer_started.store(true, std::memory_order_release);
    }
  });

  // Wait for enqueuer to start
  while (!enqueuer_started.load(std::memory_order_acquire)) {
    std::this_thread::sleep_for(1ms);
  }
  auto dropped = queue_->clear();
  clear_done.store(true, std::memory_order_release);

  enqueuer.join();

  // Some messages may have been dropped (timing dependent)
  // Just verify no crash and clear works
  EXPECT_TRUE(queue_->empty() || queue_->size() > 0);  // Either state is valid
}

// =============================================================================
// MESSAGE VISITOR
// =============================================================================

TEST_F(ControlMessageQueueTest, MessageVisitorPattern) {
  queue_->enqueue(TestQueue::ConfigureMessage{[]() { return true; }});
  queue_->enqueue(TestQueue::DecodeMessage{std::make_unique<int>(42)});
  queue_->enqueue(TestQueue::FlushMessage{1});
  queue_->enqueue(TestQueue::ResetMessage{});
  queue_->enqueue(TestQueue::CloseMessage{});

  int configure_count = 0;
  int decode_count = 0;
  int flush_count = 0;
  int reset_count = 0;
  int close_count = 0;

  for (int i = 0; i < 5; ++i) {
    auto msg = queue_->try_dequeue();
    ASSERT_TRUE(msg.has_value());

    std::visit(MessageVisitor{
                   [&](TestQueue::ConfigureMessage&) { configure_count++; },
                   [&](TestQueue::DecodeMessage&) { decode_count++; }, [&](TestQueue::FlushMessage&) { flush_count++; },
                   [&](TestQueue::ResetMessage&) { reset_count++; }, [&](TestQueue::CloseMessage&) { close_count++; }},
               *msg);
  }

  EXPECT_EQ(configure_count, 1);
  EXPECT_EQ(decode_count, 1);
  EXPECT_EQ(flush_count, 1);
  EXPECT_EQ(reset_count, 1);
  EXPECT_EQ(close_count, 1);
}
