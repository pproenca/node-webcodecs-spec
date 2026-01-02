# Task: AudioEncoder Interface Implementation

> **Created:** 2026-01-02
> **Spec:** [docs/specs/05-audioencoder-interface.md](../specs/05-audioencoder-interface.md)
> **Branch:** feat/audioencoder

## Success Criteria
- [ ] All tests pass (`npm test`) - **Note: ImageDecoder crash blocks full suite**
- [x] Type check passes (`npm run typecheck`)
- [x] Linting clean (`npm run lint`)
- [ ] All checklist items below marked complete
- [ ] PR description created

## Audit Status (2026-01-02)
**Compliance:** ~90%
**See:** [docs/audit-report.md](../audit-report.md)

---

## Phase 1: Investigation (NO CODING)
- [x] Read relevant files: `lib/AudioEncoder.ts`, `src/audio_encoder.cpp`
- [x] Document current patterns in NOTES.md
- [x] Identify integration points with FFmpeg encoders
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
  - [ ] `[[message queue blocked]]` - implicit in async worker
  - [x] `[[codec implementation]]` - `raii::AVCodecContextPtr codec_ctx_`
  - [x] `[[codec work queue]]` - `AudioEncoderWorker` thread
  - [ ] `[[codec saturated]]` - not explicitly tracked
  - [x] `[[output callback]]` - `output_callback_`
  - [x] `[[error callback]]` - `error_callback_`
  - [x] `[[active encoder config]]` (AudioEncoderConfig) - `EncoderConfig active_config_`
  - [x] `[[active output config]]` (AudioDecoderConfig for decoder init) - generated on first output
  - [x] `[[state]]` - `raii::AtomicCodecState state_`
  - [x] `[[encodeQueueSize]]` - `std::atomic<uint32_t> encode_queue_size_`
  - [x] `[[pending flush promises]]` - `std::unordered_map pending_flushes_`
  - [ ] `[[dequeue event scheduled]]` - not explicitly tracked
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.2 Constructor
- [x] Write failing tests for AudioEncoder constructor
- [x] Confirm tests fail (RED)
- [x] Implement `AudioEncoder(init)`:
  - [x] Initialize all slots per spec steps 1-15
  - [x] Validate AudioEncoderInit (output, error callbacks)
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.3 Attributes
- [x] Write failing tests for attribute getters
- [x] Confirm tests fail (RED)
- [x] Implement readonly attributes:
  - [x] `state` -> returns `[[state]]`
  - [x] `encodeQueueSize` -> returns `[[encodeQueueSize]]`
  - [x] `ondequeue` EventHandler
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.4 configure() Method
- [x] Write failing tests for configure()
- [x] Confirm tests fail (RED)
- [x] Implement configure(config):
  - [x] Validate AudioEncoderConfig
  - [x] Check state != "closed" (InvalidStateError)
  - [x] Set state to "configured"
  - [x] Store active encoder config
  - [x] Queue control message
  - [x] Check FFmpeg encoder support
  - [x] Configure codec parameters (bitrate, channels, sample rate)
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.5 encode() Method
- [x] Write failing tests for encode()
- [x] Confirm tests fail (RED)
- [x] Implement encode(data):
  - [x] Validate state == "configured" (InvalidStateError)
  - [x] Validate AudioData is not closed
  - [x] Increment encodeQueueSize
  - [x] Clone AudioData reference
  - [x] Queue encode control message
  - [ ] Handle codec saturation - not explicitly tracked
  - [x] Emit EncodedAudioChunk via output callback with metadata
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.6 flush() Method
- [x] Write failing tests for flush()
- [x] Confirm tests fail (RED)
- [x] Implement flush():
  - [x] Validate state == "configured"
  - [x] Create and track Promise
  - [x] Queue flush control message
  - [x] Signal codec to emit all pending outputs
  - [x] Emit final encoder metadata if codec config changed
  - [x] Resolve promise when complete
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.7 reset() Method
- [x] Write failing tests for reset()
- [x] Confirm tests fail (RED)
- [x] Implement reset():
  - [x] Run "Reset AudioEncoder" algorithm
  - [x] Set state to "unconfigured"
  - [x] Clear control message queue
  - [x] Clear active configs
  - [x] Reset encodeQueueSize
  - [x] Reject pending flush promises with AbortError
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.8 close() Method
- [x] Write failing tests for close()
- [x] Confirm tests fail (RED)
- [x] Implement close():
  - [x] Run "Close AudioEncoder" algorithm
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
  - [x] Check FFmpeg encoder availability for codec
  - [x] Verify sample rate, channels, bitrate support
  - [x] Return AudioEncoderSupport with cloned config and support boolean
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.10 Output Callback with Metadata
- [x] Write failing tests for EncodedAudioChunkMetadata
- [x] Confirm tests fail (RED)
- [x] Implement metadata emission:
  - [x] `decoderConfig` with AudioDecoderConfig
  - [x] Emit on first chunk and when config changes
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

## Phase 4: Integration
- [ ] Verify all components work together
- [ ] Run full test suite
- [ ] Run integration tests
- [x] Test with real audio encoding (AAC, Opus, MP3)
- [x] Test various bitrates and sample rates
- [ ] Update documentation

## Phase 5: Verification
- [ ] Code review checklist complete
- [x] No hardcoded test values
- [x] Edge cases handled:
  - [x] Sample rate conversion
  - [x] Channel layout changes
  - [x] Bitrate limits
  - [x] Close during encode
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
- FFmpeg integration: use `avcodec_send_frame` / `avcodec_receive_packet`
- Handle AVERROR(EAGAIN) and AVERROR_EOF as state transitions
- Use RAII from `ffmpeg_raii.h` for AVCodecContext
- Metadata must include codec-specific extradata (e.g., AAC AudioSpecificConfig)
- Sample format conversion may be needed (use libswresample)
- Missing: `[[codec saturated]]`, dequeue event coalescing
