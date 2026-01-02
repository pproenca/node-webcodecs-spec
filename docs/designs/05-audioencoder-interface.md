# Task: AudioEncoder Interface Implementation

> **Created:** 2026-01-02
> **Spec:** [docs/specs/05-audioencoder-interface.md](../specs/05-audioencoder-interface.md)
> **Branch:** feat/audioencoder

## Success Criteria
- [ ] All tests pass (`npm test`)
- [ ] Type check passes (`npm run typecheck`)
- [ ] Linting clean (`npm run lint`)
- [ ] All checklist items below marked complete
- [ ] PR description created

---

## Phase 1: Investigation (NO CODING)
- [ ] Read relevant files: `lib/AudioEncoder.ts`, `src/AudioEncoder.cpp`
- [ ] Document current patterns in NOTES.md
- [ ] Identify integration points with FFmpeg encoders
- [ ] List dependencies: AudioData, EncodedAudioChunk, CodecState
- [ ] Update plan.md with findings

## Phase 2: Planning
- [ ] Create detailed implementation plan
- [ ] Define interface contracts per WebIDL
- [ ] Identify parallelizable work units
- [ ] Get human approval on plan
- [ ] Create task packets for subagents (if applicable)

## Phase 3: Implementation

### 3.1 Internal Slots
- [ ] Write failing tests for internal slot initialization
- [ ] Confirm tests fail (RED)
- [ ] Implement internal slots:
  - `[[control message queue]]`
  - `[[message queue blocked]]`
  - `[[codec implementation]]`
  - `[[codec work queue]]`
  - `[[codec saturated]]`
  - `[[output callback]]`
  - `[[error callback]]`
  - `[[active encoder config]]` (AudioEncoderConfig)
  - `[[active output config]]` (AudioDecoderConfig for decoder init)
  - `[[state]]`
  - `[[encodeQueueSize]]`
  - `[[pending flush promises]]`
  - `[[dequeue event scheduled]]`
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.2 Constructor
- [ ] Write failing tests for AudioEncoder constructor
- [ ] Confirm tests fail (RED)
- [ ] Implement `AudioEncoder(init)`:
  - Initialize all slots per spec steps 1-15
  - Validate AudioEncoderInit (output, error callbacks)
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.3 Attributes
- [ ] Write failing tests for attribute getters
- [ ] Confirm tests fail (RED)
- [ ] Implement readonly attributes:
  - `state` -> returns `[[state]]`
  - `encodeQueueSize` -> returns `[[encodeQueueSize]]`
  - `ondequeue` EventHandler
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.4 configure() Method
- [ ] Write failing tests for configure()
- [ ] Confirm tests fail (RED)
- [ ] Implement configure(config):
  - Validate AudioEncoderConfig
  - Check state != "closed" (InvalidStateError)
  - Set state to "configured"
  - Store active encoder config
  - Queue control message
  - Check FFmpeg encoder support
  - Configure codec parameters (bitrate, channels, sample rate)
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.5 encode() Method
- [ ] Write failing tests for encode()
- [ ] Confirm tests fail (RED)
- [ ] Implement encode(data):
  - Validate state == "configured" (InvalidStateError)
  - Validate AudioData is not closed
  - Increment encodeQueueSize
  - Clone AudioData reference
  - Queue encode control message
  - Handle codec saturation
  - Emit EncodedAudioChunk via output callback with metadata
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.6 flush() Method
- [ ] Write failing tests for flush()
- [ ] Confirm tests fail (RED)
- [ ] Implement flush():
  - Validate state == "configured"
  - Create and track Promise
  - Queue flush control message
  - Signal codec to emit all pending outputs
  - Emit final encoder metadata if codec config changed
  - Resolve promise when complete
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.7 reset() Method
- [ ] Write failing tests for reset()
- [ ] Confirm tests fail (RED)
- [ ] Implement reset():
  - Run "Reset AudioEncoder" algorithm
  - Set state to "unconfigured"
  - Clear control message queue
  - Clear active configs
  - Reset encodeQueueSize
  - Reject pending flush promises with AbortError
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.8 close() Method
- [ ] Write failing tests for close()
- [ ] Confirm tests fail (RED)
- [ ] Implement close():
  - Run "Close AudioEncoder" algorithm
  - Set state to "closed"
  - Clear codec implementation
  - Release system resources
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.9 isConfigSupported() Static Method
- [ ] Write failing tests for isConfigSupported()
- [ ] Confirm tests fail (RED)
- [ ] Implement static isConfigSupported(config):
  - Validate config (TypeError if invalid)
  - Run "Check Configuration Support" in parallel
  - Check FFmpeg encoder availability for codec
  - Verify sample rate, channels, bitrate support
  - Return AudioEncoderSupport with cloned config and support boolean
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.10 Output Callback with Metadata
- [ ] Write failing tests for EncodedAudioChunkMetadata
- [ ] Confirm tests fail (RED)
- [ ] Implement metadata emission:
  - `decoderConfig` with AudioDecoderConfig
  - Emit on first chunk and when config changes
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

## Phase 4: Integration
- [ ] Verify all components work together
- [ ] Run full test suite
- [ ] Run integration tests
- [ ] Test with real audio encoding (AAC, Opus, MP3)
- [ ] Test various bitrates and sample rates
- [ ] Update documentation

## Phase 5: Verification
- [ ] Code review checklist complete
- [ ] No hardcoded test values
- [ ] Edge cases handled:
  - Sample rate conversion
  - Channel layout changes
  - Bitrate limits
  - Close during encode
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
- FFmpeg integration: use `avcodec_send_frame` / `avcodec_receive_packet`
- Handle AVERROR(EAGAIN) and AVERROR_EOF as state transitions
- Use RAII from `ffmpeg_raii.h` for AVCodecContext
- Metadata must include codec-specific extradata (e.g., AAC AudioSpecificConfig)
- Sample format conversion may be needed (use libswresample)
