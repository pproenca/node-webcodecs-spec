# Task: Audio Sample Format Mapping Implementation

> **Created:** 2026-01-02
> **Spec:** [docs/specs/20-audio-sample-format-mapping.md](../specs/20-audio-sample-format-mapping.md)
> **Branch:** feat/audio-formats

## Success Criteria
- [ ] All tests pass (`npm test`)
- [ ] Type check passes (`npm run typecheck`)
- [ ] Linting clean (`npm run lint`)
- [ ] All checklist items below marked complete
- [ ] PR description created

---

## Phase 1: Investigation (NO CODING)
- [ ] Read relevant files: AudioData format handling
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

### 3.1 AudioSampleFormat Enum
- [ ] Write failing tests for sample format enum
- [ ] Confirm tests fail (RED)
- [ ] Implement AudioSampleFormat:
  - `"u8"` - Unsigned 8-bit integer
  - `"s16"` - Signed 16-bit integer
  - `"s32"` - Signed 32-bit integer
  - `"f32"` - 32-bit float
  - `"u8-planar"` - Unsigned 8-bit planar
  - `"s16-planar"` - Signed 16-bit planar
  - `"s32-planar"` - Signed 32-bit planar
  - `"f32-planar"` - 32-bit float planar
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.2 FFmpeg Format Mapping
- [ ] Write failing tests for FFmpeg mapping
- [ ] Confirm tests fail (RED)
- [ ] Implement format mapping:
  ```
  u8        <-> AV_SAMPLE_FMT_U8
  s16       <-> AV_SAMPLE_FMT_S16
  s32       <-> AV_SAMPLE_FMT_S32
  f32       <-> AV_SAMPLE_FMT_FLT
  u8-planar <-> AV_SAMPLE_FMT_U8P
  s16-planar<-> AV_SAMPLE_FMT_S16P
  s32-planar<-> AV_SAMPLE_FMT_S32P
  f32-planar<-> AV_SAMPLE_FMT_FLTP
  ```
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.3 Interleaved vs Planar
- [ ] Write failing tests for layout handling
- [ ] Confirm tests fail (RED)
- [ ] Implement layout detection:
  - Interleaved: all samples contiguous (L,R,L,R,...)
  - Planar: separate planes per channel (LLL..., RRR...)
  - Detect from format name (*-planar suffix)
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.4 Bytes Per Sample
- [ ] Write failing tests for byte sizes
- [ ] Confirm tests fail (RED)
- [ ] Implement bytes per sample calculation:
  - u8: 1 byte
  - s16: 2 bytes
  - s32: 4 bytes
  - f32: 4 bytes
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.5 allocationSize() Implementation
- [ ] Write failing tests for allocation size
- [ ] Confirm tests fail (RED)
- [ ] Implement AudioData.allocationSize():
  - Calculate from format, channels, frames
  - Handle planar vs interleaved
  - Account for plane index in options
  - Return bytes needed for copyTo
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.6 Format Conversion (libswresample)
- [ ] Write failing tests for format conversion
- [ ] Confirm tests fail (RED)
- [ ] Implement format conversion:
  - Use libswresample (swr_*)
  - Support all format pairs
  - Support interleaved <-> planar
  - Support sample rate conversion
  - Support channel layout conversion
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.7 copyTo() with Format
- [ ] Write failing tests for copyTo
- [ ] Confirm tests fail (RED)
- [ ] Implement copyTo with format option:
  - Accept target format in options
  - Convert if different from source
  - Handle planeIndex for planar formats
  - Validate destination size
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.8 Decoder Output Format Handling
- [ ] Write failing tests for decoder output
- [ ] Confirm tests fail (RED)
- [ ] Handle decoder output formats:
  - Detect AVFrame format from decoder
  - Map to AudioSampleFormat
  - Handle unknown formats (null)
  - Prefer f32-planar for processing
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.9 Encoder Input Format Handling
- [ ] Write failing tests for encoder input
- [ ] Confirm tests fail (RED)
- [ ] Handle encoder input formats:
  - Query encoder supported formats
  - Convert input to supported format
  - Prefer encoder's native format
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
- Most decoders output planar formats
- Most encoders prefer planar input
- s16/f32 most common
- libswresample handles all conversions
- Channel layout important for >2 channels
