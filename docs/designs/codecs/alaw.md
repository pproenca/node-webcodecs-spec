# Task: A-law PCM Codec Implementation

> **Created:** 2026-01-02
> **Spec:** [docs/specs/codecs/alaw.md](../../specs/codecs/alaw.md)
> **Branch:** feat/codec-alaw

## Success Criteria
- [ ] All tests pass (`npm test`)
- [ ] Type check passes (`npm run typecheck`)
- [ ] Linting clean (`npm run lint`)
- [ ] All checklist items below marked complete
- [ ] PR description created

---

## Phase 1: Investigation (NO CODING)
- [ ] Read A-law spec (ITU-T G.711)
- [ ] Document FFmpeg A-law support in NOTES.md
- [ ] Identify telephony use cases
- [ ] List dependencies and constraints
- [ ] Update plan.md with findings

## Phase 2: Planning
- [ ] Create detailed implementation plan
- [ ] Define A-law handling
- [ ] Identify parallelizable work units
- [ ] Get human approval on plan
- [ ] Create task packets for subagents (if applicable)

## Phase 3: Implementation

### 3.1 Codec String
- [ ] Write failing tests for alaw codec string
- [ ] Confirm tests fail (RED)
- [ ] Implement codec string: `"alaw"`
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.2 EncodedAudioChunk data
- [ ] Write failing tests for chunk format
- [ ] Confirm tests fail (RED)
- [ ] Implement chunk handling:
  - Raw A-law encoded samples
  - 8-bit samples, 8kHz sample rate typical
  - Chunk type always "key"
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.3 A-law Decoder
- [ ] Write failing tests for A-law decoding
- [ ] Confirm tests fail (RED)
- [ ] Implement decoder:
  - FFmpeg pcm_alaw decoder
  - 8-bit A-law to 16-bit PCM
  - Handle sample rate/channels from config
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.4 A-law Encoder
- [ ] Write failing tests for A-law encoding
- [ ] Confirm tests fail (RED)
- [ ] Implement encoder:
  - FFmpeg pcm_alaw encoder
  - 16-bit PCM to 8-bit A-law
  - Single channel, 8kHz typical
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

## Phase 4: Integration
- [ ] Verify all components work together
- [ ] Run full test suite
- [ ] Test with telephony audio
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
- FFmpeg codec: AV_CODEC_ID_PCM_ALAW
- Standard telephony codec (ITU-T G.711)
- 8-bit samples, typically 8kHz mono
- No description needed in config
