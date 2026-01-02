# Task: Codec String Registry Implementation

> **Created:** 2026-01-02
> **Spec:** [docs/specs/18-codec-string-registry.md](../specs/18-codec-string-registry.md)
> **Branch:** feat/codec-registry

## Success Criteria
- [ ] All tests pass (`npm test`)
- [ ] Type check passes (`npm run typecheck`)
- [ ] Linting clean (`npm run lint`)
- [ ] All checklist items below marked complete
- [ ] PR description created

---

## Phase 1: Investigation (NO CODING)
- [ ] Read relevant files: codec string parsing
- [ ] Document current codec handling in NOTES.md
- [ ] Identify supported FFmpeg codecs
- [ ] List dependencies and constraints
- [ ] Update plan.md with findings

## Phase 2: Planning
- [ ] Create detailed implementation plan
- [ ] Define codec string parser
- [ ] Identify parallelizable work units
- [ ] Get human approval on plan
- [ ] Create task packets for subagents (if applicable)

## Phase 3: Implementation

### 3.1 Video Codec Strings
- [ ] Write failing tests for video codec strings
- [ ] Confirm tests fail (RED)
- [ ] Implement video codec string parsing:
  - AVC/H.264: `avc1.PPCCLL`, `avc3.PPCCLL`
  - HEVC/H.265: `hev1.P.T.Lxxx.Cxx`, `hvc1.P.T.Lxxx.Cxx`
  - VP8: `vp8`
  - VP9: `vp09.PP.LL.DD.CC.CP.TC.MC.FF`
  - AV1: `av01.P.LLT.DD.M.CCC.CP.TC.MC.F`
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.2 Audio Codec Strings
- [ ] Write failing tests for audio codec strings
- [ ] Confirm tests fail (RED)
- [ ] Implement audio codec string parsing:
  - AAC: `mp4a.40.2` (AAC-LC), `mp4a.40.5` (HE-AAC)
  - Opus: `opus`
  - FLAC: `flac`
  - MP3: `mp3`
  - Vorbis: `vorbis`
  - PCM: `pcm-*`
  - G.711: `alaw`, `ulaw`
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.3 Codec String Parser
- [ ] Write failing tests for codec parser
- [ ] Confirm tests fail (RED)
- [ ] Implement codec string parser:
  - Extract codec type (avc1, hev1, vp09, etc.)
  - Parse profile/level parameters
  - Validate format per RFC 6381
  - Return structured codec info
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.4 FFmpeg Codec Mapping
- [ ] Write failing tests for FFmpeg mapping
- [ ] Confirm tests fail (RED)
- [ ] Implement codec to FFmpeg mapping:
  ```
  avc1/avc3 -> AV_CODEC_ID_H264
  hev1/hvc1 -> AV_CODEC_ID_HEVC
  vp8       -> AV_CODEC_ID_VP8
  vp09      -> AV_CODEC_ID_VP9
  av01      -> AV_CODEC_ID_AV1
  mp4a.40.2 -> AV_CODEC_ID_AAC
  opus      -> AV_CODEC_ID_OPUS
  flac      -> AV_CODEC_ID_FLAC
  mp3       -> AV_CODEC_ID_MP3
  vorbis    -> AV_CODEC_ID_VORBIS
  pcm-s16le -> AV_CODEC_ID_PCM_S16LE
  alaw      -> AV_CODEC_ID_PCM_ALAW
  ulaw      -> AV_CODEC_ID_PCM_MULAW
  ```
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.5 Profile/Level Extraction
- [ ] Write failing tests for profile/level
- [ ] Confirm tests fail (RED)
- [ ] Implement profile/level extraction:
  - AVC: profile_idc, constraint_set flags, level_idc
  - HEVC: general_profile_idc, general_tier_flag, general_level_idc
  - VP9: profile, level, bit depth
  - AV1: profile, level, tier
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.6 Codec Support Detection
- [ ] Write failing tests for support detection
- [ ] Confirm tests fail (RED)
- [ ] Implement isConfigSupported codec check:
  - Query FFmpeg for decoder/encoder availability
  - Check profile support
  - Check level support
  - Check hardware acceleration availability
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.7 Codec String Validation
- [ ] Write failing tests for validation
- [ ] Confirm tests fail (RED)
- [ ] Implement codec string validation:
  - Check format correctness
  - Validate parameter ranges
  - Return TypeError for invalid strings
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

## Phase 4: Integration
- [ ] Verify all components work together
- [ ] Run full test suite
- [ ] Test all supported codecs
- [ ] Test invalid codec strings
- [ ] Update documentation

## Phase 5: Verification
- [ ] Code review checklist complete
- [ ] No hardcoded test values
- [ ] All codec strings per spec/RFC 6381
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
- Codec strings follow RFC 6381 and W3C registry
- AVC/HEVC codec strings encode profile/level
- FFmpeg may support subset of profiles
- Profile detection needed for accurate isConfigSupported
- Some codecs (VP8, Opus) have simple strings
