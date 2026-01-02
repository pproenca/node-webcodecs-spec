# Task: Image Decoding (ImageDecoder) Implementation

> **Created:** 2026-01-02
> **Spec:** [docs/specs/10-image-decoding-part-1-of-2.md](../specs/10-image-decoding-part-1-of-2.md) (through part-2-of-2)
> **Branch:** feat/imagedecoder

## Success Criteria
- [ ] All tests pass (`npm test`)
- [ ] Type check passes (`npm run typecheck`)
- [ ] Linting clean (`npm run lint`)
- [ ] All checklist items below marked complete
- [ ] PR description created

---

## Phase 1: Investigation (NO CODING)
- [ ] Read relevant files: `lib/ImageDecoder.ts`, `src/ImageDecoder.cpp`
- [ ] Document current patterns in NOTES.md
- [ ] Identify integration points with FFmpeg image decoders
- [ ] List dependencies and constraints (animated images, EXIF)
- [ ] Update plan.md with findings

## Phase 2: Planning
- [ ] Create detailed implementation plan
- [ ] Define interface contracts per WebIDL
- [ ] Identify parallelizable work units
- [ ] Get human approval on plan
- [ ] Create task packets for subagents (if applicable)

## Phase 3: Implementation

### 3.1 ImageDecoder Interface
- [ ] Write failing tests for ImageDecoder
- [ ] Confirm tests fail (RED)
- [ ] Implement ImageDecoder:
  - Constructor(init: ImageDecoderInit)
  - Internal slots:
    - `[[type]]` (MIME type)
    - `[[data complete]]` (boolean)
    - `[[tracks]]` (ImageTrackList)
    - `[[completed]]` (Promise)
    - `[[closed]]` (boolean)
  - Readonly attributes:
    - `type` (MIME type string)
    - `complete` (boolean)
    - `completed` (Promise)
    - `tracks` (ImageTrackList)
  - Static methods:
    - `isTypeSupported(type)` -> Promise<boolean>
  - Instance methods:
    - `decode(options)` -> Promise<ImageDecodeResult>
    - `reset()`
    - `close()`
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.2 ImageDecoderInit Dictionary
- [ ] Write failing tests for init validation
- [ ] Confirm tests fail (RED)
- [ ] Implement ImageDecoderInit:
  - `type` (required DOMString - MIME type)
  - `data` (required BufferSource | ReadableStream)
  - `colorSpaceConversion` (ColorSpaceConversion)
  - `desiredWidth`, `desiredHeight` (optional)
  - `preferAnimation` (optional boolean)
  - `transfer` (sequence<ArrayBuffer>)
- [ ] Validate MIME type
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.3 Streaming Data Support
- [ ] Write failing tests for ReadableStream input
- [ ] Confirm tests fail (RED)
- [ ] Implement streaming decode:
  - Handle ReadableStream as input
  - Progressive decoding as data arrives
  - Fire events on track discovery
  - Handle incomplete data gracefully
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.4 ImageDecodeOptions Dictionary
- [ ] Write failing tests for decode options
- [ ] Confirm tests fail (RED)
- [ ] Implement ImageDecodeOptions:
  - `frameIndex` (unsigned long)
  - `completeFramesOnly` (boolean, default true)
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.5 ImageDecodeResult Dictionary
- [ ] Write failing tests for decode result
- [ ] Confirm tests fail (RED)
- [ ] Implement ImageDecodeResult:
  - `image` (VideoFrame)
  - `complete` (boolean - for progressive images)
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.6 ImageTrackList Interface
- [ ] Write failing tests for track list
- [ ] Confirm tests fail (RED)
- [ ] Implement ImageTrackList:
  - Iterator support (Symbol.iterator)
  - `length` readonly attribute
  - Index getter `[index]` -> ImageTrack
  - `ready` Promise
  - `selectedIndex` (long)
  - `selectedTrack` -> ImageTrack or null
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.7 ImageTrack Interface
- [ ] Write failing tests for track
- [ ] Confirm tests fail (RED)
- [ ] Implement ImageTrack:
  - `animated` (boolean)
  - `frameCount` (unsigned long)
  - `repetitionCount` (float - Infinity for infinite loop)
  - `selected` (boolean, settable)
- [ ] Handle multi-track images (HEIC, AVIF)
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.8 Progressive Image Support
- [ ] Write failing tests for progressive decode
- [ ] Confirm tests fail (RED)
- [ ] Implement progressive decoding:
  - Return partial frames with `complete: false`
  - Track progressive frame generation
  - Support for progressive JPEG
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.9 Animated Image Support
- [ ] Write failing tests for animated images
- [ ] Confirm tests fail (RED)
- [ ] Implement animated image decode:
  - GIF animation
  - APNG animation
  - WebP animation
  - Frame timing (duration per frame)
  - Repetition count
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.10 isTypeSupported() Static Method
- [ ] Write failing tests for type support check
- [ ] Confirm tests fail (RED)
- [ ] Implement isTypeSupported:
  - Check FFmpeg decoder availability
  - Support common types:
    - image/jpeg, image/png, image/gif
    - image/webp, image/avif, image/heic
    - image/bmp, image/svg+xml (if supported)
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.11 decode() Method
- [ ] Write failing tests for decode
- [ ] Confirm tests fail (RED)
- [ ] Implement decode:
  - Validate not closed (InvalidStateError)
  - Validate frameIndex < frameCount (RangeError)
  - Decode specified frame
  - Apply color space conversion if requested
  - Return VideoFrame in ImageDecodeResult
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.12 reset() and close() Methods
- [ ] Write failing tests for reset/close
- [ ] Confirm tests fail (RED)
- [ ] Implement reset:
  - Abort pending decode operations
  - Reject promises with AbortError
  - Reset internal state for reuse
- [ ] Implement close:
  - Final cleanup
  - Release resources
  - Mark as closed
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

## Phase 4: Integration
- [ ] Verify all components work together
- [ ] Run full test suite
- [ ] Run integration tests
- [ ] Test with real image samples (JPEG, PNG, GIF, WebP, AVIF)
- [ ] Test animated images
- [ ] Test progressive decode
- [ ] Update documentation

## Phase 5: Verification
- [ ] Code review checklist complete
- [ ] No hardcoded test values
- [ ] Edge cases handled:
  - Corrupted images
  - Truncated data
  - Unsupported formats
  - Large images (memory)
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
- FFmpeg image2 demuxer and individual codec decoders
- Handle EXIF orientation metadata
- Memory management critical for large images
- AVIF/HEIC may require external libraries (libdav1d, libheif)
- Color space conversion uses libswscale
