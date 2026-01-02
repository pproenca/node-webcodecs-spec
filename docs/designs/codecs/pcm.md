# Task: Linear PCM Codec Implementation

> **Created:** 2026-01-02
> **Spec:** [docs/specs/codecs/pcm.md](../../specs/codecs/pcm.md)
> **Branch:** feat/codec-pcm

## Success Criteria
- [ ] All tests pass (`npm test`)
- [ ] Type check passes (`npm run typecheck`)
- [ ] Linting clean (`npm run lint`)
- [ ] All checklist items below marked complete
- [ ] PR description created

---

## Phase 1: Investigation (NO CODING)
- [ ] Read Linear PCM codec spec
- [ ] Document FFmpeg PCM support in NOTES.md
- [ ] Identify all PCM variants
- [ ] List dependencies and constraints
- [ ] Update plan.md with findings

## Phase 2: Planning
- [ ] Create detailed implementation plan
- [ ] Define PCM codec string mapping
- [ ] Identify parallelizable work units
- [ ] Get human approval on plan
- [ ] Create task packets for subagents (if applicable)

## Phase 3: Implementation

### 3.1 Codec String Parsing
- [ ] Write failing tests for PCM codec strings
- [ ] Confirm tests fail (RED)
- [ ] Implement codec strings:
  - `pcm-u8`: Unsigned 8-bit
  - `pcm-s16`: Signed 16-bit (native endian)
  - `pcm-s24`: Signed 24-bit
  - `pcm-s32`: Signed 32-bit
  - `pcm-f32`: 32-bit float
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.2 EncodedAudioChunk data
- [ ] Write failing tests for chunk format
- [ ] Confirm tests fail (RED)
- [ ] Implement chunk handling:
  - Raw PCM samples
  - Interleaved channel layout
  - Chunk type always "key"
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.3 PCM Decoder
- [ ] Write failing tests for PCM decoding
- [ ] Confirm tests fail (RED)
- [ ] Implement decoders:
  - Map to FFmpeg pcm_* codecs:
    - pcm-u8 -> AV_CODEC_ID_PCM_U8
    - pcm-s16 -> AV_CODEC_ID_PCM_S16LE
    - pcm-s24 -> AV_CODEC_ID_PCM_S24LE
    - pcm-s32 -> AV_CODEC_ID_PCM_S32LE
    - pcm-f32 -> AV_CODEC_ID_PCM_F32LE
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.4 PCM Encoder
- [ ] Write failing tests for PCM encoding
- [ ] Confirm tests fail (RED)
- [ ] Implement encoders:
  - Direct sample format conversion
  - Output raw PCM bytes
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.5 Endianness Handling
- [ ] Write failing tests for endianness
- [ ] Confirm tests fail (RED)
- [ ] Implement endianness:
  - Default to little-endian
  - Handle big-endian variants if needed
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

## Phase 4: Integration
- [ ] Verify all components work together
- [ ] Run full test suite
- [ ] Test all PCM variants
- [ ] Update documentation

## Phase 5: Verification
- [ ] Code review checklist complete
- [ ] No hardcoded test values
- [ ] All variants working
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
- FFmpeg codecs: AV_CODEC_ID_PCM_*
- Uncompressed audio (passthrough)
- All chunks are key frames
- Useful for raw audio processing
- Minimal CPU overhead
