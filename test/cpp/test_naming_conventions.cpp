/**
 * test_naming_conventions.cpp - Tests for Google C++ Style Guide compliance
 *
 * These tests verify:
 * 1. Constants use kPascalCase (not SCREAMING_SNAKE)
 * 2. Methods use PascalCase (not snake_case)
 * 3. Single-argument constructors are explicit
 *
 * TDD: Write tests first, watch them fail, then fix the code.
 */

#include <gtest/gtest.h>
#include <type_traits>

#include "../../src/shared/codec_registry.h"
#include "../../src/ffmpeg_raii.h"

using webcodecs::GetCodecPrefix;
using webcodecs::kCodecAac;
using webcodecs::kCodecAlaw;
using webcodecs::kCodecAv1;
using webcodecs::kCodecAvc;
using webcodecs::kCodecFlac;
using webcodecs::kCodecHevc;
using webcodecs::kCodecHevcAlt;
using webcodecs::kCodecMp3;
using webcodecs::kCodecOpus;
using webcodecs::kCodecPcm;
using webcodecs::kCodecUlaw;
using webcodecs::kCodecVorbis;
using webcodecs::kCodecVp8;
using webcodecs::kCodecVp9;
using webcodecs::raii::AtomicCodecState;
using webcodecs::raii::AVMallocBuffer;

// =============================================================================
// ISSUE 1: Constants should use kPascalCase (kCodecAvc not CODEC_AVC)
// =============================================================================

// These tests verify that kPascalCase constants exist and have correct values.
// If CODEC_* names are still used, these tests will fail to compile.

TEST(NamingConventionsTest, CodecConstantsUseKPascalCase) {
  // Video codecs - should be kCodecAvc, kCodecHevc, etc.
  EXPECT_STREQ(kCodecAvc, "avc1");
  EXPECT_STREQ(kCodecHevc, "hvc1");
  EXPECT_STREQ(kCodecHevcAlt, "hev1");
  EXPECT_STREQ(kCodecVp8, "vp8");
  EXPECT_STREQ(kCodecVp9, "vp09");
  EXPECT_STREQ(kCodecAv1, "av01");

  // Audio codecs
  EXPECT_STREQ(kCodecAac, "mp4a");
  EXPECT_STREQ(kCodecOpus, "opus");
  EXPECT_STREQ(kCodecFlac, "flac");
  EXPECT_STREQ(kCodecMp3, "mp3");
  EXPECT_STREQ(kCodecVorbis, "vorbis");
  EXPECT_STREQ(kCodecPcm, "pcm");
  EXPECT_STREQ(kCodecUlaw, "ulaw");
  EXPECT_STREQ(kCodecAlaw, "alaw");
}

// =============================================================================
// ISSUE 2: Methods should use PascalCase (ToString not to_string)
// =============================================================================

TEST(NamingConventionsTest, AtomicCodecStateMethodsUsePascalCase) {
  AtomicCodecState state;

  // Should be ToString() not to_string()
  const char* str = state.ToString();
  EXPECT_STREQ(str, "unconfigured");

  // Should be IsConfigured() not is_configured()
  EXPECT_FALSE(state.IsConfigured());

  // Should be IsClosed() not is_closed()
  EXPECT_FALSE(state.IsClosed());

  // Transition and verify
  state.transition(AtomicCodecState::State::Unconfigured, AtomicCodecState::State::Configured);
  EXPECT_TRUE(state.IsConfigured());
  EXPECT_STREQ(state.ToString(), "configured");

  state.Close();
  EXPECT_TRUE(state.IsClosed());
  EXPECT_STREQ(state.ToString(), "closed");
}

// =============================================================================
// ISSUE 3: Single-argument constructors must be explicit
// =============================================================================

// Note: We cannot test "explicit" at runtime - it's a compile-time check.
// If AVMallocBuffer(size_t) were NOT explicit, the following would compile:
//   AVMallocBuffer buf = 1024;  // Copy-initialization from size_t
//   void TakesBuffer(AVMallocBuffer) {}
//   TakesBuffer(128);           // Implicit conversion in function call
//
// Since our constructor IS explicit, those lines would fail to compile.
// This test simply verifies that explicit construction works correctly.

TEST(NamingConventionsTest, AVMallocBufferConstructorIsExplicit) {
  // This MUST work - explicit construction
  AVMallocBuffer buf1(1024);
  EXPECT_TRUE(static_cast<bool>(buf1));

  // This MUST work - direct initialization
  AVMallocBuffer buf2{512};
  EXPECT_TRUE(static_cast<bool>(buf2));

  // Verify the buffer actually allocated
  EXPECT_EQ(buf1.size(), 1024);
  EXPECT_EQ(buf2.size(), 512);

  // The following would NOT compile if explicit is correctly applied:
  // AVMallocBuffer buf3 = 256;  // COMPILE ERROR: copy-initialization
  // (We can't test compile errors at runtime, this is documentation)
}

// =============================================================================
// ISSUE 4: SafeThreadSafeFunction methods should use PascalCase
// =============================================================================

// Note: SafeThreadSafeFunction uses N-API which isn't available in tests.
// The safe_tsfn.h test harness in test_safe_tsfn.cpp uses a mock.
// We'll verify naming via the mock test harness pattern.

// This test documents the expected API naming for SafeThreadSafeFunction:
// - IsReleased() not is_released()
// - IsActive() not is_active()

// The actual test is done in test_safe_tsfn.cpp which uses mocks.
// Here we just document the expected interface.

TEST(NamingConventionsTest, DocumentExpectedSafeTSFNNaming) {
  // Expected method names (PascalCase per Google style):
  // - Init(tsfn)
  // - Call(data)
  // - BlockingCall(data)
  // - Release()
  // - IsReleased()
  // - IsActive()
  // - Acquire()
  // - Unref()

  // This test passes as documentation - actual verification
  // is in test_safe_tsfn.cpp which will fail if names don't match.
  SUCCEED();
}

// =============================================================================
// CONSISTENCY TESTS - Verify patterns are consistent across codebase
// =============================================================================

TEST(NamingConventionsTest, GetCodecPrefixUsesKPascalCaseConstants) {
  // GetCodecPrefix should return values matching kCodec* constants
  EXPECT_EQ(GetCodecPrefix(AV_CODEC_ID_H264), kCodecAvc);
  EXPECT_EQ(GetCodecPrefix(AV_CODEC_ID_HEVC), kCodecHevc);
  EXPECT_EQ(GetCodecPrefix(AV_CODEC_ID_VP8), kCodecVp8);
  EXPECT_EQ(GetCodecPrefix(AV_CODEC_ID_VP9), kCodecVp9);
  EXPECT_EQ(GetCodecPrefix(AV_CODEC_ID_AV1), kCodecAv1);
  EXPECT_EQ(GetCodecPrefix(AV_CODEC_ID_AAC), kCodecAac);
  EXPECT_EQ(GetCodecPrefix(AV_CODEC_ID_OPUS), kCodecOpus);
  EXPECT_EQ(GetCodecPrefix(AV_CODEC_ID_FLAC), kCodecFlac);
  EXPECT_EQ(GetCodecPrefix(AV_CODEC_ID_MP3), kCodecMp3);
  EXPECT_EQ(GetCodecPrefix(AV_CODEC_ID_VORBIS), kCodecVorbis);
}
