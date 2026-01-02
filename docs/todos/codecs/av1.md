# Task: AV1 Codec Implementation

> **Created:** 2026-01-02
> **Spec:** [docs/specs/codecs/av1.md](../../specs/codecs/av1.md)
> **Branch:** feat/codec-av1

## Success Criteria
- [ ] All tests pass (`npm test`)
- [ ] Type check passes (`npm run typecheck`)
- [ ] Linting clean (`npm run lint`)
- [ ] All checklist items below marked complete
- [ ] PR description created

---

## Phase 1: Investigation (NO CODING)
- [ ] Read AV1 codec spec
- [ ] Document FFmpeg AV1 support (libaom, libsvtav1, libdav1d)
- [ ] Identify profile/level support
- [ ] List dependencies and constraints
- [ ] Update plan.md with findings

## Phase 2: Planning
- [ ] Create detailed implementation plan
- [ ] Define AV1-specific types
- [ ] Identify parallelizable work units
- [ ] Get human approval on plan
- [ ] Create task packets for subagents (if applicable)

## Phase 3: Implementation

### 3.1 Codec String Parsing
- [ ] Write failing tests for AV1 codec strings
- [ ] Confirm tests fail (RED)
- [ ] Implement codec string parsing:
  - Format: `av01.P.LLT.DD.M.CCC.CP.TC.MC.F`
  - P: Profile (0=Main, 1=High, 2=Professional)
  - LL: Level (00-31)
  - T: Tier (M=Main, H=High)
  - DD: Bit depth (08, 10, 12)
  - Extract all parameters
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.2 VideoDecoderConfig description
- [ ] Write failing tests for description handling
- [ ] Confirm tests fail (RED)
- [ ] Implement description handling:
  - AV1CodecConfigurationRecord (if present)
  - Extract sequence header OBUs
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.3 EncodedVideoChunk data
- [ ] Write failing tests for chunk format
- [ ] Confirm tests fail (RED)
- [ ] Implement chunk handling:
  - OBU (Open Bitstream Unit) format
  - Temporal unit structure
  - Key frame detection (sequence header + key frame OBU)
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.4 AV1 Decoder
- [ ] Write failing tests for AV1 decoding
- [ ] Confirm tests fail (RED)
- [ ] Implement AV1 decoder:
  - Use FFmpeg libdav1d (preferred) or libaom-av1
  - Configure from VideoDecoderConfig
  - Handle all profiles and bit depths
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.5 AV1 Encoder
- [ ] Write failing tests for AV1 encoding
- [ ] Confirm tests fail (RED)
- [ ] Implement AV1 encoder:
  - Use FFmpeg libaom-av1 or libsvtav1
  - Support bitrate/quality configuration
  - Handle scalability modes
  - Generate AV1CodecConfigurationRecord
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.6 AV1EncoderConfig Extensions
- [ ] Write failing tests for encoder config
- [ ] Confirm tests fail (RED)
- [ ] Implement Av1EncoderConfig:
  - `forceScreenContentTools` (boolean)
  - Per-frame quantizer support
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.7 Scalability Mode (SVC)
- [ ] Write failing tests for SVC
- [ ] Confirm tests fail (RED)
- [ ] Implement scalabilityMode:
  - L1T1, L1T2, L1T3 (temporal layers)
  - Map to AV1 temporal_id
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

## Phase 4: Integration
- [ ] Verify all components work together
- [ ] Run full test suite
- [ ] Test with real AV1 video
- [ ] Test 8-bit, 10-bit, 12-bit
- [ ] Update documentation

## Phase 5: Verification
- [ ] Code review checklist complete
- [ ] No hardcoded test values
- [ ] All profiles working
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
- FFmpeg codec: AV_CODEC_ID_AV1
- libdav1d for decoding, libaom-av1 or libsvtav1 for encoding
- OBU-based bitstream format
- Codec string per AV1 Codec ISO Media File Format Binding
