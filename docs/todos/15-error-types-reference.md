# Task: Error Types Reference Implementation

> **Created:** 2026-01-02
> **Spec:** [docs/specs/15-error-types-reference.md](../specs/15-error-types-reference.md)
> **Branch:** feat/error-types

## Success Criteria
- [ ] All tests pass (`npm test`)
- [ ] Type check passes (`npm run typecheck`)
- [ ] Linting clean (`npm run lint`)
- [ ] All checklist items below marked complete
- [ ] PR description created

---

## Phase 1: Investigation (NO CODING)
- [ ] Read relevant files: `src/error_builder.h`, error handling patterns
- [ ] Document current error patterns in NOTES.md
- [ ] Identify missing error types
- [ ] List dependencies and constraints
- [ ] Update plan.md with findings

## Phase 2: Planning
- [ ] Create detailed implementation plan
- [ ] Define error type mappings
- [ ] Identify parallelizable work units
- [ ] Get human approval on plan
- [ ] Create task packets for subagents (if applicable)

## Phase 3: Implementation

### 3.1 DOMException Types
- [ ] Write failing tests for DOMException types
- [ ] Confirm tests fail (RED)
- [ ] Implement DOMException builders:
  - `TypeError` - Invalid argument types
  - `InvalidStateError` - Wrong codec state
  - `NotSupportedError` - Unsupported config
  - `DataError` - Invalid chunk data
  - `EncodingError` - Decode/encode failure
  - `AbortError` - Operation cancelled
  - `RangeError` - Out of bounds
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.2 Error Builder C++ Implementation
- [ ] Write failing tests for error_builder.h
- [ ] Confirm tests fail (RED)
- [ ] Enhance `src/error_builder.h`:
  - `TypeError::create(message)`
  - `InvalidStateError::create(message)`
  - `NotSupportedError::create(message)`
  - `DataError::create(message)`
  - `EncodingError::create(message)`
  - `AbortError::create(message)`
  - `RangeError::create(message)`
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.3 FFmpeg Error Mapping
- [ ] Write failing tests for FFmpeg error mapping
- [ ] Confirm tests fail (RED)
- [ ] Implement FFmpeg error mapping:
  - `AVERROR_INVALIDDATA` -> DataError
  - `AVERROR(EAGAIN)` -> Not an error, state transition
  - `AVERROR_EOF` -> Not an error, end of stream
  - `AVERROR(EINVAL)` -> TypeError
  - `AVERROR(ENOMEM)` -> QuotaExceededError
  - `AVERROR_EXTERNAL` -> EncodingError
  - `AVERROR_DECODER_NOT_FOUND` -> NotSupportedError
  - `AVERROR_ENCODER_NOT_FOUND` -> NotSupportedError
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.4 WebCodecsErrorCallback
- [ ] Write failing tests for error callback
- [ ] Confirm tests fail (RED)
- [ ] Implement error callback invocation:
  - Format DOMException correctly
  - Include meaningful message
  - Invoke from codec work queue via task
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.5 Error Messages Standardization
- [ ] Write failing tests for error messages
- [ ] Confirm tests fail (RED)
- [ ] Standardize error messages:
  - "State must be 'configured' to decode"
  - "State must not be 'closed'"
  - "Codec not supported: {codec}"
  - "Key chunk required"
  - "Invalid AudioDecoderConfig"
  - "Invalid VideoDecoderConfig"
  - "Frame is closed"
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.6 Promise Rejection Handling
- [ ] Write failing tests for promise rejections
- [ ] Confirm tests fail (RED)
- [ ] Implement promise rejection:
  - flush() rejection with InvalidStateError
  - flush() rejection with AbortError on reset
  - isConfigSupported() rejection with TypeError
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

## Phase 4: Integration
- [ ] Verify all components work together
- [ ] Run full test suite
- [ ] Verify error messages match spec
- [ ] Test error paths in all interfaces
- [ ] Update documentation

## Phase 5: Verification
- [ ] Code review checklist complete
- [ ] No hardcoded test values
- [ ] All error types spec-compliant
- [ ] Error messages are clear
- [ ] Types are strict (no `any`)

## Phase 6: Finalize
- [ ] All success criteria met
- [ ] PR description written
- [ ] Ready for human review

---

## Blockers
<!-- Add any blockers encountered -->

## Notes
- All errors must be DOMException per spec
- AVERROR(EAGAIN) and AVERROR_EOF are NOT errors - handle as state transitions
- error_builder.h should be the single source for error creation
- Error messages should be helpful but not expose internals
