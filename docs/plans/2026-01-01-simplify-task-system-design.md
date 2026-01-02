# Simplify Task System Design

## Problem

The current task system is over-engineered:

- `docs/tasks/*.json` - 14 complex JSON files (~280KB) with nested features, steps, codeLinks
- `spec/context/*.md` - 13 markdown files auto-generated from webcodecs repo
- `scripts/generate-tasks.ts` + parsers/matchers - Complex infrastructure to generate JSON

This complexity doesn't provide value. The JSON task files aren't consumed by any workflow.

## Solution

Replace everything with one GitHub issue per H2 section from the W3C WebCodecs spec.

### What to Delete

```
spec/context/AudioDecoder.md
spec/context/VideoDecoder.md
spec/context/AudioEncoder.md
spec/context/VideoEncoder.md
spec/context/EncodedAudioChunk.md
spec/context/EncodedVideoChunk.md
spec/context/AudioData.md
spec/context/VideoFrame.md
spec/context/VideoColorSpace.md
spec/context/ImageDecoder.md
spec/context/ImageTrackList.md
spec/context/ImageTrack.md

docs/tasks/*.json (all 14 files)

scripts/generate-tasks.ts
scripts/generate-tasks.test.ts
scripts/parsers/spec-markdown-parser.ts
scripts/parsers/ts-ast-parser.ts
scripts/parsers/cpp-symbol-parser.ts
scripts/matchers/symbol-matcher.ts
scripts/types/task-schema.ts (if only used by generate-tasks)
```

### What to Keep

```
spec/context/_webcodecs.idl   - Still needed for code generation
scripts/scaffold-project.ts  - Still generates C++/TS scaffolds
src/*.cpp, src/*.h           - Generated native code
lib/*.ts                     - Generated TypeScript wrappers
```

### New Script: `scripts/generate-issues.ts`

Creates GitHub issues from the WebCodecs spec TOC.

**Behavior:**

1. Clone/pull webcodecs repo to `.cache/webcodecs`
2. Render spec with bikeshed to HTML
3. Parse HTML, extract each H2 section
4. Convert section content to markdown
5. Create one GitHub issue per H2 via `gh issue create`

**H2 Sections → Issues:**

| Section | Issue Title              |
| ------- | ------------------------ |
| 2       | Codec Processing Model   |
| 3       | AudioDecoder Interface   |
| 4       | VideoDecoder Interface   |
| 5       | AudioEncoder Interface   |
| 6       | VideoEncoder Interface   |
| 7       | Configurations           |
| 8       | Encoded Media Interfaces |
| 9       | Raw Media Interfaces     |
| 10      | Image Decoding           |

**Issue Body:** The rendered markdown content from that H2 section (includes all H3/H4 subsections).

### Directory Structure After

```
spec/
  context/
    _webcodecs.idl         # Only the IDL remains

.cache/
  webcodecs/               # Gitignored, cloned webcodecs repo

scripts/
  scaffold-project.ts     # Keep - generates code scaffolds
  generate-issues.ts      # New - creates GitHub issues from spec
  verify-types.ts         # Keep
```

### .gitignore Addition

```
.cache/
```

## Implementation Notes

1. **Bikeshed dependency:** The script needs bikeshed installed (`pip install bikeshed`)
2. **gh CLI:** Script uses `gh issue create` - user must be authenticated
3. **Idempotency:** Script should check if issue exists before creating (by title match)
4. **Labels:** Add `spec-section` label to all generated issues

## Outcome

- **Before:** 14 JSON files + 13 markdown files + complex parsing infrastructure
- **After:** ~10 GitHub issues with spec content, one simple script

Task tracking moves to GitHub's native system. Claude uses TodoWrite for session work.
