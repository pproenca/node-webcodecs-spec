/**
 * test_timebase.cpp - Unit tests for timebase conversion utilities
 *
 * Tests WebCodecs/FFmpeg timestamp conversion, PTS wraparound handling,
 * and audio duration calculations.
 */

#include <gtest/gtest.h>
#include <limits>

#include "../../src/shared/timebase.h"

using namespace webcodecs::timebase;

// =============================================================================
// BASIC CONVERSION TESTS
// =============================================================================

TEST(TimebaseTest, ToMicroseconds_BasicConversion) {
  // 1 second in 90kHz timebase = 90000 ticks
  // Should convert to 1,000,000 microseconds
  auto result = to_microseconds(90000, TIMEBASE_90KHZ);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, 1000000);
}

TEST(TimebaseTest, ToMicroseconds_From1kHz) {
  // 1000 ticks in 1kHz timebase = 1 second = 1,000,000 microseconds
  auto result = to_microseconds(1000, TIMEBASE_1KHZ);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, 1000000);
}

TEST(TimebaseTest, ToMicroseconds_From48kHz) {
  // 48000 samples = 1 second = 1,000,000 microseconds
  auto result = to_microseconds(48000, TIMEBASE_48KHZ);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, 1000000);
}

TEST(TimebaseTest, ToMicroseconds_ZeroPts) {
  auto result = to_microseconds(0, TIMEBASE_90KHZ);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, 0);
}

TEST(TimebaseTest, ToMicroseconds_AV_NOPTS_VALUE) {
  auto result = to_microseconds(AV_NOPTS_VALUE, TIMEBASE_90KHZ);
  EXPECT_FALSE(result.has_value());
}

TEST(TimebaseTest, ToMicrosecondsOr_WithDefault) {
  int64_t result = to_microseconds_or(AV_NOPTS_VALUE, TIMEBASE_90KHZ, -1);
  EXPECT_EQ(result, -1);

  result = to_microseconds_or(90000, TIMEBASE_90KHZ, -1);
  EXPECT_EQ(result, 1000000);
}

TEST(TimebaseTest, FromMicroseconds_To90kHz) {
  // 1 second (1,000,000 us) -> 90,000 ticks
  int64_t result = from_microseconds(1000000, TIMEBASE_90KHZ);
  EXPECT_EQ(result, 90000);
}

TEST(TimebaseTest, FromMicroseconds_To1kHz) {
  // 1 second -> 1000 ticks
  int64_t result = from_microseconds(1000000, TIMEBASE_1KHZ);
  EXPECT_EQ(result, 1000);
}

TEST(TimebaseTest, FromMicroseconds_Zero) {
  int64_t result = from_microseconds(0, TIMEBASE_90KHZ);
  EXPECT_EQ(result, 0);
}

TEST(TimebaseTest, RoundTrip_90kHz) {
  // Convert from 90kHz to microseconds and back
  int64_t original = 123456789;
  auto us = to_microseconds(original, TIMEBASE_90KHZ);
  ASSERT_TRUE(us.has_value());
  int64_t back = from_microseconds(*us, TIMEBASE_90KHZ);
  // Should be within 1 tick due to rounding
  EXPECT_NEAR(back, original, 1);
}

// =============================================================================
// DURATION TESTS
// =============================================================================

TEST(TimebaseTest, DurationToMicroseconds_Normal) {
  int64_t result = duration_to_microseconds(90000, TIMEBASE_90KHZ);
  EXPECT_EQ(result, 1000000);
}

TEST(TimebaseTest, DurationToMicroseconds_Zero) {
  int64_t result = duration_to_microseconds(0, TIMEBASE_90KHZ);
  EXPECT_EQ(result, 0);
}

TEST(TimebaseTest, DurationToMicroseconds_Negative) {
  int64_t result = duration_to_microseconds(-100, TIMEBASE_90KHZ);
  EXPECT_EQ(result, 0);
}

TEST(TimebaseTest, DurationFromMicroseconds_Normal) {
  int64_t result = duration_from_microseconds(1000000, TIMEBASE_90KHZ);
  EXPECT_EQ(result, 90000);
}

TEST(TimebaseTest, DurationFromMicroseconds_ZeroOrNegative) {
  EXPECT_EQ(duration_from_microseconds(0, TIMEBASE_90KHZ), 0);
  EXPECT_EQ(duration_from_microseconds(-100, TIMEBASE_90KHZ), 0);
}

// =============================================================================
// FRAME DURATION TESTS
// =============================================================================

TEST(TimebaseTest, FrameDurationUs_30fps) {
  AVRational fps30 = {30, 1};
  int64_t duration = frame_duration_us(fps30);
  EXPECT_EQ(duration, 33333);  // 1000000 / 30 = 33333.33...
}

TEST(TimebaseTest, FrameDurationUs_60fps) {
  AVRational fps60 = {60, 1};
  int64_t duration = frame_duration_us(fps60);
  EXPECT_EQ(duration, 16666);  // 1000000 / 60 = 16666.66...
}

TEST(TimebaseTest, FrameDurationUs_24fps) {
  AVRational fps24 = {24, 1};
  int64_t duration = frame_duration_us(fps24);
  EXPECT_EQ(duration, 41666);  // 1000000 / 24 = 41666.66...
}

TEST(TimebaseTest, FrameDurationUs_2997fps) {
  // 29.97 fps (NTSC) = 30000/1001
  AVRational fps2997 = {30000, 1001};
  int64_t duration = frame_duration_us(fps2997);
  // Should be approximately 33366.7 us
  EXPECT_NEAR(duration, 33367, 1);
}

TEST(TimebaseTest, FrameDurationUs_InvalidRate) {
  EXPECT_EQ(frame_duration_us({0, 1}), 0);
  EXPECT_EQ(frame_duration_us({30, 0}), 0);
  EXPECT_EQ(frame_duration_us({-30, 1}), 0);
  EXPECT_EQ(frame_duration_us({30, -1}), 0);
}

TEST(TimebaseTest, FrameDuration_InTimebase) {
  AVRational fps30 = {30, 1};
  int64_t duration = frame_duration(fps30, TIMEBASE_90KHZ);
  // 1/30 second = 3000 ticks in 90kHz
  EXPECT_EQ(duration, 3000);
}

// =============================================================================
// PTS COMPARISON TESTS (Wraparound Handling)
// =============================================================================

TEST(TimebaseTest, PtsLessThan_NormalComparison) {
  // Non-90kHz timebase: simple comparison
  EXPECT_TRUE(pts_less_than(100, 200, TIMEBASE_1KHZ));
  EXPECT_FALSE(pts_less_than(200, 100, TIMEBASE_1KHZ));
  EXPECT_FALSE(pts_less_than(100, 100, TIMEBASE_1KHZ));
}

TEST(TimebaseTest, PtsLessThan_90kHz_Normal) {
  // 90kHz timebase without wraparound
  EXPECT_TRUE(pts_less_than(1000, 2000, TIMEBASE_90KHZ));
  EXPECT_FALSE(pts_less_than(2000, 1000, TIMEBASE_90KHZ));
}

TEST(TimebaseTest, PtsLessThan_90kHz_Wraparound_EndWrapped) {
  // b is near 0 (wrapped), a is near max
  // a = 2^33 - 1000 (just before wrap)
  // b = 1000 (just after wrap)
  int64_t just_before_wrap = (1LL << 33) - 1000;
  int64_t just_after_wrap = 1000;

  // a should be "less than" b because b wrapped to a later time
  EXPECT_TRUE(pts_less_than(just_before_wrap, just_after_wrap, TIMEBASE_90KHZ));
}

TEST(TimebaseTest, PtsLessThan_90kHz_Wraparound_StartWrapped) {
  // a is near 0 (wrapped), b is near max
  // This is the opposite case - a is actually later
  int64_t just_after_wrap = 1000;
  int64_t just_before_wrap = (1LL << 33) - 1000;

  // a should NOT be "less than" b because a wrapped to a later time
  EXPECT_FALSE(pts_less_than(just_after_wrap, just_before_wrap, TIMEBASE_90KHZ));
}

TEST(TimebaseTest, PtsDiffUs_Normal) {
  // Simple difference: 1 second apart
  int64_t diff = pts_diff_us(180000, 90000, TIMEBASE_90KHZ);
  EXPECT_EQ(diff, 1000000);  // 1 second
}

TEST(TimebaseTest, PtsDiffUs_Wraparound) {
  // end just after wrap, start just before wrap
  int64_t just_before_wrap = (1LL << 33) - 90000;  // 1 second before wrap
  int64_t just_after_wrap = 90000;  // 1 second after wrap

  int64_t diff = pts_diff_us(just_after_wrap, just_before_wrap, TIMEBASE_90KHZ);

  // Should be approximately 2 seconds
  EXPECT_NEAR(diff, 2000000, 100);
}

// =============================================================================
// AUDIO DURATION TESTS
// =============================================================================

TEST(TimebaseTest, AudioDurationUs_48kHz) {
  // 48000 samples at 48kHz = 1 second
  int64_t duration = audio_duration_us(48000, 48000);
  EXPECT_EQ(duration, 1000000);
}

TEST(TimebaseTest, AudioDurationUs_44100Hz) {
  // 44100 samples at 44.1kHz = 1 second
  int64_t duration = audio_duration_us(44100, 44100);
  EXPECT_EQ(duration, 1000000);
}

TEST(TimebaseTest, AudioDurationUs_1024Samples) {
  // Common AAC frame size: 1024 samples at 48kHz
  int64_t duration = audio_duration_us(1024, 48000);
  // 1024 / 48000 = ~21.33 ms = 21333 us
  EXPECT_NEAR(duration, 21333, 1);
}

TEST(TimebaseTest, AudioDurationUs_InvalidRate) {
  EXPECT_EQ(audio_duration_us(1000, 0), 0);
  EXPECT_EQ(audio_duration_us(1000, -1), 0);
}

TEST(TimebaseTest, SamplesFromDurationUs_48kHz) {
  // 1 second = 48000 samples at 48kHz
  int64_t samples = samples_from_duration_us(1000000, 48000);
  EXPECT_EQ(samples, 48000);
}

TEST(TimebaseTest, SamplesFromDurationUs_RoundTrip) {
  int64_t original_samples = 12345;
  int sample_rate = 48000;

  int64_t duration = audio_duration_us(original_samples, sample_rate);
  int64_t recovered = samples_from_duration_us(duration, sample_rate);

  // Should be very close (possible rounding)
  EXPECT_NEAR(recovered, original_samples, 1);
}

TEST(TimebaseTest, SamplesFromDurationUs_Invalid) {
  EXPECT_EQ(samples_from_duration_us(1000000, 0), 0);
  EXPECT_EQ(samples_from_duration_us(1000000, -1), 0);
  EXPECT_EQ(samples_from_duration_us(0, 48000), 0);
  EXPECT_EQ(samples_from_duration_us(-1000, 48000), 0);
}

// =============================================================================
// VALIDATION TESTS
// =============================================================================

TEST(TimebaseTest, IsValidPts) {
  EXPECT_TRUE(is_valid_pts(0));
  EXPECT_TRUE(is_valid_pts(1));
  EXPECT_TRUE(is_valid_pts(1000000000));

  EXPECT_FALSE(is_valid_pts(AV_NOPTS_VALUE));
  EXPECT_FALSE(is_valid_pts(-1));
  EXPECT_FALSE(is_valid_pts(-1000));
}

TEST(TimebaseTest, ClampPts_Normal) {
  EXPECT_EQ(clamp_pts(0), 0);
  EXPECT_EQ(clamp_pts(1000), 1000);
  EXPECT_EQ(clamp_pts(1000000000), 1000000000);
}

TEST(TimebaseTest, ClampPts_NOPTS) {
  EXPECT_EQ(clamp_pts(AV_NOPTS_VALUE), AV_NOPTS_VALUE);
}

TEST(TimebaseTest, ClampPts_Negative) {
  EXPECT_EQ(clamp_pts(-1), 0);
  EXPECT_EQ(clamp_pts(-1000), 0);
}

TEST(TimebaseTest, ClampPts_Overflow) {
  constexpr int64_t MAX_SAFE = INT64_MAX / 2;
  EXPECT_EQ(clamp_pts(MAX_SAFE), MAX_SAFE);
  EXPECT_EQ(clamp_pts(MAX_SAFE + 1), MAX_SAFE);
  EXPECT_EQ(clamp_pts(INT64_MAX), MAX_SAFE);
}

// =============================================================================
// EDGE CASES AND PRECISION TESTS
// =============================================================================

TEST(TimebaseTest, LargePtsValues) {
  // Test with large but valid PTS values (hours of content)
  // 26 hours in 90kHz ticks = 26 * 60 * 60 * 90000 = 8424000000
  int64_t pts_26_hours = 26LL * 60 * 60 * 90000;
  auto result = to_microseconds(pts_26_hours, TIMEBASE_90KHZ);
  ASSERT_TRUE(result.has_value());

  // Should be approximately 26 hours in microseconds
  int64_t expected_us = 26LL * 60 * 60 * 1000000;
  EXPECT_EQ(*result, expected_us);
}

TEST(TimebaseTest, FractionalFramerates) {
  // Test edge case with non-integer framerates
  AVRational fps_23_976 = {24000, 1001};  // 23.976 fps (film)
  int64_t duration = frame_duration_us(fps_23_976);
  // 1001/24000 seconds = ~41708.33 us
  EXPECT_NEAR(duration, 41708, 1);
}

TEST(TimebaseTest, ConstantsAreDefined) {
  // Verify constants are properly defined
  EXPECT_EQ(WEBCODECS_TIMEBASE.num, 1);
  EXPECT_EQ(WEBCODECS_TIMEBASE.den, 1000000);

  EXPECT_EQ(TIMEBASE_90KHZ.num, 1);
  EXPECT_EQ(TIMEBASE_90KHZ.den, 90000);

  EXPECT_EQ(TIMEBASE_1KHZ.num, 1);
  EXPECT_EQ(TIMEBASE_1KHZ.den, 1000);

  EXPECT_EQ(TIMEBASE_48KHZ.num, 1);
  EXPECT_EQ(TIMEBASE_48KHZ.den, 48000);

  // 2^32 threshold for wraparound detection
  EXPECT_EQ(MPEG_TS_WRAP_THRESHOLD, 1LL << 32);
}
