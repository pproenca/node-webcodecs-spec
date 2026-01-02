#pragma once
/**
 * timebase.h - WebCodecs/FFmpeg Timestamp Conversion Utilities
 *
 * WebCodecs uses microseconds for all timestamps.
 * FFmpeg uses arbitrary timebases (e.g., 1/90000 for MPEG-TS, 1/1000 for MP4).
 *
 * Without explicit conversion utilities:
 * - A/V sync bugs manifest after 30+ minutes of playback
 * - Seek precision is off
 * - Duration calculations are wrong
 * - MPEG-TS PTS wraparound (33-bit) causes jumps
 *
 * This header provides:
 * - ToMicroseconds(): FFmpeg PTS -> WebCodecs timestamp
 * - FromMicroseconds(): WebCodecs timestamp -> FFmpeg PTS
 * - PTS wraparound handling for MPEG-TS streams
 * - Frame duration calculation from framerate
 */

#include <cstdint>
#include <optional>

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/rational.h>
#include <libavutil/mathematics.h>
}

namespace webcodecs {
namespace timebase {

// ===========================================================================
// CONSTANTS (kPascalCase per Google C++ Style Guide)
// ===========================================================================

/**
 * WebCodecs uses microseconds (1/1,000,000 second) for all timestamps.
 */
constexpr AVRational kWebcodecsTimebase = {1, 1000000};

/**
 * Common FFmpeg timebases.
 */
constexpr AVRational kTimebase90Khz = {1, 90000};  // MPEG-TS, H.264
constexpr AVRational kTimebase1Khz = {1, 1000};    // MP4, WebM
constexpr AVRational kTimebase48Khz = {1, 48000};  // Common audio

/**
 * MPEG-TS PTS wrap threshold (2^32, half of 33-bit range).
 * Used for detecting PTS wraparound.
 */
constexpr int64_t kMpegTsWrapThreshold = (1LL << 32);

// ===========================================================================
// CONVERSION FUNCTIONS (PascalCase per Google C++ Style Guide)
// ===========================================================================

/**
 * Convert FFmpeg timestamp to WebCodecs microseconds.
 *
 * @param pts FFmpeg presentation timestamp
 * @param src_timebase Source timebase (from codec/stream)
 * @return Timestamp in microseconds, or std::nullopt if pts is AV_NOPTS_VALUE
 */
[[nodiscard]] inline std::optional<int64_t> ToMicroseconds(int64_t pts, AVRational src_timebase) {
  if (pts == AV_NOPTS_VALUE) {
    return std::nullopt;
  }
  return av_rescale_q(pts, src_timebase, kWebcodecsTimebase);
}

/**
 * Convert FFmpeg timestamp to WebCodecs microseconds with default value.
 *
 * @param pts FFmpeg presentation timestamp
 * @param src_timebase Source timebase
 * @param default_value Value to return if pts is AV_NOPTS_VALUE
 * @return Timestamp in microseconds
 */
[[nodiscard]] inline int64_t ToMicrosecondsOr(int64_t pts, AVRational src_timebase, int64_t default_value) {
  if (pts == AV_NOPTS_VALUE) {
    return default_value;
  }
  return av_rescale_q(pts, src_timebase, kWebcodecsTimebase);
}

/**
 * Convert WebCodecs microseconds to FFmpeg timestamp.
 *
 * @param us Timestamp in microseconds
 * @param dst_timebase Destination timebase
 * @return FFmpeg timestamp in destination timebase
 */
[[nodiscard]] inline int64_t FromMicroseconds(int64_t us, AVRational dst_timebase) {
  return av_rescale_q(us, kWebcodecsTimebase, dst_timebase);
}

/**
 * Convert duration (never AV_NOPTS_VALUE).
 *
 * @param duration FFmpeg duration
 * @param src_timebase Source timebase
 * @return Duration in microseconds, or 0 if duration <= 0
 */
[[nodiscard]] inline int64_t DurationToMicroseconds(int64_t duration, AVRational src_timebase) {
  if (duration <= 0) {
    return 0;
  }
  return av_rescale_q(duration, src_timebase, kWebcodecsTimebase);
}

/**
 * Convert WebCodecs duration to FFmpeg duration.
 *
 * @param us Duration in microseconds
 * @param dst_timebase Destination timebase
 * @return Duration in destination timebase
 */
[[nodiscard]] inline int64_t DurationFromMicroseconds(int64_t us, AVRational dst_timebase) {
  if (us <= 0) {
    return 0;
  }
  return av_rescale_q(us, kWebcodecsTimebase, dst_timebase);
}

// ===========================================================================
// FRAME DURATION
// ===========================================================================

/**
 * Calculate frame duration from framerate in microseconds.
 *
 * @param framerate Frame rate as AVRational (e.g., {30, 1} for 30fps)
 * @return Frame duration in microseconds, or 0 if framerate is unknown/variable
 */
[[nodiscard]] inline int64_t FrameDurationUs(AVRational framerate) {
  if (framerate.num <= 0 || framerate.den <= 0) {
    return 0;
  }
  // duration = 1 / fps = den / num seconds = den * 1000000 / num microseconds
  return av_rescale(1000000LL, framerate.den, framerate.num);
}

/**
 * Calculate frame duration from framerate in given timebase.
 *
 * @param framerate Frame rate as AVRational
 * @param dst_timebase Destination timebase
 * @return Frame duration in destination timebase
 */
[[nodiscard]] inline int64_t FrameDuration(AVRational framerate, AVRational dst_timebase) {
  if (framerate.num <= 0 || framerate.den <= 0) {
    return 0;
  }
  int64_t us = FrameDurationUs(framerate);
  return av_rescale_q(us, kWebcodecsTimebase, dst_timebase);
}

// ===========================================================================
// PTS COMPARISON (Handles Wraparound)
// ===========================================================================

/**
 * Safe PTS comparison that handles MPEG-TS 33-bit wraparound.
 *
 * MPEG-TS uses a 33-bit PTS counter that wraps at 2^33.
 * After ~26.5 hours of playback, the PTS wraps from max to 0.
 * Naive comparison would cause seek/sync issues.
 *
 * @param a First PTS value
 * @param b Second PTS value
 * @param timebase Timebase (used to detect if wraparound is relevant)
 * @return true if a < b (accounting for potential wraparound)
 */
[[nodiscard]] inline bool PtsLessThan(int64_t a, int64_t b, AVRational timebase) {
  // Only apply wraparound logic for 90kHz timebase (MPEG-TS)
  if (timebase.den != 90000) {
    return a < b;
  }

  int64_t diff = b - a;

  // If difference is greater than half the wrap range, assume wraparound
  if (diff > kMpegTsWrapThreshold) {
    // b wrapped, a is actually greater
    return false;
  }
  if (diff < -kMpegTsWrapThreshold) {
    // a wrapped, a is actually less
    return true;
  }
  return a < b;
}

/**
 * Calculate time difference accounting for wraparound.
 *
 * @param end End PTS
 * @param start Start PTS
 * @param timebase Timebase
 * @return Difference in microseconds
 */
[[nodiscard]] inline int64_t PtsDiffUs(int64_t end, int64_t start, AVRational timebase) {
  int64_t diff = end - start;

  // Handle wraparound for MPEG-TS
  if (timebase.den == 90000) {
    if (diff < -kMpegTsWrapThreshold) {
      // Wraparound occurred: end wrapped, add full range
      diff += (1LL << 33);
    }
  }

  return av_rescale_q(diff, timebase, kWebcodecsTimebase);
}

// ===========================================================================
// AUDIO SPECIFIC
// ===========================================================================

/**
 * Calculate audio duration from sample count.
 *
 * @param sample_count Number of audio samples
 * @param sample_rate Sample rate in Hz
 * @return Duration in microseconds
 */
[[nodiscard]] inline int64_t AudioDurationUs(int64_t sample_count, int sample_rate) {
  if (sample_rate <= 0) {
    return 0;
  }
  return av_rescale(sample_count, 1000000LL, sample_rate);
}

/**
 * Calculate sample count from duration.
 *
 * @param duration_us Duration in microseconds
 * @param sample_rate Sample rate in Hz
 * @return Number of samples
 */
[[nodiscard]] inline int64_t SamplesFromDurationUs(int64_t duration_us, int sample_rate) {
  if (sample_rate <= 0 || duration_us <= 0) {
    return 0;
  }
  return av_rescale(duration_us, sample_rate, 1000000LL);
}

// ===========================================================================
// VALIDATION
// ===========================================================================

/**
 * Check if a timestamp is valid (not AV_NOPTS_VALUE and non-negative).
 */
[[nodiscard]] inline bool IsValidPts(int64_t pts) { return pts != AV_NOPTS_VALUE && pts >= 0; }

/**
 * Clamp timestamp to valid range.
 *
 * @param pts Input PTS
 * @return PTS clamped to [0, INT64_MAX/2] or AV_NOPTS_VALUE if invalid
 */
[[nodiscard]] inline int64_t ClampPts(int64_t pts) {
  if (pts == AV_NOPTS_VALUE) {
    return AV_NOPTS_VALUE;
  }
  if (pts < 0) {
    return 0;
  }
  // Prevent overflow during conversions
  constexpr int64_t kMaxSafePts = INT64_MAX / 2;
  if (pts > kMaxSafePts) {
    return kMaxSafePts;
  }
  return pts;
}

}  // namespace timebase
}  // namespace webcodecs
