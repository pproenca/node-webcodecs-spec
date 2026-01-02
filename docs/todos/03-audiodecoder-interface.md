# Task: AudioDecoder Interface Implementation

> **Created:** 2026-01-02
> **Spec:** [docs/specs/03-audiodecoder-interface.md](../specs/03-audiodecoder-interface.md)
> **Branch:** feat/audiodecoder

## Success Criteria
- [ ] All tests pass (`npm test`)
- [ ] Type check passes (`npm run typecheck`)
- [ ] Linting clean (`npm run lint`)
- [ ] All checklist items below marked complete
- [ ] PR description created

---

## Phase 1: Investigation (NO CODING)
- [ ] Read relevant files: `lib/AudioDecoder.ts`, `src/AudioDecoder.cpp`
- [ ] Document current patterns in NOTES.md
- [ ] Identify integration points with FFmpeg decoders
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
  - `[[key chunk required]]`
  - `[[state]]`
  - `[[decodeQueueSize]]`
  - `[[pending flush promises]]`
  - `[[dequeue event scheduled]]`
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.2 Constructor
- [ ] Write failing tests for AudioDecoder constructor
- [ ] Confirm tests fail (RED)
- [ ] Implement `AudioDecoder(init)`:
  - Initialize all slots per spec steps 1-14
  - Validate AudioDecoderInit (output, error callbacks)
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.3 Attributes
- [ ] Write failing tests for attribute getters
- [ ] Confirm tests fail (RED)
- [ ] Implement readonly attributes:
  - `state` -> returns `[[state]]`
  - `decodeQueueSize` -> returns `[[decodeQueueSize]]`
  - `ondequeue` EventHandler
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.4 configure() Method
- [ ] Write failing tests for configure()
- [ ] Confirm tests fail (RED)
- [ ] Implement configure(config):
  - Validate AudioDecoderConfig
  - Check state != "closed" (InvalidStateError)
  - Set state to "configured"
  - Set key chunk required
  - Queue control message
  - Run "Check Configuration Support" on work queue
  - Handle NotSupportedError for unsupported configs
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.5 decode() Method
- [ ] Write failing tests for decode()
- [ ] Confirm tests fail (RED)
- [ ] Implement decode(chunk):
  - Validate state == "configured" (InvalidStateError)
  - Check key chunk requirement (DataError if violated)
  - Increment decodeQueueSize
  - Queue decode control message
  - Handle codec saturation ("not processed" return)
  - Emit decoded AudioData via output callback
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.6 flush() Method
- [ ] Write failing tests for flush()
- [ ] Confirm tests fail (RED)
- [ ] Implement flush():
  - Validate state == "configured"
  - Set key chunk required
  - Create and track Promise
  - Queue flush control message
  - Signal codec to emit all pending outputs
  - Resolve promise when complete
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.7 reset() Method
- [ ] Write failing tests for reset()
- [ ] Confirm tests fail (RED)
- [ ] Implement reset():
  - Run "Reset AudioDecoder" algorithm
  - Set state to "unconfigured"
  - Clear control message queue
  - Reset decodeQueueSize
  - Reject pending flush promises with AbortError
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.8 close() Method
- [ ] Write failing tests for close()
- [ ] Confirm tests fail (RED)
- [ ] Implement close():
  - Run "Close AudioDecoder" algorithm
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
  - Return AudioDecoderSupport with cloned config and support boolean
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.10 Algorithms
- [ ] Write failing tests for Schedule Dequeue Event
- [ ] Confirm tests fail (RED)
- [ ] Implement "Schedule Dequeue Event" algorithm
- [ ] Implement "Output AudioData" algorithm
- [ ] Implement "Reset AudioDecoder" algorithm
- [ ] Implement "Close AudioDecoder" algorithm
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

## Phase 4: Integration
- [ ] Verify all components work together
- [ ] Run full test suite
- [ ] Run integration tests
- [ ] Test with real audio samples (AAC, Opus, MP3)
- [ ] Update documentation

## Phase 5: Verification
- [ ] Code review checklist complete
- [ ] No hardcoded test values
- [ ] Edge cases handled:
  - Empty chunks
  - Invalid timestamps
  - Unsupported codecs
  - Close during decode
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
- FFmpeg integration: use `avcodec_send_packet` / `avcodec_receive_frame`
- Handle AVERROR(EAGAIN) and AVERROR_EOF as state transitions
- Use RAII from `ffmpeg_raii.h` for AVCodecContext
- Callback invocation must happen on JS main thread
