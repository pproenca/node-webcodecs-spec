/**
 * test_ffmpeg_raii.cpp - Unit tests for FFmpeg RAII wrappers
 *
 * Tests smart pointer wrappers, factory functions, state machine,
 * and thread-safe async context.
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

#include "../../src/ffmpeg_raii.h"

using webcodecs::raii::AtomicCodecState;
using webcodecs::raii::AVCodecContextPtr;
using webcodecs::raii::AVFilterGraphPtr;
using webcodecs::raii::AVFramePtr;
using webcodecs::raii::AVMallocBuffer;
using webcodecs::raii::AVPacketPtr;
using webcodecs::raii::CloneAvFrame;
using webcodecs::raii::CloneAvPacket;
using webcodecs::raii::MakeAvCodecContext;
using webcodecs::raii::MakeAvFrame;
using webcodecs::raii::MakeAvPacket;
using webcodecs::raii::MakeFilterGraph;
using webcodecs::raii::MakeSwrContext;
using webcodecs::raii::MakeSwrContextInitialized;
using webcodecs::raii::SafeAsyncContext;
using webcodecs::raii::SwrContextPtr;
using webcodecs::raii::SwsContextPtr;
using std::chrono_literals::operator""ms;

// =============================================================================
// AVFRAME TESTS
// =============================================================================

TEST(AVFrameRAIITest, MakeAVFrameReturnsValidFrame) {
  AVFramePtr frame = MakeAvFrame();
  ASSERT_NE(frame, nullptr);
}

TEST(AVFrameRAIITest, FrameIsFreedOnScopeExit) {
  AVFrame* raw = nullptr;
  {
    AVFramePtr frame = MakeAvFrame();
    ASSERT_NE(frame, nullptr);
    raw = frame.get();
  }
  // Cannot directly test frame was freed, but AddressSanitizer will catch leaks
}

TEST(AVFrameRAIITest, MoveSemantics) {
  AVFramePtr frame1 = MakeAvFrame();
  ASSERT_NE(frame1, nullptr);
  AVFrame* raw = frame1.get();

  AVFramePtr frame2 = std::move(frame1);
  EXPECT_EQ(frame1, nullptr);
  EXPECT_EQ(frame2.get(), raw);
}

TEST(AVFrameRAIITest, NullDeleterIsSafe) {
  AVFramePtr null_frame(nullptr);
  // Destructor should not crash
}

TEST(AVFrameRAIITest, CloneAVFrame) {
  AVFramePtr src = MakeAvFrame();
  ASSERT_NE(src, nullptr);

  src->width = 1920;
  src->height = 1080;
  src->format = AV_PIX_FMT_YUV420P;
  ASSERT_GE(av_frame_get_buffer(src.get(), 32), 0);

  AVFramePtr dst = CloneAvFrame(src.get());
  ASSERT_NE(dst, nullptr);
  EXPECT_EQ(dst->width, 1920);
  EXPECT_EQ(dst->height, 1080);
  EXPECT_EQ(dst->format, AV_PIX_FMT_YUV420P);
  // Should be a reference (same underlying buffer)
  EXPECT_EQ(dst->data[0], src->data[0]);
}

TEST(AVFrameRAIITest, CloneNullReturnsNull) {
  AVFramePtr dst = CloneAvFrame(nullptr);
  EXPECT_EQ(dst, nullptr);
}

// =============================================================================
// AVPACKET TESTS
// =============================================================================

TEST(AVPacketRAIITest, MakeAVPacketReturnsValidPacket) {
  AVPacketPtr packet = MakeAvPacket();
  ASSERT_NE(packet, nullptr);
}

TEST(AVPacketRAIITest, PacketIsFreedOnScopeExit) {
  {
    AVPacketPtr packet = MakeAvPacket();
    ASSERT_NE(packet, nullptr);
    ASSERT_GE(av_new_packet(packet.get(), 1024), 0);
    memset(packet->data, 42, 1024);
  }
  // AddressSanitizer will catch leaks
}

TEST(AVPacketRAIITest, CloneAVPacket) {
  AVPacketPtr src = MakeAvPacket();
  ASSERT_NE(src, nullptr);
  ASSERT_GE(av_new_packet(src.get(), 256), 0);
  src->pts = 12345;
  src->dts = 12300;

  AVPacketPtr dst = CloneAvPacket(src.get());
  ASSERT_NE(dst, nullptr);
  EXPECT_EQ(dst->size, 256);
  EXPECT_EQ(dst->pts, 12345);
  EXPECT_EQ(dst->dts, 12300);
  // Reference to same data
  EXPECT_EQ(dst->data, src->data);
}

TEST(AVPacketRAIITest, CloneNullReturnsNull) {
  AVPacketPtr dst = CloneAvPacket(nullptr);
  EXPECT_EQ(dst, nullptr);
}

// =============================================================================
// AVCODECCONTEXT TESTS
// =============================================================================

TEST(AVCodecContextRAIITest, MakeCodecContextReturnsValid) {
  const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
  if (!codec) {
    GTEST_SKIP() << "H264 decoder not available";
  }

  AVCodecContextPtr ctx = MakeAvCodecContext(codec);
  ASSERT_NE(ctx, nullptr);
}

TEST(AVCodecContextRAIITest, ContextIsFreedOnScopeExit) {
  const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
  if (!codec) {
    GTEST_SKIP() << "H264 decoder not available";
  }

  {
    AVCodecContextPtr ctx = MakeAvCodecContext(codec);
    ASSERT_NE(ctx, nullptr);
  }
  // AddressSanitizer will catch leaks
}

// =============================================================================
// SWSCONTEXT TESTS
// =============================================================================

TEST(SwsContextRAIITest, CreateAndUseSwsContext) {
  SwsContext* raw = sws_getContext(1920, 1080, AV_PIX_FMT_YUV420P, 1280, 720, AV_PIX_FMT_RGB24, SWS_BILINEAR, nullptr,
                                   nullptr, nullptr);

  SwsContextPtr ctx(raw);
  ASSERT_NE(ctx, nullptr);
}

TEST(SwsContextRAIITest, ContextIsFreedOnScopeExit) {
  {
    SwsContext* raw = sws_getContext(640, 480, AV_PIX_FMT_YUV420P, 320, 240, AV_PIX_FMT_RGB24, SWS_BILINEAR, nullptr,
                                     nullptr, nullptr);
    SwsContextPtr ctx(raw);
    ASSERT_NE(ctx, nullptr);
  }
  // AddressSanitizer will catch leaks
}

// =============================================================================
// SWRCONTEXT TESTS
// =============================================================================

TEST(SwrContextRAIITest, MakeSwrContext) {
  SwrContextPtr ctx = MakeSwrContext();
  ASSERT_NE(ctx, nullptr);
}

TEST(SwrContextRAIITest, MakeSwrContextInitialized) {
  AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
  AVChannelLayout mono = AV_CHANNEL_LAYOUT_MONO;

  SwrContextPtr ctx = MakeSwrContextInitialized(&stereo, AV_SAMPLE_FMT_S16, 48000, &mono, AV_SAMPLE_FMT_FLT, 44100);

  ASSERT_NE(ctx, nullptr);
}

TEST(SwrContextRAIITest, ContextIsFreedOnScopeExit) {
  AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;

  {
    SwrContextPtr ctx = MakeSwrContextInitialized(&stereo, AV_SAMPLE_FMT_S16, 48000, &stereo, AV_SAMPLE_FMT_S16, 44100);
    ASSERT_NE(ctx, nullptr);
  }
  // AddressSanitizer will catch leaks
}

// =============================================================================
// AVFILTERGRAPH TESTS
// =============================================================================

TEST(AVFilterGraphRAIITest, MakeFilterGraph) {
  AVFilterGraphPtr graph = MakeFilterGraph();
  ASSERT_NE(graph, nullptr);
}

TEST(AVFilterGraphRAIITest, GraphIsFreedOnScopeExit) {
  {
    AVFilterGraphPtr graph = MakeFilterGraph();
    ASSERT_NE(graph, nullptr);
  }
  // AddressSanitizer will catch leaks
}

// =============================================================================
// AVMALLOCBUFFER TESTS
// =============================================================================

TEST(AVMallocBufferTest, AllocateBuffer) {
  AVMallocBuffer buf(1024);
  ASSERT_TRUE(static_cast<bool>(buf));
  EXPECT_NE(buf.data(), nullptr);
  EXPECT_EQ(buf.size(), 1024);
}

TEST(AVMallocBufferTest, DefaultConstructorCreatesEmpty) {
  AVMallocBuffer buf;
  EXPECT_FALSE(static_cast<bool>(buf));
  EXPECT_EQ(buf.data(), nullptr);
  EXPECT_EQ(buf.size(), 0);
}

TEST(AVMallocBufferTest, MoveSemantics) {
  AVMallocBuffer buf1(512);
  ASSERT_TRUE(static_cast<bool>(buf1));
  uint8_t* data = buf1.data();

  AVMallocBuffer buf2 = std::move(buf1);
  EXPECT_FALSE(static_cast<bool>(buf1));
  EXPECT_EQ(buf1.data(), nullptr);
  EXPECT_EQ(buf2.data(), data);
  EXPECT_EQ(buf2.size(), 512);
}

TEST(AVMallocBufferTest, MoveAssignment) {
  AVMallocBuffer buf1(256);
  AVMallocBuffer buf2(512);

  buf2 = std::move(buf1);
  EXPECT_FALSE(static_cast<bool>(buf1));
  EXPECT_EQ(buf2.size(), 256);
}

TEST(AVMallocBufferTest, Release) {
  AVMallocBuffer buf(128);
  uint8_t* data = buf.data();

  uint8_t* released = buf.release();
  EXPECT_EQ(released, data);
  EXPECT_EQ(buf.data(), nullptr);
  EXPECT_EQ(buf.size(), 0);

  // Manual cleanup since we released
  av_free(released);
}

TEST(AVMallocBufferTest, BufferIsFreedOnScopeExit) {
  {
    AVMallocBuffer buf(4096);
    ASSERT_TRUE(static_cast<bool>(buf));
    memset(buf.data(), 42, buf.size());
  }
  // AddressSanitizer will catch leaks
}

// =============================================================================
// ATOMICCODECSTATE TESTS
// =============================================================================

TEST(AtomicCodecStateTest, InitialStateIsUnconfigured) {
  AtomicCodecState state;
  EXPECT_EQ(state.get(), AtomicCodecState::State::Unconfigured);
  EXPECT_FALSE(state.IsConfigured());
  EXPECT_FALSE(state.IsClosed());
}

TEST(AtomicCodecStateTest, TransitionUnconfiguredToConfigured) {
  AtomicCodecState state;
  bool success = state.transition(AtomicCodecState::State::Unconfigured, AtomicCodecState::State::Configured);
  EXPECT_TRUE(success);
  EXPECT_TRUE(state.IsConfigured());
}

TEST(AtomicCodecStateTest, TransitionFailsOnWrongState) {
  AtomicCodecState state;
  // Try to transition from Configured when actually Unconfigured
  bool success = state.transition(AtomicCodecState::State::Configured, AtomicCodecState::State::Closed);
  EXPECT_FALSE(success);
  EXPECT_EQ(state.get(), AtomicCodecState::State::Unconfigured);
}

TEST(AtomicCodecStateTest, CloseAlwaysSucceeds) {
  AtomicCodecState state;
  state.Close();
  EXPECT_TRUE(state.IsClosed());

  // Close again - should stay closed
  state.Close();
  EXPECT_TRUE(state.IsClosed());
}

TEST(AtomicCodecStateTest, ToString) {
  AtomicCodecState state;
  EXPECT_STREQ(state.ToString(), "unconfigured");

  state.transition(AtomicCodecState::State::Unconfigured, AtomicCodecState::State::Configured);
  EXPECT_STREQ(state.ToString(), "configured");

  state.Close();
  EXPECT_STREQ(state.ToString(), "closed");
}

TEST(AtomicCodecStateTest, ConcurrentTransitions) {
  AtomicCodecState state;

  constexpr int kThreads = 8;
  std::atomic<int> success_count{0};
  std::vector<std::thread> threads;
  SimpleLatch start_latch(kThreads);

  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([&state, &success_count, &start_latch]() {
      start_latch.arrive_and_wait();
      if (state.transition(AtomicCodecState::State::Unconfigured, AtomicCodecState::State::Configured)) {
        success_count.fetch_add(1, std::memory_order_relaxed);
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  // Exactly one thread should succeed
  EXPECT_EQ(success_count.load(), 1);
  EXPECT_TRUE(state.IsConfigured());
}

TEST(AtomicCodecStateTest, ConcurrentClose) {
  AtomicCodecState state;
  state.transition(AtomicCodecState::State::Unconfigured, AtomicCodecState::State::Configured);

  constexpr int kThreads = 4;
  std::vector<std::thread> threads;
  SimpleLatch start_latch(kThreads);

  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([&state, &start_latch]() {
      start_latch.arrive_and_wait();
      state.Close();
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  EXPECT_TRUE(state.IsClosed());
}

// =============================================================================
// SAFEASYNCCONTEXT TESTS
// =============================================================================

// Mock TSFN for testing SafeAsyncContext
struct MockTSFN {
  std::atomic<bool> released{false};
  void Release() { released.store(true); }
  explicit operator bool() const { return !released.load(); }
};

TEST(SafeAsyncContextTest, ConstructionAndDestruction) {
  {
    SafeAsyncContext<MockTSFN> ctx;
    EXPECT_FALSE(ctx.ShouldExit());
  }
  // Destructor should complete without error
}

TEST(SafeAsyncContextTest, ShouldExitSignaling) {
  SafeAsyncContext<MockTSFN> ctx;
  EXPECT_FALSE(ctx.ShouldExit());

  ctx.shouldExit.store(true, std::memory_order_release);
  EXPECT_TRUE(ctx.ShouldExit());
}

TEST(SafeAsyncContextTest, TSFNReleasedOnDestruction) {
  MockTSFN* tsfn_ptr = nullptr;
  {
    SafeAsyncContext<MockTSFN> ctx;
    tsfn_ptr = &ctx.tsfn;
    EXPECT_FALSE(tsfn_ptr->released.load());
  }
  // Cannot access tsfn_ptr after ctx destroyed - this is expected
  // The test verifies destructor runs without crash
}

TEST(SafeAsyncContextTest, WorkerThreadJoinedOnDestruction) {
  std::atomic<bool> worker_started{false};
  std::atomic<bool> worker_exited{false};

  {
    SafeAsyncContext<MockTSFN> ctx;

    ctx.workerThread = std::thread([&ctx, &worker_started, &worker_exited]() {
      worker_started.store(true, std::memory_order_release);
      while (!ctx.ShouldExit()) {
        std::this_thread::sleep_for(1ms);
      }
      worker_exited.store(true, std::memory_order_release);
    });

    // Wait for worker to start
    while (!worker_started.load(std::memory_order_acquire)) {
      std::this_thread::sleep_for(1ms);
    }
  }

  // After ctx destruction, worker should have exited
  EXPECT_TRUE(worker_exited.load(std::memory_order_acquire));
}

TEST(SafeAsyncContextTest, LockAcquiresMutex) {
  SafeAsyncContext<MockTSFN> ctx;

  std::atomic<bool> locked{false};
  std::atomic<bool> waiting{false};

  std::thread t1([&ctx, &locked, &waiting]() {
    auto lock = ctx.lock();
    locked.store(true, std::memory_order_release);
    while (!waiting.load(std::memory_order_acquire)) {
      std::this_thread::sleep_for(1ms);
    }
    std::this_thread::sleep_for(10ms);
  });

  // Wait for t1 to acquire lock
  while (!locked.load(std::memory_order_acquire)) {
    std::this_thread::sleep_for(1ms);
  }

  std::atomic<bool> t2_acquired{false};
  std::thread t2([&ctx, &t2_acquired]() {
    auto lock = ctx.lock();  // Should block
    t2_acquired.store(true, std::memory_order_release);
  });

  // Signal t1 we're waiting
  waiting.store(true, std::memory_order_release);

  // Wait a bit - t2 should still be blocked
  std::this_thread::sleep_for(5ms);
  EXPECT_FALSE(t2_acquired.load(std::memory_order_acquire));

  t1.join();
  t2.join();

  // After t1 releases, t2 should have acquired
  EXPECT_TRUE(t2_acquired.load(std::memory_order_acquire));
}

// =============================================================================
// EDGE CASES AND STRESS TESTS
// =============================================================================

TEST(FFmpegRAIITest, MultipleFramesWithBuffers) {
  std::vector<AVFramePtr> frames;
  for (int i = 0; i < 10; ++i) {
    AVFramePtr frame = MakeAvFrame();
    ASSERT_NE(frame, nullptr);
    frame->width = 1920;
    frame->height = 1080;
    frame->format = AV_PIX_FMT_YUV420P;
    ASSERT_GE(av_frame_get_buffer(frame.get(), 32), 0);
    frames.push_back(std::move(frame));
  }
  // All frames freed when vector goes out of scope
}

TEST(FFmpegRAIITest, MultiplePacketsWithData) {
  std::vector<AVPacketPtr> packets;
  for (int i = 0; i < 10; ++i) {
    AVPacketPtr packet = MakeAvPacket();
    ASSERT_NE(packet, nullptr);
    ASSERT_GE(av_new_packet(packet.get(), 1024), 0);
    packets.push_back(std::move(packet));
  }
  // All packets freed when vector goes out of scope
}

TEST(FFmpegRAIITest, ConcurrentFrameAllocation) {
  constexpr int kThreads = 8;
  constexpr int kFramesPerThread = 50;
  std::vector<std::thread> threads;
  SimpleLatch start_latch(kThreads);

  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([&start_latch]() {
      start_latch.arrive_and_wait();
      for (int i = 0; i < kFramesPerThread; ++i) {
        AVFramePtr frame = MakeAvFrame();
        EXPECT_NE(frame, nullptr);
        frame->width = 640;
        frame->height = 480;
        frame->format = AV_PIX_FMT_YUV420P;
        av_frame_get_buffer(frame.get(), 32);
        // Frame freed on loop iteration
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }
}

TEST(FFmpegRAIITest, ConcurrentPacketAllocation) {
  constexpr int kThreads = 8;
  constexpr int kPacketsPerThread = 50;
  std::vector<std::thread> threads;
  SimpleLatch start_latch(kThreads);

  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([&start_latch]() {
      start_latch.arrive_and_wait();
      for (int i = 0; i < kPacketsPerThread; ++i) {
        AVPacketPtr packet = MakeAvPacket();
        EXPECT_NE(packet, nullptr);
        av_new_packet(packet.get(), 256);
        // Packet freed on loop iteration
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }
}
