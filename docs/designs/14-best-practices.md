# Task: Best Practices Documentation & Tooling

> **Created:** 2026-01-02
> **Spec:** [docs/specs/14-best-practices-for-authors-using-webcodecs.md](../specs/14-best-practices-for-authors-using-webcodecs.md)
> **Branch:** feat/best-practices

## Success Criteria
- [ ] All tests pass (`npm test`)
- [ ] Type check passes (`npm run typecheck`)
- [ ] Linting clean (`npm run lint`)
- [ ] All checklist items below marked complete
- [ ] PR description created

---

## Phase 1: Investigation (NO CODING)
- [ ] Read spec best practices section
- [ ] Review common usage patterns
- [ ] Identify anti-patterns to detect
- [ ] List dependencies and constraints
- [ ] Update plan.md with findings

## Phase 2: Planning
- [ ] Create detailed implementation plan
- [ ] Define tooling requirements
- [ ] Identify parallelizable work units
- [ ] Get human approval on plan
- [ ] Create task packets for subagents (if applicable)

## Phase 3: Implementation

### 3.1 Resource Management Warnings
- [ ] Write failing tests for resource warnings
- [ ] Confirm tests fail (RED)
- [ ] Implement resource warnings:
  - Warn on unclosed VideoFrame after N seconds
  - Warn on unclosed AudioData after N seconds
  - Warn on high decodeQueueSize
  - Configurable warning thresholds
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.2 Configuration Validation Helpers
- [ ] Write failing tests for config helpers
- [ ] Confirm tests fail (RED)
- [ ] Implement helper utilities:
  - `validateVideoEncoderConfig()` with detailed errors
  - `validateAudioEncoderConfig()` with suggestions
  - `recommendedConfigForCodec(codec)` helper
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.3 Performance Monitoring
- [ ] Write failing tests for perf monitoring
- [ ] Confirm tests fail (RED)
- [ ] Implement performance helpers:
  - Decode/encode throughput metrics
  - Queue depth monitoring
  - Frame drop detection
  - Latency measurement
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.4 Debug Mode
- [ ] Write failing tests for debug mode
- [ ] Confirm tests fail (RED)
- [ ] Implement debug mode:
  - Enable via environment variable or API
  - Verbose logging of codec operations
  - Frame timing diagnostics
  - Memory usage reporting
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.5 Example Code Validation
- [ ] Write tests that validate example code runs
- [ ] Confirm tests fail (RED)
- [ ] Create validated example code:
  - Basic decode pipeline
  - Basic encode pipeline
  - Transcoding example
  - Streaming decode example
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

## Phase 4: Integration
- [ ] Verify all components work together
- [ ] Run full test suite
- [ ] Run integration tests
- [ ] Validate all examples work
- [ ] Update documentation

## Phase 5: Verification
- [ ] Code review checklist complete
- [ ] No hardcoded test values
- [ ] Examples are correct and complete
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
- Best practices spec emphasizes:
  - Close frames immediately when done
  - Use isConfigSupported before configure
  - Handle backpressure via queueSize
  - Use transfer for worker communication
- Debug tooling improves developer experience
- Examples should be copy-pasteable and working
