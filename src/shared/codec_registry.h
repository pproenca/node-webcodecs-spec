// src/shared/codec_registry.h
#pragma once

#include <string>
#include <optional>

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace webcodecs {

/**
 * Codec Registry - Maps W3C codec strings to FFmpeg AVCodecID.
 *
 * W3C codec strings follow the pattern defined in:
 * - RFC 6381 (MP4)
 * - WebM codec registry
 * - AV1 codec string format
 *
 * Examples:
 *   "avc1.42E01E" -> AV_CODEC_ID_H264 (Baseline Profile, Level 3.0)
 *   "hvc1.1.6.L93.B0" -> AV_CODEC_ID_HEVC
 *   "vp8" -> AV_CODEC_ID_VP8
 *   "vp09.00.10.08" -> AV_CODEC_ID_VP9
 *   "av01.0.04M.08" -> AV_CODEC_ID_AV1
 *   "mp4a.40.2" -> AV_CODEC_ID_AAC
 *   "opus" -> AV_CODEC_ID_OPUS
 *   "flac" -> AV_CODEC_ID_FLAC
 *   "mp3" -> AV_CODEC_ID_MP3
 *   "pcm-s16le" -> AV_CODEC_ID_PCM_S16LE
 */

struct CodecInfo {
  AVCodecID codec_id;
  int profile;    // -1 if not applicable
  int level;      // -1 if not applicable
  int bit_depth;  // -1 if not applicable
};

/**
 * Parse a W3C codec string and return FFmpeg codec information.
 *
 * @param codec_string The W3C codec string (e.g., "avc1.42E01E")
 * @return CodecInfo if parsing succeeded, std::nullopt otherwise
 */
std::optional<CodecInfo> ParseCodecString(const std::string& codec_string);

/**
 * Get the W3C codec string prefix for a given FFmpeg codec ID.
 *
 * @param codec_id The FFmpeg AVCodecID
 * @return The codec string prefix (e.g., "avc1" for H264)
 */
std::string GetCodecPrefix(AVCodecID codec_id);

/**
 * Check if a codec string is supported.
 *
 * @param codec_string The W3C codec string
 * @return true if the codec is recognized and FFmpeg has a decoder/encoder
 */
bool IsCodecSupported(const std::string& codec_string);

// Video codec prefixes (kPascalCase per Google C++ Style Guide)
constexpr const char* kCodecAvc = "avc1";   // H.264/AVC
constexpr const char* kCodecHevc = "hvc1";  // H.265/HEVC
constexpr const char* kCodecHevcAlt = "hev1";
constexpr const char* kCodecVp8 = "vp8";
constexpr const char* kCodecVp9 = "vp09";
constexpr const char* kCodecAv1 = "av01";

// Audio codec prefixes
constexpr const char* kCodecAac = "mp4a";
constexpr const char* kCodecOpus = "opus";
constexpr const char* kCodecFlac = "flac";
constexpr const char* kCodecMp3 = "mp3";
constexpr const char* kCodecVorbis = "vorbis";
constexpr const char* kCodecPcm = "pcm";
constexpr const char* kCodecUlaw = "ulaw";
constexpr const char* kCodecAlaw = "alaw";

}  // namespace webcodecs
