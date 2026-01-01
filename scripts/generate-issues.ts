/**
 * Generate GitHub Issues from WebCodecs Spec
 *
 * Fetches pre-rendered WebCodecs spec from W3C, extracts H2 sections,
 * creates one GitHub issue per section via `gh` CLI.
 *
 * Usage: npm run issues
 *
 * Prerequisites:
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
const SPEC_URL = 'https://www.w3.org/TR/webcodecs/';
const SPEC_CACHE_FILE = path.join(CACHE_DIR, 'webcodecs.html');

interface Section {
  number: string;
  title: string;
  content: string;
}

async function fetchSpec(): Promise<string> {
  await fs.mkdir(CACHE_DIR, { recursive: true });

  // Check if we have a cached version (less than 1 hour old)
  try {
    const stats = await fs.stat(SPEC_CACHE_FILE);
    const ageMs = Date.now() - stats.mtimeMs;
    const oneHour = 60 * 60 * 1000;

    if (ageMs < oneHour) {
      console.log('Using cached spec (less than 1 hour old)...');
      return await fs.readFile(SPEC_CACHE_FILE, 'utf-8');
    }
  } catch {
    // Cache doesn't exist, fetch fresh
  }

  console.log(`Fetching spec from ${SPEC_URL}...`);
  const response = await fetch(SPEC_URL);
  if (!response.ok) {
    throw new Error(`Failed to fetch spec: ${response.status} ${response.statusText}`);
  }

  const html = await response.text();
  await fs.writeFile(SPEC_CACHE_FILE, html);
  console.log('Spec cached.');

  return html;
}

function extractH2Sections(html: string): Section[] {
  const dom = new JSDOM(html);
  const doc = dom.window.document;
  const sections: Section[] = [];

  // Find all H2 elements that are section headings
  const h2Elements = Array.from(doc.querySelectorAll('h2.heading[id]'));

  // Skip non-API sections (definitions, acknowledgements, etc.)
  const skipSections = ['1', '11', '12', '13', '14', '15'];

  for (let i = 0; i < h2Elements.length; i++) {
    const h2 = h2Elements[i];
    const secno = h2.querySelector('.secno');
    const number = secno?.textContent?.trim().replace(/\.$/, '') || '';

    if (!number || skipSections.includes(number)) continue;

    // Get title without section number
    const titleText = h2.textContent?.replace(secno?.textContent || '', '').trim() || '';

    // Collect content from this H2 until next H2
    const contentParts: string[] = [];
    let sibling = h2.nextElementSibling;
    const nextH2 = h2Elements[i + 1];

    while (sibling && sibling !== nextH2) {
      // Stop if we hit the next H2
      if (sibling.tagName === 'H2') break;
      contentParts.push(sibling.outerHTML);
      sibling = sibling.nextElementSibling;
    }

    const content = `<h2>${h2.innerHTML}</h2>\n${contentParts.join('\n')}`;
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

  // Try with label, fall back to without if label doesn't exist
  try {
    execSync(`gh issue create --title "${title}" --body-file "${tmpFile}" --label "spec-section"`, {
      stdio: 'inherit',
    });
  } catch {
    // Label might not exist, create without it
    console.log('  (creating without label - run: gh label create spec-section)');
    execSync(`gh issue create --title "${title}" --body-file "${tmpFile}"`, {
      stdio: 'inherit',
    });
  }

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

  const html = await fetchSpec();
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
