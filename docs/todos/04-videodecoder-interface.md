# Task: VideoDecoder Interface Implementation

> **Created:** 2026-01-02
> **Spec:** [docs/specs/04-videodecoder-interface.md](../specs/04-videodecoder-interface.md)
> **Branch:** feat/videodecoder

## Success Criteria
- [ ] All tests pass (`npm test`) - **Note: ImageDecoder crash blocks full suite**
- [x] Type check passes (`npm run typecheck`)
- [x] Linting clean (`npm run lint`)
- [ ] All checklist items below marked complete
- [ ] PR description created

## Audit Status (2026-01-02)
**Compliance:** 94% (31/34 items implemented)
**See:** [docs/audit-report.md](../audit-report.md)

---

## Phase 1: Investigation (NO CODING)
- [ ] Read relevant files: `lib/VideoDecoder.ts`, `src/VideoDecoder.cpp`
- [ ] Document current patterns in NOTES.md
- [ ] Identify integration points with FFmpeg video decoders
- [ ] List dependencies: VideoFrame, EncodedVideoChunk, CodecState
- [ ] Update plan.md with findings

## Phase 2: Planning
- [ ] Create detailed implementation plan
- [ ] Define interface contracts per WebIDL
- [ ] Identify parallelizable work units
- [ ] Get human approval on plan
- [ ] Create task packets for subagents (if applicable)

## Phase 3: Implementation

### 3.1 Internal Slots
- [x] Write failing tests for internal slot initialization
- [x] Confirm tests fail (RED)
- [x] Implement internal slots:
  - [x] `[[control message queue]]` - `VideoControlQueue queue_`
  - [ ] `[[message queue blocked]]` - implicit in async worker
  - [x] `[[codec implementation]]` - `raii::AVCodecContextPtr codec_ctx_`
  - [x] `[[codec work queue]]` - `VideoDecoderWorker` thread
  - [ ] `[[codec saturated]]` - not explicitly tracked
  - [x] `[[output callback]]` - `output_callback_`
  - [x] `[[error callback]]` - `error_callback_`
  - [x] `[[active decoder config]]` - `DecoderConfig active_config_`
  - [x] `[[key chunk required]]` - `std::atomic<bool> key_chunk_required_`
  - [x] `[[state]]` - `raii::AtomicCodecState state_`
  - [x] `[[decodeQueueSize]]` - `std::atomic<uint32_t> decode_queue_size_`
  - [x] `[[pending flush promises]]` - `std::unordered_map pending_flushes_`
  - [ ] `[[dequeue event scheduled]]` - not explicitly tracked
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.2 Constructor
- [x] Write failing tests for VideoDecoder constructor
- [x] Confirm tests fail (RED)
- [x] Implement `VideoDecoder(init)`:
  - [x] Initialize all slots per spec steps 1-15
  - [x] Validate VideoDecoderInit (output, error callbacks)
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
  - [x] Validate VideoDecoderConfig
  - [x] Check state != "closed" (InvalidStateError)
  - [x] Set state to "configured"
  - [x] Set key chunk required
  - [x] Store active decoder config
  - [x] Queue control message
  - [x] Run "Check Configuration Support" on work queue
  - [x] Handle NotSupportedError for unsupported configs
  - [x] Handle codec description (AVC/HEVC extradata)
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
  - [ ] Handle codec saturation - not explicitly tracked
  - [x] Emit decoded VideoFrame via output callback
  - [x] Apply color space from config
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
  - [x] Run "Reset VideoDecoder" algorithm
  - [x] Set state to "unconfigured"
  - [x] Clear control message queue
  - [x] Clear active decoder config
  - [x] Reset decodeQueueSize
  - [x] Reject pending flush promises with AbortError
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.8 close() Method
- [x] Write failing tests for close()
- [x] Confirm tests fail (RED)
- [x] Implement close():
  - [x] Run "Close VideoDecoder" algorithm
  - [x] Set state to "closed"
  - [x] Clear codec implementation
  - [x] Release system resources (GPU memory, hardware decoder handles)
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.9 isConfigSupported() Static Method
- [x] Write failing tests for isConfigSupported()
- [x] Confirm tests fail (RED)
- [x] Implement static isConfigSupported(config):
  - [x] Validate config (TypeError if invalid)
  - [x] Run "Check Configuration Support" in parallel
  - [x] Check FFmpeg decoder availability
  - [x] Return VideoDecoderSupport with cloned config and support boolean
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.10 Algorithms
- [x] Write failing tests for decoder algorithms
- [x] Confirm tests fail (RED)
- [x] Implement "Schedule Dequeue Event" algorithm
- [x] Implement "Output VideoFrame" algorithm (with color space handling)
- [x] Implement "Reset VideoDecoder" algorithm
- [x] Implement "Close VideoDecoder" algorithm
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

## Phase 4: Integration
- [ ] Verify all components work together
- [ ] Run full test suite
- [ ] Run integration tests
- [ ] Test with real video samples (H.264, VP9, AV1)
- [ ] Test hardware acceleration if available
- [ ] Update documentation

## Phase 5: Verification
- [ ] Code review checklist complete
- [ ] No hardcoded test values
- [ ] Edge cases handled:
  - Empty chunks
  - Invalid timestamps
  - Unsupported codecs
  - Resolution changes mid-stream
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
- VideoFrame must track color space, display dimensions, visible rect
- Hardware decoding: check for hwaccel support in FFmpeg build
