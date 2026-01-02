# Generate Tasks Rewrite Implementation Plan

> **Execution:** Use `/dev-workflow:execute-plan docs/plans/2026-01-01-generate-tasks-implementation.md` to implement task-by-task.

**Goal:** Rewrite `scripts/generate-tasks.ts` to produce JSON-based PRD files with code links, using AST parsing to link IDL members to actual source code locations.

**Architecture:** The generator parses WebIDL via webidl2, extracts algorithm steps from spec markdown, then uses ts-morph for TypeScript AST parsing and regex-based C++ parsing (simpler than tree-sitter for this use case). Symbol matching maps IDL members to code locations, failing if any symbol is missing.

**Tech Stack:** TypeScript, ts-morph (TS AST), webidl2 (IDL parsing), regex (C++ symbol location)

---

## Task Group 1: Setup and Types

### Task 1: Add ts-morph Dependency

**Files:**

- Modify: `package.json`

**Step 1: Install ts-morph**

```bash
npm install --save-dev ts-morph
```

**Step 2: Verify installation**

```bash
npm ls ts-morph
```

Expected: Shows ts-morph in dependency tree

**Step 3: Commit**

```bash
git add package.json package-lock.json
git commit -m "chore: add ts-morph for TypeScript AST parsing"
```

---

### Task 2: Define JSON Schema Types

**Files:**

- Create: `scripts/types/task-schema.ts`

**Step 1: Write the type definitions**

```typescript
// scripts/types/task-schema.ts
/**
 * JSON schema types for generated task files.
 * @see docs/plans/2026-01-01-generate-tasks-rewrite-design.md
 */

export interface CodeLink {
  file: string;
  line: number;
  endLine?: number;
}

export interface Step {
  id: string;
  description: string;
  codeRef?: CodeLink;
  passes: boolean;
}

export interface Feature {
  id: string;
  category: 'constructor' | 'attribute' | 'method' | 'static-method' | 'getter' | 'iterable';
  name: string;
  description: string;
  returnType?: string;
  readonly?: boolean;
  codeLinks: {
    declaration?: CodeLink;
    implementation?: CodeLink;
    tsBinding?: CodeLink;
    test?: CodeLink;
  };
  algorithmRef?: string;
  algorithmSteps: string[];
  steps: Step[];
  passes: boolean;
}

export interface TaskFile {
  interface: string;
  type: 'interface';
  source: {
    idl: string;
    spec?: string;
  };
  inheritance?: string;
  extendedAttributes: string[];
  files: {
    cppHeader: string;
    cppImpl: string;
    tsWrapper: string;
    tests: string;
  };
  features: Feature[];
}

export interface DictionaryField {
  name: string;
  type: string;
  required: boolean;
  defaultValue?: string;
}

export interface DictionaryDef {
  name: string;
  source: { idl: string };
  fields: DictionaryField[];
}

export interface EnumDef {
  name: string;
  source: { idl: string };
  values: string[];
}

export interface TypesFile {
  dictionaries: DictionaryDef[];
  enums: EnumDef[];
}
```

**Step 2: Verify TypeScript compiles**

```bash
npx tsc scripts/types/task-schema.ts --noEmit
```

Expected: No errors

**Step 3: Commit**

```bash
git add scripts/types/task-schema.ts
git commit -m "feat(tasks): add JSON schema types for task files"
```

---

## Task Group 2: Parsers (Can run in parallel)

### Task 3: Implement Spec Markdown Parser

**Files:**

- Create: `scripts/parsers/spec-markdown-parser.ts`
- Test: `scripts/parsers/spec-markdown-parser.test.ts`

**Step 1: Write the failing test**

```typescript
// scripts/parsers/spec-markdown-parser.test.ts
import { describe, it, expect } from 'vitest';
import { parseSpecMarkdown } from './spec-markdown-parser.js';

describe('parseSpecMarkdown', () => {
  it('should extract method algorithm steps', () => {
    const markdown = `
### configure

**Signature:** \`void configure(VideoDecoderConfig* config)\`

**Algorithm:**

1. If config is not valid, throw TypeError.
2. If state is "closed", throw InvalidStateError.
3. Set state to "configured".
`;
    const result = parseSpecMarkdown(markdown);

    expect(result.methods.get('configure')).toBeDefined();
    expect(result.methods.get('configure')?.signature).toBe(
      'void configure(VideoDecoderConfig* config)'
    );
    expect(result.methods.get('configure')?.algorithmSteps).toHaveLength(3);
    expect(result.methods.get('configure')?.algorithmSteps[0]).toContain('TypeError');
  });

  it('should identify static methods', () => {
    const markdown = `
### isConfigSupported

**Static Method**

**Signature:** \`Napi::Value isConfigSupported(VideoDecoderConfig* config)\`

**Algorithm:**

1. Returns a promise.
`;
    const result = parseSpecMarkdown(markdown);

    expect(result.methods.get('isConfigSupported')?.isStatic).toBe(true);
  });

  it('should extract attributes', () => {
    const markdown = `
## Attributes

- **state** (\`CodecState*\`) [ReadOnly]
- **ondequeue** (\`EventHandler*\`)
`;
    const result = parseSpecMarkdown(markdown);

    expect(result.attributes).toHaveLength(2);
    expect(result.attributes[0].name).toBe('state');
    expect(result.attributes[0].readonly).toBe(true);
    expect(result.attributes[1].name).toBe('ondequeue');
    expect(result.attributes[1].readonly).toBe(false);
  });
});
```

**Step 2: Run test to verify it fails**

```bash
npx vitest run scripts/parsers/spec-markdown-parser.test.ts
```

Expected: FAIL (module not found)

**Step 3: Write minimal implementation**

```typescript
// scripts/parsers/spec-markdown-parser.ts
/**
 * Parses spec context markdown files to extract algorithm steps.
 */

export interface SpecMethod {
  signature: string;
  isStatic: boolean;
  algorithmSteps: string[];
}

export interface SpecAttribute {
  name: string;
  type: string;
  readonly: boolean;
}

export interface SpecParseResult {
  methods: Map<string, SpecMethod>;
  attributes: SpecAttribute[];
  description?: string;
}

export function parseSpecMarkdown(content: string): SpecParseResult {
  const methods = new Map<string, SpecMethod>();
  const attributes: SpecAttribute[] = [];

  // Extract attributes from "## Attributes" section
  const attrMatch = content.match(/## Attributes\n\n([\s\S]*?)(?=\n## |\n---|\Z)/);
  if (attrMatch) {
    const attrLines = attrMatch[1].split('\n');
    for (const line of attrLines) {
      // Match: - **name** (`type`) [ReadOnly]
      const match = line.match(/^- \*\*(\w+)\*\* \(`([^`]+)`\)( \[ReadOnly\])?/);
      if (match) {
        attributes.push({
          name: match[1],
          type: match[2],
          readonly: !!match[3],
        });
      }
    }
  }

  // Extract methods from ### headers
  const methodRegex =
    /### (\w+)\n\n(?:\*\*Static Method\*\*\n\n)?\*\*Signature:\*\* `([^`]+)`\n\n\*\*Algorithm:\*\*\n\n([\s\S]*?)(?=\n### |\n## |\Z)/g;
  let match;
  while ((match = methodRegex.exec(content)) !== null) {
    const name = match[1];
    const signature = match[2];
    const isStatic = content.slice(match.index, match.index + 200).includes('**Static Method**');
    const algorithmText = match[3];

    // Extract numbered steps
    const steps: string[] = [];
    const stepRegex = /^\d+\. (.+)$/gm;
    let stepMatch;
    while ((stepMatch = stepRegex.exec(algorithmText)) !== null) {
      steps.push(stepMatch[1]);
    }

    methods.set(name, {
      signature,
      isStatic,
      algorithmSteps: steps,
    });
  }

  return { methods, attributes };
}

export async function parseSpecFile(filePath: string): Promise<SpecParseResult> {
  const fs = await import('node:fs/promises');
  const content = await fs.readFile(filePath, 'utf-8');
  return parseSpecMarkdown(content);
}
```

**Step 4: Run test to verify it passes**

```bash
npx vitest run scripts/parsers/spec-markdown-parser.test.ts
```

Expected: PASS (3 passed)

**Step 5: Commit**

```bash
git add scripts/parsers/spec-markdown-parser.ts scripts/parsers/spec-markdown-parser.test.ts
git commit -m "feat(tasks): add spec markdown parser for algorithm extraction"
```

---

### Task 4: Implement TypeScript AST Parser

**Files:**

- Create: `scripts/parsers/ts-ast-parser.ts`
- Test: `scripts/parsers/ts-ast-parser.test.ts`

**Step 1: Write the failing test**

```typescript
// scripts/parsers/ts-ast-parser.test.ts
import { describe, it, expect } from 'vitest';
import { parseTsFile, parseTsTestFile } from './ts-ast-parser.js';
import * as path from 'node:path';

describe('parseTsFile', () => {
  it('should extract class methods with line numbers', async () => {
    const filePath = path.join(process.cwd(), 'lib/VideoDecoder.ts');
    const result = await parseTsFile(filePath);

    expect(result.className).toBe('VideoDecoder');
    expect(result.methods.get('configure')).toBeDefined();
    expect(result.methods.get('configure')?.line).toBeGreaterThan(0);
  });

  it('should extract getters', async () => {
    const filePath = path.join(process.cwd(), 'lib/VideoDecoder.ts');
    const result = await parseTsFile(filePath);

    expect(result.getters.get('state')).toBeDefined();
    expect(result.getters.get('state')?.line).toBeGreaterThan(0);
  });

  it('should extract static methods', async () => {
    const filePath = path.join(process.cwd(), 'lib/VideoDecoder.ts');
    const result = await parseTsFile(filePath);

    expect(result.staticMethods.get('isConfigSupported')).toBeDefined();
  });

  it('should extract constructor', async () => {
    const filePath = path.join(process.cwd(), 'lib/VideoDecoder.ts');
    const result = await parseTsFile(filePath);

    expect(result.constructor).toBeDefined();
    expect(result.constructor?.line).toBeGreaterThan(0);
  });
});

describe('parseTsTestFile', () => {
  it('should extract describe/it blocks with line numbers', async () => {
    const filePath = path.join(process.cwd(), 'test/video-decoder.test.ts');
    const result = await parseTsTestFile(filePath);

    expect(result.describes.get('VideoDecoder')).toBeDefined();
    expect(result.itBlocks.length).toBeGreaterThan(0);
  });
});
```

**Step 2: Run test to verify it fails**

```bash
npx vitest run scripts/parsers/ts-ast-parser.test.ts
```

Expected: FAIL (module not found)

**Step 3: Write minimal implementation**

```typescript
// scripts/parsers/ts-ast-parser.ts
/**
 * TypeScript AST parser using ts-morph for extracting class symbols.
 */
import { Project, SyntaxKind } from 'ts-morph';
import type { CodeLink } from '../types/task-schema.js';

export interface SymbolLocation {
  line: number;
  endLine?: number;
}

export interface TsClassParseResult {
  className: string;
  constructor?: SymbolLocation;
  methods: Map<string, SymbolLocation>;
  getters: Map<string, SymbolLocation>;
  setters: Map<string, SymbolLocation>;
  staticMethods: Map<string, SymbolLocation>;
}

export interface TsTestParseResult {
  describes: Map<string, SymbolLocation>;
  itBlocks: Array<{ name: string; location: SymbolLocation }>;
}

export async function parseTsFile(filePath: string): Promise<TsClassParseResult> {
  const project = new Project({ useInMemoryFileSystem: false });
  const sourceFile = project.addSourceFileAtPath(filePath);

  const classes = sourceFile.getClasses();
  if (classes.length === 0) {
    throw new Error(`No class found in ${filePath}`);
  }

  const cls = classes[0];
  const result: TsClassParseResult = {
    className: cls.getName() || 'Unknown',
    methods: new Map(),
    getters: new Map(),
    setters: new Map(),
    staticMethods: new Map(),
  };

  // Extract constructor
  const ctor = cls.getConstructors()[0];
  if (ctor) {
    result.constructor = {
      line: ctor.getStartLineNumber(),
      endLine: ctor.getEndLineNumber(),
    };
  }

  // Extract methods
  for (const method of cls.getMethods()) {
    const name = method.getName();
    const location: SymbolLocation = {
      line: method.getStartLineNumber(),
      endLine: method.getEndLineNumber(),
    };

    if (method.isStatic()) {
      result.staticMethods.set(name, location);
    } else {
      result.methods.set(name, location);
    }
  }

  // Extract getters
  for (const getter of cls.getGetAccessors()) {
    result.getters.set(getter.getName(), {
      line: getter.getStartLineNumber(),
      endLine: getter.getEndLineNumber(),
    });
  }

  // Extract setters
  for (const setter of cls.getSetAccessors()) {
    result.setters.set(setter.getName(), {
      line: setter.getStartLineNumber(),
      endLine: setter.getEndLineNumber(),
    });
  }

  return result;
}

export async function parseTsTestFile(filePath: string): Promise<TsTestParseResult> {
  const project = new Project({ useInMemoryFileSystem: false });
  const sourceFile = project.addSourceFileAtPath(filePath);

  const result: TsTestParseResult = {
    describes: new Map(),
    itBlocks: [],
  };

  // Find describe() and it() calls
  sourceFile.forEachDescendant((node) => {
    if (node.getKind() === SyntaxKind.CallExpression) {
      const callExpr = node.asKind(SyntaxKind.CallExpression);
      if (!callExpr) return;

      const expr = callExpr.getExpression();
      const exprText = expr.getText();

      if (exprText === 'describe' || exprText === 'it') {
        const args = callExpr.getArguments();
        if (args.length > 0) {
          const firstArg = args[0];
          // Get the string literal value
          const name = firstArg.getText().replace(/^['"]|['"]$/g, '');
          const location: SymbolLocation = {
            line: callExpr.getStartLineNumber(),
            endLine: callExpr.getEndLineNumber(),
          };

          if (exprText === 'describe') {
            result.describes.set(name, location);
          } else {
            result.itBlocks.push({ name, location });
          }
        }
      }
    }
  });

  return result;
}
```

**Step 4: Run test to verify it passes**

```bash
npx vitest run scripts/parsers/ts-ast-parser.test.ts
```

Expected: PASS (5 passed)

**Step 5: Commit**

```bash
git add scripts/parsers/ts-ast-parser.ts scripts/parsers/ts-ast-parser.test.ts
git commit -m "feat(tasks): add TypeScript AST parser using ts-morph"
```

---

### Task 5: Implement C++ Symbol Parser

**Files:**

- Create: `scripts/parsers/cpp-symbol-parser.ts`
- Test: `scripts/parsers/cpp-symbol-parser.test.ts`

**Step 1: Write the failing test**

```typescript
// scripts/parsers/cpp-symbol-parser.test.ts
import { describe, it, expect } from 'vitest';
import { parseCppHeader, parseCppImpl } from './cpp-symbol-parser.js';
import * as path from 'node:path';

describe('parseCppHeader', () => {
  it('should extract class name', async () => {
    const filePath = path.join(process.cwd(), 'src/VideoDecoder.h');
    const result = await parseCppHeader(filePath);

    expect(result.className).toBe('VideoDecoder');
  });

  it('should extract method declarations', async () => {
    const filePath = path.join(process.cwd(), 'src/VideoDecoder.h');
    const result = await parseCppHeader(filePath);

    expect(result.methods.get('Configure')).toBeDefined();
    expect(result.methods.get('GetState')).toBeDefined();
  });

  it('should identify static methods', async () => {
    const filePath = path.join(process.cwd(), 'src/VideoDecoder.h');
    const result = await parseCppHeader(filePath);

    expect(result.staticMethods.get('IsConfigSupported')).toBeDefined();
  });
});

describe('parseCppImpl', () => {
  it('should extract method implementations with line ranges', async () => {
    const filePath = path.join(process.cwd(), 'src/VideoDecoder.cpp');
    const result = await parseCppImpl(filePath, 'VideoDecoder');

    expect(result.methods.get('GetState')).toBeDefined();
    expect(result.methods.get('GetState')?.line).toBeGreaterThan(0);
  });

  it('should extract constructor implementation', async () => {
    const filePath = path.join(process.cwd(), 'src/VideoDecoder.cpp');
    const result = await parseCppImpl(filePath, 'VideoDecoder');

    expect(result.constructor).toBeDefined();
    expect(result.constructor?.line).toBeGreaterThan(0);
  });
});
```

**Step 2: Run test to verify it fails**

```bash
npx vitest run scripts/parsers/cpp-symbol-parser.test.ts
```

Expected: FAIL (module not found)

**Step 3: Write minimal implementation**

```typescript
// scripts/parsers/cpp-symbol-parser.ts
/**
 * C++ symbol parser using regex patterns.
 * Simpler than tree-sitter for this use case - we just need line numbers.
 */
import * as fs from 'node:fs/promises';

export interface SymbolLocation {
  line: number;
  endLine?: number;
}

export interface CppHeaderParseResult {
  className: string;
  methods: Map<string, SymbolLocation>;
  staticMethods: Map<string, SymbolLocation>;
}

export interface CppImplParseResult {
  constructor?: SymbolLocation;
  methods: Map<string, SymbolLocation>;
}

export async function parseCppHeader(filePath: string): Promise<CppHeaderParseResult> {
  const content = await fs.readFile(filePath, 'utf-8');
  const lines = content.split('\n');

  const result: CppHeaderParseResult = {
    className: '',
    methods: new Map(),
    staticMethods: new Map(),
  };

  // Extract class name: "class VideoDecoder : public ..."
  const classMatch = content.match(/class\s+(\w+)\s*:/);
  if (classMatch) {
    result.className = classMatch[1];
  }

  // Find method declarations
  for (let i = 0; i < lines.length; i++) {
    const line = lines[i];
    const lineNum = i + 1;

    // Match: "static Napi::Value IsConfigSupported(..."
    const staticMatch = line.match(/^\s*static\s+\S+\s+(\w+)\s*\(/);
    if (staticMatch) {
      result.staticMethods.set(staticMatch[1], { line: lineNum });
      continue;
    }

    // Match: "Napi::Value GetState(..." or "void Configure(..."
    const methodMatch = line.match(/^\s*(?:Napi::Value|void|bool)\s+(\w+)\s*\(/);
    if (methodMatch) {
      result.methods.set(methodMatch[1], { line: lineNum });
    }
  }

  return result;
}

export async function parseCppImpl(
  filePath: string,
  className: string
): Promise<CppImplParseResult> {
  const content = await fs.readFile(filePath, 'utf-8');
  const lines = content.split('\n');

  const result: CppImplParseResult = {
    methods: new Map(),
  };

  let braceCount = 0;
  let currentMethod: string | null = null;
  let methodStartLine = 0;

  for (let i = 0; i < lines.length; i++) {
    const line = lines[i];
    const lineNum = i + 1;

    // Match constructor: "VideoDecoder::VideoDecoder(..."
    const ctorMatch = line.match(new RegExp(`${className}::${className}\\s*\\(`));
    if (ctorMatch && !result.constructor) {
      result.constructor = { line: lineNum };
      currentMethod = 'constructor';
      methodStartLine = lineNum;
      braceCount = 0;
    }

    // Match method: "ClassName::MethodName(..."
    const methodMatch = line.match(new RegExp(`${className}::(\\w+)\\s*\\(`));
    if (methodMatch && methodMatch[1] !== className) {
      const methodName = methodMatch[1];
      currentMethod = methodName;
      methodStartLine = lineNum;
      braceCount = 0;
      result.methods.set(methodName, { line: lineNum });
    }

    // Count braces to find method end
    if (currentMethod) {
      braceCount += (line.match(/\{/g) || []).length;
      braceCount -= (line.match(/\}/g) || []).length;

      if (braceCount === 0 && line.includes('}')) {
        const location =
          currentMethod === 'constructor' ? result.constructor : result.methods.get(currentMethod);
        if (location) {
          location.endLine = lineNum;
        }
        currentMethod = null;
      }
    }
  }

  return result;
}
```

**Step 4: Run test to verify it passes**

```bash
npx vitest run scripts/parsers/cpp-symbol-parser.test.ts
```

Expected: PASS (5 passed)

**Step 5: Commit**

```bash
git add scripts/parsers/cpp-symbol-parser.ts scripts/parsers/cpp-symbol-parser.test.ts
git commit -m "feat(tasks): add C++ symbol parser using regex"
```

---

## Task Group 3: Symbol Matching and Validation

### Task 6: Implement Symbol Matcher

**Files:**

- Create: `scripts/matchers/symbol-matcher.ts`
- Test: `scripts/matchers/symbol-matcher.test.ts`

**Step 1: Write the failing test**

```typescript
// scripts/matchers/symbol-matcher.test.ts
import { describe, it, expect } from 'vitest';
import { matchIdlToCode, MissingSymbolError } from './symbol-matcher.js';
import type { TsClassParseResult } from '../parsers/ts-ast-parser.js';
import type { CppHeaderParseResult, CppImplParseResult } from '../parsers/cpp-symbol-parser.js';

describe('matchIdlToCode', () => {
  it('should match IDL attribute to C++ getter', () => {
    const idlMember = { type: 'attribute', name: 'state', readonly: true };
    const cppHeader: CppHeaderParseResult = {
      className: 'VideoDecoder',
      methods: new Map([['GetState', { line: 50 }]]),
      staticMethods: new Map(),
    };
    const cppImpl: CppImplParseResult = {
      methods: new Map([['GetState', { line: 92, endLine: 94 }]]),
    };
    const tsClass: TsClassParseResult = {
      className: 'VideoDecoder',
      methods: new Map(),
      getters: new Map([['state', { line: 47, endLine: 49 }]]),
      setters: new Map(),
      staticMethods: new Map(),
    };

    const result = matchIdlToCode('VideoDecoder', idlMember, cppHeader, cppImpl, tsClass);

    expect(result.codeLinks.declaration?.line).toBe(50);
    expect(result.codeLinks.implementation?.line).toBe(92);
    expect(result.codeLinks.tsBinding?.line).toBe(47);
  });

  it('should throw MissingSymbolError when C++ getter not found', () => {
    const idlMember = { type: 'attribute', name: 'missingAttr', readonly: true };
    const cppHeader: CppHeaderParseResult = {
      className: 'VideoDecoder',
      methods: new Map(),
      staticMethods: new Map(),
    };
    const cppImpl: CppImplParseResult = { methods: new Map() };
    const tsClass: TsClassParseResult = {
      className: 'VideoDecoder',
      methods: new Map(),
      getters: new Map(),
      setters: new Map(),
      staticMethods: new Map(),
    };

    expect(() => matchIdlToCode('VideoDecoder', idlMember, cppHeader, cppImpl, tsClass)).toThrow(
      MissingSymbolError
    );
  });
});
```

**Step 2: Run test to verify it fails**

```bash
npx vitest run scripts/matchers/symbol-matcher.test.ts
```

Expected: FAIL (module not found)

**Step 3: Write minimal implementation**

```typescript
// scripts/matchers/symbol-matcher.ts
/**
 * Maps IDL members to code locations across C++ and TypeScript files.
 */
import type { CodeLink } from '../types/task-schema.js';
import type { TsClassParseResult } from '../parsers/ts-ast-parser.js';
import type { CppHeaderParseResult, CppImplParseResult } from '../parsers/cpp-symbol-parser.js';

export class MissingSymbolError extends Error {
  constructor(
    public interfaceName: string,
    public memberName: string,
    public memberType: string,
    public expectedIn: string,
    public expectedSymbol: string
  ) {
    super(
      `Missing symbol: ${interfaceName}.${memberName} (${memberType}) - expected ${expectedSymbol} in ${expectedIn}`
    );
    this.name = 'MissingSymbolError';
  }
}

export interface IdlMember {
  type: 'attribute' | 'operation' | 'constructor';
  name: string;
  readonly?: boolean;
  special?: string; // 'static'
}

export interface CodeLinks {
  declaration?: CodeLink;
  implementation?: CodeLink;
  tsBinding?: CodeLink;
  test?: CodeLink;
}

export interface MatchResult {
  codeLinks: CodeLinks;
}

function capitalize(str: string): string {
  return str.charAt(0).toUpperCase() + str.slice(1);
}

export function matchIdlToCode(
  interfaceName: string,
  idlMember: IdlMember,
  cppHeader: CppHeaderParseResult,
  cppImpl: CppImplParseResult,
  tsClass: TsClassParseResult,
  files?: { cppHeader: string; cppImpl: string; tsWrapper: string }
): MatchResult {
  const codeLinks: CodeLinks = {};
  const filePrefix = files || {
    cppHeader: `src/${interfaceName}.h`,
    cppImpl: `src/${interfaceName}.cpp`,
    tsWrapper: `lib/${interfaceName}.ts`,
  };

  if (idlMember.type === 'attribute') {
    // Attributes map to GetXxx() in C++ and get xxx() in TS
    const cppGetterName = `Get${capitalize(idlMember.name)}`;

    // Check C++ header
    const headerLoc = cppHeader.methods.get(cppGetterName);
    if (!headerLoc) {
      throw new MissingSymbolError(
        interfaceName,
        idlMember.name,
        'attribute',
        filePrefix.cppHeader,
        cppGetterName
      );
    }
    codeLinks.declaration = { file: filePrefix.cppHeader, line: headerLoc.line };

    // Check C++ impl
    const implLoc = cppImpl.methods.get(cppGetterName);
    if (!implLoc) {
      throw new MissingSymbolError(
        interfaceName,
        idlMember.name,
        'attribute',
        filePrefix.cppImpl,
        `${interfaceName}::${cppGetterName}`
      );
    }
    codeLinks.implementation = {
      file: filePrefix.cppImpl,
      line: implLoc.line,
      endLine: implLoc.endLine,
    };

    // Check TS getter
    const tsLoc = tsClass.getters.get(idlMember.name);
    if (!tsLoc) {
      throw new MissingSymbolError(
        interfaceName,
        idlMember.name,
        'attribute',
        filePrefix.tsWrapper,
        `get ${idlMember.name}()`
      );
    }
    codeLinks.tsBinding = {
      file: filePrefix.tsWrapper,
      line: tsLoc.line,
      endLine: tsLoc.endLine,
    };
  } else if (idlMember.type === 'operation') {
    const cppMethodName = capitalize(idlMember.name);
    const isStatic = idlMember.special === 'static';

    // Check C++ header
    const methodsMap = isStatic ? cppHeader.staticMethods : cppHeader.methods;
    const headerLoc = methodsMap.get(cppMethodName);
    if (!headerLoc) {
      throw new MissingSymbolError(
        interfaceName,
        idlMember.name,
        isStatic ? 'static-method' : 'method',
        filePrefix.cppHeader,
        cppMethodName
      );
    }
    codeLinks.declaration = { file: filePrefix.cppHeader, line: headerLoc.line };

    // Check C++ impl
    const implLoc = cppImpl.methods.get(cppMethodName);
    if (!implLoc) {
      throw new MissingSymbolError(
        interfaceName,
        idlMember.name,
        isStatic ? 'static-method' : 'method',
        filePrefix.cppImpl,
        `${interfaceName}::${cppMethodName}`
      );
    }
    codeLinks.implementation = {
      file: filePrefix.cppImpl,
      line: implLoc.line,
      endLine: implLoc.endLine,
    };

    // Check TS
    const tsMap = isStatic ? tsClass.staticMethods : tsClass.methods;
    const tsLoc = tsMap.get(idlMember.name);
    if (!tsLoc) {
      throw new MissingSymbolError(
        interfaceName,
        idlMember.name,
        isStatic ? 'static-method' : 'method',
        filePrefix.tsWrapper,
        `${isStatic ? 'static ' : ''}${idlMember.name}()`
      );
    }
    codeLinks.tsBinding = {
      file: filePrefix.tsWrapper,
      line: tsLoc.line,
      endLine: tsLoc.endLine,
    };
  } else if (idlMember.type === 'constructor') {
    // Constructor
    codeLinks.declaration = {
      file: filePrefix.cppHeader,
      line: cppHeader.methods.get(interfaceName)?.line || 1,
    };

    if (cppImpl.constructor) {
      codeLinks.implementation = {
        file: filePrefix.cppImpl,
        line: cppImpl.constructor.line,
        endLine: cppImpl.constructor.endLine,
      };
    }

    if (tsClass.constructor) {
      codeLinks.tsBinding = {
        file: filePrefix.tsWrapper,
        line: tsClass.constructor.line,
        endLine: tsClass.constructor.endLine,
      };
    }
  }

  return { codeLinks };
}
```

**Step 4: Run test to verify it passes**

```bash
npx vitest run scripts/matchers/symbol-matcher.test.ts
```

Expected: PASS (2 passed)

**Step 5: Commit**

```bash
git add scripts/matchers/symbol-matcher.ts scripts/matchers/symbol-matcher.test.ts
git commit -m "feat(tasks): add symbol matcher for IDL-to-code mapping"
```

---

## Task Group 4: Main Generator

### Task 7: Rewrite generate-tasks.ts Main Entry

**Files:**

- Modify: `scripts/generate-tasks.ts`

**Step 1: Write integration test**

```typescript
// scripts/generate-tasks.test.ts
import { describe, it, expect, beforeAll, afterAll } from 'vitest';
import * as fs from 'node:fs/promises';
import * as path from 'node:path';

describe('generate-tasks', () => {
  const outputDir = path.join(process.cwd(), 'docs/tasks');

  beforeAll(async () => {
    // Run the generator
    const { main } = await import('./generate-tasks.js');
    await main();
  });

  it('should generate VideoDecoder.json', async () => {
    const content = await fs.readFile(path.join(outputDir, 'VideoDecoder.json'), 'utf-8');
    const data = JSON.parse(content);

    expect(data.interface).toBe('VideoDecoder');
    expect(data.type).toBe('interface');
    expect(data.features.length).toBeGreaterThan(0);
  });

  it('should include code links in features', async () => {
    const content = await fs.readFile(path.join(outputDir, 'VideoDecoder.json'), 'utf-8');
    const data = JSON.parse(content);

    const configureFeature = data.features.find((f: any) => f.id === 'VideoDecoder.configure');
    expect(configureFeature).toBeDefined();
    expect(configureFeature.codeLinks.declaration.file).toContain('VideoDecoder.h');
    expect(configureFeature.codeLinks.implementation.line).toBeGreaterThan(0);
  });

  it('should generate types.json with dictionaries and enums', async () => {
    const content = await fs.readFile(path.join(outputDir, 'types.json'), 'utf-8');
    const data = JSON.parse(content);

    expect(data.dictionaries.length).toBeGreaterThan(0);
    expect(data.enums.length).toBeGreaterThan(0);
    expect(data.enums.some((e: any) => e.name === 'CodecState')).toBe(true);
  });

  it('should set all passes to false', async () => {
    const content = await fs.readFile(path.join(outputDir, 'VideoDecoder.json'), 'utf-8');
    const data = JSON.parse(content);

    for (const feature of data.features) {
      expect(feature.passes).toBe(false);
      for (const step of feature.steps) {
        expect(step.passes).toBe(false);
      }
    }
  });
});
```

**Step 2: Run test to verify it fails**

```bash
npx vitest run scripts/generate-tasks.test.ts
```

Expected: FAIL (current implementation generates markdown, not JSON)

**Step 3: Rewrite generate-tasks.ts**

```typescript
// scripts/generate-tasks.ts
/**
 * Task Generator - Parses WebIDL and generates JSON task files with code links.
 *
 * Usage: npx tsx scripts/generate-tasks.ts
 *
 * Reads:
 *   - spec/context/_webcodecs.idl
 *   - spec/context/<Interface>.md
 *   - src/<Interface>.h, src/<Interface>.cpp
 *   - lib/<Interface>.ts
 *   - test/<interface>.test.ts
 *
 * Outputs:
 *   - docs/tasks/<Interface>.json
 *   - docs/tasks/types.json
 */

import * as fs from 'node:fs/promises';
import * as path from 'node:path';
import { fileURLToPath } from 'node:url';
import * as webidl2 from 'webidl2';

import type {
  TaskFile,
  TypesFile,
  Feature,
  Step,
  DictionaryDef,
  EnumDef,
} from './types/task-schema.js';
import { parseSpecFile } from './parsers/spec-markdown-parser.js';
import { parseTsFile, parseTsTestFile } from './parsers/ts-ast-parser.js';
import { parseCppHeader, parseCppImpl } from './parsers/cpp-symbol-parser.js';
import { matchIdlToCode, MissingSymbolError } from './matchers/symbol-matcher.js';

const currentDir = path.dirname(fileURLToPath(import.meta.url));
const ROOT_DIR = path.join(currentDir, '..');
const IDL_PATH = path.join(ROOT_DIR, 'spec/context/_webcodecs.idl');
const SPEC_DIR = path.join(ROOT_DIR, 'spec/context');
const SRC_DIR = path.join(ROOT_DIR, 'src');
const LIB_DIR = path.join(ROOT_DIR, 'lib');
const TEST_DIR = path.join(ROOT_DIR, 'test');
const OUTPUT_DIR = path.join(ROOT_DIR, 'docs/tasks');

function capitalize(str: string): string {
  return str.charAt(0).toUpperCase() + str.slice(1);
}

function parseIdlType(idlType: webidl2.IDLTypeDescription | null): string {
  if (!idlType) return 'void';
  if (typeof idlType.idlType === 'string') {
    return idlType.idlType;
  }
  if (Array.isArray(idlType.idlType)) {
    return idlType.idlType.map((t) => parseIdlType(t as webidl2.IDLTypeDescription)).join(' | ');
  }
  return 'unknown';
}

function getExtendedAttributes(node: webidl2.InterfaceType): string[] {
  return (
    node.extAttrs?.map((attr) => {
      if (attr.rhs) {
        return `${attr.name}=${attr.rhs.value}`;
      }
      return attr.name;
    }) || []
  );
}

function getIdlLineRange(content: string, nodeName: string): string {
  const lines = content.split('\n');
  for (let i = 0; i < lines.length; i++) {
    if (lines[i].includes(`interface ${nodeName}`)) {
      // Find end of interface
      let braceCount = 0;
      let started = false;
      for (let j = i; j < lines.length; j++) {
        if (lines[j].includes('{')) {
          braceCount++;
          started = true;
        }
        if (lines[j].includes('}')) braceCount--;
        if (started && braceCount === 0) {
          return `spec/context/_webcodecs.idl#L${i + 1}-L${j + 1}`;
        }
      }
    }
  }
  return 'spec/context/_webcodecs.idl';
}

async function fileExists(filePath: string): Promise<boolean> {
  try {
    await fs.access(filePath);
    return true;
  } catch {
    return false;
  }
}

async function generateInterfaceTask(
  node: webidl2.InterfaceType,
  idlContent: string
): Promise<TaskFile> {
  const interfaceName = node.name;
  const files = {
    cppHeader: `src/${interfaceName}.h`,
    cppImpl: `src/${interfaceName}.cpp`,
    tsWrapper: `lib/${interfaceName}.ts`,
    tests: `test/${interfaceName.toLowerCase()}.test.ts`,
  };

  // Check if files exist
  const cppHeaderPath = path.join(SRC_DIR, `${interfaceName}.h`);
  const cppImplPath = path.join(SRC_DIR, `${interfaceName}.cpp`);
  const tsWrapperPath = path.join(LIB_DIR, `${interfaceName}.ts`);
  const specPath = path.join(SPEC_DIR, `${interfaceName}.md`);

  // Parse source files
  let cppHeader, cppImpl, tsClass;
  const errors: MissingSymbolError[] = [];

  try {
    if (await fileExists(cppHeaderPath)) {
      cppHeader = await parseCppHeader(cppHeaderPath);
    }
    if (await fileExists(cppImplPath)) {
      cppImpl = await parseCppImpl(cppImplPath, interfaceName);
    }
    if (await fileExists(tsWrapperPath)) {
      tsClass = await parseTsFile(tsWrapperPath);
    }
  } catch (e) {
    console.warn(`Warning: Could not parse files for ${interfaceName}:`, e);
  }

  // Parse spec markdown for algorithms
  let specData;
  if (await fileExists(specPath)) {
    specData = await parseSpecFile(specPath);
  }

  const features: Feature[] = [];

  for (const member of node.members) {
    let feature: Feature | null = null;

    if (member.type === 'attribute') {
      const attrName = member.name;
      const specAttr = specData?.attributes.find((a) => a.name === attrName);

      feature = {
        id: `${interfaceName}.${attrName}`,
        category: 'attribute',
        name: attrName,
        description: `${member.readonly ? 'Readonly' : 'Read/write'} attribute`,
        returnType: parseIdlType(member.idlType),
        readonly: member.readonly,
        codeLinks: {},
        algorithmRef: `spec/context/${interfaceName}.md#attributes`,
        algorithmSteps: [],
        steps: [
          {
            id: `${interfaceName}.${attrName}.cpp-getter`,
            description: `C++: Implement Get${capitalize(attrName)}() returning ${parseIdlType(member.idlType)}`,
            passes: false,
          },
          ...(member.readonly
            ? []
            : [
                {
                  id: `${interfaceName}.${attrName}.cpp-setter`,
                  description: `C++: Implement Set${capitalize(attrName)}()`,
                  passes: false,
                },
              ]),
          {
            id: `${interfaceName}.${attrName}.ts-binding`,
            description: `TS: Wire getter${member.readonly ? '' : ' and setter'} to native`,
            passes: false,
          },
          {
            id: `${interfaceName}.${attrName}.test`,
            description: 'Test: Verify initial value and behavior',
            passes: false,
          },
        ],
        passes: false,
      };

      // Try to match code links
      if (cppHeader && cppImpl && tsClass) {
        try {
          const match = matchIdlToCode(
            interfaceName,
            { type: 'attribute', name: attrName, readonly: member.readonly },
            cppHeader,
            cppImpl,
            tsClass,
            files
          );
          feature.codeLinks = match.codeLinks;
        } catch (e) {
          if (e instanceof MissingSymbolError) {
            errors.push(e);
          }
        }
      }
    } else if (member.type === 'operation' && member.name) {
      const methodName = member.name;
      const isStatic = member.special === 'static';
      const specMethod = specData?.methods.get(methodName);
      const params = member.arguments.map((arg) => `${arg.name}: ${parseIdlType(arg.idlType)}`);

      feature = {
        id: `${interfaceName}.${methodName}`,
        category: isStatic ? 'static-method' : 'method',
        name: `${methodName}(${params.join(', ')})`,
        description:
          specMethod?.algorithmSteps[0] || `${isStatic ? 'Static method' : 'Method'} ${methodName}`,
        returnType: parseIdlType(member.idlType),
        codeLinks: {},
        algorithmRef: `spec/context/${interfaceName}.md#${methodName.toLowerCase()}`,
        algorithmSteps: specMethod?.algorithmSteps || [],
        steps: [
          {
            id: `${interfaceName}.${methodName}.cpp-impl`,
            description: 'C++: Implement method logic per W3C spec algorithm',
            passes: false,
          },
          ...(parseIdlType(member.idlType).includes('Promise')
            ? [
                {
                  id: `${interfaceName}.${methodName}.cpp-async`,
                  description: 'C++: Use AsyncWorker for non-blocking execution',
                  passes: false,
                },
              ]
            : []),
          {
            id: `${interfaceName}.${methodName}.cpp-errors`,
            description: 'C++: Handle error cases with proper DOMException types',
            passes: false,
          },
          {
            id: `${interfaceName}.${methodName}.ts-binding`,
            description: 'TS: Wire method to native implementation',
            passes: false,
          },
          {
            id: `${interfaceName}.${methodName}.test-happy`,
            description: 'Test: Write test case for happy path',
            passes: false,
          },
          {
            id: `${interfaceName}.${methodName}.test-errors`,
            description: 'Test: Write test case for error conditions',
            passes: false,
          },
        ],
        passes: false,
      };

      // Try to match code links
      if (cppHeader && cppImpl && tsClass) {
        try {
          const match = matchIdlToCode(
            interfaceName,
            { type: 'operation', name: methodName, special: member.special },
            cppHeader,
            cppImpl,
            tsClass,
            files
          );
          feature.codeLinks = match.codeLinks;
        } catch (e) {
          if (e instanceof MissingSymbolError) {
            errors.push(e);
          }
        }
      }
    } else if (member.type === 'constructor') {
      const params = member.arguments.map((arg) => `${arg.name}: ${parseIdlType(arg.idlType)}`);

      feature = {
        id: `${interfaceName}.constructor`,
        category: 'constructor',
        name: `constructor(${params.join(', ')})`,
        description: `Create ${interfaceName} instance`,
        codeLinks: {},
        algorithmRef: `spec/context/${interfaceName}.md#constructor`,
        algorithmSteps: specData?.methods.get('constructor')?.algorithmSteps || [
          'Initialize internal slots',
        ],
        steps: [
          {
            id: `${interfaceName}.constructor.cpp-impl`,
            description: 'C++: Implement constructor with parameter validation',
            passes: false,
          },
          {
            id: `${interfaceName}.constructor.cpp-init`,
            description: 'C++: Initialize internal state slots',
            passes: false,
          },
          {
            id: `${interfaceName}.constructor.ts-binding`,
            description: 'TS: Wire TypeScript wrapper to native constructor',
            passes: false,
          },
          {
            id: `${interfaceName}.constructor.test`,
            description: 'Test: Verify constructor creates valid instance',
            passes: false,
          },
        ],
        passes: false,
      };

      // Try to match code links
      if (cppHeader && cppImpl && tsClass) {
        try {
          const match = matchIdlToCode(
            interfaceName,
            { type: 'constructor', name: 'constructor' },
            cppHeader,
            cppImpl,
            tsClass,
            files
          );
          feature.codeLinks = match.codeLinks;
        } catch (e) {
          if (e instanceof MissingSymbolError) {
            errors.push(e);
          }
        }
      }
    }

    if (feature) {
      features.push(feature);
    }
  }

  // Report errors but don't fail (for now - change to throw for strict mode)
  if (errors.length > 0) {
    console.warn(`\nWarning: Missing symbols for ${interfaceName}:`);
    for (const err of errors) {
      console.warn(
        `  - ${err.memberName} (${err.memberType}): expected ${err.expectedSymbol} in ${err.expectedIn}`
      );
    }
  }

  const inheritance = node.inheritance?.name;

  return {
    interface: interfaceName,
    type: 'interface',
    source: {
      idl: getIdlLineRange(idlContent, interfaceName),
      spec: (await fileExists(specPath)) ? `spec/context/${interfaceName}.md` : undefined,
    },
    inheritance,
    extendedAttributes: getExtendedAttributes(node),
    files,
    features,
  };
}

function generateTypesFile(ast: webidl2.IDLRootType[], idlContent: string): TypesFile {
  const dictionaries: DictionaryDef[] = [];
  const enums: EnumDef[] = [];

  for (const node of ast) {
    if (node.type === 'dictionary') {
      dictionaries.push({
        name: node.name,
        source: { idl: `spec/context/_webcodecs.idl` },
        fields: node.members.map((m) => ({
          name: m.name,
          type: parseIdlType(m.idlType),
          required: m.required,
          defaultValue: m.default?.value?.toString(),
        })),
      });
    } else if (node.type === 'enum') {
      enums.push({
        name: node.name,
        source: { idl: `spec/context/_webcodecs.idl` },
        values: node.values.map((v) => v.value),
      });
    }
  }

  return { dictionaries, enums };
}

export async function main(): Promise<void> {
  // Ensure output directory exists
  await fs.mkdir(OUTPUT_DIR, { recursive: true });

  // Read and parse IDL
  const idlContent = await fs.readFile(IDL_PATH, 'utf-8');
  const ast = webidl2.parse(idlContent);

  let interfaceCount = 0;
  let featureCount = 0;

  // Generate interface task files
  for (const node of ast) {
    if (node.type === 'interface') {
      const taskFile = await generateInterfaceTask(node, idlContent);
      const outputPath = path.join(OUTPUT_DIR, `${node.name}.json`);
      await fs.writeFile(outputPath, JSON.stringify(taskFile, null, 2));
      console.log(`Generated: ${outputPath} (${taskFile.features.length} features)`);
      interfaceCount++;
      featureCount += taskFile.features.length;
    }
  }

  // Generate types.json
  const typesFile = generateTypesFile(ast, idlContent);
  const typesPath = path.join(OUTPUT_DIR, 'types.json');
  await fs.writeFile(typesPath, JSON.stringify(typesFile, null, 2));
  console.log(
    `Generated: ${typesPath} (${typesFile.dictionaries.length} dictionaries, ${typesFile.enums.length} enums)`
  );

  console.log(
    `\nDone! Generated ${interfaceCount} interface files with ${featureCount} total features.`
  );
}

// Run if executed directly
if (import.meta.url === `file://${process.argv[1]}`) {
  main().catch((err) => {
    console.error(err);
    process.exit(1);
  });
}
```

**Step 4: Run test to verify it passes**

```bash
npx vitest run scripts/generate-tasks.test.ts
```

Expected: PASS (4 passed)

**Step 5: Commit**

```bash
git add scripts/generate-tasks.ts scripts/generate-tasks.test.ts
git commit -m "feat(tasks): rewrite generator to produce JSON with code links"
```

---

## Task Group 5: Cleanup and Documentation

### Task 8: Update package.json Scripts

**Files:**

- Modify: `package.json`

**Step 1: Update scripts**

Add to package.json scripts:

```json
{
  "scripts": {
    "generate:tasks": "tsx scripts/generate-tasks.ts",
    "tasks": "npm run generate:tasks",
    "pipeline": "npm run scaffold && npm run generate:types && npm run generate:tasks"
  }
}
```

**Step 2: Run the pipeline**

```bash
npm run tasks
```

Expected: Generates JSON files in docs/tasks/

**Step 3: Verify output**

```bash
ls -la docs/tasks/*.json
cat docs/tasks/VideoDecoder.json | head -50
```

Expected: JSON files with code links

**Step 4: Commit**

```bash
git add package.json docs/tasks/
git commit -m "feat(tasks): update scripts and generate initial task JSON files"
```

---

### Task 9: Delete Old Markdown Task Files

**Files:**

- Delete: `docs/tasks/*.md`

**Step 1: Remove old markdown files**

```bash
rm docs/tasks/*.md
```

**Step 2: Verify only JSON remains**

```bash
ls docs/tasks/
```

Expected: Only .json files

**Step 3: Commit**

```bash
git add -A
git commit -m "chore(tasks): remove deprecated markdown task files"
```

---

### Task 10: Code Review

**Files:**

- All modified files

**Step 1: Run all tests**

```bash
npm test
```

Expected: All tests pass

**Step 2: Run linting**

```bash
npm run format:check
```

**Step 3: Review generated output**

```bash
cat docs/tasks/VideoDecoder.json | jq '.features[0]'
```

Verify:

- `codeLinks` has `declaration`, `implementation`, `tsBinding`
- `line` numbers are positive integers
- `passes` is `false`

---

## Parallel Group Summary

| Task Group | Tasks    | Can Run Parallel          |
| ---------- | -------- | ------------------------- |
| Group 1    | 1, 2     | No (sequential setup)     |
| Group 2    | 3, 4, 5  | Yes (independent parsers) |
| Group 3    | 6        | No (depends on parsers)   |
| Group 4    | 7        | No (depends on matcher)   |
| Group 5    | 8, 9, 10 | No (sequential cleanup)   |
