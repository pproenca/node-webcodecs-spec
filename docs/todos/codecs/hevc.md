# Task: HEVC (H.265) Codec Implementation

> **Created:** 2026-01-02
> **Spec:** [docs/specs/codecs/hevc.md](../../specs/codecs/hevc.md)
> **Branch:** feat/codec-hevc

## Success Criteria
- [ ] All tests pass (`npm test`)
- [ ] Type check passes (`npm run typecheck`)
- [ ] Linting clean (`npm run lint`)
- [ ] All checklist items below marked complete
- [ ] PR description created

---

## Phase 1: Investigation (NO CODING)
- [ ] Read HEVC codec spec
- [ ] Document FFmpeg HEVC support (libx265, hevc_nvenc)
- [ ] Identify profile/level/tier support
- [ ] List dependencies and constraints
- [ ] Update plan.md with findings

## Phase 2: Planning
- [ ] Create detailed implementation plan
- [ ] Define HEVC-specific types
- [ ] Identify parallelizable work units
- [ ] Get human approval on plan
- [ ] Create task packets for subagents (if applicable)

## Phase 3: Implementation

### 3.1 Codec String Parsing
- [ ] Write failing tests for HEVC codec strings
- [ ] Confirm tests fail (RED)
- [ ] Implement codec string parsing:
  - `hev1.P.T.Lxxx.Cxx` (HEVC, VPS/SPS/PPS in description)
  - `hvc1.P.T.Lxxx.Cxx` (HEVC, parameter sets in-band)
  - P: general_profile_idc (1=Main, 2=Main10)
  - T: general_tier_flag (L=Main, H=High)
  - L: general_level_idc
  - C: constraint flags
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.2 VideoDecoderConfig description
- [ ] Write failing tests for description handling
- [ ] Confirm tests fail (RED)
- [ ] Implement description handling:
  - HEVCDecoderConfigurationRecord (ISO 14496-15)
  - Extract VPS/SPS/PPS NAL units
  - Parse profile, level, tier, dimensions
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.3 Bitstream Formats
- [ ] Write failing tests for bitstream formats
- [ ] Confirm tests fail (RED)
- [ ] Implement HevcBitstreamFormat:
  - `"hevc"`: Length-prefixed NALUs (MP4 style)
  - `"annexb"`: Start code prefixed NALUs
- [ ] Handle conversion between formats
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.4 EncodedVideoChunk data
- [ ] Write failing tests for chunk format
- [ ] Confirm tests fail (RED)
- [ ] Implement chunk handling:
  - Access unit containing one picture
  - Key chunk = IDR/CRA picture (hevc format)
  - Key chunk = IDR/CRA + VPS/SPS/PPS (annexb)
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.5 HEVC Decoder
- [ ] Write failing tests for HEVC decoding
- [ ] Confirm tests fail (RED)
- [ ] Implement HEVC decoder:
  - FFmpeg hevc decoder
  - Configure extradata from description
  - Handle 8-bit and 10-bit content
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.6 HEVC Encoder
- [ ] Write failing tests for HEVC encoding
- [ ] Confirm tests fail (RED)
- [ ] Implement HEVC encoder:
  - FFmpeg libx265 (preferred)
  - Support hardware encoders (hevc_nvenc, hevc_videotoolbox)
  - Generate HEVCDecoderConfigurationRecord
  - Emit metadata with decoderConfig
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.7 HevcEncoderConfig Extensions
- [ ] Write failing tests for encoder config
- [ ] Confirm tests fail (RED)
- [ ] Implement HevcEncoderConfig:
  - `format`: HevcBitstreamFormat ("hevc" | "annexb")
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

## Phase 4: Integration
- [ ] Verify all components work together
- [ ] Run full test suite
- [ ] Test with real HEVC video
- [ ] Test 8-bit and 10-bit content
- [ ] Update documentation

## Phase 5: Verification
- [ ] Code review checklist complete
- [ ] No hardcoded test values
- [ ] All profiles working (Main, Main10)
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
- FFmpeg codec: AV_CODEC_ID_HEVC
- libx265 for encoding, native hevc for decoding
- VPS/SPS/PPS handling similar to AVC but more complex
- hev1 vs hvc1: where parameter sets live
- 10-bit HDR content common with HEVC
