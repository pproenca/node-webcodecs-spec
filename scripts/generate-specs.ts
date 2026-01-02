/**
 * Generate Spec Markdown Files from WebCodecs Spec
 *
 * Fetches pre-rendered WebCodecs spec from W3C, extracts H2 sections,
 * generates local .md files in docs/specs/.
 *
 * Also fetches codec registry specs for FFmpeg implementation mapping.
 *
 * Usage:
 *   npm run specs           # Generate .md files locally
 */

import * as fs from 'node:fs/promises';
import * as path from 'node:path';
import { fileURLToPath } from 'node:url';
import { JSDOM } from 'jsdom';
import TurndownService from 'turndown';

const currentDir = path.dirname(fileURLToPath(import.meta.url));
const ROOT_DIR = path.resolve(currentDir, '..');
const CACHE_DIR = path.join(ROOT_DIR, '.spec-cache');
const OUTPUT_DIR = path.join(ROOT_DIR, 'docs', 'specs');
const CODECS_OUTPUT_DIR = path.join(OUTPUT_DIR, 'codecs');
const SPEC_URL = 'https://www.w3.org/TR/webcodecs/';
const SPEC_CACHE_FILE = path.join(CACHE_DIR, 'webcodecs.html');

// Max length for spec file content (generous limit for local files)
const MAX_SPEC_LENGTH = 60000;

// Codec registry specs - separate W3C documents with FFmpeg-critical info
const CODEC_REGISTRY_SPECS = [
  {
    slug: '00-codec-registry',
    url: 'https://www.w3.org/TR/webcodecs-codec-registry/',
    title: 'Codec Registry',
  },
  {
    slug: 'avc',
    url: 'https://www.w3.org/TR/webcodecs-avc-codec-registration/',
    title: 'AVC (H.264)',
  },
  {
    slug: 'hevc',
    url: 'https://www.w3.org/TR/webcodecs-hevc-codec-registration/',
    title: 'HEVC (H.265)',
  },
  { slug: 'vp9', url: 'https://www.w3.org/TR/webcodecs-vp9-codec-registration/', title: 'VP9' },
  { slug: 'vp8', url: 'https://www.w3.org/TR/webcodecs-vp8-codec-registration/', title: 'VP8' },
  { slug: 'av1', url: 'https://www.w3.org/TR/webcodecs-av1-codec-registration/', title: 'AV1' },
  { slug: 'opus', url: 'https://www.w3.org/TR/webcodecs-opus-codec-registration/', title: 'Opus' },
  { slug: 'aac', url: 'https://www.w3.org/TR/webcodecs-aac-codec-registration/', title: 'AAC' },
  { slug: 'flac', url: 'https://www.w3.org/TR/webcodecs-flac-codec-registration/', title: 'FLAC' },
  { slug: 'mp3', url: 'https://www.w3.org/TR/webcodecs-mp3-codec-registration/', title: 'MP3' },
  {
    slug: 'pcm',
    url: 'https://www.w3.org/TR/webcodecs-pcm-codec-registration/',
    title: 'Linear PCM',
  },
  {
    slug: 'ulaw',
    url: 'https://www.w3.org/TR/webcodecs-ulaw-codec-registration/',
    title: 'u-law PCM',
  },
  {
    slug: 'alaw',
    url: 'https://www.w3.org/TR/webcodecs-alaw-codec-registration/',
    title: 'A-law PCM',
  },
  {
    slug: 'vorbis',
    url: 'https://www.w3.org/TR/webcodecs-vorbis-codec-registration/',
    title: 'Vorbis',
  },
];

interface Section {
  number: string;
  title: string;
  content: string;
}

interface SpecFile {
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

async function fetchCodecSpec(spec: (typeof CODEC_REGISTRY_SPECS)[0]): Promise<string> {
  const cacheFile = path.join(CACHE_DIR, `${spec.slug}.html`);

  // Check cache
  try {
    const stats = await fs.stat(cacheFile);
    const ageMs = Date.now() - stats.mtimeMs;
    const oneHour = 60 * 60 * 1000;

    if (ageMs < oneHour) {
      return await fs.readFile(cacheFile, 'utf-8');
    }
  } catch {
    // Cache doesn't exist
  }

  console.log(`  Fetching ${spec.title}...`);
  const response = await fetch(spec.url);
  if (!response.ok) {
    throw new Error(`Failed to fetch ${spec.url}: ${response.status}`);
  }

  const html = await response.text();
  await fs.writeFile(cacheFile, html);
  return html;
}

function extractH2Sections(html: string): Section[] {
  const dom = new JSDOM(html);
  const doc = dom.window.document;
  const sections: Section[] = [];

  // Find all H2 elements that are section headings
  const h2Elements = Array.from(doc.querySelectorAll('h2.heading[id]'));

  // Skip only security/privacy/acknowledgements - keep definitions, resource reclamation, best practices
  const skipSections = ['12', '13', '15'];

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

function extractCodecSpecContent(html: string): string {
  const dom = new JSDOM(html);
  const doc = dom.window.document;

  // Get the main content section
  const mainContent = doc.querySelector('section#main') || doc.querySelector('body');
  if (!mainContent) {
    return '';
  }

  // Remove navigation, header, footer elements
  const elementsToRemove = mainContent.querySelectorAll('nav, header, footer, .head, #toc');
  elementsToRemove.forEach((el) => el.remove());

  return mainContent.innerHTML;
}

function createTurndownService(specUrl: string): TurndownService {
  const turndown = new TurndownService({
    headingStyle: 'atx',
    codeBlockStyle: 'fenced',
  });

  // Custom rule for WebIDL code blocks - use raw text to strip all HTML tags
  turndown.addRule('webidl', {
    filter: (node) => {
      return node.nodeName === 'PRE' && node.classList.contains('idl');
    },
    replacement: (_content, node) => {
      // Get raw text content, stripping all HTML tags including links
      const rawText = (node as HTMLElement).textContent || '';
      return '\n```webidl\n' + rawText.trim() + '\n```\n';
    },
  });

  // Linked headings - make header text link to W3C spec section
  turndown.addRule('linkedHeading', {
    filter: ['h2', 'h3', 'h4'],
    replacement: (content, node) => {
      const el = node as HTMLElement;
      const selfLink = el.querySelector('a.self-link');
      const href = selfLink?.getAttribute('href') || '';
      const level = parseInt(el.tagName[1]);
      const hashes = '#'.repeat(level);

      // Clean up: remove self-link, unescape periods after numbers
      const cleanContent = content
        .replace(/\[\]\([^)]*\)$/, '') // Remove trailing empty self-link
        .replace(/(\d+)\\\./g, '$1.') // Unescape periods after numbers
        .trim();

      if (href) {
        const absoluteHref = href.startsWith('#') ? specUrl + href : href;
        return `\n${hashes} [${cleanContent}](${absoluteHref})\n\n`;
      }
      return `\n${hashes} ${cleanContent}\n\n`;
    },
  });

  // Definition list handling
  turndown.addRule('definitionList', {
    filter: 'dl',
    replacement: (content) => '\n' + content + '\n',
  });

  // Definition terms - algorithm definitions become #### headings, others become bold
  turndown.addRule('definitionTerm', {
    filter: 'dt',
    replacement: (content, node) => {
      const el = node as HTMLElement;
      const dfn = el.querySelector('dfn[data-dfn-type="dfn"]');

      // Algorithm definitions become #### headings
      if (dfn) {
        const text = content.trim();
        return `\n#### ${text}\n\n`;
      }

      // Regular definition terms (internal slots, attributes) become bold
      return '\n**' + content.trim() + '**\n\n';
    },
  });

  // Definition descriptions
  turndown.addRule('definitionDescription', {
    filter: 'dd',
    replacement: (content) => content.trim() + '\n',
  });

  // Handle <code> tags containing links - render as link without backticks
  turndown.addRule('codeWithLink', {
    filter: (node) => {
      if (node.nodeName !== 'CODE') return false;
      // Check if code contains only a single anchor element
      const children = Array.from(node.childNodes);
      return (
        children.length === 1 &&
        children[0].nodeType === 1 &&
        (children[0] as Element).nodeName === 'A'
      );
    },
    replacement: (_content, node) => {
      const anchor = node.querySelector('a') as HTMLAnchorElement;
      if (!anchor) return _content;
      const href = anchor.getAttribute('href') || '';
      const text = anchor.textContent || '';
      // Convert relative links to absolute
      const absoluteHref = href.startsWith('#') ? specUrl + href : href;
      return `[${text}](${absoluteHref})`;
    },
  });

  // Fix internal links - convert to absolute URLs
  turndown.addRule('specLinks', {
    filter: (node) => {
      if (node.nodeName !== 'A') return false;
      const href = (node as HTMLAnchorElement).getAttribute('href');
      return href !== null && href.startsWith('#');
    },
    replacement: (content, node) => {
      const href = (node as HTMLAnchorElement).getAttribute('href');
      return `[${content}](${specUrl}${href})`;
    },
  });

  return turndown;
}

function htmlToMarkdown(html: string, specUrl: string = SPEC_URL): string {
  const turndown = createTurndownService(specUrl);
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

function generateSpecFiles(sections: Section[]): SpecFile[] {
  const files: SpecFile[] = [];

  for (const section of sections) {
    const markdown = htmlToMarkdown(section.content);
    const baseTitle = `${section.number}. ${section.title}`;
    const baseSlug = `${section.number.padStart(2, '0')}-${slugify(section.title)}`;
    const header = `> Section ${section.number} from [W3C WebCodecs Specification](${SPEC_URL})\n\n`;

    // Split content if too large
    const parts = splitContentIntoParts(markdown, MAX_SPEC_LENGTH - header.length - 200);

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
        const partHeader = header + `**Part ${partNum} of ${parts.length}**\n\n---\n\n`;

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

async function writeSpecFiles(files: SpecFile[]): Promise<void> {
  await fs.mkdir(OUTPUT_DIR, { recursive: true });

  // Clear existing .md files (but not subdirectories)
  const existingFiles = await fs.readdir(OUTPUT_DIR).catch(() => []);
  for (const file of existingFiles) {
    const filePath = path.join(OUTPUT_DIR, file);
    const stat = await fs.stat(filePath);
    if (stat.isFile() && file.endsWith('.md')) {
      await fs.unlink(filePath);
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

async function fetchAndWriteCodecSpecs(): Promise<void> {
  await fs.mkdir(CODECS_OUTPUT_DIR, { recursive: true });

  // Clear existing codec files
  const existingFiles = await fs.readdir(CODECS_OUTPUT_DIR).catch(() => []);
  for (const file of existingFiles) {
    if (file.endsWith('.md')) {
      await fs.unlink(path.join(CODECS_OUTPUT_DIR, file));
    }
  }

  console.log('\nFetching codec registry specs...');

  for (const spec of CODEC_REGISTRY_SPECS) {
    try {
      const html = await fetchCodecSpec(spec);
      const content = extractCodecSpecContent(html);
      const markdown = htmlToMarkdown(content, spec.url);

      const header = `> From [${spec.title} Registration](${spec.url})\n\n`;
      const body = header + markdown;

      const filePath = path.join(CODECS_OUTPUT_DIR, `${spec.slug}.md`);
      const fileContent = `---\ntitle: "${spec.title}"\n---\n\n${body}`;
      await fs.writeFile(filePath, fileContent);
      console.log(`  Created: codecs/${spec.slug}.md`);
    } catch (err) {
      console.error(`  Failed to fetch ${spec.title}: ${err}`);
    }
  }
}

async function main(): Promise<void> {
  // Fetch and process main WebCodecs spec
  const html = await fetchSpec();
  const sections = extractH2Sections(html);

  console.log(`Found ${sections.length} spec sections`);

  const files = generateSpecFiles(sections);
  console.log(`Generated ${files.length} spec files`);

  console.log(`\nWriting to ${OUTPUT_DIR}/`);
  await writeSpecFiles(files);

  // Fetch codec registry specs
  await fetchAndWriteCodecSpecs();

  console.log('\nDone!');
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
