# Task: Codec Registry Implementation

> **Created:** 2026-01-02
> **Spec:** [docs/specs/codecs/00-codec-registry.md](../../specs/codecs/00-codec-registry.md)
> **Branch:** feat/codec-registry

## Success Criteria
- [ ] All tests pass (`npm test`)
- [ ] Type check passes (`npm run typecheck`)
- [ ] Linting clean (`npm run lint`)
- [ ] All checklist items below marked complete
- [ ] PR description created

---

## Phase 1: Investigation (NO CODING)
- [ ] Read codec string parsing code
- [ ] Document current registry patterns in NOTES.md
- [ ] Identify missing codec support
- [ ] List dependencies and constraints
- [ ] Update plan.md with findings

## Phase 2: Planning
- [ ] Create detailed implementation plan
- [ ] Define registry data structures
- [ ] Identify parallelizable work units
- [ ] Get human approval on plan
- [ ] Create task packets for subagents (if applicable)

## Phase 3: Implementation

### 3.1 Audio Codec Registry
- [ ] Write failing tests for audio codecs
- [ ] Confirm tests fail (RED)
- [ ] Implement audio codec registry:
  - `flac` -> FLAC
  - `mp3` -> MP3
  - `mp4a.*` -> AAC
  - `opus` -> Opus
  - `vorbis` -> Vorbis
  - `ulaw` -> u-law PCM
  - `alaw` -> A-law PCM
  - `pcm-*` -> Linear PCM
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.2 Video Codec Registry
- [ ] Write failing tests for video codecs
- [ ] Confirm tests fail (RED)
- [ ] Implement video codec registry:
  - `av01.*` -> AV1
  - `avc1.*`, `avc3.*` -> AVC/H.264
  - `hev1.*`, `hvc1.*` -> HEVC/H.265
  - `vp8` -> VP8
  - `vp09.*` -> VP9
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.3 Codec String Validation
- [ ] Write failing tests for string validation
- [ ] Confirm tests fail (RED)
- [ ] Implement validation:
  - Fixed strings (opus, mp3, vp8)
  - Prefix with variable suffix (avc1.*, av01.*)
  - Multiple string aliases (avc1/avc3)
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.4 FFmpeg Codec ID Mapping
- [ ] Write failing tests for FFmpeg mapping
- [ ] Confirm tests fail (RED)
- [ ] Implement FFmpeg codec mapping:
  - Map codec strings to AV_CODEC_ID_*
  - Query decoder/encoder availability
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

## Phase 4: Integration
- [ ] Verify all components work together
- [ ] Run full test suite
- [ ] Test all registered codecs
- [ ] Update documentation

## Phase 5: Verification
- [ ] Code review checklist complete
- [ ] No hardcoded test values
- [ ] All registered codecs working
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
- Registry is the canonical source for codec support
- Each codec has detailed registration spec
- FFmpeg must have decoder/encoder for support
- Codec strings follow RFC 6381 where applicable
