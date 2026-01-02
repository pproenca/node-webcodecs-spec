# Task: Configurations Implementation

> **Created:** 2026-01-02
> **Spec:** [docs/specs/07-configurations.md](../specs/07-configurations.md)
> **Branch:** feat/configurations

## Success Criteria
- [ ] All tests pass (`npm test`)
- [ ] Type check passes (`npm run typecheck`)
- [ ] Linting clean (`npm run lint`)
- [ ] All checklist items below marked complete
- [ ] PR description created

---

## Phase 1: Investigation (NO CODING)
- [ ] Read relevant files: `types/webcodecs.d.ts`, `lib/`, configuration usage
- [ ] Document current patterns in NOTES.md
- [ ] Identify integration points with codec interfaces
- [ ] List dependencies and constraints
- [ ] Update plan.md with findings

## Phase 2: Planning
- [ ] Create detailed implementation plan
- [ ] Define interface contracts per WebIDL
- [ ] Identify parallelizable work units
- [ ] Get human approval on plan
- [ ] Create task packets for subagents (if applicable)

## Phase 3: Implementation

### 3.1 AudioDecoderConfig
- [ ] Write failing tests for AudioDecoderConfig validation
- [ ] Confirm tests fail (RED)
- [ ] Implement AudioDecoderConfig dictionary:
  - `codec` (required DOMString)
  - `sampleRate` (required unsigned long)
  - `numberOfChannels` (required unsigned long)
  - `description` (optional BufferSource - codec-specific)
- [ ] Implement "valid AudioDecoderConfig" algorithm
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.2 VideoDecoderConfig
- [ ] Write failing tests for VideoDecoderConfig validation
- [ ] Confirm tests fail (RED)
- [ ] Implement VideoDecoderConfig dictionary:
  - `codec` (required DOMString)
  - `description` (optional BufferSource - extradata)
  - `codedWidth`, `codedHeight` (optional unsigned long)
  - `displayAspectWidth`, `displayAspectHeight` (optional)
  - `colorSpace` (optional VideoColorSpaceInit)
  - `hardwareAcceleration` (HardwareAcceleration enum)
  - `optimizeForLatency` (optional boolean)
- [ ] Implement "valid VideoDecoderConfig" algorithm
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.3 AudioEncoderConfig
- [ ] Write failing tests for AudioEncoderConfig validation
- [ ] Confirm tests fail (RED)
- [ ] Implement AudioEncoderConfig dictionary:
  - `codec` (required DOMString)
  - `sampleRate` (optional unsigned long)
  - `numberOfChannels` (optional unsigned long)
  - `bitrate` (optional unsigned long long)
  - `bitrateMode` (BitrateMode enum)
- [ ] Implement "valid AudioEncoderConfig" algorithm
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.4 VideoEncoderConfig
- [ ] Write failing tests for VideoEncoderConfig validation
- [ ] Confirm tests fail (RED)
- [ ] Implement VideoEncoderConfig dictionary:
  - `codec` (required DOMString)
  - `width`, `height` (required unsigned long)
  - `displayWidth`, `displayHeight` (optional)
  - `bitrate` (optional unsigned long long)
  - `bitrateMode` (BitrateMode: constant, variable, quantizer)
  - `framerate` (optional double)
  - `hardwareAcceleration` (HardwareAcceleration enum)
  - `alpha` (AlphaOption: discard, keep)
  - `scalabilityMode` (optional DOMString)
  - `latencyMode` (LatencyMode: quality, realtime)
  - `contentHint` (optional DOMString)
- [ ] Implement "valid VideoEncoderConfig" algorithm
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.5 VideoColorSpaceInit
- [ ] Write failing tests for VideoColorSpaceInit
- [ ] Confirm tests fail (RED)
- [ ] Implement VideoColorSpaceInit dictionary:
  - `primaries` (VideoColorPrimaries enum)
  - `transfer` (VideoTransferCharacteristics enum)
  - `matrix` (VideoMatrixCoefficients enum)
  - `fullRange` (optional boolean)
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.6 Enums
- [ ] Write failing tests for enum values
- [ ] Confirm tests fail (RED)
- [ ] Implement enums:
  - `HardwareAcceleration`: no-preference, prefer-hardware, prefer-software
  - `BitrateMode`: constant, variable, quantizer
  - `LatencyMode`: quality, realtime
  - `AlphaOption`: discard, keep
  - `VideoColorPrimaries`: bt709, bt470bg, smpte170m, bt2020, smpte432
  - `VideoTransferCharacteristics`: bt709, smpte170m, iec61966-2-1, linear, pq, hlg
  - `VideoMatrixCoefficients`: rgb, bt709, bt470bg, smpte170m, bt2020-ncl
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.7 Support Dictionaries
- [ ] Write failing tests for support dictionaries
- [ ] Confirm tests fail (RED)
- [ ] Implement:
  - `AudioDecoderSupport` { supported, config }
  - `VideoDecoderSupport` { supported, config }
  - `AudioEncoderSupport` { supported, config }
  - `VideoEncoderSupport` { supported, config }
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.8 Clone Configuration Algorithm
- [ ] Write failing tests for config cloning
- [ ] Confirm tests fail (RED)
- [ ] Implement "Clone Configuration" algorithm:
  - Deep clone all dictionary members
  - Handle BufferSource (description) properly
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.9 Check Configuration Support Algorithm
- [ ] Write failing tests for support checking
- [ ] Confirm tests fail (RED)
- [ ] Implement "Check Configuration Support":
  - Validate codec string format
  - Query FFmpeg for codec availability
  - Check parameter bounds (resolution, bitrate, etc.)
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

## Phase 4: Integration
- [ ] Verify all components work together
- [ ] Run full test suite
- [ ] Run integration tests
- [ ] Test config validation with all codec types
- [ ] Update documentation

## Phase 5: Verification
- [ ] Code review checklist complete
- [ ] No hardcoded test values
- [ ] Edge cases handled:
  - Missing required fields
  - Invalid codec strings
  - Out-of-range values
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
- Configurations are shared across all codec interfaces
- Codec string validation per codec-string-registry.md
- BufferSource handling must be careful with ArrayBuffer views
- HardwareAcceleration maps to FFmpeg hwaccel options
