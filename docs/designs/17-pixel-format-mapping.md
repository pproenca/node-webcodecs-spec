# Task: Pixel Format Mapping Implementation

> **Created:** 2026-01-02
> **Spec:** [docs/specs/17-pixel-format-mapping.md](../specs/17-pixel-format-mapping.md)
> **Branch:** feat/pixel-formats

## Success Criteria
- [ ] All tests pass (`npm test`)
- [ ] Type check passes (`npm run typecheck`)
- [ ] Linting clean (`npm run lint`)
- [ ] All checklist items below marked complete
- [ ] PR description created

---

## Phase 1: Investigation (NO CODING)
- [ ] Read relevant files: VideoFrame pixel format handling
- [ ] Document current format mappings in NOTES.md
- [ ] Identify FFmpeg format mappings
- [ ] List dependencies and constraints
- [ ] Update plan.md with findings

## Phase 2: Planning
- [ ] Create detailed implementation plan
- [ ] Define format mapping table
- [ ] Identify parallelizable work units
- [ ] Get human approval on plan
- [ ] Create task packets for subagents (if applicable)

## Phase 3: Implementation

### 3.1 VideoPixelFormat Enum
- [ ] Write failing tests for pixel format enum
- [ ] Confirm tests fail (RED)
- [ ] Implement VideoPixelFormat:
  - `"I420"` - YUV 4:2:0 planar
  - `"I420A"` - YUV 4:2:0 planar with alpha
  - `"I422"` - YUV 4:2:2 planar
  - `"I444"` - YUV 4:4:4 planar
  - `"NV12"` - YUV 4:2:0 semi-planar (Y + interleaved UV)
  - `"RGBA"` - RGB 8-bit with alpha
  - `"RGBX"` - RGB 8-bit with padding
  - `"BGRA"` - BGR 8-bit with alpha
  - `"BGRX"` - BGR 8-bit with padding
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.2 FFmpeg Format Mapping
- [ ] Write failing tests for FFmpeg mapping
- [ ] Confirm tests fail (RED)
- [ ] Implement format mapping:
  ```
  I420  <-> AV_PIX_FMT_YUV420P
  I420A <-> AV_PIX_FMT_YUVA420P
  I422  <-> AV_PIX_FMT_YUV422P
  I444  <-> AV_PIX_FMT_YUV444P
  NV12  <-> AV_PIX_FMT_NV12
  RGBA  <-> AV_PIX_FMT_RGBA
  RGBX  <-> AV_PIX_FMT_RGB0
  BGRA  <-> AV_PIX_FMT_BGRA
  BGRX  <-> AV_PIX_FMT_BGR0
  ```
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.3 Plane Layout Calculation
- [ ] Write failing tests for plane layouts
- [ ] Confirm tests fail (RED)
- [ ] Implement plane layout calculation:
  - I420: 3 planes (Y, U, V), U/V half width/height
  - I420A: 4 planes (Y, U, V, A)
  - I422: 3 planes (Y, U, V), U/V half width
  - I444: 3 planes (Y, U, V), same dimensions
  - NV12: 2 planes (Y, UV interleaved)
  - RGBA/RGBX/BGRA/BGRX: 1 plane, 4 bytes/pixel
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.4 Bytes Per Sample
- [ ] Write failing tests for bytes per sample
- [ ] Confirm tests fail (RED)
- [ ] Implement bytes per sample:
  - Standard formats: 1 byte per sample
  - 10-bit formats: 2 bytes per sample (if supported)
  - Calculate total allocation size
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.5 Format Conversion (libswscale)
- [ ] Write failing tests for format conversion
- [ ] Confirm tests fail (RED)
- [ ] Implement format conversion:
  - Use libswscale for pixel format conversion
  - Support conversion between all format pairs
  - Handle alpha channel correctly
  - Optimize for common conversions
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.6 allocationSize() Implementation
- [ ] Write failing tests for allocation size
- [ ] Confirm tests fail (RED)
- [ ] Implement VideoFrame.allocationSize():
  - Calculate per-plane sizes
  - Account for stride alignment
  - Handle rect cropping
  - Return total bytes needed
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.7 copyTo() with Format
- [ ] Write failing tests for copyTo with format
- [ ] Confirm tests fail (RED)
- [ ] Implement copyTo with format conversion:
  - Accept target format in options
  - Convert if different from source
  - Handle layout specification
  - Validate destination size
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

## Phase 4: Integration
- [ ] Verify all components work together
- [ ] Run full test suite
- [ ] Test all format conversions
- [ ] Performance test conversions
- [ ] Update documentation

## Phase 5: Verification
- [ ] Code review checklist complete
- [ ] No hardcoded test values
- [ ] All formats spec-compliant
- [ ] Conversion quality verified
- [ ] Types are strict (no `any`)

## Phase 6: Finalize
- [ ] All success criteria met
- [ ] PR description written
- [ ] Ready for human review

---

## Blockers
<!-- Add any blockers encountered -->

## Notes
- I420 is most common for video encoding
- NV12 common for hardware acceleration
- RGBA useful for Canvas integration
- libswscale handles all conversions
- Stride alignment important for performance
