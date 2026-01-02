# Task: Raw Media Interfaces (AudioData & VideoFrame) Implementation

> **Created:** 2026-01-02
> **Spec:** [docs/specs/09-raw-media-interfaces-part-1-of-4.md](../specs/09-raw-media-interfaces-part-1-of-4.md) (through part-4-of-4)
> **Branch:** feat/raw-media

## Success Criteria
- [ ] All tests pass (`npm test`) - **Note: ImageDecoder crash blocks full suite**
- [x] Type check passes (`npm run typecheck`)
- [x] Linting clean (`npm run lint`)
- [ ] All checklist items below marked complete
- [ ] PR description created

## Audit Status (2026-01-02)
**VideoFrame Compliance:** ~75% (12/12 properties, 9/21 pixel formats)
**AudioData Compliance:** ~60% (missing constructor from AudioDataInit)
**See:** [docs/audit-report.md](../audit-report.md)

---

## Phase 1: Investigation (NO CODING)
- [x] Read relevant files: `lib/AudioData.ts`, `lib/VideoFrame.ts`, `src/`
- [x] Document current patterns in NOTES.md
- [x] Identify integration points with FFmpeg AVFrame
- [x] List dependencies and constraints (memory management, RAII)
- [x] Update plan.md with findings

## Phase 2: Planning
- [x] Create detailed implementation plan
- [x] Define interface contracts per WebIDL
- [x] Identify parallelizable work units
- [x] Get human approval on plan
- [x] Create task packets for subagents (if applicable)

## Phase 3: Implementation

### 3.1 Memory Model - Resource Reference Counting
- [x] Write failing tests for reference counting
- [x] Confirm tests fail (RED)
- [x] Implement media resource reference system:
  - [x] `[[resource reference]]` internal slot
  - [x] Reference counting for shared media resources
  - [x] Resource destruction when refcount hits 0
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.2 AudioData Interface
- [x] Write failing tests for AudioData
- [x] Confirm tests fail (RED)
- [x] Implement AudioData:
  - [ ] Constructor(init: AudioDataInit) - **NOT IMPLEMENTED**
  - [x] Internal slots:
    - [x] `[[resource reference]]`
    - [x] `[[format]]` (AudioSampleFormat or null)
    - [x] `[[sample rate]]`
    - [x] `[[number of frames]]`
    - [x] `[[number of channels]]`
    - [x] `[[timestamp]]`
  - [x] Readonly attributes:
    - [x] `format`, `sampleRate`, `numberOfFrames`
    - [x] `numberOfChannels`, `duration`, `timestamp`
  - [x] Methods:
    - [x] `allocationSize(options)` - **PARTIAL (options may be ignored)**
    - [x] `copyTo(destination, options)` - **PARTIAL (options may be ignored)**
    - [x] `clone()`
    - [x] `close()`
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.3 AudioSampleFormat Enum
- [x] Write failing tests for sample formats
- [x] Confirm tests fail (RED)
- [x] Implement AudioSampleFormat:
  - [x] `u8` (unsigned 8-bit)
  - [x] `s16` (signed 16-bit)
  - [x] `s32` (signed 32-bit)
  - [x] `f32` (float 32-bit)
  - [ ] `u8-planar`, `s16-planar`, `s32-planar`, `f32-planar` - **NOT DISTINCT in format string**
- [x] Map to FFmpeg AV_SAMPLE_FMT_*
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.4 AudioData copyTo Algorithm
- [x] Write failing tests for copyTo with options
- [x] Confirm tests fail (RED)
- [x] Implement copyTo:
  - [x] Validate not closed (InvalidStateError)
  - [ ] Handle AudioDataCopyToOptions:
    - [ ] `planeIndex` - **PARTIAL**
    - [ ] `frameOffset`, `frameCount` - **PARTIAL**
    - [ ] `format` (conversion) - **NOT IMPLEMENTED**
  - [x] Calculate allocation size
  - [ ] Copy with optional format conversion - **NOT IMPLEMENTED**
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.5 VideoFrame Interface
- [x] Write failing tests for VideoFrame
- [x] Confirm tests fail (RED)
- [x] Implement VideoFrame:
  - [ ] Constructor(image, init) - from CanvasImageSource - **NOT IMPLEMENTED (no Canvas)**
  - [ ] Constructor(data, init) - from BufferSource - **PARTIAL (no layout, rotation, flip)**
  - [x] Internal slots:
    - [x] `[[resource reference]]`
    - [x] `[[format]]` (VideoPixelFormat or null)
    - [x] `[[coded width/height]]`
    - [x] `[[visible rect]]` (DOMRectReadOnly)
    - [x] `[[display width/height]]`
    - [x] `[[duration]]`, `[[timestamp]]`
    - [x] `[[color space]]` (VideoColorSpace)
  - [x] Readonly attributes (12/12)
  - [x] Methods:
    - [x] `allocationSize(options)` - **PARTIAL (options may be ignored)**
    - [x] `copyTo(destination, options)` - **PARTIAL (options may be ignored)**
    - [x] `clone()`
    - [x] `close()`
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.6 VideoPixelFormat Enum
- [x] Write failing tests for pixel formats
- [x] Confirm tests fail (RED)
- [x] Implement VideoPixelFormat (9/21 supported):
  - [x] YUV formats: `I420`, `I420A`, `I422`, `I444`, `NV12`
  - [x] RGB formats: `RGBA`, `RGBX`, `BGRA`, `BGRX`
  - [ ] Missing: `I420P10`, `I420P12`, `I422P10`, `I422P12`, `I444P10`, `I444P12`, `NV12P10`, `RGB565`, `RGBF16`, `BGRF16`, `RGBAF16`, `BGRAF16`
- [x] Map to FFmpeg AV_PIX_FMT_*
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.7 VideoFrame copyTo Algorithm
- [x] Write failing tests for copyTo with options
- [x] Confirm tests fail (RED)
- [x] Implement copyTo:
  - [x] Validate not closed (InvalidStateError)
  - [ ] Handle VideoFrameCopyToOptions:
    - [ ] `rect` (visible rectangle to copy) - **NOT IMPLEMENTED**
    - [ ] `layout` (PlaneLayout array) - **NOT IMPLEMENTED**
    - [ ] `format` (conversion) - **NOT IMPLEMENTED**
    - [ ] `colorSpace` (conversion) - **NOT IMPLEMENTED**
  - [x] Calculate allocation size per plane
  - [ ] Copy with optional format/colorspace conversion (libswscale) - **NOT IMPLEMENTED**
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.8 VideoColorSpace Interface
- [x] Write failing tests for VideoColorSpace
- [x] Confirm tests fail (RED)
- [x] Implement VideoColorSpace:
  - [x] Constructor(init: VideoColorSpaceInit)
  - [x] Readonly attributes:
    - [x] `primaries` (VideoColorPrimaries)
    - [x] `transfer` (VideoTransferCharacteristics)
    - [x] `matrix` (VideoMatrixCoefficients)
    - [x] `fullRange` (boolean)
  - [x] Method:
    - [x] `toJSON()` -> VideoColorSpaceInit
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.9 PlaneLayout Dictionary
- [x] Write failing tests for plane layout
- [x] Confirm tests fail (RED)
- [x] Implement PlaneLayout:
  - [x] `offset` (unsigned long)
  - [x] `stride` (unsigned long)
- [ ] Validate layout consistency - **NOT FULLY VALIDATED**
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.10 clone() Algorithm
- [x] Write failing tests for clone
- [x] Confirm tests fail (RED)
- [x] Implement clone:
  - [x] Validate not closed (InvalidStateError)
  - [x] Create new object sharing same media resource
  - [x] Increment resource reference count
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.11 close() Algorithm
- [x] Write failing tests for close
- [x] Confirm tests fail (RED)
- [x] Implement close:
  - [x] Clear resource reference
  - [x] Decrement refcount, free if zero
  - [x] Mark as closed (future operations throw)
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.12 Transfer and Serialization
- [ ] Write failing tests for transfer/serialize
- [ ] Confirm tests fail (RED)
- [ ] Implement Transferable:
  - [ ] Transfer moves resource reference to destination - **NOT IMPLEMENTED**
  - [ ] Source becomes closed after transfer
- [ ] Implement Serializable:
  - [ ] Serialize clones the resource reference - **NOT IMPLEMENTED**
  - [ ] Both objects share same underlying resource
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.13 metadata() Method
- [ ] Write failing tests for metadata
- [ ] Confirm tests fail (RED)
- [ ] Implement VideoFrame.metadata():
  - [ ] Return VideoFrameMetadata dictionary - **STUB (returns undefined)**
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

## Phase 4: Integration
- [ ] Verify all components work together
- [ ] Run full test suite
- [ ] Run integration tests with encoders/decoders
- [x] Test format conversions
- [x] Memory leak testing (`npm run test:native:asan`) - PASS
- [ ] Update documentation

## Phase 5: Verification
- [ ] Code review checklist complete
- [x] No hardcoded test values
- [x] Edge cases handled:
  - [x] Closed frames
  - [x] Zero-size frames
  - [ ] Format conversion edge cases - **NOT FULLY TESTED**
  - [ ] Alpha channel handling - **NOT FULLY TESTED**
- [x] Error handling complete
- [x] Types are strict (no `any`)
- [x] Memory safety verified (no leaks)

## Phase 6: Finalize
- [ ] All success criteria met
- [ ] PR description written
- [ ] Ready for human review

---

## Blockers
- ImageDecoder crash blocks full TypeScript test suite

## Missing Features (P3)

### AudioData
- Constructor from AudioDataInit
- Planar format string distinction (u8-planar vs u8)
- Full copyTo options support (format conversion)

### VideoFrame
- Constructor from CanvasImageSource (not applicable in Node.js)
- Constructor from BufferSource with full options (layout, rotation, flip)
- copyTo with rect, layout, format, colorSpace options
- metadata() method (currently returns undefined)
- 12 additional pixel formats
- Transfer and Serialization support

## Notes
- Use RAII from `ffmpeg_raii.h` for AVFrame management
- Format conversion uses libswscale (video) and libswresample (audio)
- Reference counting must be thread-safe for worker transfer
- Large frames should use pool allocation from `shared/frame_pool.h`
- Close immediately when done to release memory pressure
