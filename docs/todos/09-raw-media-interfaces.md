# Task: Raw Media Interfaces (AudioData & VideoFrame) Implementation

> **Created:** 2026-01-02
> **Spec:** [docs/specs/09-raw-media-interfaces-part-1-of-4.md](../specs/09-raw-media-interfaces-part-1-of-4.md) (through part-4-of-4)
> **Branch:** feat/raw-media

## Success Criteria
- [ ] All tests pass (`npm test`)
- [ ] Type check passes (`npm run typecheck`)
- [ ] Linting clean (`npm run lint`)
- [ ] All checklist items below marked complete
- [ ] PR description created

---

## Phase 1: Investigation (NO CODING)
- [ ] Read relevant files: `lib/AudioData.ts`, `lib/VideoFrame.ts`, `src/`
- [ ] Document current patterns in NOTES.md
- [ ] Identify integration points with FFmpeg AVFrame
- [ ] List dependencies and constraints (memory management, RAII)
- [ ] Update plan.md with findings

## Phase 2: Planning
- [ ] Create detailed implementation plan
- [ ] Define interface contracts per WebIDL
- [ ] Identify parallelizable work units
- [ ] Get human approval on plan
- [ ] Create task packets for subagents (if applicable)

## Phase 3: Implementation

### 3.1 Memory Model - Resource Reference Counting
- [ ] Write failing tests for reference counting
- [ ] Confirm tests fail (RED)
- [ ] Implement media resource reference system:
  - `[[resource reference]]` internal slot
  - Reference counting for shared media resources
  - Resource destruction when refcount hits 0
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.2 AudioData Interface
- [ ] Write failing tests for AudioData
- [ ] Confirm tests fail (RED)
- [ ] Implement AudioData:
  - Constructor(init: AudioDataInit)
  - Internal slots:
    - `[[resource reference]]`
    - `[[format]]` (AudioSampleFormat or null)
    - `[[sample rate]]`
    - `[[number of frames]]`
    - `[[number of channels]]`
    - `[[timestamp]]`
  - Readonly attributes:
    - `format`, `sampleRate`, `numberOfFrames`
    - `numberOfChannels`, `duration`, `timestamp`
  - Methods:
    - `allocationSize(options)`
    - `copyTo(destination, options)`
    - `clone()`
    - `close()`
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.3 AudioSampleFormat Enum
- [ ] Write failing tests for sample formats
- [ ] Confirm tests fail (RED)
- [ ] Implement AudioSampleFormat:
  - `u8` (unsigned 8-bit)
  - `s16` (signed 16-bit)
  - `s32` (signed 32-bit)
  - `f32` (float 32-bit)
  - `u8-planar`, `s16-planar`, `s32-planar`, `f32-planar`
- [ ] Map to FFmpeg AV_SAMPLE_FMT_*
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.4 AudioData copyTo Algorithm
- [ ] Write failing tests for copyTo with options
- [ ] Confirm tests fail (RED)
- [ ] Implement copyTo:
  - Validate not closed (InvalidStateError)
  - Handle AudioDataCopyToOptions:
    - `planeIndex`
    - `frameOffset`, `frameCount`
    - `format` (conversion)
  - Calculate allocation size
  - Copy with optional format conversion
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.5 VideoFrame Interface
- [ ] Write failing tests for VideoFrame
- [ ] Confirm tests fail (RED)
- [ ] Implement VideoFrame:
  - Constructor(image, init) - from CanvasImageSource
  - Constructor(data, init) - from BufferSource
  - Internal slots:
    - `[[resource reference]]`
    - `[[format]]` (VideoPixelFormat or null)
    - `[[coded width/height]]`
    - `[[visible rect]]` (DOMRectReadOnly)
    - `[[display width/height]]`
    - `[[duration]]`, `[[timestamp]]`
    - `[[color space]]` (VideoColorSpace)
  - Readonly attributes
  - Methods:
    - `allocationSize(options)`
    - `copyTo(destination, options)`
    - `clone()`
    - `close()`
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.6 VideoPixelFormat Enum
- [ ] Write failing tests for pixel formats
- [ ] Confirm tests fail (RED)
- [ ] Implement VideoPixelFormat:
  - YUV formats: `I420`, `I420A`, `I422`, `I444`, `NV12`
  - RGB formats: `RGBA`, `RGBX`, `BGRA`, `BGRX`
- [ ] Map to FFmpeg AV_PIX_FMT_*
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.7 VideoFrame copyTo Algorithm
- [ ] Write failing tests for copyTo with options
- [ ] Confirm tests fail (RED)
- [ ] Implement copyTo:
  - Validate not closed (InvalidStateError)
  - Handle VideoFrameCopyToOptions:
    - `rect` (visible rectangle to copy)
    - `layout` (PlaneLayout array)
    - `format` (conversion)
    - `colorSpace` (conversion)
  - Calculate allocation size per plane
  - Copy with optional format/colorspace conversion (libswscale)
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.8 VideoColorSpace Interface
- [ ] Write failing tests for VideoColorSpace
- [ ] Confirm tests fail (RED)
- [ ] Implement VideoColorSpace:
  - Constructor(init: VideoColorSpaceInit)
  - Readonly attributes:
    - `primaries` (VideoColorPrimaries)
    - `transfer` (VideoTransferCharacteristics)
    - `matrix` (VideoMatrixCoefficients)
    - `fullRange` (boolean)
  - Method:
    - `toJSON()` -> VideoColorSpaceInit
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.9 PlaneLayout Dictionary
- [ ] Write failing tests for plane layout
- [ ] Confirm tests fail (RED)
- [ ] Implement PlaneLayout:
  - `offset` (unsigned long)
  - `stride` (unsigned long)
- [ ] Validate layout consistency
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.10 clone() Algorithm
- [ ] Write failing tests for clone
- [ ] Confirm tests fail (RED)
- [ ] Implement clone:
  - Validate not closed (InvalidStateError)
  - Create new object sharing same media resource
  - Increment resource reference count
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.11 close() Algorithm
- [ ] Write failing tests for close
- [ ] Confirm tests fail (RED)
- [ ] Implement close:
  - Clear resource reference
  - Decrement refcount, free if zero
  - Mark as closed (future operations throw)
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.12 Transfer and Serialization
- [ ] Write failing tests for transfer/serialize
- [ ] Confirm tests fail (RED)
- [ ] Implement Transferable:
  - Transfer moves resource reference to destination
  - Source becomes closed after transfer
- [ ] Implement Serializable:
  - Serialize clones the resource reference
  - Both objects share same underlying resource
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

## Phase 4: Integration
- [ ] Verify all components work together
- [ ] Run full test suite
- [ ] Run integration tests with encoders/decoders
- [ ] Test format conversions
- [ ] Memory leak testing (`npm run test:native:asan`)
- [ ] Update documentation

## Phase 5: Verification
- [ ] Code review checklist complete
- [ ] No hardcoded test values
- [ ] Edge cases handled:
  - Closed frames
  - Zero-size frames
  - Format conversion edge cases
  - Alpha channel handling
- [ ] Error handling complete
- [ ] Types are strict (no `any`)
- [ ] Memory safety verified (no leaks)

## Phase 6: Finalize
- [ ] All success criteria met
- [ ] PR description written
- [ ] Ready for human review

---

## Blockers
<!-- Add any blockers encountered -->

## Notes
- Use RAII from `ffmpeg_raii.h` for AVFrame management
- Format conversion uses libswscale (video) and libswresample (audio)
- Reference counting must be thread-safe for worker transfer
- Large frames should use pool allocation from `shared/frame_pool.h`
- Close immediately when done to release memory pressure
