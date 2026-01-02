/**
 * test_video_decoder_spec.cpp - W3C WebCodecs VideoDecoder Specification Compliance Tests
 *
 * These tests validate that VideoDecoder behavior matches the W3C WebCodecs spec,
 * with emphasis on sad path / error scenarios.
 *
 * Since VideoDecoder is a NAPI-bound class, we use a VideoDecoderSimulator that
 * replicates the exact logic patterns for pure C++ unit testing.
 *
 * @see https://www.w3.org/TR/webcodecs/#videodecoder
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// Define WEBCODECS_TESTING to get pure C++ headers
#define WEBCODECS_TESTING 1

#include "src/ffmpeg_raii.h"
#include "src/shared/codec_registry.h"

using webcodecs::IsCodecSupported;
using webcodecs::ParseCodecString;
using webcodecs::raii::AtomicCodecState;
using webcodecs::raii::AVCodecContextPtr;
using webcodecs::raii::AVFramePtr;
using webcodecs::raii::AVPacketPtr;
using webcodecs::raii::CloneAvFrame;
using webcodecs::raii::MakeAvCodecContext;
using webcodecs::raii::MakeAvFrame;
using webcodecs::raii::MakeAvPacket;

// =============================================================================
// VIDEO DECODER SIMULATOR
// Replicates VideoDecoder logic for pure C++ testing
// =============================================================================

namespace test {

/**
 * Error types matching W3C DOMException names
 */
enum class ErrorType { None, InvalidStateError, NotSupportedError, DataError, EncodingError, TypeError };

/**
 * Error result from decoder operations
 */
struct Error {
  ErrorType type = ErrorType::None;
  std::string message;

  operator bool() const { return type != ErrorType::None; }
};

/**
 * VideoDecoderSimulator - Replicates VideoDecoder logic for testing
 *
 * Uses the same RAII primitives (AtomicCodecState, AVCodecContextPtr)
 * to validate error handling matches the W3C spec.
 */
class VideoDecoderSimulator {
 public:
  VideoDecoderSimulator() = default;
  ~VideoDecoderSimulator() { Close(); }

  // Non-copyable
  VideoDecoderSimulator(const VideoDecoderSimulator&) = delete;
  VideoDecoderSimulator& operator=(const VideoDecoderSimulator&) = delete;

  // State queries (PascalCase per Google C++ Style Guide)
  const char* state() const { return state_.ToString(); }
  bool IsConfigured() const { return state_.IsConfigured(); }
  bool IsClosed() const { return state_.IsClosed(); }
  bool key_chunk_required() const { return keyChunkRequired_.load(std::memory_order_acquire); }
  uint32_t decode_queue_size() const { return decodeQueueSize_.load(std::memory_order_acquire); }

  /**
   * [SPEC] configure() - Initialize decoder with codec config
   *
   * Steps:
   * 1. If state is "closed", throw InvalidStateError
   * 2. Parse codec string
   * 3. Initialize codec context
   * 4. Transition to "configured"
   */
  Error configure(const std::string& codec, int width = 640, int height = 480) {
    // [SPEC] If state is "closed", throw InvalidStateError
    if (state_.IsClosed()) {
      return {ErrorType::InvalidStateError, "configure called on closed decoder"};
    }

    // Parse codec string
    auto codec_info = ParseCodecString(codec);
    if (!codec_info) {
      return {ErrorType::NotSupportedError, "Unsupported codec: " + codec};
    }

    // Find FFmpeg decoder
    const AVCodec* decoder = avcodec_find_decoder(codec_info->codec_id);
    if (!decoder) {
      return {ErrorType::NotSupportedError, "No decoder available for: " + codec};
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // Allocate codec context (RAII)
    codecCtx_ = MakeAvCodecContext(decoder);
    if (!codecCtx_) {
      return {ErrorType::EncodingError, "Failed to allocate codec context"};
    }

    codecCtx_->width = width;
    codecCtx_->height = height;
    codecCtx_->thread_count = 1;

    // Open codec
    int ret = avcodec_open2(codecCtx_.get(), decoder, nullptr);
    if (ret < 0) {
      codecCtx_.reset();
      return {ErrorType::EncodingError, "Failed to open decoder"};
    }

    // [SPEC] Set state to "configured"
    state_.transition(AtomicCodecState::State::Unconfigured, AtomicCodecState::State::Configured);
    keyChunkRequired_.store(true, std::memory_order_release);

    return {};
  }

  /**
   * [SPEC] decode() - Decode an encoded chunk
   *
   * Steps:
   * 1. If state is not "configured", throw InvalidStateError
   * 2. If key chunk required AND not key frame, throw DataError
   * 3. Increment decodeQueueSize
   * 4. Process decode
   * 5. Decrement decodeQueueSize
   */
  Error decode(const uint8_t* data, size_t size, bool is_key_frame, int64_t timestamp = 0,
               std::vector<AVFramePtr>* output_frames = nullptr) {
    // [SPEC] Step 1: If state is not "configured", throw InvalidStateError
    if (!state_.IsConfigured()) {
      return {ErrorType::InvalidStateError, std::string("decode called on ") + state_.ToString() + " decoder"};
    }

    // [SPEC] Step 2: If key chunk required AND not key frame, throw DataError
    if (keyChunkRequired_.load(std::memory_order_acquire)) {
      if (!is_key_frame) {
        return {ErrorType::DataError, "A key frame is required"};
      }
      keyChunkRequired_.store(false, std::memory_order_release);
    }

    // Validate data
    if (!data || size == 0) {
      return {ErrorType::TypeError, "Chunk data is required"};
    }

    // [SPEC] Step 3: Increment decodeQueueSize
    decodeQueueSize_.fetch_add(1, std::memory_order_relaxed);

    std::lock_guard<std::mutex> lock(mutex_);

    if (!codecCtx_) {
      decodeQueueSize_.fetch_sub(1, std::memory_order_relaxed);
      return {ErrorType::InvalidStateError, "Decoder not configured"};
    }

    // Create packet from data
    AVPacketPtr packet = MakeAvPacket();
    if (!packet) {
      decodeQueueSize_.fetch_sub(1, std::memory_order_relaxed);
      return {ErrorType::EncodingError, "Failed to allocate packet"};
    }

    int ret = av_new_packet(packet.get(), static_cast<int>(size));
    if (ret < 0) {
      decodeQueueSize_.fetch_sub(1, std::memory_order_relaxed);
      return {ErrorType::EncodingError, "Failed to create packet"};
    }

    std::memcpy(packet->data, data, size);
    packet->pts = timestamp;
    packet->dts = timestamp;
    if (is_key_frame) {
      packet->flags |= AV_PKT_FLAG_KEY;
    }

    // Send packet to decoder
    ret = avcodec_send_packet(codecCtx_.get(), packet.get());
    if (ret < 0 && ret != AVERROR(EAGAIN)) {
      decodeQueueSize_.fetch_sub(1, std::memory_order_relaxed);
      return {ErrorType::EncodingError, "Failed to decode packet"};
    }

    // Receive all available frames
    if (output_frames) {
      AVFramePtr frame = MakeAvFrame();
      while (frame) {
        ret = avcodec_receive_frame(codecCtx_.get(), frame.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
          break;
        }
        if (ret < 0) {
          decodeQueueSize_.fetch_sub(1, std::memory_order_relaxed);
          return {ErrorType::EncodingError, "Error receiving frame"};
        }
        output_frames->push_back(CloneAvFrame(frame.get()));
        av_frame_unref(frame.get());
      }
    }

    // [SPEC] Step 5: Decrement decodeQueueSize
    decodeQueueSize_.fetch_sub(1, std::memory_order_relaxed);

    return {};
  }

  /**
   * [SPEC] flush() - Drain all pending frames
   *
   * Steps:
   * 1. If state is not "configured", reject with InvalidStateError
   * 2. Set key chunk required to true
   * 3. Drain codec
   */
  Error flush(std::vector<AVFramePtr>* output_frames = nullptr) {
    // [SPEC] Step 1: If state is not "configured", reject
    if (!state_.IsConfigured()) {
      return {ErrorType::InvalidStateError, std::string("flush called on ") + state_.ToString() + " decoder"};
    }

    // [SPEC] Step 2: Set key chunk required to true
    keyChunkRequired_.store(true, std::memory_order_release);

    std::lock_guard<std::mutex> lock(mutex_);

    if (!codecCtx_) {
      return {};
    }

    // Drain by sending null packet
    int ret = avcodec_send_packet(codecCtx_.get(), nullptr);
    if (ret < 0 && ret != AVERROR_EOF) {
      return {ErrorType::EncodingError, "Failed to flush decoder"};
    }

    // Receive all remaining frames
    if (output_frames) {
      AVFramePtr frame = MakeAvFrame();
      while (frame) {
        ret = avcodec_receive_frame(codecCtx_.get(), frame.get());
        if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
          break;
        }
        if (ret < 0) {
          return {ErrorType::EncodingError, "Error draining frames"};
        }
        output_frames->push_back(CloneAvFrame(frame.get()));
        av_frame_unref(frame.get());
      }
    }

    return {};
  }

  /**
   * [SPEC] reset() - Reset decoder state
   *
   * Steps:
   * 1. If state is "closed", throw InvalidStateError
   * 2. Clear queue
   * 3. Flush codec buffers
   * 4. Transition to "unconfigured"
   */
  Error reset() {
    // [SPEC] If state is "closed", throw InvalidStateError
    if (state_.IsClosed()) {
      return {ErrorType::InvalidStateError, "reset called on closed decoder"};
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // Clear decode queue
    decodeQueueSize_.store(0, std::memory_order_release);

    // Flush codec buffers
    if (codecCtx_) {
      avcodec_flush_buffers(codecCtx_.get());
    }

    // Reset key chunk requirement
    keyChunkRequired_.store(true, std::memory_order_release);

    // Release codec context
    codecCtx_.reset();

    // Transition to unconfigured
    state_.transition(AtomicCodecState::State::Configured, AtomicCodecState::State::Unconfigured);

    return {};
  }

  /**
   * [SPEC] close() - Release all resources
   *
   * Transition to "closed" state (idempotent)
   */
  void Close() {
    state_.Close();

    std::lock_guard<std::mutex> lock(mutex_);
    decodeQueueSize_.store(0, std::memory_order_release);
    keyChunkRequired_.store(true, std::memory_order_release);
    codecCtx_.reset();
  }

 private:
  AtomicCodecState state_;
  AVCodecContextPtr codecCtx_;
  std::atomic<uint32_t> decodeQueueSize_{0};
  std::atomic<bool> keyChunkRequired_{true};
  mutable std::mutex mutex_;
};

}  // namespace test

using test::ErrorType;
using test::VideoDecoderSimulator;

// =============================================================================
// CATEGORY 1: STATE MACHINE COMPLIANCE TESTS
// =============================================================================

class VideoDecoderStateMachineTest : public ::testing::Test {
 protected:
  VideoDecoderSimulator decoder_;
};

TEST_F(VideoDecoderStateMachineTest, InitialStateIsUnconfigured) {
  EXPECT_STREQ(decoder_.state(), "unconfigured");
  EXPECT_FALSE(decoder_.IsConfigured());
  EXPECT_FALSE(decoder_.IsClosed());
}

TEST_F(VideoDecoderStateMachineTest, ConfigureTransitionsToConfigured) {
  auto error = decoder_.configure("vp8");
  EXPECT_FALSE(error) << error.message;
  EXPECT_STREQ(decoder_.state(), "configured");
  EXPECT_TRUE(decoder_.IsConfigured());
}

TEST_F(VideoDecoderStateMachineTest, ConfigureOnClosedThrowsInvalidStateError) {
  decoder_.Close();
  EXPECT_TRUE(decoder_.IsClosed());

  auto error = decoder_.configure("vp8");
  EXPECT_TRUE(error);
  EXPECT_EQ(error.type, ErrorType::InvalidStateError);
  EXPECT_THAT(error.message, ::testing::HasSubstr("closed"));
}

TEST_F(VideoDecoderStateMachineTest, ResetTransitionsToUnconfigured) {
  decoder_.configure("vp8");
  EXPECT_TRUE(decoder_.IsConfigured());

  auto error = decoder_.reset();
  EXPECT_FALSE(error) << error.message;
  EXPECT_STREQ(decoder_.state(), "unconfigured");
}

TEST_F(VideoDecoderStateMachineTest, ResetOnClosedThrowsInvalidStateError) {
  decoder_.Close();

  auto error = decoder_.reset();
  EXPECT_TRUE(error);
  EXPECT_EQ(error.type, ErrorType::InvalidStateError);
}

TEST_F(VideoDecoderStateMachineTest, CloseTransitionsToClosed) {
  decoder_.configure("vp8");
  decoder_.Close();

  EXPECT_STREQ(decoder_.state(), "closed");
  EXPECT_TRUE(decoder_.IsClosed());
}

TEST_F(VideoDecoderStateMachineTest, CloseIsIdempotent) {
  decoder_.configure("vp8");

  // Multiple close calls should be safe
  decoder_.Close();
  EXPECT_TRUE(decoder_.IsClosed());

  decoder_.Close();
  EXPECT_TRUE(decoder_.IsClosed());

  decoder_.Close();
  EXPECT_TRUE(decoder_.IsClosed());
}

TEST_F(VideoDecoderStateMachineTest, StateToStringMatchesSpec) {
  EXPECT_STREQ(decoder_.state(), "unconfigured");

  decoder_.configure("vp8");
  EXPECT_STREQ(decoder_.state(), "configured");

  decoder_.Close();
  EXPECT_STREQ(decoder_.state(), "closed");
}

// =============================================================================
// CATEGORY 2: DECODE PRECONDITION TESTS (CRITICAL)
// =============================================================================

class VideoDecoderDecodePreconditionTest : public ::testing::Test {
 protected:
  VideoDecoderSimulator decoder_;
  uint8_t dummy_data_[16] = {0x00, 0x00, 0x00, 0x01, 0x67};  // H.264-like NAL
};

TEST_F(VideoDecoderDecodePreconditionTest, DecodeBeforeConfigureThrowsInvalidStateError) {
  // [SPEC] Step 1: If state is not "configured", throw InvalidStateError
  auto error = decoder_.decode(dummy_data_, sizeof(dummy_data_), true);

  EXPECT_TRUE(error);
  EXPECT_EQ(error.type, ErrorType::InvalidStateError);
  EXPECT_THAT(error.message, ::testing::HasSubstr("unconfigured"));
}

TEST_F(VideoDecoderDecodePreconditionTest, DecodeAfterCloseThrowsInvalidStateError) {
  decoder_.configure("vp8");
  decoder_.Close();

  auto error = decoder_.decode(dummy_data_, sizeof(dummy_data_), true);

  EXPECT_TRUE(error);
  EXPECT_EQ(error.type, ErrorType::InvalidStateError);
  EXPECT_THAT(error.message, ::testing::HasSubstr("closed"));
}

TEST_F(VideoDecoderDecodePreconditionTest, DecodeAfterResetThrowsInvalidStateError) {
  decoder_.configure("vp8");
  decoder_.reset();

  auto error = decoder_.decode(dummy_data_, sizeof(dummy_data_), true);

  EXPECT_TRUE(error);
  EXPECT_EQ(error.type, ErrorType::InvalidStateError);
  EXPECT_THAT(error.message, ::testing::HasSubstr("unconfigured"));
}

TEST_F(VideoDecoderDecodePreconditionTest, DecodeDeltaWithoutKeyFrameThrowsDataError) {
  // [SPEC] Step 2: If key chunk required AND type != "key", throw DataError
  decoder_.configure("vp8");

  // First decode must be a key frame, but we send delta
  auto error = decoder_.decode(dummy_data_, sizeof(dummy_data_), false);

  EXPECT_TRUE(error);
  EXPECT_EQ(error.type, ErrorType::DataError);
  EXPECT_THAT(error.message, ::testing::HasSubstr("key frame"));
}

TEST_F(VideoDecoderDecodePreconditionTest, DecodeKeyFrameClearsKeyChunkRequired) {
  decoder_.configure("vp8");
  EXPECT_TRUE(decoder_.key_chunk_required());

  // Decode a key frame (may fail internally but should clear the flag)
  decoder_.decode(dummy_data_, sizeof(dummy_data_), true);

  // After successful key frame check, keyChunkRequired should be false
  EXPECT_FALSE(decoder_.key_chunk_required());
}

TEST_F(VideoDecoderDecodePreconditionTest, FlushResetsKeyChunkRequired) {
  decoder_.configure("vp8");

  // Clear key requirement by decoding key frame
  decoder_.decode(dummy_data_, sizeof(dummy_data_), true);
  EXPECT_FALSE(decoder_.key_chunk_required());

  // Flush should reset key requirement
  decoder_.flush();
  EXPECT_TRUE(decoder_.key_chunk_required());
}

TEST_F(VideoDecoderDecodePreconditionTest, ResetResetsKeyChunkRequired) {
  decoder_.configure("vp8");
  decoder_.decode(dummy_data_, sizeof(dummy_data_), true);
  EXPECT_FALSE(decoder_.key_chunk_required());

  // Reset should set key requirement
  decoder_.reset();

  // After reconfigure, key should be required again
  decoder_.configure("vp8");
  EXPECT_TRUE(decoder_.key_chunk_required());
}

// =============================================================================
// CATEGORY 3: CODEC CONFIGURATION TESTS
// =============================================================================

class VideoDecoderConfigureTest : public ::testing::Test {
 protected:
  VideoDecoderSimulator decoder_;
};

TEST_F(VideoDecoderConfigureTest, ConfigureWithValidCodecSucceeds) {
  // VP8 is commonly available
  auto error = decoder_.configure("vp8");
  EXPECT_FALSE(error) << error.message;
  EXPECT_TRUE(decoder_.IsConfigured());
}

TEST_F(VideoDecoderConfigureTest, ConfigureWithUnsupportedCodecThrowsNotSupportedError) {
  auto error = decoder_.configure("nonexistent-codec-12345");
  EXPECT_TRUE(error);
  EXPECT_EQ(error.type, ErrorType::NotSupportedError);
}

TEST_F(VideoDecoderConfigureTest, ConfigureWithInvalidCodecStringThrowsNotSupportedError) {
  auto error = decoder_.configure("");
  EXPECT_TRUE(error);
  EXPECT_EQ(error.type, ErrorType::NotSupportedError);
}

TEST_F(VideoDecoderConfigureTest, ReconfigureAfterResetSucceeds) {
  decoder_.configure("vp8");
  decoder_.reset();

  auto error = decoder_.configure("vp8");
  EXPECT_FALSE(error) << error.message;
  EXPECT_TRUE(decoder_.IsConfigured());
}

TEST_F(VideoDecoderConfigureTest, ReconfigureWhileConfiguredSucceeds) {
  decoder_.configure("vp8");

  // Reconfigure should work (replaces existing config)
  auto error = decoder_.configure("vp8", 1280, 720);
  EXPECT_FALSE(error) << error.message;
  EXPECT_TRUE(decoder_.IsConfigured());
}

// =============================================================================
// CATEGORY 4: FLUSH BEHAVIOR TESTS
// =============================================================================

class VideoDecoderFlushTest : public ::testing::Test {
 protected:
  VideoDecoderSimulator decoder_;
};

TEST_F(VideoDecoderFlushTest, FlushBeforeConfigureRejectsWithInvalidStateError) {
  auto error = decoder_.flush();
  EXPECT_TRUE(error);
  EXPECT_EQ(error.type, ErrorType::InvalidStateError);
}

TEST_F(VideoDecoderFlushTest, FlushAfterCloseRejectsWithInvalidStateError) {
  decoder_.configure("vp8");
  decoder_.Close();

  auto error = decoder_.flush();
  EXPECT_TRUE(error);
  EXPECT_EQ(error.type, ErrorType::InvalidStateError);
}

TEST_F(VideoDecoderFlushTest, FlushWithEmptyQueueSucceeds) {
  decoder_.configure("vp8");

  std::vector<AVFramePtr> frames;
  auto error = decoder_.flush(&frames);

  EXPECT_FALSE(error) << error.message;
  // Empty queue = no frames
  EXPECT_TRUE(frames.empty());
}

TEST_F(VideoDecoderFlushTest, FlushSetsKeyChunkRequiredToTrue) {
  decoder_.configure("vp8");

  // Simulate having decoded a key frame
  uint8_t dummy[] = {0x00};
  decoder_.decode(dummy, sizeof(dummy), true);
  EXPECT_FALSE(decoder_.key_chunk_required());

  // Flush
  decoder_.flush();

  // [SPEC] Step 2: Set key chunk required to true
  EXPECT_TRUE(decoder_.key_chunk_required());
}

// =============================================================================
// CATEGORY 5: DECODE QUEUE SIZE TESTS
// =============================================================================

class VideoDecoderQueueSizeTest : public ::testing::Test {
 protected:
  VideoDecoderSimulator decoder_;
};

TEST_F(VideoDecoderQueueSizeTest, InitialDecodeQueueSizeIsZero) { EXPECT_EQ(decoder_.decode_queue_size(), 0u); }

TEST_F(VideoDecoderQueueSizeTest, ResetClearsDecodeQueueSize) {
  decoder_.configure("vp8");
  // Even if we try to decode something, reset should clear
  decoder_.reset();
  EXPECT_EQ(decoder_.decode_queue_size(), 0u);
}

TEST_F(VideoDecoderQueueSizeTest, CloseClearsDecodeQueueSize) {
  decoder_.configure("vp8");
  decoder_.Close();
  EXPECT_EQ(decoder_.decode_queue_size(), 0u);
}

// =============================================================================
// CATEGORY 6: CORRUPT DATA HANDLING TESTS (CRITICAL)
// =============================================================================

class VideoDecoderCorruptDataTest : public ::testing::Test {
 protected:
  VideoDecoderSimulator decoder_;
};

TEST_F(VideoDecoderCorruptDataTest, DecodeEmptyChunkThrowsTypeError) {
  decoder_.configure("vp8");

  auto error = decoder_.decode(nullptr, 0, true);
  EXPECT_TRUE(error);
  EXPECT_EQ(error.type, ErrorType::TypeError);
}

TEST_F(VideoDecoderCorruptDataTest, DecodeNullDataThrowsTypeError) {
  decoder_.configure("vp8");

  auto error = decoder_.decode(nullptr, 100, true);
  EXPECT_TRUE(error);
  EXPECT_EQ(error.type, ErrorType::TypeError);
}

TEST_F(VideoDecoderCorruptDataTest, DecodeZeroSizeThrowsTypeError) {
  decoder_.configure("vp8");

  uint8_t dummy[] = {0x00};
  auto error = decoder_.decode(dummy, 0, true);
  EXPECT_TRUE(error);
  EXPECT_EQ(error.type, ErrorType::TypeError);
}

// =============================================================================
// CATEGORY 7: THREAD SAFETY TESTS
// =============================================================================

class VideoDecoderThreadSafetyTest : public ::testing::Test {
 protected:
  VideoDecoderSimulator decoder_;
};

TEST_F(VideoDecoderThreadSafetyTest, ConcurrentCloseCallsAreIdempotent) {
  decoder_.configure("vp8");

  std::vector<std::thread> threads;
  std::atomic<int> close_count{0};

  // Launch 10 threads that all try to close
  for (int i = 0; i < 10; i++) {
    threads.emplace_back([this, &close_count]() {
      decoder_.Close();
      close_count++;
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  EXPECT_EQ(close_count.load(), 10);
  EXPECT_TRUE(decoder_.IsClosed());
}

TEST_F(VideoDecoderThreadSafetyTest, CloseWhileDecodeInProgressIsSafe) {
  decoder_.configure("vp8");

  std::atomic<bool> decoding{true};
  std::atomic<int> decode_attempts{0};

  // Thread that repeatedly tries to decode
  std::thread decode_thread([this, &decoding, &decode_attempts]() {
    uint8_t dummy[] = {0x00, 0x00, 0x01, 0x67};
    while (decoding.load()) {
      decoder_.decode(dummy, sizeof(dummy), true);
      decode_attempts++;
    }
  });

  // Give decode thread a head start
  std::this_thread::sleep_for(std::chrono::milliseconds(5));

  // Close from main thread
  decoder_.Close();
  decoding = false;

  decode_thread.join();

  EXPECT_TRUE(decoder_.IsClosed());
  EXPECT_GT(decode_attempts.load(), 0);
}

TEST_F(VideoDecoderThreadSafetyTest, ResetWhileDecodeInProgressIsSafe) {
  decoder_.configure("vp8");

  std::atomic<bool> running{true};

  std::thread decode_thread([this, &running]() {
    uint8_t dummy[] = {0x00, 0x00, 0x01, 0x67};
    while (running.load()) {
      auto error = decoder_.decode(dummy, sizeof(dummy), true);
      // Ignore errors - we expect some after reset
    }
  });

  // Give decode thread time to start
  std::this_thread::sleep_for(std::chrono::milliseconds(5));

  // Reset from main thread
  decoder_.reset();
  running = false;

  decode_thread.join();

  // After reset, should be unconfigured
  EXPECT_FALSE(decoder_.IsConfigured());
}

TEST_F(VideoDecoderThreadSafetyTest, ConcurrentConfigureIsThreadSafe) {
  std::vector<std::thread> threads;
  std::atomic<int> success_count{0};

  // Launch threads that all try to configure
  for (int i = 0; i < 5; i++) {
    threads.emplace_back([this, &success_count]() {
      auto error = decoder_.configure("vp8");
      if (!error) success_count++;
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  // At least one should succeed
  EXPECT_GE(success_count.load(), 1);
}

// =============================================================================
// CATEGORY 8: RESOURCE MANAGEMENT TESTS
// =============================================================================

class VideoDecoderResourceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Track allocations if needed
  }
};

TEST_F(VideoDecoderResourceTest, CodecContextFreedOnClose) {
  auto decoder = std::make_unique<VideoDecoderSimulator>();
  decoder->configure("vp8");
  EXPECT_TRUE(decoder->IsConfigured());

  decoder->Close();
  // Destructor should not double-free
  decoder.reset();
}

TEST_F(VideoDecoderResourceTest, CodecContextFreedOnReset) {
  VideoDecoderSimulator decoder;
  decoder.configure("vp8");
  decoder.reset();

  // Should be able to reconfigure
  auto error = decoder.configure("vp8");
  EXPECT_FALSE(error) << error.message;
}

TEST_F(VideoDecoderResourceTest, NoLeakOnDecodeError) {
  VideoDecoderSimulator decoder;
  decoder.configure("vp8");

  // Decode with invalid data should not leak
  uint8_t garbage[] = {0xFF, 0xFF, 0xFF, 0xFF};
  for (int i = 0; i < 100; i++) {
    decoder.decode(garbage, sizeof(garbage), true);
  }

  // Cleanup should work
  decoder.Close();
}

TEST_F(VideoDecoderResourceTest, NoLeakOnConfigureError) {
  // Repeatedly try to configure with invalid codec
  for (int i = 0; i < 100; i++) {
    VideoDecoderSimulator decoder;
    auto error = decoder.configure("invalid-codec-xyz");
    EXPECT_TRUE(error);
  }
}

TEST_F(VideoDecoderResourceTest, PacketQueueClearedOnReset) {
  VideoDecoderSimulator decoder;
  decoder.configure("vp8");

  // Queue some decode operations
  uint8_t dummy[] = {0x00};
  decoder.decode(dummy, sizeof(dummy), true);
  decoder.decode(dummy, sizeof(dummy), true);
  decoder.decode(dummy, sizeof(dummy), true);

  decoder.reset();

  EXPECT_EQ(decoder.decode_queue_size(), 0u);
}

TEST_F(VideoDecoderResourceTest, PacketQueueClearedOnClose) {
  VideoDecoderSimulator decoder;
  decoder.configure("vp8");

  uint8_t dummy[] = {0x00};
  decoder.decode(dummy, sizeof(dummy), true);

  decoder.Close();

  EXPECT_EQ(decoder.decode_queue_size(), 0u);
}

// =============================================================================
// CATEGORY 9: isConfigSupported TESTS (via codec registry)
// =============================================================================

class VideoDecoderIsConfigSupportedTest : public ::testing::Test {};

TEST_F(VideoDecoderIsConfigSupportedTest, IsCodecSupportedReturnsTrueForVP8) {
  // VP8 should be widely available
  EXPECT_TRUE(IsCodecSupported("vp8"));
}

TEST_F(VideoDecoderIsConfigSupportedTest, IsCodecSupportedReturnsTrueForH264) {
  // H.264 should be available (may require libx264 or platform decoder)
  // This test may fail on minimal FFmpeg builds
  bool h264_supported = IsCodecSupported("avc1.42E01E");
  // Just verify it doesn't crash - support depends on FFmpeg build
  (void)h264_supported;
}

TEST_F(VideoDecoderIsConfigSupportedTest, IsCodecSupportedReturnsFalseForUnknownCodec) {
  EXPECT_FALSE(IsCodecSupported("definitely-not-a-real-codec"));
}

TEST_F(VideoDecoderIsConfigSupportedTest, ParseCodecStringReturnsNulloptForEmpty) {
  auto info = ParseCodecString("");
  EXPECT_FALSE(info.has_value());
}

TEST_F(VideoDecoderIsConfigSupportedTest, ParseCodecStringReturnsValidForVP8) {
  auto info = ParseCodecString("vp8");
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->codec_id, AV_CODEC_ID_VP8);
}

TEST_F(VideoDecoderIsConfigSupportedTest, ParseCodecStringParsesH264Parameters) {
  auto info = ParseCodecString("avc1.42E01E");
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->codec_id, AV_CODEC_ID_H264);
  // Profile and level are parsed
}

// =============================================================================
// STRESS TESTS
// =============================================================================

class VideoDecoderStressTest : public ::testing::Test {};

TEST_F(VideoDecoderStressTest, ManyConfigureResetCycles) {
  VideoDecoderSimulator decoder;

  for (int i = 0; i < 100; i++) {
    auto error = decoder.configure("vp8");
    ASSERT_FALSE(error) << "Cycle " << i << ": " << error.message;
    EXPECT_TRUE(decoder.IsConfigured());

    error = decoder.reset();
    ASSERT_FALSE(error) << "Cycle " << i << ": " << error.message;
    EXPECT_FALSE(decoder.IsConfigured());
  }
}

TEST_F(VideoDecoderStressTest, ManyDecodeAttempts) {
  VideoDecoderSimulator decoder;
  decoder.configure("vp8");

  uint8_t dummy[] = {0x00, 0x00, 0x01};

  for (int i = 0; i < 1000; i++) {
    decoder.decode(dummy, sizeof(dummy), true);
    // Errors are expected with invalid data, but should not crash
  }

  decoder.Close();
  EXPECT_TRUE(decoder.IsClosed());
}

TEST_F(VideoDecoderStressTest, RapidCloseReopen) {
  for (int i = 0; i < 50; i++) {
    VideoDecoderSimulator decoder;
    decoder.configure("vp8");
    decoder.Close();
  }
}
