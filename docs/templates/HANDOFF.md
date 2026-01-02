# Artifact: [COMPONENT_NAME]

> **Agent:** [SUBAGENT_ID]
> **Task Packet:** [PACKET_REF]
> **Completed:** [TIMESTAMP]

## Status
- **Implementation:** COMPLETE | PARTIAL | BLOCKED
- **Tests:** PASSING | FAILING | NOT_WRITTEN
- **Blocking issues:** NONE | [DESCRIPTION]

## Files Modified
| File | Action | Lines Changed |
|------|--------|---------------|
| [PATH] | Created/Modified | [+X/-Y] |
| [PATH] | Created/Modified | [+X/-Y] |

## Exported Interface

```typescript
// [FILE_PATH]

export interface [InterfaceName] {
  [PROPERTY]: [TYPE];
}

export function [functionName]([PARAMS]): [RETURN_TYPE];
export function [functionName]([PARAMS]): [RETURN_TYPE];
```

## Dependencies Required
- **Internal:** [MODULE_1], [MODULE_2]
- **External:** [PACKAGE_1]@[VERSION]
- **Environment:** [ENV_VAR_1], [ENV_VAR_2]

## Test Summary
- **Total tests:** [N]
- **Passing:** [N]
- **Failing:** [N]
- **Skipped:** [N]

### Test Coverage
| Area | Coverage |
|------|----------|
| [COMPONENT] | [X]% |
| Edge cases | [X]% |
| Error paths | [X]% |

### Key Test Cases
1. [TEST_DESCRIPTION] - PASS
2. [TEST_DESCRIPTION] - PASS
3. [TEST_DESCRIPTION] - PASS

## Integration Notes

### How to Use This Component
```typescript
import { [EXPORTS] } from '[PATH]';

// Basic usage
const result = [functionName]([EXAMPLE_PARAMS]);
```

### Expected Inputs
- [INPUT_1]: [TYPE] - [DESCRIPTION]
- [INPUT_2]: [TYPE] - [DESCRIPTION]

### Expected Outputs
- [OUTPUT_1]: [TYPE] - [DESCRIPTION]

### Error Handling
- [ERROR_CONDITION]: Throws [ERROR_TYPE] with message [MSG]
- [ERROR_CONDITION]: Returns [FALLBACK]

## Coordination with Other Components

### Depends On (Upstream)
- [COMPONENT]: Uses [INTERFACE] for [PURPOSE]

### Depended By (Downstream)
- [COMPONENT]: Will consume [INTERFACE] for [PURPOSE]

### Shared State
- [STATE_ITEM]: [WHERE_STORED], [HOW_ACCESSED]

## Implementation Notes
<!-- Brief notes on non-obvious implementation choices -->
- [NOTE]
- [NOTE]

## Known Limitations
- [LIMITATION_1]
- [LIMITATION_2]

## Deferred Work
<!-- Items explicitly out of scope for this task packet -->
- [ ] [DEFERRED_ITEM] - Reason: [WHY]
- [ ] [DEFERRED_ITEM] - Reason: [WHY]

---

## Verification Checklist (For Review Agent)
- [ ] No hardcoded values specific to tests
- [ ] Handles edge cases: [LIST]
- [ ] Error handling complete
- [ ] Types are strict (no `any`, no `as` casts without justification)
- [ ] Follows codebase patterns
- [ ] No security vulnerabilities
- [ ] Performance acceptable

## Handoff Ready
- [ ] All above sections completed
- [ ] Tests passing
- [ ] Ready for integration