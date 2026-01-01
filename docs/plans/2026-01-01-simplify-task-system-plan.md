# Simplify Task System Implementation Plan

> **Execution:** Use `/dev-workflow:execute-plan docs/plans/2026-01-01-simplify-task-system-plan.md` to implement task-by-task.

**Goal:** Replace complex JSON task system with simple GitHub issues derived from WebCodecs spec TOC.

**Architecture:** Delete all task-generation infrastructure (JSON files, markdown context, parsers). Create one script that clones webcodecs repo, renders with bikeshed, extracts H2 sections, and creates GitHub issues via `gh` CLI.

**Tech Stack:** TypeScript, JSDOM for HTML parsing, Turndown for HTML→Markdown, `gh` CLI for issue creation, bikeshed for spec rendering.

---

## Task Group 1: Cleanup (Parallel)

### Task 1: Delete spec context markdown files

**Files:**
- Delete: `spec/context/AudioDecoder.md`
- Delete: `spec/context/VideoDecoder.md`
- Delete: `spec/context/AudioEncoder.md`
- Delete: `spec/context/VideoEncoder.md`
- Delete: `spec/context/EncodedAudioChunk.md`
- Delete: `spec/context/EncodedVideoChunk.md`
- Delete: `spec/context/AudioData.md`
- Delete: `spec/context/VideoFrame.md`
- Delete: `spec/context/VideoColorSpace.md`
- Delete: `spec/context/ImageDecoder.md`
- Delete: `spec/context/ImageTrackList.md`
- Delete: `spec/context/ImageTrack.md`

**Step 1: Delete all markdown files except IDL** (1 min)

```bash
rm spec/context/AudioDecoder.md spec/context/VideoDecoder.md spec/context/AudioEncoder.md spec/context/VideoEncoder.md spec/context/EncodedAudioChunk.md spec/context/EncodedVideoChunk.md spec/context/AudioData.md spec/context/VideoFrame.md spec/context/VideoColorSpace.md spec/context/ImageDecoder.md spec/context/ImageTrackList.md spec/context/ImageTrack.md
```

**Step 2: Verify only IDL remains** (30 sec)

```bash
ls spec/context/
```

Expected: Only `_webcodecs.idl`

**Step 3: Commit** (30 sec)

```bash
git add -A && git commit -m "chore: remove generated spec context markdown files"
```

---

### Task 2: Delete JSON task files

**Files:**
- Delete: `docs/tasks/*.json` (all 14 files)

**Step 1: Delete all JSON task files** (30 sec)

```bash
rm -rf docs/tasks/
```

**Step 2: Verify directory is gone** (30 sec)

```bash
ls docs/
```

Expected: Only `plans/` directory

**Step 3: Commit** (30 sec)

```bash
git add -A && git commit -m "chore: remove JSON task files"
```

---

### Task 3: Delete task generation scripts

**Files:**
- Delete: `scripts/generate-tasks.ts`
- Delete: `scripts/generate-tasks.test.ts`
- Delete: `scripts/parsers/` (entire directory)
- Delete: `scripts/matchers/` (entire directory)
- Delete: `scripts/types/task-schema.ts`

**Step 1: Delete generate-tasks and test** (30 sec)

```bash
rm scripts/generate-tasks.ts scripts/generate-tasks.test.ts
```

**Step 2: Delete parsers directory** (30 sec)

```bash
rm -rf scripts/parsers/
```

**Step 3: Delete matchers directory** (30 sec)

```bash
rm -rf scripts/matchers/
```

**Step 4: Delete task-schema.ts** (30 sec)

```bash
rm scripts/types/task-schema.ts
```

**Step 5: Check if types directory is empty, remove if so** (30 sec)

```bash
ls scripts/types/ || rmdir scripts/types/
```

**Step 6: Verify scripts directory structure** (30 sec)

```bash
ls scripts/
```

Expected: `scaffold-project.ts`, `verify-types.ts`

**Step 7: Commit** (30 sec)

```bash
git add -A && git commit -m "chore: remove task generation infrastructure"
```

---

## Task Group 2: Update Configuration (Sequential)

### Task 4: Update package.json scripts

**Files:**
- Modify: `package.json`

**Step 1: Read current package.json** (30 sec)

Verify current scripts section contains `generate:tasks`, `tasks`, `pipeline`.

**Step 2: Update package.json** (2 min)

Remove these scripts:
- `"generate:tasks": "tsx scripts/generate-tasks.ts"`
- `"tasks": "npm run generate:tasks"`
- `"pipeline": "npm run scaffold && npm run generate:tasks"`

Add new script:
- `"issues": "tsx scripts/generate-issues.ts"`

Keep unchanged:
- `"scaffold": "tsx scripts/scaffold-project.ts"`

**Step 3: Verify package.json is valid** (30 sec)

```bash
npm run scaffold --help 2>&1 | head -1
```

Expected: No JSON parse errors

**Step 4: Commit** (30 sec)

```bash
git add package.json && git commit -m "chore: update npm scripts for new issue system"
```

---

### Task 5: Update .gitignore for cache directory

**Files:**
- Modify: `.gitignore`

**Step 1: Check current .gitignore** (30 sec)

Verify `.spec-cache/` exists. Decision: reuse it (already gitignored) instead of adding `.cache/`.

**Step 2: Update .gitignore** (1 min)

Rename `.spec-cache/` to `.cache/` for consistency, or keep `.spec-cache/` and use that in the script.

Actually, keep it simple: the existing `.spec-cache/` line works. No change needed unless we want to rename.

**Step 3: Commit if changed** (30 sec)

```bash
git add .gitignore && git commit -m "chore: update gitignore for cache" || echo "No changes"
```

---

## Task Group 3: Create New Script (Sequential)

### Task 6: Create generate-issues.ts script

**Files:**
- Create: `scripts/generate-issues.ts`

**Step 1: Write the script skeleton** (5 min)

```typescript
/**
 * Generate GitHub Issues from WebCodecs Spec
 *
 * Clones w3c/webcodecs repo, renders with bikeshed, extracts H2 sections,
 * creates one GitHub issue per section via `gh` CLI.
 *
 * Usage: npm run issues
 *
 * Prerequisites:
 *   - bikeshed installed: pip install bikeshed
 *   - gh CLI installed and authenticated
 */

import * as fs from 'node:fs/promises';
import * as path from 'node:path';
import { fileURLToPath } from 'node:url';
import { execSync } from 'node:child_process';
import { JSDOM } from 'jsdom';
import TurndownService from 'turndown';

const currentDir = path.dirname(fileURLToPath(import.meta.url));
const ROOT_DIR = path.resolve(currentDir, '..');
const CACHE_DIR = path.join(ROOT_DIR, '.spec-cache');
const WEBCODECS_DIR = path.join(CACHE_DIR, 'webcodecs');
const SPEC_SOURCE = path.join(WEBCODECS_DIR, 'index.src.html');
const SPEC_OUTPUT = path.join(CACHE_DIR, 'webcodecs.html');

interface Section {
  number: string;
  title: string;
  content: string;
}

async function ensureWebCodecsRepo(): Promise<void> {
  await fs.mkdir(CACHE_DIR, { recursive: true });

  try {
    await fs.access(path.join(WEBCODECS_DIR, '.git'));
    console.log('Updating webcodecs repo...');
    execSync('git pull', { cwd: WEBCODECS_DIR, stdio: 'inherit' });
  } catch {
    console.log('Cloning webcodecs repo...');
    execSync('git clone --depth 1 https://github.com/w3c/webcodecs.git', {
      cwd: CACHE_DIR,
      stdio: 'inherit',
    });
  }
}

async function renderWithBikeshed(): Promise<void> {
  console.log('Rendering spec with bikeshed...');
  execSync(`bikeshed spec "${SPEC_SOURCE}" "${SPEC_OUTPUT}"`, {
    cwd: WEBCODECS_DIR,
    stdio: 'inherit',
  });
}

function extractH2Sections(html: string): Section[] {
  const dom = new JSDOM(html);
  const doc = dom.window.document;
  const sections: Section[] = [];

  // Find all H2 elements that are section headings
  const h2Elements = doc.querySelectorAll('h2[id]');

  for (const h2 of h2Elements) {
    const section = h2.closest('section');
    if (!section) continue;

    const secno = h2.querySelector('.secno');
    const number = secno?.textContent?.trim().replace(/\.$/, '') || '';

    // Get title without section number
    const titleText = h2.textContent?.replace(secno?.textContent || '', '').trim() || '';

    // Skip non-API sections (definitions, acknowledgements, etc.)
    const skipSections = ['1', '11', '12', '13', '14', '15'];
    if (skipSections.includes(number)) continue;

    // Get section HTML content
    const content = section.innerHTML;

    sections.push({ number, title: titleText, content });
  }

  return sections;
}

function htmlToMarkdown(html: string): string {
  const turndown = new TurndownService({
    headingStyle: 'atx',
    codeBlockStyle: 'fenced',
  });

  // Custom rule for WebIDL code blocks
  turndown.addRule('webidl', {
    filter: (node) => {
      return node.nodeName === 'PRE' && node.classList.contains('idl');
    },
    replacement: (content) => {
      return '\n```webidl\n' + content.trim() + '\n```\n';
    },
  });

  return turndown.turndown(html);
}

async function issueExists(title: string): Promise<boolean> {
  try {
    const result = execSync(`gh issue list --search "${title}" --json title`, {
      encoding: 'utf-8',
    });
    const issues = JSON.parse(result);
    return issues.some((issue: { title: string }) => issue.title === title);
  } catch {
    return false;
  }
}

async function createIssue(section: Section): Promise<void> {
  const title = `${section.number}. ${section.title}`;

  if (await issueExists(title)) {
    console.log(`Skipping "${title}" - already exists`);
    return;
  }

  const markdown = htmlToMarkdown(section.content);
  const body = `> Section ${section.number} from W3C WebCodecs Specification\n\n${markdown}`;

  // Write body to temp file to handle large content
  const tmpFile = path.join(CACHE_DIR, 'issue-body.md');
  await fs.writeFile(tmpFile, body);

  console.log(`Creating issue: ${title}`);
  execSync(`gh issue create --title "${title}" --body-file "${tmpFile}" --label "spec-section"`, {
    stdio: 'inherit',
  });

  await fs.unlink(tmpFile);
}

async function main(): Promise<void> {
  // Ensure gh CLI is authenticated
  try {
    execSync('gh auth status', { stdio: 'pipe' });
  } catch {
    console.error('Error: gh CLI not authenticated. Run: gh auth login');
    process.exit(1);
  }

  // Ensure bikeshed is installed
  try {
    execSync('bikeshed --version', { stdio: 'pipe' });
  } catch {
    console.error('Error: bikeshed not installed. Run: pip install bikeshed');
    process.exit(1);
  }

  await ensureWebCodecsRepo();
  await renderWithBikeshed();

  const html = await fs.readFile(SPEC_OUTPUT, 'utf-8');
  const sections = extractH2Sections(html);

  console.log(`Found ${sections.length} sections to create as issues`);

  for (const section of sections) {
    await createIssue(section);
  }

  console.log('Done!');
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
```

**Step 2: Verify TypeScript compiles** (30 sec)

```bash
npx tsc --noEmit scripts/generate-issues.ts
```

Expected: No errors (or only type warnings we can ignore)

**Step 3: Commit** (30 sec)

```bash
git add scripts/generate-issues.ts && git commit -m "feat: add generate-issues script for spec-based GitHub issues"
```

---

### Task 7: Test the script (dry run)

**Files:**
- None (testing only)

**Step 1: Run script prerequisites check** (1 min)

```bash
gh auth status
bikeshed --version
```

Expected: Both commands succeed

**Step 2: Run script** (2-5 min)

```bash
npm run issues
```

Expected: Script clones repo, renders spec, creates issues (or skips if they exist)

**Step 3: Verify issues created** (30 sec)

```bash
gh issue list --label spec-section
```

Expected: 8-10 issues with titles like "2. Codec Processing Model", "3. AudioDecoder Interface", etc.

---

## Task Group 4: Final Cleanup (Sequential)

### Task 8: Remove unused devDependencies

**Files:**
- Modify: `package.json`

**Step 1: Check which deps were only used by generate-tasks** (1 min)

The deleted scripts used:
- `ts-morph` - used by ts-ast-parser.ts (DELETE candidate)
- `webidl2` - still used by scaffold-project.ts (KEEP)
- `jsdom` - still used by scaffold-project.ts and new script (KEEP)
- `turndown` - still used by scaffold-project.ts and new script (KEEP)

**Step 2: Remove ts-morph** (30 sec)

```bash
npm uninstall ts-morph
```

**Step 3: Verify build still works** (1 min)

```bash
npm run scaffold -- --help
```

Expected: No errors

**Step 4: Commit** (30 sec)

```bash
git add package.json package-lock.json && git commit -m "chore: remove unused ts-morph dependency"
```

---

### Task 9: Code Review

Review all changes from this plan execution.

```bash
git log --oneline -10
git diff HEAD~8..HEAD --stat
```

Verify:
- All markdown context files deleted
- All JSON task files deleted
- All parser/matcher scripts deleted
- package.json updated correctly
- New generate-issues.ts script works
- No broken imports or references

---

## Parallel Groups Summary

| Task Group | Tasks | Rationale |
|------------|-------|-----------|
| Group 1 | 1, 2, 3 | Independent deletions, no file overlap |
| Group 2 | 4, 5 | Sequential - both modify config files |
| Group 3 | 6, 7 | Sequential - create then test |
| Group 4 | 8, 9 | Sequential - cleanup then review |
