/**
 * test_codec_registry.cpp - Unit tests for codec string parsing
 *
 * Tests W3C codec string parsing including edge cases that previously
 * used std::stoi (which throws exceptions on invalid input).
 *
 * Key requirement: NO EXCEPTIONS should propagate from parsing functions.
 * Invalid input should return std::nullopt or -1, not throw.
 */

#include <gtest/gtest.h>
#include <string>

#include "../../src/shared/codec_registry.h"

using webcodecs::GetCodecPrefix;
using webcodecs::IsCodecSupported;
using webcodecs::ParseCodecString;

// =============================================================================
// VALID CODEC STRINGS - HAPPY PATH
// =============================================================================

TEST(CodecRegistryTest, ParseAVC_ValidBaseline) {
  auto info = ParseCodecString("avc1.42E01E");
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->codec_id, AV_CODEC_ID_H264);
  EXPECT_EQ(info->profile, 0x42);  // Baseline
  EXPECT_EQ(info->level, 0x1E);    // Level 3.0
}

TEST(CodecRegistryTest, ParseAVC_ValidHigh) {
  auto info = ParseCodecString("avc1.640028");
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->codec_id, AV_CODEC_ID_H264);
  EXPECT_EQ(info->profile, 0x64);  // High
  EXPECT_EQ(info->level, 0x28);    // Level 4.0
}

TEST(CodecRegistryTest, ParseAVC_Lowercase) {
  auto info = ParseCodecString("AVC1.42E01E");
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->codec_id, AV_CODEC_ID_H264);
}

TEST(CodecRegistryTest, ParseHEVC_Valid) {
  auto info = ParseCodecString("hvc1.1.6.L93.B0");
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->codec_id, AV_CODEC_ID_HEVC);
}

TEST(CodecRegistryTest, ParseVP8_Valid) {
  auto info = ParseCodecString("vp8");
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->codec_id, AV_CODEC_ID_VP8);
}

TEST(CodecRegistryTest, ParseVP9_Valid) {
  auto info = ParseCodecString("vp09.00.10.08");
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->codec_id, AV_CODEC_ID_VP9);
  EXPECT_EQ(info->profile, 0);
  EXPECT_EQ(info->level, 10);
  EXPECT_EQ(info->bit_depth, 8);
}

TEST(CodecRegistryTest, ParseAV1_Valid) {
  auto info = ParseCodecString("av01.0.04M.08");
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->codec_id, AV_CODEC_ID_AV1);
  EXPECT_EQ(info->profile, 0);
  EXPECT_EQ(info->level, 4);
  EXPECT_EQ(info->bit_depth, 8);
}

TEST(CodecRegistryTest, ParseOpus_Valid) {
  auto info = ParseCodecString("opus");
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->codec_id, AV_CODEC_ID_OPUS);
}

TEST(CodecRegistryTest, ParseAAC_Valid) {
  auto info = ParseCodecString("mp4a.40.2");
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->codec_id, AV_CODEC_ID_AAC);
  EXPECT_EQ(info->profile, 2);  // AAC-LC
}

TEST(CodecRegistryTest, ParsePCM_Valid) {
  auto info = ParseCodecString("pcm-s16le");
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->codec_id, AV_CODEC_ID_PCM_S16LE);
}

// =============================================================================
// INVALID CODEC STRINGS - SAD PATH (NO EXCEPTIONS!)
// =============================================================================

TEST(CodecRegistryTest, ParseCodecString_Empty) {
  auto info = ParseCodecString("");
  EXPECT_FALSE(info.has_value());
}

TEST(CodecRegistryTest, ParseCodecString_Unknown) {
  auto info = ParseCodecString("unknown.codec.string");
  EXPECT_FALSE(info.has_value());
}

// =============================================================================
// HEX PARSING EDGE CASES - These would throw with std::stoi
// =============================================================================

TEST(CodecRegistryTest, ParseAVC_InvalidHex_NotHex) {
  // "ZZZZZZ" is not valid hex - std::stoi would throw std::invalid_argument
  // This MUST NOT throw - should return valid CodecInfo with profile/level = -1
  auto info = ParseCodecString("avc1.ZZZZZZ");
  ASSERT_TRUE(info.has_value());  // Still H264, just can't parse profile/level
  EXPECT_EQ(info->codec_id, AV_CODEC_ID_H264);
  EXPECT_EQ(info->profile, -1);
  EXPECT_EQ(info->level, -1);
}

TEST(CodecRegistryTest, ParseAVC_InvalidHex_Empty) {
  // Empty params - should not crash
  auto info = ParseCodecString("avc1.");
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->codec_id, AV_CODEC_ID_H264);
}

TEST(CodecRegistryTest, ParseAVC_InvalidHex_TooShort) {
  // Too short to parse profile/level
  auto info = ParseCodecString("avc1.42");
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->codec_id, AV_CODEC_ID_H264);
}

TEST(CodecRegistryTest, ParseAVC_InvalidHex_Overflow) {
  // Very large hex values - would overflow int
  auto info = ParseCodecString("avc1.FFFFFFFFFFFFFFFF");
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->codec_id, AV_CODEC_ID_H264);
}

TEST(CodecRegistryTest, ParseAVC_InvalidHex_Negative) {
  // "-4" is valid input to strtol base 16 - parses as negative 4
  // This tests that we don't throw on unusual but parseable input
  auto info = ParseCodecString("avc1.-42E0-1E");
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->codec_id, AV_CODEC_ID_H264);
  // "-4" parses as -4 (strtol accepts leading minus for negative hex)
  EXPECT_EQ(info->profile, -4);
  // substr(4,2) of "-42E0-1E" is "0-" which fails to parse
  EXPECT_EQ(info->level, -1);
}

TEST(CodecRegistryTest, ParseAVC_InvalidHex_SpecialChars) {
  // Special characters in hex string
  auto info = ParseCodecString("avc1.42!@#$");
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->codec_id, AV_CODEC_ID_H264);
}

// =============================================================================
// VP9 PARSING EDGE CASES - Uses std::stoi for profile/level/bitdepth
// =============================================================================

TEST(CodecRegistryTest, ParseVP9_InvalidProfile_NotNumber) {
  // "XX" is not a valid integer - std::stoi would throw
  auto info = ParseCodecString("vp09.XX.10.08");
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->codec_id, AV_CODEC_ID_VP9);
  EXPECT_EQ(info->profile, -1);  // Should be -1 on parse failure
}

TEST(CodecRegistryTest, ParseVP9_InvalidLevel_NotNumber) {
  auto info = ParseCodecString("vp09.00.YY.08");
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->codec_id, AV_CODEC_ID_VP9);
  EXPECT_EQ(info->level, -1);
}

TEST(CodecRegistryTest, ParseVP9_InvalidBitDepth_NotNumber) {
  auto info = ParseCodecString("vp09.00.10.ZZ");
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->codec_id, AV_CODEC_ID_VP9);
  EXPECT_EQ(info->bit_depth, -1);
}

TEST(CodecRegistryTest, ParseVP9_Overflow) {
  // Very large numbers that would overflow
  auto info = ParseCodecString("vp09.999999999999999.10.08");
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->codec_id, AV_CODEC_ID_VP9);
}

TEST(CodecRegistryTest, ParseVP9_NegativeNumbers) {
  auto info = ParseCodecString("vp09.-1.-10.-8");
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->codec_id, AV_CODEC_ID_VP9);
  // Negative values parsed but should be treated as valid
  EXPECT_EQ(info->profile, -1);
}

TEST(CodecRegistryTest, ParseVP9_EmptyParams) {
  auto info = ParseCodecString("vp09.");
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->codec_id, AV_CODEC_ID_VP9);
}

TEST(CodecRegistryTest, ParseVP9_PartialParams) {
  // Only profile, missing level and bitdepth
  auto info = ParseCodecString("vp09.00");
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->codec_id, AV_CODEC_ID_VP9);
  EXPECT_EQ(info->profile, 0);
  EXPECT_EQ(info->level, -1);
  EXPECT_EQ(info->bit_depth, -1);
}

TEST(CodecRegistryTest, ParseVP9_SpecialChars) {
  auto info = ParseCodecString("vp09.!@#.$%^.&*(");
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->codec_id, AV_CODEC_ID_VP9);
}

// =============================================================================
// AV1 PARSING EDGE CASES - Uses std::stoi for profile/level/bitdepth
// =============================================================================

TEST(CodecRegistryTest, ParseAV1_InvalidProfile_NotNumber) {
  auto info = ParseCodecString("av01.X.04M.08");
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->codec_id, AV_CODEC_ID_AV1);
  EXPECT_EQ(info->profile, -1);
}

TEST(CodecRegistryTest, ParseAV1_InvalidLevel_NotNumber) {
  // Level token "XXM" - substr(0,2) = "XX" which is not a number
  auto info = ParseCodecString("av01.0.XXM.08");
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->codec_id, AV_CODEC_ID_AV1);
  EXPECT_EQ(info->level, -1);
}

TEST(CodecRegistryTest, ParseAV1_InvalidBitDepth_NotNumber) {
  auto info = ParseCodecString("av01.0.04M.YY");
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->codec_id, AV_CODEC_ID_AV1);
  EXPECT_EQ(info->bit_depth, -1);
}

TEST(CodecRegistryTest, ParseAV1_Overflow) {
  auto info = ParseCodecString("av01.9999999999999.04M.08");
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->codec_id, AV_CODEC_ID_AV1);
}

TEST(CodecRegistryTest, ParseAV1_ShortLevel) {
  // Level token too short for substr(0,2)
  auto info = ParseCodecString("av01.0.X.08");
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->codec_id, AV_CODEC_ID_AV1);
}

TEST(CodecRegistryTest, ParseAV1_EmptyParams) {
  auto info = ParseCodecString("av01.");
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->codec_id, AV_CODEC_ID_AV1);
}

// =============================================================================
// AAC PARSING EDGE CASES - Uses std::stoi for profile
// =============================================================================

TEST(CodecRegistryTest, ParseAAC_InvalidProfile_NotNumber) {
  auto info = ParseCodecString("mp4a.40.XX");
  // Should return nullopt since we can't parse AAC profile
  // (AAC requires valid profile for identification)
  // Actually, looking at code, it still returns AAC with profile=-1
  // Let me check... yes, it returns AAC with -1 profile
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->codec_id, AV_CODEC_ID_AAC);
}

TEST(CodecRegistryTest, ParseAAC_Overflow) {
  auto info = ParseCodecString("mp4a.40.99999999999999999");
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->codec_id, AV_CODEC_ID_AAC);
}

TEST(CodecRegistryTest, ParseAAC_NegativeProfile) {
  auto info = ParseCodecString("mp4a.40.-2");
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->codec_id, AV_CODEC_ID_AAC);
}

// =============================================================================
// IsCodecSupported TESTS
// =============================================================================

TEST(CodecRegistryTest, IsCodecSupported_ValidCodec) {
  // H.264 is almost always supported
  EXPECT_TRUE(IsCodecSupported("avc1.42E01E"));
}

TEST(CodecRegistryTest, IsCodecSupported_UnknownCodec) { EXPECT_FALSE(IsCodecSupported("unknown.codec")); }

TEST(CodecRegistryTest, IsCodecSupported_Empty) { EXPECT_FALSE(IsCodecSupported("")); }

TEST(CodecRegistryTest, IsCodecSupported_InvalidInput_NoThrow) {
  // This should not throw even with garbage input
  EXPECT_FALSE(IsCodecSupported("!@#$%^&*()"));
}

// =============================================================================
// GetCodecPrefix TESTS
// =============================================================================

TEST(CodecRegistryTest, GetCodecPrefix_H264) { EXPECT_EQ(GetCodecPrefix(AV_CODEC_ID_H264), "avc1"); }

TEST(CodecRegistryTest, GetCodecPrefix_VP9) { EXPECT_EQ(GetCodecPrefix(AV_CODEC_ID_VP9), "vp09"); }

TEST(CodecRegistryTest, GetCodecPrefix_Unknown) { EXPECT_EQ(GetCodecPrefix(AV_CODEC_ID_NONE), ""); }

// =============================================================================
// STRESS TESTS - Many invalid inputs should not crash
// =============================================================================

TEST(CodecRegistryTest, StressTest_ManyInvalidStrings) {
  // Should not crash or throw on any of these
  const char* invalid_strings[] = {
      "",
      ".",
      "..",
      "...",
      "avc1.",
      "avc1..",
      "avc1...",
      "vp09.a.b.c",
      "av01.x.y.z",
      "mp4a.40.",
      "pcm-",
      "pcm-unknown",
      "\x00\x01\x02",  // Binary garbage
      "avc1.FFFFFFFFFFFFFFFFFFFFFFFFFF",
      "vp09.99999999999999999999999999.0.0",
      nullptr  // End marker
  };

  for (int i = 0; invalid_strings[i] != nullptr; i++) {
    // Should not throw or crash
    auto result = ParseCodecString(invalid_strings[i]);
    // Result can be valid or invalid, just shouldn't crash
    (void)result;
  }
}
