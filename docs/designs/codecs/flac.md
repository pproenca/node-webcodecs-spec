# Task: FLAC Codec Implementation

> **Created:** 2026-01-02
> **Spec:** [docs/specs/codecs/flac.md](../../specs/codecs/flac.md)
> **Branch:** feat/codec-flac

## Success Criteria
- [ ] All tests pass (`npm test`)
- [ ] Type check passes (`npm run typecheck`)
- [ ] Linting clean (`npm run lint`)
- [ ] All checklist items below marked complete
- [ ] PR description created

---

## Phase 1: Investigation (NO CODING)
- [ ] Read FLAC codec spec
- [ ] Document FFmpeg FLAC support in NOTES.md
- [ ] Identify lossless audio requirements
- [ ] List dependencies and constraints
- [ ] Update plan.md with findings

## Phase 2: Planning
- [ ] Create detailed implementation plan
- [ ] Define FLAC handling
- [ ] Identify parallelizable work units
- [ ] Get human approval on plan
- [ ] Create task packets for subagents (if applicable)

## Phase 3: Implementation

### 3.1 Codec String
- [ ] Write failing tests for flac codec string
- [ ] Confirm tests fail (RED)
- [ ] Implement codec string: `"flac"`
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.2 AudioDecoderConfig description
- [ ] Write failing tests for description handling
- [ ] Confirm tests fail (RED)
- [ ] Implement description handling:
  - Optional FLAC STREAMINFO block
  - Extract sample rate, channels, bits per sample
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.3 EncodedAudioChunk data
- [ ] Write failing tests for chunk format
- [ ] Confirm tests fail (RED)
- [ ] Implement chunk handling:
  - FLAC audio frame
  - Chunk type always "key" (each frame independent)
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.4 FLAC Decoder
- [ ] Write failing tests for FLAC decoding
- [ ] Confirm tests fail (RED)
- [ ] Implement decoder:
  - FFmpeg flac decoder
  - Handle STREAMINFO extradata
  - Support various bit depths (16, 24)
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.5 FLAC Encoder
- [ ] Write failing tests for FLAC encoding
- [ ] Confirm tests fail (RED)
- [ ] Implement encoder:
  - FFmpeg flac encoder
  - Configure compression level
  - Generate STREAMINFO metadata
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

## Phase 4: Integration
- [ ] Verify all components work together
- [ ] Run full test suite
- [ ] Test with real FLAC audio
- [ ] Verify lossless roundtrip
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
- FFmpeg codec: AV_CODEC_ID_FLAC
- Lossless compression
- All chunks are key frames
- STREAMINFO contains critical metadata
