// src/shared/codec_registry.cpp
#include "codec_registry.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <unordered_map>

namespace webcodecs {

namespace {

// Convert hex string to integer
int HexToInt(const std::string& hex) {
  try {
    return std::stoi(hex, nullptr, 16);
  } catch (...) {
    return -1;
  }
}

// Parse AVC codec string: avc1.PPCCLL
// PP = profile_idc, CC = constraint_set flags, LL = level_idc
std::optional<CodecInfo> ParseAVC(const std::string& params) {
  if (params.length() < 6) {
    return CodecInfo{AV_CODEC_ID_H264, -1, -1, -1};
  }

  int profile = HexToInt(params.substr(0, 2));
  int level = HexToInt(params.substr(4, 2));

  return CodecInfo{AV_CODEC_ID_H264, profile, level, -1};
}

// Parse HEVC codec string: hvc1.P.T.Lxx
std::optional<CodecInfo> ParseHEVC(const std::string& params) {
  if (params.empty()) {
    return CodecInfo{AV_CODEC_ID_HEVC, -1, -1, -1};
  }

  // Basic parsing - full HEVC parsing is complex
  return CodecInfo{AV_CODEC_ID_HEVC, -1, -1, -1};
}

// Parse VP9 codec string: vp09.PP.LL.DD
std::optional<CodecInfo> ParseVP9(const std::string& params) {
  if (params.empty()) {
    return CodecInfo{AV_CODEC_ID_VP9, -1, -1, -1};
  }

  std::istringstream ss(params);
  std::string token;
  int profile = -1, level = -1, bit_depth = -1;

  if (std::getline(ss, token, '.')) profile = std::stoi(token);
  if (std::getline(ss, token, '.')) level = std::stoi(token);
  if (std::getline(ss, token, '.')) bit_depth = std::stoi(token);

  return CodecInfo{AV_CODEC_ID_VP9, profile, level, bit_depth};
}

// Parse AV1 codec string: av01.P.LLT.DD
std::optional<CodecInfo> ParseAV1(const std::string& params) {
  if (params.empty()) {
    return CodecInfo{AV_CODEC_ID_AV1, -1, -1, -1};
  }

  std::istringstream ss(params);
  std::string token;
  int profile = -1, level = -1, bit_depth = -1;

  if (std::getline(ss, token, '.')) profile = std::stoi(token);
  if (std::getline(ss, token, '.')) {
    // Level + tier (e.g., "04M" = level 4, Main tier)
    level = std::stoi(token.substr(0, 2));
  }
  if (std::getline(ss, token, '.')) bit_depth = std::stoi(token);

  return CodecInfo{AV_CODEC_ID_AV1, profile, level, bit_depth};
}

// Parse AAC codec string: mp4a.40.X
std::optional<CodecInfo> ParseAAC(const std::string& params) {
  if (params.empty() || params.substr(0, 2) != "40") {
    return std::nullopt;  // Not AAC
  }

  int profile = -1;
  if (params.length() >= 4 && params[2] == '.') {
    profile = std::stoi(params.substr(3));
  }

  return CodecInfo{AV_CODEC_ID_AAC, profile, -1, -1};
}

// Parse PCM codec string: pcm-<format>
std::optional<CodecInfo> ParsePCM(const std::string& format) {
  static const std::unordered_map<std::string, AVCodecID> pcm_formats = {
    {"s16le", AV_CODEC_ID_PCM_S16LE},
    {"s16be", AV_CODEC_ID_PCM_S16BE},
    {"s24le", AV_CODEC_ID_PCM_S24LE},
    {"s24be", AV_CODEC_ID_PCM_S24BE},
    {"s32le", AV_CODEC_ID_PCM_S32LE},
    {"s32be", AV_CODEC_ID_PCM_S32BE},
    {"f32le", AV_CODEC_ID_PCM_F32LE},
    {"f32be", AV_CODEC_ID_PCM_F32BE},
    {"u8", AV_CODEC_ID_PCM_U8},
  };

  auto it = pcm_formats.find(format);
  if (it != pcm_formats.end()) {
    return CodecInfo{it->second, -1, -1, -1};
  }
  return std::nullopt;
}

}  // namespace

std::optional<CodecInfo> ParseCodecString(const std::string& codec_string) {
  if (codec_string.empty()) {
    return std::nullopt;
  }

  // Find the prefix (before first dot or entire string)
  size_t dot_pos = codec_string.find('.');
  std::string prefix = (dot_pos != std::string::npos)
    ? codec_string.substr(0, dot_pos)
    : codec_string;
  std::string params = (dot_pos != std::string::npos)
    ? codec_string.substr(dot_pos + 1)
    : "";

  // Convert prefix to lowercase for comparison
  std::string lower_prefix = prefix;
  std::transform(lower_prefix.begin(), lower_prefix.end(),
                 lower_prefix.begin(), ::tolower);

  // Video codecs
  if (lower_prefix == "avc1" || lower_prefix == "avc3") {
    return ParseAVC(params);
  }
  if (lower_prefix == "hvc1" || lower_prefix == "hev1") {
    return ParseHEVC(params);
  }
  if (lower_prefix == "vp8") {
    return CodecInfo{AV_CODEC_ID_VP8, -1, -1, -1};
  }
  if (lower_prefix == "vp09" || lower_prefix == "vp9") {
    return ParseVP9(params);
  }
  if (lower_prefix == "av01" || lower_prefix == "av1") {
    return ParseAV1(params);
  }

  // Audio codecs
  if (lower_prefix == "mp4a") {
    return ParseAAC(params);
  }
  if (lower_prefix == "opus") {
    return CodecInfo{AV_CODEC_ID_OPUS, -1, -1, -1};
  }
  if (lower_prefix == "flac") {
    return CodecInfo{AV_CODEC_ID_FLAC, -1, -1, -1};
  }
  if (lower_prefix == "mp3") {
    return CodecInfo{AV_CODEC_ID_MP3, -1, -1, -1};
  }
  if (lower_prefix == "vorbis") {
    return CodecInfo{AV_CODEC_ID_VORBIS, -1, -1, -1};
  }
  if (lower_prefix == "ulaw") {
    return CodecInfo{AV_CODEC_ID_PCM_MULAW, -1, -1, -1};
  }
  if (lower_prefix == "alaw") {
    return CodecInfo{AV_CODEC_ID_PCM_ALAW, -1, -1, -1};
  }

  // PCM formats
  if (codec_string.substr(0, 4) == "pcm-") {
    return ParsePCM(codec_string.substr(4));
  }

  return std::nullopt;
}

std::string GetCodecPrefix(AVCodecID codec_id) {
  switch (codec_id) {
    case AV_CODEC_ID_H264: return CODEC_AVC;
    case AV_CODEC_ID_HEVC: return CODEC_HEVC;
    case AV_CODEC_ID_VP8: return CODEC_VP8;
    case AV_CODEC_ID_VP9: return CODEC_VP9;
    case AV_CODEC_ID_AV1: return CODEC_AV1;
    case AV_CODEC_ID_AAC: return CODEC_AAC;
    case AV_CODEC_ID_OPUS: return CODEC_OPUS;
    case AV_CODEC_ID_FLAC: return CODEC_FLAC;
    case AV_CODEC_ID_MP3: return CODEC_MP3;
    case AV_CODEC_ID_VORBIS: return CODEC_VORBIS;
    case AV_CODEC_ID_PCM_MULAW: return CODEC_ULAW;
    case AV_CODEC_ID_PCM_ALAW: return CODEC_ALAW;
    default: return "";
  }
}

bool IsCodecSupported(const std::string& codec_string) {
  auto info = ParseCodecString(codec_string);
  if (!info) {
    return false;
  }

  // Check if FFmpeg has a decoder for this codec
  const AVCodec* decoder = avcodec_find_decoder(info->codec_id);
  return decoder != nullptr;
}

}  // namespace webcodecs
