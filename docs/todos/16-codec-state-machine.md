# Task: Codec State Machine Implementation

> **Created:** 2026-01-02
> **Spec:** [docs/specs/16-codec-state-machine.md](../specs/16-codec-state-machine.md)
> **Branch:** feat/state-machine

## Success Criteria
- [ ] All tests pass (`npm test`)
- [ ] Type check passes (`npm run typecheck`)
- [ ] Linting clean (`npm run lint`)
- [ ] All checklist items below marked complete
- [ ] PR description created

---

## Phase 1: Investigation (NO CODING)
- [ ] Read relevant files: codec implementations
- [ ] Document current state management in NOTES.md
- [ ] Identify state transition gaps
- [ ] List dependencies and constraints
- [ ] Update plan.md with findings

## Phase 2: Planning
- [ ] Create detailed implementation plan
- [ ] Define state transition table
- [ ] Identify parallelizable work units
- [ ] Get human approval on plan
- [ ] Create task packets for subagents (if applicable)

## Phase 3: Implementation

### 3.1 CodecState Enum
- [ ] Write failing tests for CodecState enum
- [ ] Confirm tests fail (RED)
- [ ] Implement CodecState:
  - `"unconfigured"` - Initial state
  - `"configured"` - Ready for encode/decode
  - `"closed"` - Terminal state
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.2 State Transition Validation
- [ ] Write failing tests for state transitions
- [ ] Confirm tests fail (RED)
- [ ] Implement state machine:
  ```
  unconfigured --configure()--> configured
  configured --decode/encode()--> configured
  configured --flush()--> configured
  configured --reset()--> unconfigured
  configured --close()--> closed
  unconfigured --close()--> closed
  * --error--> closed (for fatal errors)
  ```
- [ ] Throw InvalidStateError for invalid transitions
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.3 AudioDecoder State Machine
- [ ] Write failing tests for AudioDecoder states
- [ ] Confirm tests fail (RED)
- [ ] Implement AudioDecoder state handling:
  - configure() requires unconfigured or configured
  - decode() requires configured
  - flush() requires configured
  - reset() requires not closed
  - close() always allowed
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.4 VideoDecoder State Machine
- [ ] Write failing tests for VideoDecoder states
- [ ] Confirm tests fail (RED)
- [ ] Implement VideoDecoder state handling:
  - configure() requires unconfigured or configured
  - decode() requires configured
  - flush() requires configured
  - reset() requires not closed
  - close() always allowed
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.5 AudioEncoder State Machine
- [ ] Write failing tests for AudioEncoder states
- [ ] Confirm tests fail (RED)
- [ ] Implement AudioEncoder state handling:
  - configure() requires unconfigured or configured
  - encode() requires configured
  - flush() requires configured
  - reset() requires not closed
  - close() always allowed
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.6 VideoEncoder State Machine
- [ ] Write failing tests for VideoEncoder states
- [ ] Confirm tests fail (RED)
- [ ] Implement VideoEncoder state handling:
  - configure() requires unconfigured or configured
  - encode() requires configured
  - flush() requires configured
  - reset() requires not closed
  - close() always allowed
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.7 Fatal Error Handling
- [ ] Write failing tests for fatal errors
- [ ] Confirm tests fail (RED)
- [ ] Implement fatal error state transition:
  - On EncodingError -> close() with error
  - Invoke error callback
  - Set state to closed
  - Cannot recover - must create new instance
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.8 State Change Events
- [ ] Write failing tests for state change detection
- [ ] Confirm tests fail (RED)
- [ ] Implement state observation:
  - Expose current state via readonly attribute
  - Consider state change events (optional, non-spec)
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

## Phase 4: Integration
- [ ] Verify all components work together
- [ ] Run full test suite
- [ ] Test all state transitions
- [ ] Test invalid transition handling
- [ ] Update documentation

## Phase 5: Verification
- [ ] Code review checklist complete
- [ ] No hardcoded test values
- [ ] All state transitions spec-compliant
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
- State machine diagram in spec section 16
- "closed" is terminal - no recovery
- reset() returns to unconfigured but keeps instance
- configure() can be called while configured (reconfiguration)
- Queue size resets on reset() and close()
