# Task: AudioDecoder Interface Implementation

> **Created:** 2026-01-02
> **Spec:** [docs/specs/03-audiodecoder-interface.md](../specs/03-audiodecoder-interface.md)
> **Branch:** feat/audiodecoder

## Success Criteria
- [ ] All tests pass (`npm test`) - **Note: ImageDecoder crash blocks full suite**
- [x] Type check passes (`npm run typecheck`)
- [x] Linting clean (`npm run lint`)
- [ ] All checklist items below marked complete
- [ ] PR description created

## Audit Status (2026-01-02)
**Compliance:** 100% (internal slots and algorithms complete)
**See:** [docs/audit-report.md](../audit-report.md)

---

## Phase 1: Investigation (NO CODING)
- [x] Read relevant files: `lib/AudioDecoder.ts`, `src/audio_decoder.cpp`
- [x] Document current patterns in NOTES.md
- [x] Identify integration points with FFmpeg decoders
- [x] List dependencies: AudioData, EncodedAudioChunk, CodecState
- [x] Update plan.md with findings

## Phase 2: Planning
- [x] Create detailed implementation plan
- [x] Define interface contracts per WebIDL
- [x] Identify parallelizable work units
- [x] Get human approval on plan
- [x] Create task packets for subagents (if applicable)

## Phase 3: Implementation

### 3.1 Internal Slots
- [x] Write failing tests for internal slot initialization
- [x] Confirm tests fail (RED)
- [x] Implement internal slots:
  - [x] `[[control message queue]]` - `AudioControlQueue queue_`
  - [x] `[[message queue blocked]]` - implicit in CodecWorker message processing
  - [x] `[[codec implementation]]` - `raii::AVCodecContextPtr codec_ctx_`
  - [x] `[[codec work queue]]` - `AudioDecoderWorker` thread
  - [x] `[[codec saturated]]` - handled via AVERROR(EAGAIN) in worker
  - [x] `[[output callback]]` - `output_callback_`
  - [x] `[[error callback]]` - `error_callback_`
  - [x] `[[key chunk required]]` - `std::atomic<bool> key_chunk_required_`
  - [x] `[[state]]` - `raii::AtomicCodecState state_`
  - [x] `[[decodeQueueSize]]` - `std::atomic<uint32_t> decode_queue_size_`
  - [x] `[[pending flush promises]]` - `std::unordered_map pending_flushes_`
  - [x] `[[dequeue event scheduled]]` - `std::atomic<bool> dequeue_event_scheduled_` with compare_exchange
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.2 Constructor
- [x] Write failing tests for AudioDecoder constructor
- [x] Confirm tests fail (RED)
- [x] Implement `AudioDecoder(init)`:
  - [x] Initialize all slots per spec steps 1-14
  - [x] Validate AudioDecoderInit (output, error callbacks)
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.3 Attributes
- [x] Write failing tests for attribute getters
- [x] Confirm tests fail (RED)
- [x] Implement readonly attributes:
  - [x] `state` -> returns `[[state]]`
  - [x] `decodeQueueSize` -> returns `[[decodeQueueSize]]`
  - [x] `ondequeue` EventHandler
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.4 configure() Method
- [x] Write failing tests for configure()
- [x] Confirm tests fail (RED)
- [x] Implement configure(config):
  - [x] Validate AudioDecoderConfig
  - [x] Check state != "closed" (InvalidStateError)
  - [x] Set state to "configured"
  - [x] Set key chunk required
  - [x] Queue control message
  - [x] Run "Check Configuration Support" on work queue
  - [x] Handle NotSupportedError for unsupported configs
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.5 decode() Method
- [x] Write failing tests for decode()
- [x] Confirm tests fail (RED)
- [x] Implement decode(chunk):
  - [x] Validate state == "configured" (InvalidStateError)
  - [x] Check key chunk requirement (DataError if violated)
  - [x] Increment decodeQueueSize
  - [x] Queue decode control message
  - [x] Handle codec saturation via AVERROR(EAGAIN) retry loop
  - [x] Emit decoded AudioData via output callback
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.6 flush() Method
- [x] Write failing tests for flush()
- [x] Confirm tests fail (RED)
- [x] Implement flush():
  - [x] Validate state == "configured"
  - [x] Set key chunk required
  - [x] Create and track Promise
  - [x] Queue flush control message
  - [x] Signal codec to emit all pending outputs
  - [x] Resolve promise when complete
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.7 reset() Method
- [x] Write failing tests for reset()
- [x] Confirm tests fail (RED)
- [x] Implement reset():
  - [x] Run "Reset AudioDecoder" algorithm
  - [x] Set state to "unconfigured"
  - [x] Clear control message queue
  - [x] Reset decodeQueueSize
  - [x] Reject pending flush promises with AbortError
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.8 close() Method
- [x] Write failing tests for close()
- [x] Confirm tests fail (RED)
- [x] Implement close():
  - [x] Run "Close AudioDecoder" algorithm
  - [x] Set state to "closed"
  - [x] Clear codec implementation
  - [x] Release system resources
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.9 isConfigSupported() Static Method
- [x] Write failing tests for isConfigSupported()
- [x] Confirm tests fail (RED)
- [x] Implement static isConfigSupported(config):
  - [x] Validate config (TypeError if invalid)
  - [x] Run "Check Configuration Support" in parallel
  - [x] Return AudioDecoderSupport with cloned config and support boolean
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.10 Algorithms
- [x] Write failing tests for Schedule Dequeue Event
- [x] Confirm tests fail (RED)
- [x] Implement "Schedule Dequeue Event" algorithm
- [x] Implement "Output AudioData" algorithm
- [x] Implement "Reset AudioDecoder" algorithm
- [x] Implement "Close AudioDecoder" algorithm
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

## Phase 4: Integration
- [ ] Verify all components work together
- [ ] Run full test suite
- [ ] Run integration tests
- [x] Test with real audio samples (AAC, Opus, MP3)
- [ ] Update documentation

## Phase 5: Verification
- [ ] Code review checklist complete
- [x] No hardcoded test values
- [x] Edge cases handled:
  - [x] Empty chunks
  - [x] Invalid timestamps
  - [x] Unsupported codecs
  - [x] Close during decode
- [x] Error handling complete
- [x] Types are strict (no `any`)

## Phase 6: Finalize
- [ ] All success criteria met
- [ ] PR description written
- [ ] Ready for human review

---

## Blockers
- ImageDecoder crash blocks full TypeScript test suite

## Notes
- FFmpeg integration: use `avcodec_send_packet` / `avcodec_receive_frame`
- Handle AVERROR(EAGAIN) and AVERROR_EOF as state transitions
- Use RAII from `ffmpeg_raii.h` for AVCodecContext
- Callback invocation must happen on JS main thread via SafeThreadSafeFunction
- All internal slots implemented:
  - `[[dequeue event scheduled]]`: atomic compare_exchange coalesces events
  - `[[codec saturated]]`: AVERROR(EAGAIN) triggers receive loop before retry
  - `[[message queue blocked]]`: implicit in CodecWorker single-threaded processing
