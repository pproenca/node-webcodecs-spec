# Generate Tasks Rewrite Design

> Created: 2026-01-01
> Status: Design Complete

## Overview

Rewrite `scripts/generate-tasks.ts` to produce JSON-based PRD files with code links, replacing the current markdown output. The generator will parse both WebIDL and spec markdown to produce comprehensive feature lists that link directly to implementation code.

## Pipeline Architecture

```
┌────────────────────────────────────────────────────────────────┐
│  Stage 1: scaffold-project.ts (EXISTS)                         │
│  Input:  W3C bikeshed specs                                    │
│  Output: spec/context/_webcodecs.idl                           │
│          spec/context/<Interface>.md (with algorithms)         │
└────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌────────────────────────────────────────────────────────────────┐
│  Stage 2: Code Scaffold (EXISTS)                               │
│  Files already generated:                                      │
│    src/<Interface>.h   → GetState(), Configure(), etc.         │
│    src/<Interface>.cpp → Implementation                        │
│    lib/<Interface>.ts  → TypeScript wrapper                    │
│    test/*.test.ts      → Test files                            │
└────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌────────────────────────────────────────────────────────────────┐
│  Stage 3: generate-tasks.ts (REWRITE)                          │
│  NEW: Generates JSON PRD with code links                       │
│                                                                │
│  Inputs:                                                       │
│    - spec/context/_webcodecs.idl (WebIDL)                     │
│    - spec/context/*.md (algorithm steps)                       │
│    - src/*.h, src/*.cpp (C++ code via tree-sitter)            │
│    - lib/*.ts, test/*.ts (TS code via ts-morph)               │
│                                                                │
│  Outputs:                                                      │
│    - docs/tasks/<Interface>.json (per interface)              │
│    - docs/tasks/types.json (dictionaries + enums)             │
│                                                                │
│  Behavior:                                                     │
│    - FAIL if any IDL symbol not found in code                 │
│    - All features have passes: false initially                │
│    - Links point to actual line numbers                        │
└────────────────────────────────────────────────────────────────┘
```

## JSON Output Schema

### Interface File (`docs/tasks/<Interface>.json`)

```typescript
interface TaskFile {
  interface: string;
  type: 'interface';
  source: {
    idl: string; // "spec/context/_webcodecs.idl#L606-L620"
    spec: string; // "spec/context/VideoDecoder.md"
  };
  inheritance?: string; // "EventTarget"
  extendedAttributes: string[]; // ["Exposed=(Window,DedicatedWorker)", "SecureContext"]
  files: {
    cppHeader: string;
    cppImpl: string;
    tsWrapper: string;
    tests: string;
  };
  features: Feature[];
}

interface Feature {
  id: string; // "VideoDecoder.configure"
  category: 'constructor' | 'attribute' | 'method' | 'static-method' | 'getter' | 'iterable';
  name: string; // "configure(VideoDecoderConfig config)"
  description: string; // From spec
  returnType?: string;
  readonly?: boolean;
  codeLinks: {
    declaration: CodeLink;
    implementation: CodeLink;
    tsBinding: CodeLink;
    test?: CodeLink;
  };
  algorithmRef: string; // "spec/context/VideoDecoder.md#configure"
  algorithmSteps: string[]; // Extracted from spec markdown
  steps: Step[];
  passes: boolean; // Always false initially
}

interface Step {
  id: string; // "VideoDecoder.configure.validate-config"
  description: string;
  codeRef: CodeLink; // Points to implementation line range
  passes: boolean;
}

interface CodeLink {
  file: string;
  line: number;
  endLine?: number;
}
```

### Types File (`docs/tasks/types.json`)

```typescript
interface TypesFile {
  dictionaries: DictionaryDef[];
  enums: EnumDef[];
}

interface DictionaryDef {
  name: string;
  source: { idl: string };
  fields: DictionaryField[];
  features: Feature[];
}

interface DictionaryField {
  name: string;
  type: string;
  required: boolean;
  defaultValue?: string;
}

interface EnumDef {
  name: string;
  source: { idl: string };
  values: string[];
  features: Feature[];
}
```

## Example Output

### VideoDecoder.json

```json
{
  "interface": "VideoDecoder",
  "type": "interface",
  "source": {
    "idl": "spec/context/_webcodecs.idl#L606-L620",
    "spec": "spec/context/VideoDecoder.md"
  },
  "inheritance": "EventTarget",
  "extendedAttributes": ["Exposed=(Window,DedicatedWorker)", "SecureContext"],
  "files": {
    "cppHeader": "src/VideoDecoder.h",
    "cppImpl": "src/VideoDecoder.cpp",
    "tsWrapper": "lib/VideoDecoder.ts",
    "tests": "test/videodecoder.test.ts"
  },
  "features": [
    {
      "id": "VideoDecoder.constructor",
      "category": "constructor",
      "name": "constructor(VideoDecoderInit init)",
      "description": "Create VideoDecoder with output/error callbacks",
      "codeLinks": {
        "declaration": { "file": "src/VideoDecoder.h", "line": 22 },
        "implementation": { "file": "src/VideoDecoder.cpp", "line": 35, "endLine": 55 },
        "tsBinding": { "file": "lib/VideoDecoder.ts", "line": 42, "endLine": 45 }
      },
      "algorithmRef": "spec/context/VideoDecoder.md#constructor",
      "algorithmSteps": ["Initialize internal slots"],
      "steps": [
        {
          "id": "VideoDecoder.constructor.validate-init",
          "description": "Validate init parameter contains output callback",
          "codeRef": { "file": "src/VideoDecoder.cpp", "line": 38, "endLine": 42 },
          "passes": false
        },
        {
          "id": "VideoDecoder.constructor.init-state",
          "description": "Initialize state slot to 'unconfigured'",
          "codeRef": { "file": "src/VideoDecoder.cpp", "line": 44 },
          "passes": false
        },
        {
          "id": "VideoDecoder.constructor.init-queue",
          "description": "Initialize decodeQueueSize to 0",
          "codeRef": { "file": "src/VideoDecoder.cpp", "line": 45 },
          "passes": false
        }
      ],
      "passes": false
    },
    {
      "id": "VideoDecoder.state",
      "category": "attribute",
      "name": "state",
      "description": "Returns current codec state (unconfigured/configured/closed)",
      "returnType": "CodecState",
      "readonly": true,
      "codeLinks": {
        "declaration": { "file": "src/VideoDecoder.h", "line": 50 },
        "implementation": { "file": "src/VideoDecoder.cpp", "line": 80, "endLine": 82 },
        "tsBinding": { "file": "lib/VideoDecoder.ts", "line": 47, "endLine": 49 }
      },
      "algorithmRef": "spec/context/VideoDecoder.md#attributes",
      "algorithmSteps": [],
      "steps": [
        {
          "id": "VideoDecoder.state.getter",
          "description": "C++: Implement GetState() returning CodecState enum",
          "codeRef": { "file": "src/VideoDecoder.cpp", "line": 80, "endLine": 82 },
          "passes": false
        },
        {
          "id": "VideoDecoder.state.ts-binding",
          "description": "TS: Wire getter to native",
          "codeRef": { "file": "lib/VideoDecoder.ts", "line": 47, "endLine": 49 },
          "passes": false
        },
        {
          "id": "VideoDecoder.state.test-initial",
          "description": "Test: Verify initial value is 'unconfigured'",
          "codeRef": { "file": "test/videodecoder.test.ts", "line": 15, "endLine": 20 },
          "passes": false
        }
      ],
      "passes": false
    },
    {
      "id": "VideoDecoder.configure",
      "category": "method",
      "name": "configure(VideoDecoderConfig config)",
      "description": "Configure decoder for given codec configuration",
      "returnType": "void",
      "codeLinks": {
        "declaration": { "file": "src/VideoDecoder.h", "line": 56 },
        "implementation": { "file": "src/VideoDecoder.cpp", "line": 120, "endLine": 180 },
        "tsBinding": { "file": "lib/VideoDecoder.ts", "line": 61, "endLine": 63 }
      },
      "algorithmRef": "spec/context/VideoDecoder.md#configure",
      "algorithmSteps": [
        "If config is not a valid VideoDecoderConfig, throw a TypeError",
        "If state is 'closed', throw an InvalidStateError",
        "Set state to 'configured'",
        "Set key chunk required to true",
        "Queue a control message to configure the decoder with config",
        "Process the control message queue"
      ],
      "steps": [
        {
          "id": "VideoDecoder.configure.validate-config",
          "description": "C++: Validate VideoDecoderConfig structure",
          "codeRef": { "file": "src/VideoDecoder.cpp", "line": 122, "endLine": 130 },
          "passes": false
        },
        {
          "id": "VideoDecoder.configure.check-closed",
          "description": "C++: Check state is not 'closed', throw InvalidStateError",
          "codeRef": { "file": "src/VideoDecoder.cpp", "line": 132, "endLine": 135 },
          "passes": false
        },
        {
          "id": "VideoDecoder.configure.set-state",
          "description": "C++: Transition state to 'configured'",
          "codeRef": { "file": "src/VideoDecoder.cpp", "line": 137 },
          "passes": false
        },
        {
          "id": "VideoDecoder.configure.key-chunk-flag",
          "description": "C++: Set key chunk required flag to true",
          "codeRef": { "file": "src/VideoDecoder.cpp", "line": 138 },
          "passes": false
        },
        {
          "id": "VideoDecoder.configure.queue-message",
          "description": "C++: Queue async configuration via control message",
          "codeRef": { "file": "src/VideoDecoder.cpp", "line": 140, "endLine": 160 },
          "passes": false
        },
        {
          "id": "VideoDecoder.configure.ts-binding",
          "description": "TS: Wire method to native implementation",
          "codeRef": { "file": "lib/VideoDecoder.ts", "line": 61, "endLine": 63 },
          "passes": false
        },
        {
          "id": "VideoDecoder.configure.test-valid-h264",
          "description": "Test: Configure with valid H.264 config",
          "codeRef": { "file": "test/videodecoder.test.ts", "line": 45, "endLine": 55 },
          "passes": false
        },
        {
          "id": "VideoDecoder.configure.test-invalid",
          "description": "Test: Throw TypeError for invalid config",
          "codeRef": { "file": "test/videodecoder.test.ts", "line": 57, "endLine": 65 },
          "passes": false
        }
      ],
      "passes": false
    },
    {
      "id": "VideoDecoder.isConfigSupported",
      "category": "static-method",
      "name": "isConfigSupported(VideoDecoderConfig config)",
      "description": "Check if config is supported without creating decoder",
      "returnType": "Promise<VideoDecoderSupport>",
      "codeLinks": {
        "declaration": { "file": "src/VideoDecoder.h", "line": 61 },
        "implementation": { "file": "src/VideoDecoder.cpp", "line": 250, "endLine": 290 },
        "tsBinding": { "file": "lib/VideoDecoder.ts", "line": 77, "endLine": 80 }
      },
      "algorithmRef": "spec/context/VideoDecoder.md#isconfigsupported",
      "algorithmSteps": [
        "If config is not a valid VideoDecoderConfig, reject with TypeError",
        "Let p be a new Promise",
        "Enqueue check to parallel queue",
        "Run Check Configuration Support algorithm",
        "Resolve p with VideoDecoderSupport"
      ],
      "steps": [
        {
          "id": "VideoDecoder.isConfigSupported.static-method",
          "description": "C++: Implement as static method on class",
          "codeRef": { "file": "src/VideoDecoder.cpp", "line": 250, "endLine": 255 },
          "passes": false
        },
        {
          "id": "VideoDecoder.isConfigSupported.validate",
          "description": "C++: Validate config before checking support",
          "codeRef": { "file": "src/VideoDecoder.cpp", "line": 257, "endLine": 262 },
          "passes": false
        },
        {
          "id": "VideoDecoder.isConfigSupported.query-ffmpeg",
          "description": "C++: Query FFmpeg for codec support",
          "codeRef": { "file": "src/VideoDecoder.cpp", "line": 264, "endLine": 280 },
          "passes": false
        },
        {
          "id": "VideoDecoder.isConfigSupported.return-promise",
          "description": "C++: Return Promise with VideoDecoderSupport",
          "codeRef": { "file": "src/VideoDecoder.cpp", "line": 282, "endLine": 290 },
          "passes": false
        },
        {
          "id": "VideoDecoder.isConfigSupported.ts-binding",
          "description": "TS: Expose as static method on class",
          "codeRef": { "file": "lib/VideoDecoder.ts", "line": 77, "endLine": 80 },
          "passes": false
        },
        {
          "id": "VideoDecoder.isConfigSupported.test-supported",
          "description": "Test: Check H.264 support returns true",
          "codeRef": { "file": "test/videodecoder.test.ts", "line": 120, "endLine": 128 },
          "passes": false
        }
      ],
      "passes": false
    }
  ]
}
```

## Symbol Matching Rules

| IDL Member                   | C++ Header Symbol              | C++ Impl Symbol                   | TS Symbol                    |
| ---------------------------- | ------------------------------ | --------------------------------- | ---------------------------- |
| `attribute state` (readonly) | `GetState`                     | `VideoDecoder::GetState`          | `get state()`                |
| `attribute ondequeue`        | `GetOndequeue`, `SetOndequeue` | Both                              | `get/set ondequeue`          |
| `operation configure`        | `Configure`                    | `VideoDecoder::Configure`         | `configure()`                |
| `static isConfigSupported`   | `IsConfigSupported`            | `VideoDecoder::IsConfigSupported` | `static isConfigSupported()` |
| `constructor`                | class constructor              | `ClassName::ClassName`            | `constructor()`              |
| `getter (unsigned long)`     | `operator[]` or `Get`          | Implementation                    | `[index: number]`            |

## Dependencies

```json
{
  "devDependencies": {
    "tree-sitter": "^0.21.0",
    "tree-sitter-cpp": "^0.22.0",
    "ts-morph": "^21.0.0"
  }
}
```

## Implementation Steps

### Task 1: Add Dependencies

- Install tree-sitter, tree-sitter-cpp, ts-morph
- Verify C++ parsing works with sample header

### Task 2: Implement C++ AST Parser

- Create `parseCppFile(path)` returning symbol map
- Handle class declarations, method declarations
- Extract line numbers for each symbol

### Task 3: Implement TypeScript AST Parser

- Create `parseTsFile(path)` returning symbol map
- Handle class, methods, getters/setters, static methods
- Parse test files for describe/it blocks

### Task 4: Implement Spec Markdown Parser

- Create `parseSpecMarkdown(path)` returning algorithms
- Extract method signatures and algorithm steps
- Handle ## Methods, ### method_name, **Algorithm:** sections

### Task 5: Implement Symbol Matcher

- Create `matchSymbols(idl, cppSymbols, tsSymbols)`
- Map IDL members to code locations
- FAIL with detailed error if symbol not found

### Task 6: Generate JSON Output

- Create interface JSON files
- Create types.json for dictionaries/enums
- All `passes: false` initially

### Task 7: Update package.json

- Add `npm run tasks` script
- Update `npm run pipeline` to include tasks step

## Validation

Generator MUST fail if:

- Any IDL interface method has no C++ declaration
- Any IDL attribute has no corresponding getter
- Any interface has no test file
- Any method has no TS binding

## Usage

```bash
# Generate tasks after scaffold exists
npm run tasks

# Full pipeline
npm run pipeline  # scaffold → tasks
```
