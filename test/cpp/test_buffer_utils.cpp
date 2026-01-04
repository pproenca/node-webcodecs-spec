/**
 * test_buffer_utils.cpp - Unit tests for buffer utilities
 *
 * Tests buffer size calculations, copy operations, and error handling.
 * Includes both happy path and sad path (error condition) tests.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

#include "../../src/shared/buffer_utils.h"
#include "../../src/ffmpeg_raii.h"

using webcodecs::buffer_utils::CalculateAudioBufferSize;
using webcodecs::buffer_utils::CalculateFrameBufferSize;
using webcodecs::buffer_utils::CopyFrameToBuffer;
using webcodecs::buffer_utils::CopyPacketToBuffer;
using webcodecs::buffer_utils::CreateFrameFromBuffer;
using webcodecs::buffer_utils::CreatePacketFromBuffer;
using webcodecs::buffer_utils::GetPlaneCount;
using webcodecs::buffer_utils::GetPlaneSize;
using webcodecs::raii::AVFramePtr;
using webcodecs::raii::AVPacketPtr;
using webcodecs::raii::MakeAvFrame;
using webcodecs::raii::MakeAvPacket;

// =============================================================================
// FRAME BUFFER SIZE CALCULATIONS - HAPPY PATH
// =============================================================================

TEST(BufferUtilsTest, CalculateFrameBufferSize_YUV420P) {
  // YUV420P: Y plane = w*h, U plane = w*h/4, V plane = w*h/4
  // Total = w*h * 1.5
  int size = CalculateFrameBufferSize(AV_PIX_FMT_YUV420P, 1920, 1080, 1);
  EXPECT_GT(size, 0);
  EXPECT_GE(size, 1920 * 1080 * 3 / 2);  // At least Y + UV
}

TEST(BufferUtilsTest, CalculateFrameBufferSize_RGB24) {
  // RGB24: 3 bytes per pixel
  int size = CalculateFrameBufferSize(AV_PIX_FMT_RGB24, 640, 480, 1);
  EXPECT_GT(size, 0);
  EXPECT_GE(size, 640 * 480 * 3);
}

TEST(BufferUtilsTest, CalculateFrameBufferSize_RGBA) {
  // RGBA: 4 bytes per pixel
  int size = CalculateFrameBufferSize(AV_PIX_FMT_RGBA, 100, 100, 1);
  EXPECT_GT(size, 0);
  EXPECT_GE(size, 100 * 100 * 4);
}

// =============================================================================
// FRAME BUFFER SIZE CALCULATIONS - SAD PATH (ERROR CONDITIONS)
// =============================================================================

TEST(BufferUtilsTest, CalculateFrameBufferSize_InvalidWidth) {
  int size = CalculateFrameBufferSize(AV_PIX_FMT_YUV420P, 0, 1080, 1);
  EXPECT_LT(size, 0);  // Should return error (negative)
}

TEST(BufferUtilsTest, CalculateFrameBufferSize_NegativeWidth) {
  int size = CalculateFrameBufferSize(AV_PIX_FMT_YUV420P, -100, 1080, 1);
  EXPECT_LT(size, 0);  // Should return error
}

TEST(BufferUtilsTest, CalculateFrameBufferSize_InvalidHeight) {
  int size = CalculateFrameBufferSize(AV_PIX_FMT_YUV420P, 1920, 0, 1);
  EXPECT_LT(size, 0);  // Should return error
}

TEST(BufferUtilsTest, CalculateFrameBufferSize_NegativeHeight) {
  int size = CalculateFrameBufferSize(AV_PIX_FMT_YUV420P, 1920, -100, 1);
  EXPECT_LT(size, 0);  // Should return error
}

TEST(BufferUtilsTest, CalculateFrameBufferSize_InvalidFormat) {
  int size = CalculateFrameBufferSize(AV_PIX_FMT_NONE, 1920, 1080, 1);
  EXPECT_LT(size, 0);  // Should return error
}

TEST(BufferUtilsTest, CalculateFrameBufferSize_UnknownFormat) {
  // Use an invalid format value
  int size = CalculateFrameBufferSize(999, 1920, 1080, 1);
  EXPECT_LT(size, 0);  // Should return error
}

// =============================================================================
// COPY FRAME TO BUFFER - HAPPY PATH
// =============================================================================

TEST(BufferUtilsTest, CopyFrameToBuffer_ValidFrame) {
  AVFramePtr frame = MakeAvFrame();
  ASSERT_NE(frame, nullptr);

  frame->width = 64;
  frame->height = 64;
  frame->format = AV_PIX_FMT_YUV420P;
  ASSERT_GE(av_frame_get_buffer(frame.get(), 32), 0);

  // Fill with test pattern
  memset(frame->data[0], 0x10, frame->linesize[0] * frame->height);      // Y
  memset(frame->data[1], 0x80, frame->linesize[1] * frame->height / 2);  // U
  memset(frame->data[2], 0x80, frame->linesize[2] * frame->height / 2);  // V

  int required_size = CalculateFrameBufferSize(frame->format, frame->width, frame->height, 1);
  ASSERT_GT(required_size, 0);

  std::vector<uint8_t> buffer(required_size);
  int copied = CopyFrameToBuffer(frame.get(), buffer.data(), buffer.size(), 1);

  EXPECT_GT(copied, 0);
  EXPECT_LE(copied, required_size);
}

TEST(BufferUtilsTest, CopyFrameToBuffer_RGB24) {
  AVFramePtr frame = MakeAvFrame();
  ASSERT_NE(frame, nullptr);

  frame->width = 100;
  frame->height = 100;
  frame->format = AV_PIX_FMT_RGB24;
  ASSERT_GE(av_frame_get_buffer(frame.get(), 1), 0);

  // Fill with red color
  for (int y = 0; y < frame->height; y++) {
    uint8_t* row = frame->data[0] + y * frame->linesize[0];
    for (int x = 0; x < frame->width; x++) {
      row[x * 3 + 0] = 255;  // R
      row[x * 3 + 1] = 0;    // G
      row[x * 3 + 2] = 0;    // B
    }
  }

  int required_size = CalculateFrameBufferSize(frame->format, frame->width, frame->height, 1);
  std::vector<uint8_t> buffer(required_size);
  int copied = CopyFrameToBuffer(frame.get(), buffer.data(), buffer.size(), 1);

  EXPECT_GT(copied, 0);
}

// =============================================================================
// COPY FRAME TO BUFFER - SAD PATH (ERROR CONDITIONS)
// =============================================================================

TEST(BufferUtilsTest, CopyFrameToBuffer_NullFrame) {
  std::vector<uint8_t> buffer(1024);
  int result = CopyFrameToBuffer(nullptr, buffer.data(), buffer.size(), 1);
  EXPECT_LT(result, 0);  // Should return error
}

TEST(BufferUtilsTest, CopyFrameToBuffer_NullDestination) {
  AVFramePtr frame = MakeAvFrame();
  ASSERT_NE(frame, nullptr);
  frame->width = 64;
  frame->height = 64;
  frame->format = AV_PIX_FMT_YUV420P;
  ASSERT_GE(av_frame_get_buffer(frame.get(), 32), 0);

  int result = CopyFrameToBuffer(frame.get(), nullptr, 1024, 1);
  EXPECT_LT(result, 0);  // Should return error
}

TEST(BufferUtilsTest, CopyFrameToBuffer_ZeroDestSize) {
  AVFramePtr frame = MakeAvFrame();
  ASSERT_NE(frame, nullptr);
  frame->width = 64;
  frame->height = 64;
  frame->format = AV_PIX_FMT_YUV420P;
  ASSERT_GE(av_frame_get_buffer(frame.get(), 32), 0);

  std::vector<uint8_t> buffer(1024);
  int result = CopyFrameToBuffer(frame.get(), buffer.data(), 0, 1);
  EXPECT_LT(result, 0);  // Should return error
}

TEST(BufferUtilsTest, CopyFrameToBuffer_DestinationTooSmall) {
  AVFramePtr frame = MakeAvFrame();
  ASSERT_NE(frame, nullptr);
  frame->width = 1920;
  frame->height = 1080;
  frame->format = AV_PIX_FMT_YUV420P;
  ASSERT_GE(av_frame_get_buffer(frame.get(), 32), 0);

  // Provide a buffer that's way too small
  std::vector<uint8_t> buffer(100);  // Need millions of bytes
  int result = CopyFrameToBuffer(frame.get(), buffer.data(), buffer.size(), 1);
  EXPECT_LT(result, 0);  // Should return ENOSPC error
}

TEST(BufferUtilsTest, CopyFrameToBuffer_InvalidFormat) {
  AVFramePtr frame = MakeAvFrame();
  ASSERT_NE(frame, nullptr);
  frame->width = 64;
  frame->height = 64;
  frame->format = AV_PIX_FMT_NONE;  // Invalid format

  std::vector<uint8_t> buffer(1024);
  int result = CopyFrameToBuffer(frame.get(), buffer.data(), buffer.size(), 1);
  EXPECT_LT(result, 0);  // Should return error
}

// =============================================================================
// CREATE FRAME FROM BUFFER - HAPPY PATH
// =============================================================================

TEST(BufferUtilsTest, CreateFrameFromBuffer_ValidData) {
  // Create a simple 4x4 YUV420P frame worth of data
  // Y: 4*4 = 16 bytes, U: 2*2 = 4 bytes, V: 2*2 = 4 bytes = 24 bytes total
  int width = 4;
  int height = 4;
  int size = CalculateFrameBufferSize(AV_PIX_FMT_YUV420P, width, height, 1);
  ASSERT_GT(size, 0);

  std::vector<uint8_t> buffer(size, 0x80);  // Fill with gray

  AVFramePtr frame = CreateFrameFromBuffer(buffer.data(), buffer.size(), width, height, AV_PIX_FMT_YUV420P);
  ASSERT_NE(frame, nullptr);
  EXPECT_EQ(frame->width, width);
  EXPECT_EQ(frame->height, height);
  EXPECT_EQ(frame->format, AV_PIX_FMT_YUV420P);
}

TEST(BufferUtilsTest, CreateFrameFromBuffer_RGB24) {
  int width = 10;
  int height = 10;
  int size = CalculateFrameBufferSize(AV_PIX_FMT_RGB24, width, height, 1);
  ASSERT_GT(size, 0);

  std::vector<uint8_t> buffer(size, 0xFF);  // Fill with white

  AVFramePtr frame = CreateFrameFromBuffer(buffer.data(), buffer.size(), width, height, AV_PIX_FMT_RGB24);
  ASSERT_NE(frame, nullptr);
  EXPECT_EQ(frame->width, width);
  EXPECT_EQ(frame->height, height);
}

// =============================================================================
// CREATE FRAME FROM BUFFER - SAD PATH (ERROR CONDITIONS)
// =============================================================================

TEST(BufferUtilsTest, CreateFrameFromBuffer_NullData) {
  AVFramePtr frame = CreateFrameFromBuffer(nullptr, 1024, 64, 64, AV_PIX_FMT_YUV420P);
  EXPECT_EQ(frame, nullptr);
}

TEST(BufferUtilsTest, CreateFrameFromBuffer_ZeroSize) {
  std::vector<uint8_t> buffer(1024);
  AVFramePtr frame = CreateFrameFromBuffer(buffer.data(), 0, 64, 64, AV_PIX_FMT_YUV420P);
  EXPECT_EQ(frame, nullptr);
}

TEST(BufferUtilsTest, CreateFrameFromBuffer_InvalidWidth) {
  std::vector<uint8_t> buffer(1024);
  AVFramePtr frame = CreateFrameFromBuffer(buffer.data(), buffer.size(), 0, 64, AV_PIX_FMT_YUV420P);
  EXPECT_EQ(frame, nullptr);
}

TEST(BufferUtilsTest, CreateFrameFromBuffer_NegativeWidth) {
  std::vector<uint8_t> buffer(1024);
  AVFramePtr frame = CreateFrameFromBuffer(buffer.data(), buffer.size(), -10, 64, AV_PIX_FMT_YUV420P);
  EXPECT_EQ(frame, nullptr);
}

TEST(BufferUtilsTest, CreateFrameFromBuffer_InvalidHeight) {
  std::vector<uint8_t> buffer(1024);
  AVFramePtr frame = CreateFrameFromBuffer(buffer.data(), buffer.size(), 64, 0, AV_PIX_FMT_YUV420P);
  EXPECT_EQ(frame, nullptr);
}

TEST(BufferUtilsTest, CreateFrameFromBuffer_InvalidFormat) {
  std::vector<uint8_t> buffer(1024);
  AVFramePtr frame = CreateFrameFromBuffer(buffer.data(), buffer.size(), 64, 64, AV_PIX_FMT_NONE);
  EXPECT_EQ(frame, nullptr);
}

TEST(BufferUtilsTest, CreateFrameFromBuffer_BufferTooSmall) {
  // Request a 1920x1080 frame but only provide 100 bytes
  std::vector<uint8_t> buffer(100);
  AVFramePtr frame = CreateFrameFromBuffer(buffer.data(), buffer.size(), 1920, 1080, AV_PIX_FMT_YUV420P);
  EXPECT_EQ(frame, nullptr);  // Should fail due to insufficient buffer
}

// =============================================================================
// AUDIO BUFFER UTILITIES - HAPPY PATH
// =============================================================================

TEST(BufferUtilsTest, CalculateAudioBufferSize_Valid) {
  // 1024 samples, stereo, S16 = 1024 * 2 * 2 = 4096 bytes
  int size = CalculateAudioBufferSize(1024, 2, AV_SAMPLE_FMT_S16, 1);
  EXPECT_GT(size, 0);
  EXPECT_GE(size, 4096);
}

TEST(BufferUtilsTest, CalculateAudioBufferSize_Float) {
  // 1024 samples, stereo, float = 1024 * 2 * 4 = 8192 bytes
  int size = CalculateAudioBufferSize(1024, 2, AV_SAMPLE_FMT_FLT, 1);
  EXPECT_GT(size, 0);
  EXPECT_GE(size, 8192);
}

// =============================================================================
// AUDIO BUFFER UTILITIES - SAD PATH
// =============================================================================

TEST(BufferUtilsTest, CalculateAudioBufferSize_InvalidSamples) {
  int size = CalculateAudioBufferSize(0, 2, AV_SAMPLE_FMT_S16, 1);
  EXPECT_LT(size, 0);
}

TEST(BufferUtilsTest, CalculateAudioBufferSize_NegativeSamples) {
  int size = CalculateAudioBufferSize(-100, 2, AV_SAMPLE_FMT_S16, 1);
  EXPECT_LT(size, 0);
}

TEST(BufferUtilsTest, CalculateAudioBufferSize_InvalidChannels) {
  int size = CalculateAudioBufferSize(1024, 0, AV_SAMPLE_FMT_S16, 1);
  EXPECT_LT(size, 0);
}

TEST(BufferUtilsTest, CalculateAudioBufferSize_InvalidFormat) {
  int size = CalculateAudioBufferSize(1024, 2, AV_SAMPLE_FMT_NONE, 1);
  EXPECT_LT(size, 0);
}

// =============================================================================
// PACKET BUFFER UTILITIES - HAPPY PATH
// =============================================================================

TEST(BufferUtilsTest, CopyPacketToBuffer_ValidPacket) {
  AVPacketPtr packet = MakeAvPacket();
  ASSERT_NE(packet, nullptr);
  ASSERT_GE(av_new_packet(packet.get(), 256), 0);

  // Fill with test data
  memset(packet->data, 0xAB, packet->size);

  std::vector<uint8_t> buffer(512);
  int copied = CopyPacketToBuffer(packet.get(), buffer.data(), buffer.size());

  EXPECT_EQ(copied, 256);
  EXPECT_EQ(buffer[0], 0xAB);
}

TEST(BufferUtilsTest, CreatePacketFromBuffer_ValidData) {
  std::vector<uint8_t> data(128, 0xCD);

  AVPacketPtr packet = CreatePacketFromBuffer(data.data(), data.size());
  ASSERT_NE(packet, nullptr);
  EXPECT_EQ(packet->size, 128);
  EXPECT_EQ(packet->data[0], 0xCD);
}

// =============================================================================
// PACKET BUFFER UTILITIES - SAD PATH
// =============================================================================

TEST(BufferUtilsTest, CopyPacketToBuffer_NullPacket) {
  std::vector<uint8_t> buffer(256);
  int result = CopyPacketToBuffer(nullptr, buffer.data(), buffer.size());
  EXPECT_LT(result, 0);
}

TEST(BufferUtilsTest, CopyPacketToBuffer_NullDest) {
  AVPacketPtr packet = MakeAvPacket();
  ASSERT_NE(packet, nullptr);
  ASSERT_GE(av_new_packet(packet.get(), 256), 0);

  int result = CopyPacketToBuffer(packet.get(), nullptr, 256);
  EXPECT_LT(result, 0);
}

TEST(BufferUtilsTest, CopyPacketToBuffer_DestTooSmall) {
  AVPacketPtr packet = MakeAvPacket();
  ASSERT_NE(packet, nullptr);
  ASSERT_GE(av_new_packet(packet.get(), 256), 0);

  std::vector<uint8_t> buffer(100);  // Too small
  int result = CopyPacketToBuffer(packet.get(), buffer.data(), buffer.size());
  EXPECT_LT(result, 0);  // Should return ENOSPC
}

TEST(BufferUtilsTest, CreatePacketFromBuffer_NullData) {
  AVPacketPtr packet = CreatePacketFromBuffer(nullptr, 128);
  EXPECT_EQ(packet, nullptr);
}

TEST(BufferUtilsTest, CreatePacketFromBuffer_ZeroSize) {
  std::vector<uint8_t> data(128);
  AVPacketPtr packet = CreatePacketFromBuffer(data.data(), 0);
  EXPECT_EQ(packet, nullptr);
}

// =============================================================================
// PLANE UTILITIES
// =============================================================================

TEST(BufferUtilsTest, GetPlaneCount_YUV420P) {
  int planes = GetPlaneCount(AV_PIX_FMT_YUV420P);
  EXPECT_EQ(planes, 3);  // Y, U, V
}

TEST(BufferUtilsTest, GetPlaneCount_RGB24) {
  int planes = GetPlaneCount(AV_PIX_FMT_RGB24);
  EXPECT_EQ(planes, 1);  // Single packed plane
}

TEST(BufferUtilsTest, GetPlaneCount_InvalidFormat) {
  int planes = GetPlaneCount(AV_PIX_FMT_NONE);
  EXPECT_EQ(planes, 0);
}

TEST(BufferUtilsTest, GetPlaneSize_ValidFrame) {
  AVFramePtr frame = MakeAvFrame();
  ASSERT_NE(frame, nullptr);
  frame->width = 1920;
  frame->height = 1080;
  frame->format = AV_PIX_FMT_YUV420P;
  ASSERT_GE(av_frame_get_buffer(frame.get(), 32), 0);

  size_t y_size = GetPlaneSize(frame.get(), 0);
  size_t u_size = GetPlaneSize(frame.get(), 1);
  size_t v_size = GetPlaneSize(frame.get(), 2);

  EXPECT_GT(y_size, 0);
  EXPECT_GT(u_size, 0);
  EXPECT_GT(v_size, 0);

  // Y plane should be larger than U/V planes for YUV420P
  EXPECT_GT(y_size, u_size);
  EXPECT_EQ(u_size, v_size);
}

TEST(BufferUtilsTest, GetPlaneSize_NullFrame) {
  size_t size = GetPlaneSize(nullptr, 0);
  EXPECT_EQ(size, 0);
}

TEST(BufferUtilsTest, GetPlaneSize_InvalidPlaneIndex) {
  AVFramePtr frame = MakeAvFrame();
  ASSERT_NE(frame, nullptr);
  frame->width = 64;
  frame->height = 64;
  frame->format = AV_PIX_FMT_YUV420P;
  ASSERT_GE(av_frame_get_buffer(frame.get(), 32), 0);

  size_t size = GetPlaneSize(frame.get(), -1);
  EXPECT_EQ(size, 0);

  size = GetPlaneSize(frame.get(), 100);
  EXPECT_EQ(size, 0);
}

// =============================================================================
// ROUNDTRIP TESTS - Verify data integrity
// =============================================================================

TEST(BufferUtilsTest, RoundtripFrameData_YUV420P) {
  // Create original frame
  AVFramePtr original = MakeAvFrame();
  ASSERT_NE(original, nullptr);
  original->width = 64;
  original->height = 64;
  original->format = AV_PIX_FMT_YUV420P;
  ASSERT_GE(av_frame_get_buffer(original.get(), 1), 0);

  // Fill with pattern
  for (int i = 0; i < original->height; i++) {
    memset(original->data[0] + i * original->linesize[0], i, original->width);
  }

  // Copy to buffer
  int size = CalculateFrameBufferSize(original->format, original->width, original->height, 1);
  ASSERT_GT(size, 0);
  std::vector<uint8_t> buffer(size);
  int copied = CopyFrameToBuffer(original.get(), buffer.data(), buffer.size(), 1);
  ASSERT_GT(copied, 0);

  // Create new frame from buffer
  AVFramePtr restored =
      CreateFrameFromBuffer(buffer.data(), buffer.size(), original->width, original->height, original->format);
  ASSERT_NE(restored, nullptr);

  // Verify dimensions match
  EXPECT_EQ(restored->width, original->width);
  EXPECT_EQ(restored->height, original->height);
  EXPECT_EQ(restored->format, original->format);

  // Verify Y plane data matches (first row as sample)
  EXPECT_EQ(memcmp(restored->data[0], original->data[0], original->width), 0);
}

// =============================================================================
// STRESS TESTS
// =============================================================================

TEST(BufferUtilsTest, StressTest_ManyFrameCopies) {
  AVFramePtr frame = MakeAvFrame();
  ASSERT_NE(frame, nullptr);
  frame->width = 640;
  frame->height = 480;
  frame->format = AV_PIX_FMT_YUV420P;
  ASSERT_GE(av_frame_get_buffer(frame.get(), 32), 0);

  int size = CalculateFrameBufferSize(frame->format, frame->width, frame->height, 1);
  std::vector<uint8_t> buffer(size);

  // Copy many times - should not leak
  for (int i = 0; i < 100; i++) {
    int copied = CopyFrameToBuffer(frame.get(), buffer.data(), buffer.size(), 1);
    EXPECT_GT(copied, 0);
  }
}

TEST(BufferUtilsTest, StressTest_ManyPacketCopies) {
  AVPacketPtr packet = MakeAvPacket();
  ASSERT_NE(packet, nullptr);
  ASSERT_GE(av_new_packet(packet.get(), 1024), 0);

  std::vector<uint8_t> buffer(2048);

  // Copy many times - should not leak
  for (int i = 0; i < 100; i++) {
    int copied = CopyPacketToBuffer(packet.get(), buffer.data(), buffer.size());
    EXPECT_EQ(copied, 1024);
  }
}

// =============================================================================
// CREATE FRAME FROM BUFFER WITH LAYOUT - EDGE CASES
// =============================================================================

using webcodecs::buffer_utils::CreateFrameFromBufferWithLayout;

TEST(BufferUtilsTest, CreateFrameFromBufferWithLayout_ValidLayout) {
  // Create a 4x4 YUV420P frame worth of data
  // Y: 4*4 = 16 bytes, U: 2*2 = 4 bytes, V: 2*2 = 4 bytes = 24 bytes total
  int width = 4;
  int height = 4;

  // Custom layout with padding between planes
  int offsets[] = {0, 32, 48};   // Y at 0, U at 32, V at 48
  int strides[] = {8, 4, 4};     // Y stride 8, U/V stride 4

  // Buffer needs: Y: 4 rows * 8 stride = 32, U: 2 rows * 4 = 8, V: 2 rows * 4 = 8
  // Layout: [Y@0..32][U@32..40][V@48..56]
  std::vector<uint8_t> buffer(64, 0x80);  // Fill with gray

  AVFramePtr frame = CreateFrameFromBufferWithLayout(
      buffer.data(), buffer.size(), width, height, AV_PIX_FMT_YUV420P,
      offsets, strides, 3);

  ASSERT_NE(frame, nullptr);
  EXPECT_EQ(frame->width, width);
  EXPECT_EQ(frame->height, height);
  EXPECT_EQ(frame->format, AV_PIX_FMT_YUV420P);
}

TEST(BufferUtilsTest, CreateFrameFromBufferWithLayout_NullData) {
  int offsets[] = {0, 16, 20};
  int strides[] = {4, 2, 2};

  AVFramePtr frame = CreateFrameFromBufferWithLayout(
      nullptr, 1024, 4, 4, AV_PIX_FMT_YUV420P, offsets, strides, 3);
  EXPECT_EQ(frame, nullptr);
}

TEST(BufferUtilsTest, CreateFrameFromBufferWithLayout_ZeroSize) {
  int offsets[] = {0, 16, 20};
  int strides[] = {4, 2, 2};
  std::vector<uint8_t> buffer(64);

  AVFramePtr frame = CreateFrameFromBufferWithLayout(
      buffer.data(), 0, 4, 4, AV_PIX_FMT_YUV420P, offsets, strides, 3);
  EXPECT_EQ(frame, nullptr);
}

TEST(BufferUtilsTest, CreateFrameFromBufferWithLayout_InvalidWidth) {
  int offsets[] = {0, 16, 20};
  int strides[] = {4, 2, 2};
  std::vector<uint8_t> buffer(64);

  AVFramePtr frame = CreateFrameFromBufferWithLayout(
      buffer.data(), buffer.size(), 0, 4, AV_PIX_FMT_YUV420P, offsets, strides, 3);
  EXPECT_EQ(frame, nullptr);
}

TEST(BufferUtilsTest, CreateFrameFromBufferWithLayout_NegativeOffset) {
  int offsets[] = {-1, 16, 20};  // Negative offset
  int strides[] = {4, 2, 2};
  std::vector<uint8_t> buffer(64);

  AVFramePtr frame = CreateFrameFromBufferWithLayout(
      buffer.data(), buffer.size(), 4, 4, AV_PIX_FMT_YUV420P, offsets, strides, 3);
  EXPECT_EQ(frame, nullptr);  // Should fail due to negative offset
}

TEST(BufferUtilsTest, CreateFrameFromBufferWithLayout_ZeroStride) {
  int offsets[] = {0, 16, 20};
  int strides[] = {0, 2, 2};  // Zero stride
  std::vector<uint8_t> buffer(64);

  AVFramePtr frame = CreateFrameFromBufferWithLayout(
      buffer.data(), buffer.size(), 4, 4, AV_PIX_FMT_YUV420P, offsets, strides, 3);
  EXPECT_EQ(frame, nullptr);  // Should fail due to zero stride
}

TEST(BufferUtilsTest, CreateFrameFromBufferWithLayout_NegativeStride) {
  int offsets[] = {0, 16, 20};
  int strides[] = {-4, 2, 2};  // Negative stride
  std::vector<uint8_t> buffer(64);

  AVFramePtr frame = CreateFrameFromBufferWithLayout(
      buffer.data(), buffer.size(), 4, 4, AV_PIX_FMT_YUV420P, offsets, strides, 3);
  EXPECT_EQ(frame, nullptr);  // Should fail due to negative stride
}

TEST(BufferUtilsTest, CreateFrameFromBufferWithLayout_BufferTooSmall) {
  int offsets[] = {0, 32, 48};
  int strides[] = {8, 4, 4};
  std::vector<uint8_t> buffer(32);  // Too small for layout

  AVFramePtr frame = CreateFrameFromBufferWithLayout(
      buffer.data(), buffer.size(), 4, 4, AV_PIX_FMT_YUV420P, offsets, strides, 3);
  EXPECT_EQ(frame, nullptr);  // Should fail - buffer too small
}

TEST(BufferUtilsTest, CreateFrameFromBufferWithLayout_OverflowPrevention) {
  // Test that integer overflow in bounds calculation is prevented
  // Large values that would overflow if multiplied as int32
  int offsets[] = {0, 0, 0};
  int strides[] = {INT32_MAX / 2, 1, 1};  // Large stride value
  std::vector<uint8_t> buffer(1024);

  // With a large stride and reasonable height, the calculation
  // (height - 1) * stride would overflow a 32-bit int
  // Our fix uses size_t arithmetic to prevent this
  AVFramePtr frame = CreateFrameFromBufferWithLayout(
      buffer.data(), buffer.size(), 4, 1000, AV_PIX_FMT_YUV420P, offsets, strides, 3);

  // Should return nullptr because the required size exceeds buffer
  EXPECT_EQ(frame, nullptr);
}

TEST(BufferUtilsTest, CreateFrameFromBufferWithLayout_LayoutExceedsBuffer) {
  // Layout offset + data would exceed buffer
  int offsets[] = {0, 100, 200};  // Offsets exceed buffer size
  int strides[] = {4, 2, 2};
  std::vector<uint8_t> buffer(64);

  AVFramePtr frame = CreateFrameFromBufferWithLayout(
      buffer.data(), buffer.size(), 4, 4, AV_PIX_FMT_YUV420P, offsets, strides, 3);
  EXPECT_EQ(frame, nullptr);
}

TEST(BufferUtilsTest, CreateFrameFromBufferWithLayout_NoLayoutProvided) {
  // Test fallback to default (tightly packed) layout
  int width = 4;
  int height = 4;
  int size = CalculateFrameBufferSize(AV_PIX_FMT_YUV420P, width, height, 1);
  ASSERT_GT(size, 0);

  std::vector<uint8_t> buffer(size, 0x80);

  // Pass nullptr for layout arrays - should use default packing
  AVFramePtr frame = CreateFrameFromBufferWithLayout(
      buffer.data(), buffer.size(), width, height, AV_PIX_FMT_YUV420P,
      nullptr, nullptr, 0);

  ASSERT_NE(frame, nullptr);
  EXPECT_EQ(frame->width, width);
  EXPECT_EQ(frame->height, height);
}

// =============================================================================
// FORMAT CONVERTER - CalculateSizeWithLayout OVERFLOW TESTS
// =============================================================================

#include "../../src/shared/format_converter.h"

TEST(FormatConverterTest, CalculateSizeWithLayout_IntegerOverflowPrevention) {
  // Test that integer overflow in CalculateSizeWithLayout is prevented
  // Large stride values that would overflow int32 when multiplied by height
  int offsets[] = {0, 0, 0};
  int strides[] = {INT32_MAX / 100, 1, 1};  // Large stride

  // With height=1000, (height-1) * stride = 999 * (INT32_MAX/100) would overflow int32
  // The function should return an error since the result exceeds INT32_MAX
  int result = webcodecs::format_converter::CalculateSizeWithLayout(
      AV_PIX_FMT_YUV420P, 64, 1000, offsets, strides, 3);

  // Function should return error (AVERROR(EINVAL)) for sizes that would overflow
  EXPECT_LT(result, 0);  // Expect error code (negative)
}

TEST(FormatConverterTest, CalculateSizeWithLayout_ValidLayout) {
  // Test normal operation with valid layout
  int offsets[] = {0, 256, 320};  // Y at 0, U at 256, V at 320
  int strides[] = {16, 8, 8};      // Y stride 16, U/V stride 8

  int result = webcodecs::format_converter::CalculateSizeWithLayout(
      AV_PIX_FMT_YUV420P, 16, 16, offsets, strides, 3);

  // Should return positive valid size
  EXPECT_GT(result, 0);
  // Should be at least as big as the last plane's end
  // V plane: offset(320) + (height/2-1)*stride(8) + row_bytes(8) = 320 + 7*8 + 8 = 384
  EXPECT_GE(result, 384);
}

// =============================================================================
// YUVA420P (4-PLANE) FORMAT TESTS
// =============================================================================

TEST(BufferUtilsTest, GetPlaneCount_YUVA420P) {
  int planes = GetPlaneCount(AV_PIX_FMT_YUVA420P);
  EXPECT_EQ(planes, 4);  // Y, U, V, A
}

TEST(BufferUtilsTest, CalculateFrameBufferSize_YUVA420P) {
  int size = CalculateFrameBufferSize(AV_PIX_FMT_YUVA420P, 16, 16, 1);
  EXPECT_GT(size, 0);
  // YUVA420P: Y=16*16=256, U=8*8=64, V=8*8=64, A=16*16=256 = 640 minimum
  EXPECT_GE(size, 640);
}

TEST(BufferUtilsTest, GetPlaneCount_YUV422P) {
  int planes = GetPlaneCount(AV_PIX_FMT_YUV422P);
  EXPECT_EQ(planes, 3);  // Y, U, V (horizontal subsampling only)
}

TEST(BufferUtilsTest, CalculateFrameBufferSize_YUV422P) {
  int size = CalculateFrameBufferSize(AV_PIX_FMT_YUV422P, 16, 16, 1);
  EXPECT_GT(size, 0);
  // YUV422P: Y=16*16=256, U=8*16=128, V=8*16=128 = 512 minimum
  EXPECT_GE(size, 512);
}

TEST(BufferUtilsTest, GetPlaneCount_YUV444P) {
  int planes = GetPlaneCount(AV_PIX_FMT_YUV444P);
  EXPECT_EQ(planes, 3);  // Y, U, V (no subsampling)
}

TEST(BufferUtilsTest, CalculateFrameBufferSize_YUV444P) {
  int size = CalculateFrameBufferSize(AV_PIX_FMT_YUV444P, 16, 16, 1);
  EXPECT_GT(size, 0);
  // YUV444P: Y=16*16=256, U=16*16=256, V=16*16=256 = 768 minimum
  EXPECT_GE(size, 768);
}
