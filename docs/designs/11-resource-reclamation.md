# Task: Resource Reclamation Implementation

> **Created:** 2026-01-02
> **Spec:** [docs/specs/11-resource-reclamation.md](../specs/11-resource-reclamation.md)
> **Branch:** feat/resource-reclamation

## Success Criteria
- [ ] All tests pass (`npm test`)
- [ ] Type check passes (`npm run typecheck`)
- [ ] Linting clean (`npm run lint`)
- [ ] All checklist items below marked complete
- [ ] PR description created

---

## Phase 1: Investigation (NO CODING)
- [ ] Read relevant files: memory management code in `src/`
- [ ] Document current patterns in NOTES.md
- [ ] Identify integration points with garbage collection
- [ ] List dependencies and constraints
- [ ] Update plan.md with findings

## Phase 2: Planning
- [ ] Create detailed implementation plan
- [ ] Define interface contracts
- [ ] Identify parallelizable work units
- [ ] Get human approval on plan
- [ ] Create task packets for subagents (if applicable)

## Phase 3: Implementation

### 3.1 Codec System Resources Tracking
- [ ] Write failing tests for resource tracking
- [ ] Confirm tests fail (RED)
- [ ] Implement resource tracking:
  - CPU memory allocation tracking
  - GPU memory usage estimation
  - Hardware decoder handle counting
  - Expose metrics for debugging
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.2 VideoFrame Resource Management
- [ ] Write failing tests for VideoFrame cleanup
- [ ] Confirm tests fail (RED)
- [ ] Implement proactive cleanup:
  - Track open VideoFrame instances
  - Log warnings for unclosed frames
  - Implement weak reference tracking
  - Automatic cleanup on garbage collection
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.3 AudioData Resource Management
- [ ] Write failing tests for AudioData cleanup
- [ ] Confirm tests fail (RED)
- [ ] Implement proactive cleanup:
  - Track open AudioData instances
  - Log warnings for unclosed data
  - Implement weak reference tracking
  - Automatic cleanup on garbage collection
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.4 Frame Pool Implementation
- [ ] Write failing tests for frame pooling
- [ ] Confirm tests fail (RED)
- [ ] Implement frame pool (`src/shared/frame_pool.h`):
  - Pre-allocated frame buffers
  - Reuse frames after close()
  - Configurable pool size
  - Thread-safe allocation
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.5 Memory Pressure Handling
- [ ] Write failing tests for memory pressure
- [ ] Confirm tests fail (RED)
- [ ] Implement memory pressure response:
  - Detect high memory usage
  - Force-close oldest unclosed frames
  - Throttle decoder output
  - Emit warnings/events
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.6 Codec Close Cleanup
- [ ] Write failing tests for codec cleanup
- [ ] Confirm tests fail (RED)
- [ ] Implement codec close:
  - Release AVCodecContext properly
  - Free hardware acceleration resources
  - Clear pending frames
  - Cancel pending operations
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.7 Finalizer Integration
- [ ] Write failing tests for finalizer behavior
- [ ] Confirm tests fail (RED)
- [ ] Implement C++ destructor integration:
  - Use Weak persistent handles
  - Clean up native resources on GC
  - Prevent leaks from abandoned JS objects
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

## Phase 4: Integration
- [ ] Verify all components work together
- [ ] Run full test suite
- [ ] Run memory leak tests (`npm run test:native:asan`)
- [ ] Run Valgrind analysis
- [ ] Stress test with many frames
- [ ] Update documentation

## Phase 5: Verification
- [ ] Code review checklist complete
- [ ] No hardcoded test values
- [ ] Edge cases handled:
  - GC timing variations
  - Rapid frame creation
  - Memory exhaustion
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
- Spec recommends immediate close() when frames no longer needed
- Node.js GC may delay cleanup - proactive close() is critical
- Hardware decoder resources are scarce - release immediately
- Use Napi weak references for finalizer integration
- Pool reuse significantly improves performance
