# Task: AAC Codec Implementation

> **Created:** 2026-01-02
> **Spec:** [docs/specs/codecs/aac.md](../../specs/codecs/aac.md)
> **Branch:** feat/codec-aac

## Success Criteria
- [ ] All tests pass (`npm test`)
- [ ] Type check passes (`npm run typecheck`)
- [ ] Linting clean (`npm run lint`)
- [ ] All checklist items below marked complete
- [ ] PR description created

---

## Phase 1: Investigation (NO CODING)
- [ ] Read AAC codec spec
- [ ] Document FFmpeg AAC support in NOTES.md
- [ ] Identify profile support (AAC-LC, HE-AAC, etc.)
- [ ] List dependencies and constraints
- [ ] Update plan.md with findings

## Phase 2: Planning
- [ ] Create detailed implementation plan
- [ ] Define AAC-specific types
- [ ] Identify parallelizable work units
- [ ] Get human approval on plan
- [ ] Create task packets for subagents (if applicable)

## Phase 3: Implementation

### 3.1 Codec String Parsing
- [ ] Write failing tests for AAC codec strings
- [ ] Confirm tests fail (RED)
- [ ] Implement codec string parsing:
  - `mp4a.40.2` (AAC-LC)
  - `mp4a.40.5` (HE-AAC v1)
  - `mp4a.40.29` (HE-AAC v2)
  - Extract object type indication
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.2 AudioDecoderConfig description
- [ ] Write failing tests for description handling
- [ ] Confirm tests fail (RED)
- [ ] Implement description handling:
  - Parse AudioSpecificConfig if present
  - Extract sample rate, channels from ASC
  - Handle missing description (raw AAC)
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.3 EncodedAudioChunk data
- [ ] Write failing tests for chunk format
- [ ] Confirm tests fail (RED)
- [ ] Implement chunk handling:
  - ADTS format (with headers)
  - Raw AAC access units (without ADTS)
  - Validate frame boundaries
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.4 AAC Decoder
- [ ] Write failing tests for AAC decoding
- [ ] Confirm tests fail (RED)
- [ ] Implement AAC decoder:
  - Use FFmpeg libfdk_aac or native aac
  - Configure from AudioDecoderConfig
  - Handle AudioSpecificConfig extradata
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.5 AAC Encoder
- [ ] Write failing tests for AAC encoding
- [ ] Confirm tests fail (RED)
- [ ] Implement AAC encoder:
  - Use FFmpeg aac or libfdk_aac encoder
  - Support bitrate configuration
  - Generate AudioSpecificConfig
  - Emit metadata with decoderConfig
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.6 AacEncoderConfig (optional extension)
- [ ] Write failing tests for encoder config
- [ ] Confirm tests fail (RED)
- [ ] Implement AacEncoderConfig if needed:
  - Profile selection (AAC-LC, HE-AAC)
  - VBR/CBR mode
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

## Phase 4: Integration
- [ ] Verify all components work together
- [ ] Run full test suite
- [ ] Test with real AAC audio files
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
- FFmpeg codec: AV_CODEC_ID_AAC
- libfdk_aac preferred but may not be available
- AudioSpecificConfig is critical for proper decoding
- mp4a.40.x format per ISO 14496-3
