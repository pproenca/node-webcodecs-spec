# Task: Opus Codec Implementation

> **Created:** 2026-01-02
> **Spec:** [docs/specs/codecs/opus.md](../../specs/codecs/opus.md)
> **Branch:** feat/codec-opus

## Success Criteria
- [ ] All tests pass (`npm test`)
- [ ] Type check passes (`npm run typecheck`)
- [ ] Linting clean (`npm run lint`)
- [ ] All checklist items below marked complete
- [ ] PR description created

---

## Phase 1: Investigation (NO CODING)
- [ ] Read Opus codec spec (RFC 6716)
- [ ] Document FFmpeg Opus support (libopus)
- [ ] Identify encoder configuration options
- [ ] List dependencies and constraints
- [ ] Update plan.md with findings

## Phase 2: Planning
- [ ] Create detailed implementation plan
- [ ] Define Opus-specific types
- [ ] Identify parallelizable work units
- [ ] Get human approval on plan
- [ ] Create task packets for subagents (if applicable)

## Phase 3: Implementation

### 3.1 Codec String
- [ ] Write failing tests for opus codec string
- [ ] Confirm tests fail (RED)
- [ ] Implement codec string: `"opus"`
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.2 AudioDecoderConfig description
- [ ] Write failing tests for description handling
- [ ] Confirm tests fail (RED)
- [ ] Implement description handling:
  - Optional Ogg Opus Identification Header
  - If present, bitstream is in "ogg" format
  - If absent, bitstream is in "opus" format
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.3 Bitstream Formats
- [ ] Write failing tests for bitstream formats
- [ ] Confirm tests fail (RED)
- [ ] Implement OpusBitstreamFormat:
  - `"opus"`: Raw Opus packets (RFC 6716)
  - `"ogg"`: Opus in Ogg (RFC 7845)
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.4 EncodedAudioChunk data
- [ ] Write failing tests for chunk format
- [ ] Confirm tests fail (RED)
- [ ] Implement chunk handling:
  - Opus packet (opus format)
  - Audio data packet (ogg format)
  - Chunk type always "key"
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.5 Opus Decoder
- [ ] Write failing tests for Opus decoding
- [ ] Confirm tests fail (RED)
- [ ] Implement decoder:
  - FFmpeg libopus decoder
  - Handle Identification Header extradata
  - Support various sample rates
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.6 Opus Encoder
- [ ] Write failing tests for Opus encoding
- [ ] Confirm tests fail (RED)
- [ ] Implement encoder:
  - FFmpeg libopus encoder
  - Configure from OpusEncoderConfig
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.7 OpusEncoderConfig Extensions
- [ ] Write failing tests for encoder config
- [ ] Confirm tests fail (RED)
- [ ] Implement OpusEncoderConfig:
  - `format`: OpusBitstreamFormat ("opus" | "ogg")
  - `signal`: OpusSignal ("auto" | "music" | "voice")
  - `application`: OpusApplication ("voip" | "audio" | "lowdelay")
  - `frameDuration`: Frame duration in microseconds (2500-120000)
  - `complexity`: Encoder complexity (0-10)
  - `packetlossperc`: Expected packet loss (0-100)
  - `useinbandfec`: In-band FEC
  - `usedtx`: Discontinuous transmission
- [ ] Validate frameDuration per RFC 6716 section 2.1.4
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.8 Validation
- [ ] Write failing tests for config validation
- [ ] Confirm tests fail (RED)
- [ ] Implement OpusEncoderConfig validation:
  - frameDuration in valid range
  - complexity 0-10
  - packetlossperc 0-100
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

## Phase 4: Integration
- [ ] Verify all components work together
- [ ] Run full test suite
- [ ] Test with real Opus audio
- [ ] Test WebRTC-style usage
- [ ] Update documentation

## Phase 5: Verification
- [ ] Code review checklist complete
- [ ] No hardcoded test values
- [ ] All config options working
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
- FFmpeg codec: AV_CODEC_ID_OPUS
- libopus required for encoding/decoding
- All chunks are key frames
- Widely used for WebRTC and modern streaming
- Excellent quality at low bitrates
