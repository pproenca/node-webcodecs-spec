# Task: VP9 Codec Implementation

> **Created:** 2026-01-02
> **Spec:** [docs/specs/codecs/vp9.md](../../specs/codecs/vp9.md)
> **Branch:** feat/codec-vp9

## Success Criteria
- [ ] All tests pass (`npm test`)
- [ ] Type check passes (`npm run typecheck`)
- [ ] Linting clean (`npm run lint`)
- [ ] All checklist items below marked complete
- [ ] PR description created

---

## Phase 1: Investigation (NO CODING)
- [ ] Read VP9 codec spec
- [ ] Document FFmpeg VP9 support (libvpx-vp9)
- [ ] Identify profile/level/bit depth support
- [ ] List dependencies and constraints
- [ ] Update plan.md with findings

## Phase 2: Planning
- [ ] Create detailed implementation plan
- [ ] Define VP9-specific types
- [ ] Identify parallelizable work units
- [ ] Get human approval on plan
- [ ] Create task packets for subagents (if applicable)

## Phase 3: Implementation

### 3.1 Codec String Parsing
- [ ] Write failing tests for VP9 codec strings
- [ ] Confirm tests fail (RED)
- [ ] Implement codec string parsing:
  - Format: `vp09.PP.LL.DD.CC.CP.TC.MC.FF`
  - PP: Profile (00-03)
  - LL: Level (10-62)
  - DD: Bit depth (08, 10, 12)
  - CC: Chroma subsampling
  - Optional: color primaries, transfer, matrix, full range
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.2 VideoDecoderConfig description
- [ ] Write failing tests for description handling
- [ ] Confirm tests fail (RED)
- [ ] Implement description handling:
  - Optional VP9CodecConfigurationRecord
  - Extract profile, bit depth from config
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.3 EncodedVideoChunk data
- [ ] Write failing tests for chunk format
- [ ] Confirm tests fail (RED)
- [ ] Implement chunk handling:
  - VP9 compressed frame
  - Key frame detection from frame header
  - Support superframes
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.4 VP9 Decoder
- [ ] Write failing tests for VP9 decoding
- [ ] Confirm tests fail (RED)
- [ ] Implement decoder:
  - FFmpeg libvpx-vp9 decoder
  - Handle 8-bit and 10-bit content
  - Support all profiles (0-3)
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.5 VP9 Encoder
- [ ] Write failing tests for VP9 encoding
- [ ] Confirm tests fail (RED)
- [ ] Implement encoder:
  - FFmpeg libvpx-vp9 encoder
  - Configure bitrate/quality
  - Support temporal scalability
  - Generate VP9CodecConfigurationRecord
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.6 Vp9EncoderConfig Extensions
- [ ] Write failing tests for encoder config
- [ ] Confirm tests fail (RED)
- [ ] Implement VideoEncoderEncodeOptionsForVp9:
  - `quantizer`: per-frame QP
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.7 Scalability Mode (SVC)
- [ ] Write failing tests for SVC
- [ ] Confirm tests fail (RED)
- [ ] Implement scalabilityMode:
  - L1T2, L1T3 (temporal layers)
  - Map to VP9 temporal_id
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

## Phase 4: Integration
- [ ] Verify all components work together
- [ ] Run full test suite
- [ ] Test with real VP9 video
- [ ] Test 8-bit and 10-bit content
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
- FFmpeg codec: AV_CODEC_ID_VP9
- libvpx-vp9 for encoding/decoding
- Complex codec string with many optional parameters
- Profiles 2/3 support 10/12-bit
- SVC (temporal scalability) commonly used in WebRTC
