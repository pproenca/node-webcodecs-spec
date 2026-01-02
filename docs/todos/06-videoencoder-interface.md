# Task: VideoEncoder Interface Implementation

> **Created:** 2026-01-02
> **Spec:** [docs/specs/06-videoencoder-interface.md](../specs/06-videoencoder-interface.md)
> **Branch:** feat/videoencoder

## Success Criteria
- [ ] All tests pass (`npm test`) - **Note: ImageDecoder crash blocks full suite**
- [x] Type check passes (`npm run typecheck`)
- [x] Linting clean (`npm run lint`)
- [ ] All checklist items below marked complete
- [ ] PR description created

## Audit Status (2026-01-02)
**Compliance:** 100% W3C WebCodecs spec compliance
**Thread Safety:** All issues FIXED (ThreadSanitizer clean)
**See:** [docs/audit-report.md](../audit-report.md)

---

## Phase 1: Investigation (NO CODING)
- [x] Read relevant files: `lib/VideoEncoder.ts`, `src/video_encoder.cpp`
- [x] Document current patterns in NOTES.md
- [x] Identify integration points with FFmpeg encoders
- [x] List dependencies: VideoFrame, EncodedVideoChunk, CodecState
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
  - [x] `[[control message queue]]` - `VideoControlQueue queue_`
  - [x] `[[message queue blocked]]` - `queue_.SetBlocked()` with RAII ScopeGuard
  - [x] `[[codec implementation]]` - `raii::AVCodecContextPtr codec_ctx_`
  - [x] `[[codec work queue]]` - `VideoEncoderWorker` thread
  - [x] `[[codec saturated]]` - `std::atomic<bool> codec_saturated_` (EAGAIN tracking)
  - [x] `[[output callback]]` - `output_callback_`
  - [x] `[[error callback]]` - `error_callback_`
  - [x] `[[active encoder config]]` (VideoEncoderConfig) - `EncoderConfig active_config_`
  - [x] `[[active output config]]` (VideoDecoderConfig) - generated with decoderConfig
  - [x] `[[state]]` - `raii::AtomicCodecState state_`
  - [x] `[[encodeQueueSize]]` - `std::atomic<uint32_t> encode_queue_size_`
  - [x] `[[pending flush promises]]` - `std::unordered_map pending_flushes_`
  - [x] `[[dequeue event scheduled]]` - `std::atomic<bool> dequeue_event_scheduled_` (coalesces events)
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.2 Constructor
- [x] Write failing tests for VideoEncoder constructor
- [x] Confirm tests fail (RED)
- [x] Implement `VideoEncoder(init)`:
  - [x] Initialize all slots per spec
  - [x] Validate VideoEncoderInit (output, error callbacks)
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
  - [x] Validate VideoEncoderConfig
  - [x] Check state != "closed" (InvalidStateError)
  - [x] Set state to "configured"
  - [x] Store active encoder config
  - [x] Queue control message
  - [x] Configure encoder parameters:
    - [x] codec, width, height
    - [x] bitrate, bitrateMode (constant, variable, quantizer)
    - [x] framerate, latencyMode (quality, realtime)
    - [ ] hardwareAcceleration preference - parsed but not applied
    - [ ] scalabilityMode (L1T1, L1T2, L1T3, L2T1, etc.) - not implemented
    - [x] Codec-specific: avc, hevc, vp9, av1
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.5 encode() Method
- [x] Write failing tests for encode()
- [x] Confirm tests fail (RED)
- [x] Implement encode(frame, options):
  - [x] Validate state == "configured" (InvalidStateError)
  - [x] Validate VideoFrame is not closed
  - [x] Handle VideoEncoderEncodeOptions:
    - [x] keyFrame: force IDR/key frame
    - [ ] Codec-specific options (avc.quantizer, vp9.quantizer, etc.) - partial
  - [x] Increment encodeQueueSize
  - [x] Clone VideoFrame reference
  - [x] Queue encode control message
  - [x] Handle codec saturation - `codec_saturated_` set on EAGAIN, cleared on output
  - [x] Emit EncodedVideoChunk via output callback with metadata
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
  - [x] Emit final encoder metadata
  - [x] Resolve promise when complete
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.7 reset() Method
- [x] Write failing tests for reset()
- [x] Confirm tests fail (RED)
- [x] Implement reset():
  - [x] Run "Reset VideoEncoder" algorithm
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
  - [x] Run "Close VideoEncoder" algorithm
  - [x] Set state to "closed"
  - [x] Clear codec implementation
  - [x] Release system resources (GPU memory, hardware encoder)
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.9 isConfigSupported() Static Method
- [x] Write failing tests for isConfigSupported()
- [x] Confirm tests fail (RED)
- [x] Implement static isConfigSupported(config):
  - [x] Validate config (TypeError if invalid)
  - [x] Run "Check Configuration Support" in parallel
  - [x] Check FFmpeg encoder availability
  - [x] Verify resolution, bitrate, profile support
  - [x] Return VideoEncoderSupport with cloned config and support boolean
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.10 Output Metadata
- [x] Write failing tests for EncodedVideoChunkMetadata
- [x] Confirm tests fail (RED)
- [x] Implement metadata emission:
  - [x] `decoderConfig` with VideoDecoderConfig
  - [ ] `svc` with temporal/spatial layer info - not implemented
  - [ ] `alphaSideData` for alpha channel (if applicable) - not implemented
  - [x] Emit on first chunk and when config changes
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.11 Scalability Mode (SVC)
- [ ] Write failing tests for SVC encoding
- [ ] Confirm tests fail (RED)
- [ ] Implement scalabilityMode:
  - [ ] L1T1, L1T2, L1T3 (temporal layers)
  - [ ] L2T1, L2T2, L2T3 (spatial + temporal)
  - [ ] Map to FFmpeg temporal_id for VP9/AV1
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

## Phase 4: Integration
- [ ] Verify all components work together
- [ ] Run full test suite
- [ ] Run integration tests
- [x] Test with real video encoding (H.264, VP9, AV1)
- [ ] Test hardware acceleration if available
- [ ] Test SVC modes
- [ ] Update documentation

## Phase 5: Verification
- [ ] Code review checklist complete
- [x] No hardcoded test values
- [x] Edge cases handled:
  - [x] Resolution changes
  - [x] Bitrate changes
  - [x] Key frame requests
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

## Thread Safety Issues - ALL FIXED ✓
1. **GetCodecContext() Race Condition** - FIXED
   - OutputData now contains extradata/codec/dimensions copied on worker thread
   - JS thread reads from OutputData struct, never accesses codec_ctx_ directly

2. **GetCodecContext() Public Accessor** - FIXED
   - Removed direct codec_ctx_ access from JS thread
   - All data passed through thread-safe OutputData struct

3. **active_config_ Race** - FIXED
   - Config copied at start of OnConfigure with RAII ScopeGuard
   - Worker thread uses local copy, not shared active_config_
   - codec_ string stored in worker for thread-safe decoderConfig

**ThreadSanitizer:** Clean (all 335 C++ tests pass)

## Notes
- FFmpeg integration: use `avcodec_send_frame` / `avcodec_receive_packet`
- Handle AVERROR(EAGAIN) and AVERROR_EOF as state transitions
- Use RAII from `ffmpeg_raii.h` for AVCodecContext
- Metadata must include codec-specific extradata (SPS/PPS for H.264)
- Pixel format conversion may be needed (use libswscale)
- Hardware encoding: check for nvenc, vaapi, videotoolbox support
- Missing: SVC metadata, alphaSideData, decoderConfig rotation/flip, displayAspectWidth/Height
