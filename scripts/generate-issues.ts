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
