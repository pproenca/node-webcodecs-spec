/**
 * Generate Issue Markdown Files from WebCodecs Spec
 *
 * Fetches pre-rendered WebCodecs spec from W3C, extracts H2 sections,
 * generates local .md files in docs/issues/.
 *
 * Usage:
 *   npm run issues           # Generate .md files locally
 *   npm run issues -- --push # Also create GitHub issues from .md files
 *
 * Prerequisites (for --push only):
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
const OUTPUT_DIR = path.join(ROOT_DIR, 'docs', 'issues');
const SPEC_URL = 'https://www.w3.org/TR/webcodecs/';
const SPEC_CACHE_FILE = path.join(CACHE_DIR, 'webcodecs.html');

// GitHub issue body limit is 65536, use 60000 to be safe with formatting
const MAX_ISSUE_LENGTH = 60000;

interface Section {
  number: string;
  title: string;
  content: string;
}

interface IssueFile {
  filename: string;
  title: string;
  body: string;
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

function slugify(text: string): string {
  return text
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, '-')
    .replace(/^-|-$/g, '');
}

function splitContentIntoParts(markdown: string, maxLength: number): string[] {
  if (markdown.length <= maxLength) {
    return [markdown];
  }

  const parts: string[] = [];
  let remaining = markdown;

  while (remaining.length > 0) {
    if (remaining.length <= maxLength) {
      parts.push(remaining);
      break;
    }

    // Find a good break point (paragraph or heading boundary)
    let breakPoint = maxLength;

    // Look for paragraph break (double newline) near the limit
    const paragraphBreak = remaining.lastIndexOf('\n\n', maxLength);
    if (paragraphBreak > maxLength * 0.5) {
      breakPoint = paragraphBreak;
    } else {
      // Fall back to single newline
      const lineBreak = remaining.lastIndexOf('\n', maxLength);
      if (lineBreak > maxLength * 0.5) {
        breakPoint = lineBreak;
      }
    }

    parts.push(remaining.slice(0, breakPoint).trim());
    remaining = remaining.slice(breakPoint).trim();
  }

  return parts;
}

function generateIssueFiles(sections: Section[]): IssueFile[] {
  const files: IssueFile[] = [];

  for (const section of sections) {
    const markdown = htmlToMarkdown(section.content);
    const baseTitle = `${section.number}. ${section.title}`;
    const baseSlug = `${section.number.padStart(2, '0')}-${slugify(section.title)}`;
    const header = `> Section ${section.number} from [W3C WebCodecs Specification](${SPEC_URL})\n\n`;

    // Split content if too large
    const parts = splitContentIntoParts(markdown, MAX_ISSUE_LENGTH - header.length - 200);

    if (parts.length === 1) {
      // Single file
      files.push({
        filename: `${baseSlug}.md`,
        title: baseTitle,
        body: header + markdown,
      });
    } else {
      // Multiple files
      for (let i = 0; i < parts.length; i++) {
        const partNum = i + 1;
        const partHeader =
          header + `**Part ${partNum} of ${parts.length}**\n\n---\n\n`;

        files.push({
          filename: `${baseSlug}-part-${partNum}-of-${parts.length}.md`,
          title: `${baseTitle} (Part ${partNum} of ${parts.length})`,
          body: partHeader + parts[i],
        });
      }
    }
  }

  return files;
}

async function writeIssueFiles(files: IssueFile[]): Promise<void> {
  await fs.mkdir(OUTPUT_DIR, { recursive: true });

  // Clear existing files
  const existingFiles = await fs.readdir(OUTPUT_DIR).catch(() => []);
  for (const file of existingFiles) {
    if (file.endsWith('.md')) {
      await fs.unlink(path.join(OUTPUT_DIR, file));
    }
  }

  for (const file of files) {
    const filePath = path.join(OUTPUT_DIR, file.filename);
    // Add title as YAML frontmatter
    const content = `---\ntitle: "${file.title}"\n---\n\n${file.body}`;
    await fs.writeFile(filePath, content);
    console.log(`  Created: ${file.filename}`);
  }
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

async function pushIssuesToGitHub(files: IssueFile[]): Promise<void> {
  // Ensure gh CLI is authenticated
  try {
    execSync('gh auth status', { stdio: 'pipe' });
  } catch {
    console.error('Error: gh CLI not authenticated. Run: gh auth login');
    process.exit(1);
  }

  // Ensure label exists
  try {
    execSync('gh label create spec-section --description "WebCodecs spec section" --color 0366d6', {
      stdio: 'pipe',
    });
    console.log('Created label: spec-section');
  } catch {
    // Label already exists
  }

  for (const file of files) {
    if (await issueExists(file.title)) {
      console.log(`  Skipping "${file.title}" - already exists`);
      continue;
    }

    const tmpFile = path.join(CACHE_DIR, 'issue-body.md');
    await fs.writeFile(tmpFile, file.body);

    console.log(`  Creating: ${file.title}`);
    try {
      execSync(`gh issue create --title "${file.title}" --body-file "${tmpFile}" --label "spec-section"`, {
        stdio: 'inherit',
      });
    } catch (err) {
      console.error(`  Failed to create issue: ${file.title}`);
    }

    await fs.unlink(tmpFile);
  }
}

async function main(): Promise<void> {
  const args = process.argv.slice(2);
  const shouldPush = args.includes('--push');

  const html = await fetchSpec();
  const sections = extractH2Sections(html);

  console.log(`Found ${sections.length} spec sections`);

  const files = generateIssueFiles(sections);
  console.log(`Generated ${files.length} issue files`);

  console.log(`\nWriting to ${OUTPUT_DIR}/`);
  await writeIssueFiles(files);

  if (shouldPush) {
    console.log('\nPushing to GitHub...');
    await pushIssuesToGitHub(files);
  } else {
    console.log(`\nTo create GitHub issues, run: npm run issues -- --push`);
  }

  console.log('\nDone!');
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
