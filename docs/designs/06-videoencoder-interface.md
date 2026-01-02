# Task: VideoEncoder Interface Implementation

> **Created:** 2026-01-02
> **Spec:** [docs/specs/06-videoencoder-interface.md](../specs/06-videoencoder-interface.md)
> **Branch:** feat/videoencoder

## Success Criteria
- [ ] All tests pass (`npm test`)
- [ ] Type check passes (`npm run typecheck`)
- [ ] Linting clean (`npm run lint`)
- [ ] All checklist items below marked complete
- [ ] PR description created

---

## Phase 1: Investigation (NO CODING)
- [ ] Read relevant files: `lib/VideoEncoder.ts`, `src/VideoEncoder.cpp`
- [ ] Document current patterns in NOTES.md
- [ ] Identify integration points with FFmpeg encoders
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
  - `[[active encoder config]]` (VideoEncoderConfig)
  - `[[active output config]]` (VideoDecoderConfig)
  - `[[state]]`
  - `[[encodeQueueSize]]`
  - `[[pending flush promises]]`
  - `[[dequeue event scheduled]]`
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.2 Constructor
- [ ] Write failing tests for VideoEncoder constructor
- [ ] Confirm tests fail (RED)
- [ ] Implement `VideoEncoder(init)`:
  - Initialize all slots per spec
  - Validate VideoEncoderInit (output, error callbacks)
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
  - Validate VideoEncoderConfig
  - Check state != "closed" (InvalidStateError)
  - Set state to "configured"
  - Store active encoder config
  - Queue control message
  - Configure encoder parameters:
    - codec, width, height
    - bitrate, bitrateMode (constant, variable, quantizer)
    - framerate, latencyMode (quality, realtime)
    - hardwareAcceleration preference
    - scalabilityMode (L1T1, L1T2, L1T3, L2T1, etc.)
    - Codec-specific: avc, hevc, vp9, av1
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.5 encode() Method
- [ ] Write failing tests for encode()
- [ ] Confirm tests fail (RED)
- [ ] Implement encode(frame, options):
  - Validate state == "configured" (InvalidStateError)
  - Validate VideoFrame is not closed
  - Handle VideoEncoderEncodeOptions:
    - keyFrame: force IDR/key frame
    - Codec-specific options (avc.quantizer, vp9.quantizer, etc.)
  - Increment encodeQueueSize
  - Clone VideoFrame reference
  - Queue encode control message
  - Handle codec saturation
  - Emit EncodedVideoChunk via output callback with metadata
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
  - Emit final encoder metadata
  - Resolve promise when complete
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.7 reset() Method
- [ ] Write failing tests for reset()
- [ ] Confirm tests fail (RED)
- [ ] Implement reset():
  - Run "Reset VideoEncoder" algorithm
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
  - Run "Close VideoEncoder" algorithm
  - Set state to "closed"
  - Clear codec implementation
  - Release system resources (GPU memory, hardware encoder)
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.9 isConfigSupported() Static Method
- [ ] Write failing tests for isConfigSupported()
- [ ] Confirm tests fail (RED)
- [ ] Implement static isConfigSupported(config):
  - Validate config (TypeError if invalid)
  - Run "Check Configuration Support" in parallel
  - Check FFmpeg encoder availability
  - Verify resolution, bitrate, profile support
  - Return VideoEncoderSupport with cloned config and support boolean
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.10 Output Metadata
- [ ] Write failing tests for EncodedVideoChunkMetadata
- [ ] Confirm tests fail (RED)
- [ ] Implement metadata emission:
  - `decoderConfig` with VideoDecoderConfig
  - `svc` with temporal/spatial layer info
  - `alphaSideData` for alpha channel (if applicable)
  - Emit on first chunk and when config changes
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.11 Scalability Mode (SVC)
- [ ] Write failing tests for SVC encoding
- [ ] Confirm tests fail (RED)
- [ ] Implement scalabilityMode:
  - L1T1, L1T2, L1T3 (temporal layers)
  - L2T1, L2T2, L2T3 (spatial + temporal)
  - Map to FFmpeg temporal_id for VP9/AV1
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

## Phase 4: Integration
- [ ] Verify all components work together
- [ ] Run full test suite
- [ ] Run integration tests
- [ ] Test with real video encoding (H.264, VP9, AV1)
- [ ] Test hardware acceleration if available
- [ ] Test SVC modes
- [ ] Update documentation

## Phase 5: Verification
- [ ] Code review checklist complete
- [ ] No hardcoded test values
- [ ] Edge cases handled:
  - Resolution changes
  - Bitrate changes
  - Key frame requests
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
- Metadata must include codec-specific extradata (SPS/PPS for H.264)
- Pixel format conversion may be needed (use libswscale)
- Hardware encoding: check for nvenc, vaapi, videotoolbox support
