# Task: AVC (H.264) Codec Implementation

> **Created:** 2026-01-02
> **Spec:** [docs/specs/codecs/avc.md](../../specs/codecs/avc.md)
> **Branch:** feat/codec-avc

## Success Criteria
- [ ] All tests pass (`npm test`)
- [ ] Type check passes (`npm run typecheck`)
- [ ] Linting clean (`npm run lint`)
- [ ] All checklist items below marked complete
- [ ] PR description created

---

## Phase 1: Investigation (NO CODING)
- [ ] Read AVC codec spec
- [ ] Document FFmpeg H.264 support (libx264, h264_nvenc, etc.)
- [ ] Identify profile/level support
- [ ] List dependencies and constraints
- [ ] Update plan.md with findings

## Phase 2: Planning
- [ ] Create detailed implementation plan
- [ ] Define AVC-specific types
- [ ] Identify parallelizable work units
- [ ] Get human approval on plan
- [ ] Create task packets for subagents (if applicable)

## Phase 3: Implementation

### 3.1 Codec String Parsing
- [ ] Write failing tests for AVC codec strings
- [ ] Confirm tests fail (RED)
- [ ] Implement codec string parsing:
  - `avc1.PPCCLL` (AVC, SPS in description)
  - `avc3.PPCCLL` (AVC, SPS in-band)
  - PP: profile_idc (42=Baseline, 4D=Main, 64=High)
  - CC: constraint_set flags
  - LL: level_idc (e.g., 1F=3.1, 28=4.0)
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.2 VideoDecoderConfig description
- [ ] Write failing tests for description handling
- [ ] Confirm tests fail (RED)
- [ ] Implement description handling:
  - AVCDecoderConfigurationRecord (ISO 14496-15)
  - Extract SPS/PPS NAL units
  - Parse profile, level, dimensions from SPS
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.3 Bitstream Formats
- [ ] Write failing tests for bitstream formats
- [ ] Confirm tests fail (RED)
- [ ] Implement AvcBitstreamFormat:
  - `"avc"`: Length-prefixed NALUs (MP4 style)
  - `"annexb"`: Start code prefixed NALUs (0x000001)
- [ ] Handle conversion between formats
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.4 EncodedVideoChunk data
- [ ] Write failing tests for chunk format
- [ ] Confirm tests fail (RED)
- [ ] Implement chunk handling:
  - Access unit containing one primary picture
  - Key chunk = IDR picture (avc format)
  - Key chunk = IDR + SPS/PPS (annexb format)
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.5 AVC Decoder
- [ ] Write failing tests for AVC decoding
- [ ] Confirm tests fail (RED)
- [ ] Implement AVC decoder:
  - FFmpeg h264 decoder
  - Configure extradata from description
  - Handle both avc and annexb formats
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.6 AVC Encoder
- [ ] Write failing tests for AVC encoding
- [ ] Confirm tests fail (RED)
- [ ] Implement AVC encoder:
  - FFmpeg libx264 (preferred)
  - Support hardware encoders (h264_nvenc, h264_videotoolbox)
  - Generate AVCDecoderConfigurationRecord
  - Emit metadata with decoderConfig
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.7 AvcEncoderConfig Extensions
- [ ] Write failing tests for encoder config
- [ ] Confirm tests fail (RED)
- [ ] Implement AvcEncoderConfig:
  - `format`: AvcBitstreamFormat ("avc" | "annexb")
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.8 AVC Encode Options
- [ ] Write failing tests for encode options
- [ ] Confirm tests fail (RED)
- [ ] Implement VideoEncoderEncodeOptionsForAvc:
  - `quantizer`: per-frame QP (0-51)
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

## Phase 4: Integration
- [ ] Verify all components work together
- [ ] Run full test suite
- [ ] Test with real H.264 video
- [ ] Test hardware encoding if available
- [ ] Update documentation

## Phase 5: Verification
- [ ] Code review checklist complete
- [ ] No hardcoded test values
- [ ] All profiles working (Baseline, Main, High)
- [ ] Error handling complete
- [ ] Types are strict (no `any`)

## Phase 6: Finalize
- [ ] All success criteria met
- [ ] PR description written
- [ ] Ready for human review

---

## Blockers
<!-- Add any blockers encountered -->

## Notes
- FFmpeg codec: AV_CODEC_ID_H264
- libx264 for encoding, native h264 for decoding
- SPS/PPS handling critical for proper playback
- avc1 vs avc3: where SPS/PPS lives
- Hardware acceleration: nvenc, videotoolbox, vaapi
