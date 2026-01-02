# Task: FFmpeg Implementation Notes

> **Created:** 2026-01-02
> **Spec:** [docs/specs/19-ffmpeg-implementation-notes.md](../specs/19-ffmpeg-implementation-notes.md)
> **Branch:** feat/ffmpeg-integration

## Success Criteria
- [ ] All tests pass (`npm test`)
- [ ] Type check passes (`npm run typecheck`)
- [ ] Linting clean (`npm run lint`)
- [ ] All checklist items below marked complete
- [ ] PR description created

---

## Phase 1: Investigation (NO CODING)
- [ ] Read relevant files: `src/*.cpp`, FFmpeg usage patterns
- [ ] Document current FFmpeg patterns in NOTES.md
- [ ] Identify areas needing improvement
- [ ] List dependencies and constraints
- [ ] Update plan.md with findings

## Phase 2: Planning
- [ ] Create detailed implementation plan
- [ ] Define FFmpeg integration patterns
- [ ] Identify parallelizable work units
- [ ] Get human approval on plan
- [ ] Create task packets for subagents (if applicable)

## Phase 3: Implementation

### 3.1 RAII Wrapper Audit
- [ ] Write failing tests for RAII wrappers
- [ ] Confirm tests fail (RED)
- [ ] Audit and enhance `ffmpeg_raii.h`:
  - `AVFramePtr` - ffmpeg::make_frame()
  - `AVPacketPtr` - ffmpeg::make_packet()
  - `AVCodecContextPtr` - ffmpeg::make_codec_context()
  - `SwsContextPtr` - ffmpeg::make_sws_context()
  - `SwrContextPtr` - ffmpeg::make_swr_context()
  - All with proper deleters
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.2 Decoder Integration Pattern
- [ ] Write failing tests for decoder pattern
- [ ] Confirm tests fail (RED)
- [ ] Document and implement standard decoder pattern:
  ```cpp
  // 1. Find decoder
  avcodec_find_decoder(codec_id)
  // 2. Allocate context
  avcodec_alloc_context3(codec)
  // 3. Set parameters from config
  // 4. Open codec
  avcodec_open2(ctx, codec, nullptr)
  // 5. Send packet
  avcodec_send_packet(ctx, pkt)
  // 6. Receive frames
  while (avcodec_receive_frame(ctx, frame) == 0) { ... }
  // 7. Handle EAGAIN/EOF
  ```
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.3 Encoder Integration Pattern
- [ ] Write failing tests for encoder pattern
- [ ] Confirm tests fail (RED)
- [ ] Document and implement standard encoder pattern:
  ```cpp
  // 1. Find encoder
  avcodec_find_encoder(codec_id)
  // 2. Allocate context
  avcodec_alloc_context3(codec)
  // 3. Set encoding parameters
  // 4. Open codec
  avcodec_open2(ctx, codec, nullptr)
  // 5. Send frame
  avcodec_send_frame(ctx, frame)
  // 6. Receive packets
  while (avcodec_receive_packet(ctx, pkt) == 0) { ... }
  // 7. Flush with nullptr frame
  ```
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.4 Error Handling Patterns
- [ ] Write failing tests for error handling
- [ ] Confirm tests fail (RED)
- [ ] Implement consistent error handling:
  - Check all FFmpeg return values
  - Map AVERROR to DOMException
  - Handle EAGAIN as non-error
  - Handle EOF as end-of-stream
  - Log errors with av_strerror
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.5 Thread Safety
- [ ] Write failing tests for thread safety
- [ ] Confirm tests fail (RED)
- [ ] Ensure thread safety:
  - AVCodecContext per-thread
  - No concurrent access to context
  - Thread-safe packet/frame allocation
  - Safe callback dispatch via TSFN
- [ ] Run ThreadSanitizer tests
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.6 Hardware Acceleration
- [ ] Write failing tests for hwaccel
- [ ] Confirm tests fail (RED)
- [ ] Implement hardware acceleration detection:
  - Query available hwaccels
  - VAAPI (Linux)
  - VideoToolbox (macOS)
  - NVDEC/NVENC (NVIDIA)
  - Map to HardwareAcceleration enum
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.7 Memory Pool Integration
- [ ] Write failing tests for memory pools
- [ ] Confirm tests fail (RED)
- [ ] Implement AVBufferPool usage:
  - Frame buffer pools
  - Packet buffer pools
  - Reduce allocation overhead
  - Track pool statistics
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.8 Extradata Handling
- [ ] Write failing tests for extradata
- [ ] Confirm tests fail (RED)
- [ ] Implement codec extradata:
  - H.264 SPS/PPS in extradata
  - HEVC VPS/SPS/PPS in extradata
  - AAC AudioSpecificConfig
  - Extract from encoder, provide to decoder
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

## Phase 4: Integration
- [ ] Verify all components work together
- [ ] Run full test suite
- [ ] Run sanitizer tests
- [ ] Run memory leak tests
- [ ] Update documentation

## Phase 5: Verification
- [ ] Code review checklist complete
- [ ] No hardcoded test values
- [ ] All FFmpeg patterns correct
- [ ] Error handling complete
- [ ] Types are strict (no `any`)
- [ ] No memory leaks

## Phase 6: Finalize
- [ ] All success criteria met
- [ ] PR description written
- [ ] Ready for human review

---

## Blockers
<!-- Add any blockers encountered -->

## Notes
- FFmpeg requires 5.0+ (libavcodec 59+)
- RAII wrappers prevent resource leaks
- EAGAIN and EOF are state transitions, not errors
- Hardware acceleration may not be available
- Extradata critical for H.264/HEVC/AAC
