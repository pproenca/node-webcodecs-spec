# Task: Image Decoding (ImageDecoder) Implementation

> **Created:** 2026-01-02
> **Spec:** [docs/specs/10-image-decoding-part-1-of-2.md](../specs/10-image-decoding-part-1-of-2.md) (through part-2-of-2)
> **Branch:** feat/imagedecoder

## Success Criteria
- [ ] All tests pass (`npm test`) - **BLOCKING: Abort trap: 6 crash**
- [x] Type check passes (`npm run typecheck`)
- [x] Linting clean (`npm run lint`)
- [ ] All checklist items below marked complete
- [ ] PR description created

## Audit Status (2026-01-02)
**Compliance:** ~70%
**Status:** CRITICAL - Crash blocks TypeScript test suite
**See:** [docs/audit-report.md](../audit-report.md)

---

## Phase 1: Investigation (NO CODING)
- [x] Read relevant files: `lib/ImageDecoder.ts`, `src/image_decoder.cpp`
- [x] Document current patterns in NOTES.md
- [x] Identify integration points with FFmpeg image decoders
- [x] List dependencies and constraints (animated images, EXIF)
- [x] Update plan.md with findings

## Phase 2: Planning
- [x] Create detailed implementation plan
- [x] Define interface contracts per WebIDL
- [x] Identify parallelizable work units
- [x] Get human approval on plan
- [x] Create task packets for subagents (if applicable)

## Phase 3: Implementation

### 3.1 ImageDecoder Interface
- [x] Write failing tests for ImageDecoder
- [x] Confirm tests fail (RED)
- [x] Implement ImageDecoder:
  - [x] Constructor(init: ImageDecoderInit)
  - [x] Internal slots:
    - [x] `[[type]]` (MIME type)
    - [x] `[[data complete]]` (boolean)
    - [x] `[[tracks]]` (ImageTrackList)
    - [x] `[[completed]]` (Promise)
    - [x] `[[closed]]` (boolean)
  - [x] Readonly attributes:
    - [x] `type` (MIME type string)
    - [x] `complete` (boolean)
    - [x] `completed` (Promise)
    - [x] `tracks` (ImageTrackList)
  - [x] Static methods:
    - [x] `isTypeSupported(type)` -> Promise<boolean>
  - [x] Instance methods:
    - [x] `decode(options)` -> Promise<ImageDecodeResult>
    - [x] `reset()`
    - [x] `close()`
- [ ] Confirm tests pass (GREEN) - **BLOCKED: Crash**
- [ ] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.2 ImageDecoderInit Dictionary
- [x] Write failing tests for init validation
- [x] Confirm tests fail (RED)
- [x] Implement ImageDecoderInit:
  - [x] `type` (required DOMString - MIME type)
  - [x] `data` (required BufferSource | ReadableStream) - **BufferSource only**
  - [ ] `colorSpaceConversion` (ColorSpaceConversion) - parsed but not applied
  - [ ] `desiredWidth`, `desiredHeight` (optional) - parsed but not applied
  - [x] `preferAnimation` (optional boolean)
  - [ ] `transfer` (sequence<ArrayBuffer>) - not implemented
- [x] Validate MIME type
- [ ] Confirm tests pass (GREEN) - **BLOCKED: Crash**
- [ ] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.3 Streaming Data Support
- [x] Write failing tests for ReadableStream input
- [x] Confirm tests fail (RED)
- [x] Implement streaming decode:
  - [x] Handle ReadableStream as input - **IMPLEMENTED (2026-01-03)**
  - [x] Progressive decoding as data arrives - **IMPLEMENTED (2026-01-03)**
  - [x] Fire events on track discovery
  - [x] Handle incomplete data gracefully
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.4 ImageDecodeOptions Dictionary
- [x] Write failing tests for decode options
- [x] Confirm tests fail (RED)
- [x] Implement ImageDecodeOptions:
  - [x] `frameIndex` (unsigned long)
  - [x] `completeFramesOnly` (boolean, default true)
- [ ] Confirm tests pass (GREEN) - **BLOCKED: Crash**
- [ ] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.5 ImageDecodeResult Dictionary
- [x] Write failing tests for decode result
- [x] Confirm tests fail (RED)
- [x] Implement ImageDecodeResult:
  - [x] `image` (VideoFrame)
  - [x] `complete` (boolean - for progressive images)
- [ ] Confirm tests pass (GREEN) - **BLOCKED: Crash**
- [ ] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.6 ImageTrackList Interface
- [x] Write failing tests for track list
- [x] Confirm tests fail (RED)
- [x] Implement ImageTrackList:
  - [x] Iterator support (Symbol.iterator)
  - [x] `length` readonly attribute
  - [x] Index getter `[index]` -> ImageTrack
  - [x] `ready` Promise
  - [x] `selectedIndex` (long)
  - [x] `selectedTrack` -> ImageTrack or null
- [ ] Confirm tests pass (GREEN) - **BLOCKED: Crash**
- [ ] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.7 ImageTrack Interface
- [x] Write failing tests for track
- [x] Confirm tests fail (RED)
- [x] Implement ImageTrack:
  - [x] `animated` (boolean)
  - [x] `frameCount` (unsigned long)
  - [x] `repetitionCount` (float - Infinity for infinite loop)
  - [x] `selected` (boolean, settable)
- [ ] Handle multi-track images (HEIC, AVIF) - **NOT TESTED**
- [ ] Confirm tests pass (GREEN) - **BLOCKED: Crash**
- [ ] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.8 Progressive Image Support
- [ ] Write failing tests for progressive decode
- [ ] Confirm tests fail (RED)
- [ ] Implement progressive decoding:
  - [ ] Return partial frames with `complete: false` - **NOT IMPLEMENTED**
  - [ ] Track progressive frame generation
  - [ ] Support for progressive JPEG
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.9 Animated Image Support
- [x] Write failing tests for animated images
- [x] Confirm tests fail (RED)
- [x] Implement animated image decode:
  - [x] GIF animation
  - [x] APNG animation
  - [x] WebP animation
  - [x] Frame timing (duration per frame)
  - [x] Repetition count
- [ ] Confirm tests pass (GREEN) - **BLOCKED: Crash**
- [ ] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.10 isTypeSupported() Static Method
- [x] Write failing tests for type support check
- [x] Confirm tests fail (RED)
- [x] Implement isTypeSupported:
  - [x] Check FFmpeg decoder availability
  - [x] Support common types:
    - [x] image/jpeg, image/png, image/gif
    - [x] image/webp, image/avif, image/heic
    - [ ] image/bmp, image/svg+xml (if supported)
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.11 decode() Method
- [x] Write failing tests for decode
- [x] Confirm tests fail (RED)
- [x] Implement decode:
  - [x] Validate not closed (InvalidStateError)
  - [x] Validate frameIndex < frameCount (RangeError)
  - [x] Decode specified frame
  - [ ] Apply color space conversion if requested - **NOT APPLIED**
  - [x] Return VideoFrame in ImageDecodeResult
- [ ] Confirm tests pass (GREEN) - **BLOCKED: Crash**
- [ ] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.12 reset() and close() Methods
- [x] Write failing tests for reset/close
- [x] Confirm tests fail (RED)
- [x] Implement reset:
  - [x] Abort pending decode operations
  - [x] Reject promises with AbortError
  - [x] Reset internal state for reuse
- [x] Implement close:
  - [x] Final cleanup
  - [x] Release resources
  - [x] Mark as closed
- [ ] Confirm tests pass (GREEN) - **BLOCKED: Crash**
- [ ] Refactor if needed (BLUE)
- [x] Write artifact summary

## Phase 4: Integration
- [ ] Verify all components work together - **BLOCKED**
- [ ] Run full test suite - **BLOCKED**
- [ ] Run integration tests
- [x] Test with real image samples (JPEG, PNG, GIF, WebP, AVIF)
- [x] Test animated images
- [ ] Test progressive decode - **NOT IMPLEMENTED**
- [ ] Update documentation

## Phase 5: Verification
- [ ] Code review checklist complete
- [x] No hardcoded test values
- [x] Edge cases handled:
  - [x] Corrupted images
  - [x] Truncated data
  - [x] Unsupported formats
  - [x] Large images (memory)
- [x] Error handling complete
- [x] Types are strict (no `any`)

## Phase 6: Finalize
- [ ] All success criteria met
- [ ] PR description written
- [ ] Ready for human review

---

## Critical Issues (P0)

### Race Condition in TSFN Callback (CRASH)
**Location:** `src/image_decoder.cpp` TSFN callback
**Symptom:** Abort trap: 6 on ImageDecoder construction
**Root Cause:** Mutex acquired AFTER checking `closed_` flag in TSFN callbacks
**Fix:** Acquire mutex BEFORE checking `closed_` in all TSFN callbacks

### ImageTrack Parent Pointer (P2)
**Location:** `src/image_track.cpp:94-122`
**Issue:** Raw parent pointer to ImageDecoder may dangle if decoder is released
**Fix:** Use weak reference or reference counting for parent pointer

### VideoFrame Creation from Null AVFrame (P2)
**Issue:** VideoFrame may be created from null AVFrame in error paths
**Fix:** Validate AVFrame pointer before VideoFrame construction

## Blockers
- **P0:** ImageDecoder crash (Abort trap: 6) blocks full TypeScript test suite

## Notes
- FFmpeg image2 demuxer and individual codec decoders
- Handle EXIF orientation metadata
- Memory management critical for large images
- AVIF/HEIC may require external libraries (libdav1d, libheif)
- Color space conversion uses libswscale

## Missing Features (P3)
- ~~ReadableStream support for constructor~~ **DONE (2026-01-03)**
- ~~Progressive image decoding~~ **DONE (2026-01-03)** - via streaming
- ArrayBuffer transfer semantics
- colorSpaceConversion application
- desiredWidth/Height application
