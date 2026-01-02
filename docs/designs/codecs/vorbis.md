# Task: Vorbis Codec Implementation

> **Created:** 2026-01-02
> **Spec:** [docs/specs/codecs/vorbis.md](../../specs/codecs/vorbis.md)
> **Branch:** feat/codec-vorbis

## Success Criteria
- [ ] All tests pass (`npm test`)
- [ ] Type check passes (`npm run typecheck`)
- [ ] Linting clean (`npm run lint`)
- [ ] All checklist items below marked complete
- [ ] PR description created

---

## Phase 1: Investigation (NO CODING)
- [ ] Read Vorbis codec spec
- [ ] Document FFmpeg Vorbis support (libvorbis)
- [ ] Identify setup header requirements
- [ ] List dependencies and constraints
- [ ] Update plan.md with findings

## Phase 2: Planning
- [ ] Create detailed implementation plan
- [ ] Define Vorbis handling
- [ ] Identify parallelizable work units
- [ ] Get human approval on plan
- [ ] Create task packets for subagents (if applicable)

## Phase 3: Implementation

### 3.1 Codec String
- [ ] Write failing tests for vorbis codec string
- [ ] Confirm tests fail (RED)
- [ ] Implement codec string: `"vorbis"`
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.2 AudioDecoderConfig description
- [ ] Write failing tests for description handling
- [ ] Confirm tests fail (RED)
- [ ] Implement description handling:
  - Required: Vorbis setup headers
  - Identification header + Comment header + Setup header
  - Format per Ogg Vorbis spec
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.3 EncodedAudioChunk data
- [ ] Write failing tests for chunk format
- [ ] Confirm tests fail (RED)
- [ ] Implement chunk handling:
  - Vorbis audio packet
  - Chunk type always "key"
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.4 Vorbis Decoder
- [ ] Write failing tests for Vorbis decoding
- [ ] Confirm tests fail (RED)
- [ ] Implement decoder:
  - FFmpeg libvorbis decoder
  - Handle setup headers in extradata
  - Support various sample rates
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.5 Vorbis Encoder
- [ ] Write failing tests for Vorbis encoding
- [ ] Confirm tests fail (RED)
- [ ] Implement encoder:
  - FFmpeg libvorbis encoder
  - Configure quality/bitrate
  - Generate setup headers for metadata
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

## Phase 4: Integration
- [ ] Verify all components work together
- [ ] Run full test suite
- [ ] Test with real Vorbis audio
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
- FFmpeg codec: AV_CODEC_ID_VORBIS
- libvorbis required for encoding/decoding
- Setup headers critical for decoding
- description is REQUIRED (unlike some other audio codecs)
- Commonly used in Ogg and WebM containers
