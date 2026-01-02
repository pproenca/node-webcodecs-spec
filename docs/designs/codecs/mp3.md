# Task: MP3 Codec Implementation

> **Created:** 2026-01-02
> **Spec:** [docs/specs/codecs/mp3.md](../../specs/codecs/mp3.md)
> **Branch:** feat/codec-mp3

## Success Criteria
- [ ] All tests pass (`npm test`)
- [ ] Type check passes (`npm run typecheck`)
- [ ] Linting clean (`npm run lint`)
- [ ] All checklist items below marked complete
- [ ] PR description created

---

## Phase 1: Investigation (NO CODING)
- [ ] Read MP3 codec spec (ISO 11172-3, ISO 13818-3)
- [ ] Document FFmpeg MP3 support in NOTES.md
- [ ] Identify layer support (typically Layer 3)
- [ ] List dependencies and constraints
- [ ] Update plan.md with findings

## Phase 2: Planning
- [ ] Create detailed implementation plan
- [ ] Define MP3 handling
- [ ] Identify parallelizable work units
- [ ] Get human approval on plan
- [ ] Create task packets for subagents (if applicable)

## Phase 3: Implementation

### 3.1 Codec String
- [ ] Write failing tests for mp3 codec string
- [ ] Confirm tests fail (RED)
- [ ] Implement codec string: `"mp3"`
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.2 EncodedAudioChunk data
- [ ] Write failing tests for chunk format
- [ ] Confirm tests fail (RED)
- [ ] Implement chunk handling:
  - MP3 audio frame with sync word
  - Each frame is independent
  - Chunk type always "key"
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.3 MP3 Decoder
- [ ] Write failing tests for MP3 decoding
- [ ] Confirm tests fail (RED)
- [ ] Implement decoder:
  - FFmpeg mp3 decoder (or mp3float)
  - Handle frame headers
  - Support various sample rates (44.1kHz, 48kHz)
  - Support stereo and joint stereo
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.4 MP3 Encoder
- [ ] Write failing tests for MP3 encoding
- [ ] Confirm tests fail (RED)
- [ ] Implement encoder:
  - FFmpeg libmp3lame encoder
  - Configure bitrate (CBR/VBR)
  - Support various quality levels
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

## Phase 4: Integration
- [ ] Verify all components work together
- [ ] Run full test suite
- [ ] Test with real MP3 audio
- [ ] Update documentation

## Phase 5: Verification
- [ ] Code review checklist complete
- [ ] No hardcoded test values
- [ ] Edge cases handled
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
- FFmpeg codec: AV_CODEC_ID_MP3
- libmp3lame for encoding
- Each frame is self-contained (key frame)
- No description needed in config
- Widely supported, ubiquitous format
