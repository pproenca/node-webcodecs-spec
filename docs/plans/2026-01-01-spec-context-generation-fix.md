# Spec Context Generation Fix Implementation Plan

> **Execution:** Use `/dev-workflow:execute-plan docs/plans/2026-01-01-spec-context-generation-fix.md` to implement task-by-task.

**Goal:** Fix the spec context markdown generation to produce properly formatted reference documents by using Bikeshed API to pre-render the W3C spec before parsing.

**Architecture:** Pragmatic approach - modify scaffold-project.ts to:
1. Fetch raw Bikeshed source from W3C repo
2. Submit to Bikeshed API (https://api.csswg.org/bikeshed/) to get rendered HTML
3. Parse rendered HTML (cross-references already resolved)
4. Generate structured Markdown with TOC, numbered algorithm steps, and clean sections

**Tech Stack:** TypeScript, JSDOM, Turndown, fetch API, Bikeshed API

---

## Current Problems

1. **Unprocessed Bikeshed markup**: `\[=term=\]`, `{{Type}}`, `|variable|` appear raw in output
2. **Algorithm steps on single lines**: Numbered steps (1. 2. 3.) not formatted as lists
3. **Empty sections**: Some interfaces have "Methods & Algorithms" with no content
4. **No Table of Contents**: Missing navigation structure
5. **Placeholder text**: "See spec/context file." instead of actual algorithm steps

## Solution Overview

The Bikeshed API at `https://api.csswg.org/bikeshed/` accepts raw `.src.html` content and returns fully rendered HTML with:
- Cross-references resolved to actual text/links
- Proper HTML structure for algorithm steps
- All Bikeshed markup processed

We'll use this rendered HTML instead of the raw source for parsing.

---

### Task 1: Add Bikeshed API Build Function

**Files:**
- Modify: `scripts/scaffold-project.ts:130-145`

**Step 1: Write failing test for Bikeshed API function** (2-5 min)

Create a simple test that verifies the function signature exists:

```typescript
// In a new test file or inline verification
// For now, we'll verify via TypeScript compilation
```

Since this is a script file without formal tests, we'll use TypeScript type checking as our verification.

**Step 2: Add the buildWithBikeshed function** (5 min)

Add this function after the `TYPE_MAP` constant (around line 106):

```typescript
/**
 * Builds Bikeshed source using the CSSWG API.
 * @param source - Raw Bikeshed source HTML content
 * @returns Rendered HTML with resolved cross-references
 * @throws Error if API request fails
 */
async function buildWithBikeshed(source: string): Promise<string> {
  const formData = new FormData();
  formData.append('file', new Blob([source], { type: 'text/plain' }), 'spec.bs');
  formData.append('force', '1'); // Force build even with warnings
  formData.append('output', 'html');

  const response = await fetch('https://api.csswg.org/bikeshed/', {
    method: 'POST',
    body: formData,
  });

  if (!response.ok) {
    const errorText = await response.text();
    throw new Error(`Bikeshed API failed: ${response.status} - ${errorText}`);
  }

  return response.text();
}
```

**Step 3: Verify compilation** (30 sec)

```bash
npx tsc --noEmit scripts/scaffold-project.ts
```

Expected: No errors (types are correct)

**Step 4: Commit** (30 sec)

```bash
git add scripts/scaffold-project.ts
git commit -m "feat(scaffold): add buildWithBikeshed API function"
```

---

### Task 2: Integrate Bikeshed Build into Main Flow

**Files:**
- Modify: `scripts/scaffold-project.ts:131-160` (main function)

**Step 1: Update the main function to use Bikeshed API** (5 min)

Replace the direct HTML parsing with Bikeshed-built HTML. Modify the main() function:

```typescript
// In main() function, after fetching raw content:

// 1. Direct Fetch (No Git Clone)
const response = await fetch(SPEC_URL);
if (!response.ok) {
  throw new Error(`Failed to fetch spec: ${response.status} ${response.statusText}`);
}
const rawBikeshedSource = await response.text();
console.log(`[Scaffold] Fetched raw Bikeshed source (${(rawBikeshedSource.length / 1024).toFixed(1)} KB)`);

// 2. Build with Bikeshed API
console.log('[Scaffold] Building spec with Bikeshed API...');
let htmlContent: string;
try {
  htmlContent = await buildWithBikeshed(rawBikeshedSource);
  console.log(`[Scaffold] Built spec (${(htmlContent.length / 1024).toFixed(1)} KB)`);
} catch (err) {
  console.warn('[Scaffold] Bikeshed API failed, falling back to raw source:', err);
  htmlContent = rawBikeshedSource;
}
```

**Step 2: Verify the script runs** (1 min)

```bash
npm run scaffold -- --help 2>&1 | head -5
```

Expected: Script loads without syntax errors

**Step 3: Commit** (30 sec)

```bash
git add scripts/scaffold-project.ts
git commit -m "feat(scaffold): integrate Bikeshed API build step"
```

---

### Task 3: Fix Algorithm Step Parsing

**Files:**
- Modify: `scripts/scaffold-project.ts:1106-1177` (parseBikeshed function)

**Step 1: Update the algorithm extraction to handle rendered HTML** (5 min)

The rendered Bikeshed HTML uses `<ol>` and `<li>` for algorithm steps. Update parseBikeshed:

```typescript
function parseBikeshed(html: string): {
  idlText: string;
  algorithmMap: Map<string, string[]>;
  interfaceDescriptions: Map<string, string>;
} {
  const dom = new JSDOM(html);
  const doc = dom.window.document;
  const turndown = new TurndownService({
    headingStyle: 'atx',
    bulletListMarker: '-',
  });

  // Configure Turndown to preserve list structure
  turndown.addRule('orderedListItem', {
    filter: 'li',
    replacement: (content, node) => {
      const parent = node.parentNode as Element;
      if (parent?.tagName === 'OL') {
        const items = Array.from(parent.children);
        const index = items.indexOf(node as Element) + 1;
        return `${index}. ${content.trim()}\n`;
      }
      return `- ${content.trim()}\n`;
    },
  });

  // ... rest of existing code
```

**Step 2: Update algorithm step extraction** (5 min)

Improve the algorithm extraction to find `<dd>` content with proper structure:

```typescript
  // 2. Extract Algorithms - look for method definitions
  const algorithmMap = new Map<string, string[]>();

  // In rendered Bikeshed, methods are in <dt>/<dd> pairs with data-dfn-for attribute
  const definitions = doc.querySelectorAll('dfn[data-dfn-for]');

  definitions.forEach((dfn) => {
    const forAttr = dfn.getAttribute('data-dfn-for');
    const dfnType = dfn.getAttribute('data-dfn-type');

    if (forAttr && (dfnType === 'method' || dfnType === 'constructor')) {
      const methodName = dfnType === 'constructor' ? 'constructor' : dfn.textContent?.split('(')[0].trim() || '';
      const key = `${forAttr}.${methodName}`;

      // Find the containing <dt> and get the next <dd>
      const dt = dfn.closest('dt');
      const dd = dt?.nextElementSibling;

      if (dd && dd.tagName === 'DD') {
        const md = turndown.turndown(dd.innerHTML);
        const steps = md
          .split('\n')
          .map((s) => s.trim())
          .filter(Boolean);
        algorithmMap.set(key, steps);
      }
    }
  });
```

**Step 3: Test the parsing locally** (1 min)

```bash
npm run scaffold 2>&1 | grep -E "(Processing|Algorithm)"
```

**Step 4: Commit** (30 sec)

```bash
git add scripts/scaffold-project.ts
git commit -m "fix(scaffold): improve algorithm step parsing for rendered Bikeshed HTML"
```

---

### Task 4: Add Table of Contents to Markdown Output

**Files:**
- Modify: `scripts/scaffold-project.ts:365-400` (generateMarkdownContext function)

**Step 1: Update generateMarkdownContext to include TOC** (5 min)

```typescript
function generateMarkdownContext(ctx: InterfaceContext): string {
  // Build TOC
  const tocLines: string[] = ['## Table of Contents', ''];
  tocLines.push('- [Description](#description)');
  if (ctx.attributes.length > 0) {
    tocLines.push('- [Attributes](#attributes)');
  }
  if (ctx.methods.length > 0) {
    tocLines.push('- [Methods](#methods)');
    for (const m of ctx.methods) {
      const anchor = m.name.toLowerCase().replace(/[^a-z0-9]/g, '-');
      tocLines.push(`  - [${m.name}](#${anchor})`);
    }
  }
  tocLines.push('');

  return `# ${ctx.name} Specification

> **Source:** W3C WebCodecs (Auto-generated by scaffold-project.ts)

${tocLines.join('\n')}

## Description

${ctx.desc}

## Attributes

${
  ctx.attributes.length === 0
    ? '_None_'
    : ctx.attributes
        .map((a) => `- **${a.name}** (\`${a.type}\`)${a.readonly ? ' [ReadOnly]' : ''}`)
        .join('\n')
}

## Methods

${ctx.methods
  .map(
    (m) => `
### ${m.name}

${m.isStatic ? '**Static Method**\n\n' : ''}**Signature:** \`${m.signature}\`

**Algorithm:**

${m.steps.map((s, i) => {
  // Check if step already has a number prefix
  if (/^\d+\./.test(s)) {
    return s;
  }
  return `${i + 1}. ${s}`;
}).join('\n')}
`
  )
  .join('\n')}
`;
}
```

**Step 2: Verify output format** (1 min)

```bash
npm run scaffold 2>&1 | tail -20
```

**Step 3: Commit** (30 sec)

```bash
git add scripts/scaffold-project.ts
git commit -m "feat(scaffold): add table of contents to spec context markdown"
```

---

### Task 5: Handle Empty Sections and Fallbacks

**Files:**
- Modify: `scripts/scaffold-project.ts:287-361` (buildInterfaceContext function)

**Step 1: Improve fallback handling for missing algorithm steps** (5 min)

```typescript
function buildInterfaceContext(
  iface: InterfaceType,
  algoMap: Map<string, string[]>,
  descMap: Map<string, string>
): InterfaceContext {
  const methods: MethodContext[] = [];
  const attributes: AttributeContext[] = [];
  let hasConstructor = false;

  for (const member of iface.members) {
    if (member.type === 'operation' && member.name) {
      const op = member as OperationMemberType;
      const key = `${iface.name}.${op.name}`;
      const args = (op.arguments || []).map((arg: Argument) => ({
        name: arg.name,
        type: getCppType(arg.idlType),
        tsType: getTsType(arg.idlType),
      }));
      const returnIdlType = op.idlType;
      const opName = op.name!;

      // Get algorithm steps with better fallback
      let steps = algoMap.get(key);
      if (!steps || steps.length === 0 || steps[0] === 'See spec/context file.') {
        // Try alternate key formats
        steps = algoMap.get(`${iface.name}.${opName}()`) ||
                algoMap.get(opName) ||
                [`Implementation follows W3C WebCodecs ${iface.name}.${opName}() specification.`];
      }

      methods.push({
        name: opName,
        isStatic: op.special === 'static',
        signature: returnIdlType
          ? `${getCppType(returnIdlType)} ${opName}(${args.map((a) => `${a.type} ${a.name}`).join(', ')})`
          : `void ${opName}(${args.map((a) => `${a.type} ${a.name}`).join(', ')})`,
        steps,
        args,
        returnType: returnIdlType ? getCppType(returnIdlType) : 'void',
        tsReturnType: returnIdlType ? getTsType(returnIdlType) : 'void',
      });
    }
    // ... rest of existing code for constructor and attributes
  }

  // Filter out methods section if empty
  return {
    name: iface.name,
    desc: descMap.get(iface.name) || `${iface.name} interface from the W3C WebCodecs specification.`,
    methods,
    attributes,
    hasConstructor,
  };
}
```

**Step 2: Verify improved output** (1 min)

```bash
npm run scaffold && head -50 spec/context/ImageTrack.md
```

**Step 3: Commit** (30 sec)

```bash
git add scripts/scaffold-project.ts
git commit -m "fix(scaffold): improve fallback handling for missing algorithm steps"
```

---

### Task 6: Clean Up Bikeshed Markup Remnants

**Files:**
- Modify: `scripts/scaffold-project.ts` (add cleanBikeshedMarkup helper)

**Step 1: Add cleanup function for any remaining Bikeshed markup** (5 min)

Add after the `buildWithBikeshed` function:

```typescript
/**
 * Cleans any remaining Bikeshed markup that wasn't fully rendered.
 * @param text - Text that may contain Bikeshed markup
 * @returns Cleaned text with markup converted to plain text
 */
function cleanBikeshedMarkup(text: string): string {
  return text
    // [=term=] -> term
    .replace(/\[=([^\]]+)=\]/g, '$1')
    // {{Type}} -> Type
    .replace(/\{\{([^}]+)\}\}/g, '$1')
    // |variable| -> variable
    .replace(/\|([^|]+)\|/g, '$1')
    // [[internal slot]] -> [internal slot]
    .replace(/\[\[([^\]]+)\]\]/g, '[$1]')
    // `"value"` -> "value"
    .replace(/`"([^"]+)"`/g, '"$1"')
    // Clean up escaped brackets
    .replace(/\\?\[=/g, '')
    .replace(/=\\?\]/g, '')
    .replace(/\\\[/g, '[')
    .replace(/\\\]/g, ']');
}
```

**Step 2: Apply cleanup in algorithm step processing** (2 min)

In the `parseBikeshed` function, wrap the markdown conversion:

```typescript
const md = cleanBikeshedMarkup(turndown.turndown(dd.innerHTML));
```

And in `generateMarkdownContext`, clean the steps:

```typescript
${m.steps.map((s, i) => {
  const cleanStep = cleanBikeshedMarkup(s);
  if (/^\d+\./.test(cleanStep)) {
    return cleanStep;
  }
  return `${i + 1}. ${cleanStep}`;
}).join('\n')}
```

**Step 3: Test cleanup** (1 min)

```bash
npm run scaffold && grep -c '\[=' spec/context/*.md || echo "No raw markup found - success!"
```

**Step 4: Commit** (30 sec)

```bash
git add scripts/scaffold-project.ts
git commit -m "fix(scaffold): clean remaining Bikeshed markup from output"
```

---

### Task 7: Run Full Generation and Verify

**Files:**
- Verify: `spec/context/*.md`

**Step 1: Run full scaffold with force regeneration** (2 min)

```bash
# Remove old generated files to force regeneration
rm -f spec/context/*.md

# Run scaffold
npm run scaffold
```

**Step 2: Verify output quality** (2 min)

```bash
# Check for TOC
head -30 spec/context/VideoDecoder.md

# Check algorithm formatting
grep -A 10 "Algorithm:" spec/context/VideoDecoder.md | head -15

# Check no raw markup remains
grep -E '\[=|{{|}}\||\\?\[' spec/context/*.md || echo "Clean output!"
```

**Step 3: Commit all regenerated spec context** (30 sec)

```bash
git add spec/context/
git commit -m "docs: regenerate spec context with improved formatting"
```

---

### Task 8: Code Review

**Files:**
- Review: `scripts/scaffold-project.ts`
- Review: `spec/context/*.md`

Review the implementation for:
1. Error handling for Bikeshed API failures
2. Proper TypeScript types
3. Clean algorithm step formatting
4. TOC links work correctly
5. No remaining Bikeshed markup in output

---

## Parallel Task Groups

| Task Group | Tasks | Rationale |
|------------|-------|-----------|
| Group 1 | 1, 2 | Core Bikeshed API integration, sequential dependency |
| Group 2 | 3, 4, 5, 6 | Independent parsing/formatting improvements |
| Group 3 | 7, 8 | Verification and review, depends on Group 1+2 |

---

## Success Criteria

- [ ] Spec context files have Table of Contents
- [ ] Algorithm steps are formatted as numbered lists
- [ ] No raw Bikeshed markup (`[=`, `{{`, `|var|`) in output
- [ ] Empty sections show meaningful fallback text
- [ ] Script handles Bikeshed API errors gracefully
