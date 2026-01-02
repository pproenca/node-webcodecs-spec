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

// --- FFmpeg Implementation Documentation Generator ---

/**
 * Generates supplemental documentation for FFmpeg implementation.
 * Covers gaps in W3C spec that are critical for correct implementation.
 */
async function generateFFmpegImplementationDocs(): Promise<void> {
  const docs = [
    { filename: '12-security-considerations.md', content: generateSecurityConsiderations() },
    { filename: '13-privacy-considerations.md', content: generatePrivacyConsiderations() },
    { filename: '15-error-types-reference.md', content: generateErrorTypesReference() },
    { filename: '16-codec-state-machine.md', content: generateStateMachineDoc() },
    { filename: '17-pixel-format-mapping.md', content: generatePixelFormatMapping() },
    { filename: '18-codec-string-registry.md', content: generateCodecStringRegistry() },
    { filename: '19-ffmpeg-implementation-notes.md', content: generateFFmpegNotes() },
    { filename: '20-audio-sample-format-mapping.md', content: generateAudioFormatMapping() },
  ];

  for (const doc of docs) {
    const filePath = path.join(OUTPUT_DIR, doc.filename);
    await fs.writeFile(filePath, doc.content);
    console.log(`  Created: ${doc.filename}`);
  }
}

function generateSecurityConsiderations(): string {
  return `---
title: '12. Security Considerations'
---

> Section 12 from [W3C WebCodecs Specification](https://www.w3.org/TR/webcodecs/)

## [12. Security Considerations](https://www.w3.org/TR/webcodecs/#security-considerations)

This section documents security considerations for WebCodecs implementations, particularly relevant for FFmpeg-backed Node.js implementations.

### 12.1. Input Validation

**Malformed Input Handling**

Codec implementations MUST handle malformed or malicious input gracefully:

1. **Buffer Overflow Prevention**: Validate all buffer sizes before processing
2. **Integer Overflow**: Check arithmetic operations on dimensions, timestamps, durations
3. **Null Pointer Safety**: Validate all pointers before dereferencing

**FFmpeg Implementation** (see \`src/video_decoder.cpp\`):
\`\`\`cpp
#include "error_builder.h"

// Always validate input before FFmpeg calls
if (!chunk || chunk->byteLength() == 0) {
  webcodecs::errors::ThrowTypeError(env, "Invalid chunk data");
  return env.Undefined();
}

// Check for reasonable dimensions (prevent allocation attacks)
if (config.codedWidth > 16384 || config.codedHeight > 16384) {
  webcodecs::errors::ThrowTypeError(env, "Dimensions exceed maximum supported size");
  return env.Undefined();
}
\`\`\`

### 12.2. Resource Exhaustion

**Memory Limits**

User Agents SHOULD implement limits to prevent resource exhaustion:

| Resource | Recommended Limit | FFmpeg Consideration |
|----------|-------------------|---------------------|
| Max concurrent codecs | 16-32 | Track active \`raii::AVCodecContextPtr\` instances |
| Max frame dimensions | 16384x16384 | Check before \`avcodec_open2()\` |
| Max decode queue | 32 frames | Limit pending \`avcodec_send_packet()\` calls |
| Max memory per frame | 256MB | Validate \`allocationSize()\` results |

**Timeout Handling**

Long-running codec operations SHOULD have timeouts:

\`\`\`cpp
#include "error_builder.h"

// Set decode timeout (implementation-specific)
constexpr auto kDecodeTimeout = std::chrono::seconds(30);

// Monitor codec operations for hangs
if (operation_duration > kDecodeTimeout) {
  // Close codec with EncodingError
  webcodecs::errors::ThrowEncodingError(env, "Decode operation timed out");
  state_.Close();
}
\`\`\`

### 12.3. Codec-Specific Vulnerabilities

**Known Vulnerability Patterns**

| Vulnerability | Affected Codecs | Mitigation |
|---------------|-----------------|------------|
| Heap overflow | H.264, HEVC | Update FFmpeg regularly, validate NAL units |
| Integer overflow | VP9, AV1 | Validate tile/frame dimensions |
| Use-after-free | All | RAII wrappers from \`ffmpeg_raii.h\`, reference counting |
| Infinite loops | GIF, APNG | Frame count limits, timeouts |

**FFmpeg Safety Flags** (see \`src/video_decoder.cpp\`):

\`\`\`cpp
// Enable FFmpeg safety features
codec_ctx_->err_recognition = AV_EF_CRCCHECK | AV_EF_BITSTREAM;
codec_ctx_->flags2 |= AV_CODEC_FLAG2_CHUNKS; // Handle incomplete frames safely
\`\`\`

### 12.4. Side-Channel Attacks

**Timing Attacks**

Codec timing can leak information about content:

- Hardware vs software codec selection
- Frame complexity (I-frame vs P-frame decode time)
- Resolution/bitrate of content

**Mitigation**: Avoid exposing precise timing information in error messages or callbacks.

### 12.5. Origin Security

**VideoFrame Origin**

In browser contexts, VideoFrames track origin for security:

\`\`\`typescript
// Browser: Throws SecurityError if image is not origin-clean
new VideoFrame(crossOriginImage); // SecurityError

// Node.js: No origin restrictions (trusted environment)
\`\`\`

**Node.js Consideration**: Since Node.js runs in a trusted environment, origin checks are not applicable but implementations SHOULD still validate input sources.

### 12.6. Sandboxing

For production deployments, consider:

1. **Process Isolation**: Run codec operations in separate processes
2. **Memory Limits**: Use \`ulimit\` or cgroups to limit memory
3. **Syscall Filtering**: Use seccomp to restrict dangerous syscalls
4. **Fuzzing**: Regularly fuzz codec inputs with tools like AFL or libFuzzer
`;
}

function generatePrivacyConsiderations(): string {
  return `---
title: '13. Privacy Considerations'
---

> Section 13 from [W3C WebCodecs Specification](https://www.w3.org/TR/webcodecs/)

## [13. Privacy Considerations](https://www.w3.org/TR/webcodecs/#privacy-considerations)

### 13.1. Hardware Fingerprinting

**isConfigSupported() Fingerprinting Risk**

The \`isConfigSupported()\` static method can reveal hardware capabilities:

\`\`\`javascript
// Potential fingerprinting vector
const h264Support = await VideoDecoder.isConfigSupported({
  codec: 'avc1.42001E',
  hardwareAcceleration: 'prefer-hardware'
});
// Reveals: Does this device have H.264 hardware decoder?
\`\`\`

**W3C Mitigation Requirement**:
> "To prevent fingerprinting, if a User Agent implements [media-capabilities], the User Agent MUST ensure rejection or acceptance of a given HardwareAcceleration preference reveals no additional information on top of what is inherent to the User Agent and revealed by [media-capabilities]."

**Node.js Implementation**:

Since Node.js doesn't run in a browser context with fingerprinting concerns, implementations MAY report accurate hardware capabilities. However, for privacy-conscious applications:

\`\`\`cpp
// Option: Always report software-only support
bool IsConfigSupported(const Config& config) {
  if (config.hardwareAcceleration == "prefer-hardware") {
    // Could return false to hide hardware capabilities
    return CheckSoftwareSupport(config);
  }
  return CheckSoftwareSupport(config);
}
\`\`\`

### 13.2. Timing Side Channels

**Decode/Encode Timing**

Timing variations can reveal content characteristics:

| Observable | Information Leaked |
|------------|-------------------|
| Decode time variance | Frame type (I/P/B), complexity |
| Hardware vs software | GPU presence, driver version |
| First frame latency | Codec initialization time |

**Mitigation** (if needed for privacy):
- Add random delays to normalize timing
- Use constant-time comparisons for sensitive data
- Avoid exposing precise timing in error messages

### 13.3. Resource Usage Patterns

**Memory Allocation Patterns**

Frame allocation sizes reveal video characteristics:

\`\`\`javascript
const frame = await decoder.decode(chunk);
// frame.allocationSize() reveals resolution
// Multiple frames reveal framerate patterns
\`\`\`

**Node.js Context**: These privacy concerns are primarily for browser environments. In server-side Node.js applications, the operator typically controls both the code and data, making these concerns less relevant.

### 13.4. Recommendations for Privacy-Sensitive Applications

If building privacy-preserving media pipelines in Node.js:

1. **Normalize Timing**: Add consistent delays to codec operations
2. **Limit Config Queries**: Cache \`isConfigSupported()\` results
3. **Software-Only Mode**: Use \`prefer-software\` to avoid hardware fingerprinting
4. **Audit Logging**: Be careful what codec metadata gets logged
`;
}

function generateErrorTypesReference(): string {
  return `---
title: '15. Error Types Reference'
---

> DOMException types used throughout the W3C WebCodecs specification

## Error Types and When They Are Thrown

This document provides a complete reference of all DOMException types used in WebCodecs, when they are thrown, and how to handle them in FFmpeg implementations.

See \`src/error_builder.h\` for the implementation of these error types.

### Error Type Summary

| Error Type | When Thrown | FFmpeg Mapping |
|------------|-------------|----------------|
| \`TypeError\` | Invalid config, detached frame, wrong argument type | Validation failures |
| \`InvalidStateError\` | State is "closed", operation invalid for current state | State machine violations |
| \`DataError\` | Key chunk required but delta provided, orientation mismatch | Codec protocol violations |
| \`NotSupportedError\` | Config not supported by UA | \`avcodec_find_decoder()\` returns null |
| \`EncodingError\` | Codec error during encode/decode | FFmpeg \`AVERROR\` codes |
| \`QuotaExceededError\` | Resource reclamation (section 11) | System resource limits |
| \`AbortError\` | Used internally for reset/close | User-initiated cancellation |
| \`DataCloneError\` | Transfer/serialize of detached object | Structured clone failures |
| \`RangeError\` | copyTo buffer too small, invalid plane index | Bounds checking |
| \`SecurityError\` | Non-origin-clean VideoFrame source | Origin violations (browser) |

---

### TypeError

**When Thrown:**
- \`configure()\`: Config is not valid (empty codec string, zero dimensions)
- \`decode()\`/\`encode()\`: Frame/chunk is detached
- \`new VideoFrame()\`: Missing required \`timestamp\` for certain sources
- Any method: Wrong argument types

**FFmpeg Mapping** (see \`src/video_decoder.cpp\`):
\`\`\`cpp
#include "error_builder.h"

Napi::Value VideoDecoder::Configure(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // Validation that throws TypeError
  if (config.codec.empty()) {
    webcodecs::errors::ThrowTypeError(env, "codec string cannot be empty");
    return env.Undefined();
  }
  if (config.codedWidth == 0 || config.codedHeight == 0) {
    webcodecs::errors::ThrowTypeError(env, "codedWidth and codedHeight must be non-zero");
    return env.Undefined();
  }
  // ...
}
\`\`\`

---

### InvalidStateError

**When Thrown:**
- \`configure()\`: State is "closed"
- \`decode()\`/\`encode()\`: State is not "configured"
- \`flush()\`: State is not "configured"
- \`reset()\`/\`close()\`: State is "closed"
- \`clone()\`/\`copyTo()\`: Frame is detached

**FFmpeg Mapping** (see \`src/video_decoder.cpp\`):
\`\`\`cpp
#include "error_builder.h"
#include "ffmpeg_raii.h"

Napi::Value VideoDecoder::Decode(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // Use helper from error_builder.h
  if (!webcodecs::errors::RequireConfiguredState(env, state_, "decode")) {
    return env.Undefined();
  }
  // ... decode logic
}
\`\`\`

---

### DataError

**When Thrown:**
- \`decode()\`: Key chunk required but \`chunk.type\` is "delta"
- \`decode()\`: Chunk inspection reveals it's not actually a key frame
- \`encode()\`: Frame orientation doesn't match first encoded frame

**FFmpeg Mapping** (see \`src/video_decoder.cpp\`):
\`\`\`cpp
#include "error_builder.h"

Napi::Value VideoDecoder::Decode(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (key_chunk_required_.load() && chunk_type != "key") {
    webcodecs::errors::ThrowDataError(env,
        "A key frame is required after configure() or flush()");
    return env.Undefined();
  }

  key_chunk_required_.store(false);
  // ...
}
\`\`\`

---

### NotSupportedError

**When Thrown:**
- \`configure()\`: Codec not supported by user agent
- \`isConfigSupported()\`: Never thrown (returns \`{supported: false}\` instead)

**FFmpeg Mapping** (see \`src/video_decoder.cpp\`):
\`\`\`cpp
#include "error_builder.h"
#include "ffmpeg_raii.h"

void VideoDecoder::ConfigureInternal(const VideoDecoderConfig& config) {
  AVCodecID codec_id = ParseCodecString(config.codec);
  const AVCodec* codec = avcodec_find_decoder(codec_id);

  if (!codec) {
    webcodecs::errors::ThrowNotSupportedError(env,
        "Codec not supported: " + config.codec);
    return;
  }

  // Create codec context with RAII wrapper
  codec_ctx_ = raii::MakeAvCodecContext(codec);
  if (!codec_ctx_) {
    webcodecs::errors::ThrowNotSupportedError(env, "Failed to allocate codec context");
    return;
  }
}
\`\`\`

---

### EncodingError

**When Thrown:**
- \`decode()\`: Codec encounters unrecoverable decode error
- \`encode()\`: Codec encounters unrecoverable encode error

**FFmpeg Mapping** (see \`src/video_decoder.cpp\`, \`src/error_builder.h\`):
\`\`\`cpp
#include "error_builder.h"
#include "ffmpeg_raii.h"

void VideoDecoder::DecodeInternal(raii::AVPacketPtr packet) {
  int ret = avcodec_send_packet(codec_ctx_.get(), packet.get());

  // Use error classification from error_builder.h
  auto error_class = webcodecs::errors::ClassifyFfmpegError(ret);

  switch (error_class) {
    case webcodecs::errors::FFmpegErrorClass::Success:
    case webcodecs::errors::FFmpegErrorClass::Again:
    case webcodecs::errors::FFmpegErrorClass::Eof:
      // Normal state transitions - not errors
      break;
    case webcodecs::errors::FFmpegErrorClass::Error:
      // Actual error - close codec and invoke error callback
      webcodecs::errors::ThrowEncodingError(env, ret, "Decode failed");
      state_.Close();
      break;
  }
}
\`\`\`

---

### QuotaExceededError

**When Thrown:**
- Resource reclamation: System needs to reclaim codec resources

**FFmpeg Mapping:**
\`\`\`cpp
#include "error_builder.h"

// Called when system is under memory pressure
void CodecManager::ReclaimResources(Napi::Env env) {
  // Find inactive/background codecs to reclaim
  for (auto& codec : codecs_) {
    if (codec->IsInactive() || codec->IsBackground()) {
      // Invoke error callback with QuotaExceededError
      // (custom implementation needed - not in error_builder.h)
      codec->state_.Close();
    }
  }
}
\`\`\`

---

### AbortError

**When Thrown:**
- Used internally for \`reset()\` and \`close()\`
- Does NOT trigger error callback (special case)

**FFmpeg Mapping** (see \`src/video_decoder.cpp\`):
\`\`\`cpp
#include "error_builder.h"
#include "ffmpeg_raii.h"

Napi::Value VideoDecoder::Close(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // Close state machine
  state_.Close();

  // Release FFmpeg resources (RAII handles cleanup)
  codec_ctx_.reset();

  // Clear decode queue
  {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!decode_queue_.empty()) {
      decode_queue_.pop();
    }
  }

  return env.Undefined();
}
\`\`\`

---

### Error Callback Behavior

**Important**: The error callback is NOT called for user-initiated operations:

| Operation | Error Callback Called? |
|-----------|----------------------|
| \`reset()\` | ❌ No (AbortError) |
| \`close()\` | ❌ No (AbortError) |
| Decode error | ✅ Yes (EncodingError) |
| Config not supported | ✅ Yes (NotSupportedError) |
| Resource reclaimed | ✅ Yes (QuotaExceededError) |
`;
}

function generateStateMachineDoc(): string {
  return `---
title: '16. Codec State Machine'
---

> State machine for VideoDecoder, AudioDecoder, VideoEncoder, AudioEncoder

## Codec State Machine

All codec interfaces (VideoDecoder, AudioDecoder, VideoEncoder, AudioEncoder) follow the same state machine.

See \`src/ffmpeg_raii.h\` for the \`AtomicCodecState\` implementation.

### State Diagram

\`\`\`
                      configure()
     ┌──────────────────────────────────────┐
     │                                      ▼
┌────────────┐       reset()        ┌─────────────┐
│unconfigured│◄─────────────────────│ configured  │
└────────────┘                      └─────────────┘
     │                                      │
     │ close()                       close()│
     │                                      │
     ▼                                      ▼
┌───────────────────────────────────────────────┐
│                    closed                      │
└───────────────────────────────────────────────┘
\`\`\`

### CodecState Enum

\`\`\`webidl
enum CodecState {
  "unconfigured",  // Initial state, after reset()
  "configured",    // After successful configure()
  "closed"         // Terminal state, after close() or error
};
\`\`\`

### State Transitions

| Current State | Method | New State | Notes |
|---------------|--------|-----------|-------|
| unconfigured | \`configure()\` | configured | Async - may fail with NotSupportedError |
| unconfigured | \`decode()/encode()\` | - | Throws InvalidStateError |
| unconfigured | \`flush()\` | - | Returns rejected promise |
| unconfigured | \`reset()\` | - | Throws InvalidStateError |
| unconfigured | \`close()\` | closed | Releases resources |
| configured | \`configure()\` | configured | Reconfigures codec |
| configured | \`decode()/encode()\` | configured | Queues work |
| configured | \`flush()\` | configured | Returns promise, sets key_chunk_required |
| configured | \`reset()\` | unconfigured | Clears queues, rejects pending promises |
| configured | \`close()\` | closed | Releases resources |
| closed | Any method | - | Throws InvalidStateError |

### Internal State: [[key chunk required]]

Decoders track whether the next chunk must be a key frame:

| Event | [[key chunk required]] |
|-------|----------------------|
| Constructor | true |
| After \`configure()\` | true |
| After \`flush()\` resolves | true |
| After decoding a key frame | false |

**Implementation** (see \`src/video_decoder.h\`):
\`\`\`cpp
#include "ffmpeg_raii.h"
#include "error_builder.h"

class VideoDecoder : public Napi::ObjectWrap<VideoDecoder> {
  // Atomic for thread-safe access
  std::atomic<bool> key_chunk_required_{true};

  Napi::Value Configure(const Napi::CallbackInfo& info) {
    // ...
    key_chunk_required_.store(true);  // Reset on configure
    // ...
  }

  Napi::Value Flush(const Napi::CallbackInfo& info) {
    // After flush completes (in async worker):
    key_chunk_required_.store(true);  // Reset after flush
    // ...
  }

  Napi::Value Decode(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (key_chunk_required_.load() && chunk_type != "key") {
      webcodecs::errors::ThrowDataError(env, "Key frame required");
      return env.Undefined();
    }
    key_chunk_required_.store(false);
    // ...
  }
};
\`\`\`

### Internal State: [[codec saturated]]

Tracks backpressure from underlying codec:

\`\`\`
┌──────────────────────────────────────────────────────┐
│                    decode() called                    │
└──────────────────────────────────────────────────────┘
                           │
                           ▼
              ┌───────────────────────┐
              │ [[codec saturated]]?  │
              └───────────────────────┘
                    │           │
               true │           │ false
                    ▼           ▼
          ┌─────────────┐  ┌─────────────────────┐
          │ Return "not │  │ Queue to codec work │
          │ processed"  │  │ queue, decrement    │
          │             │  │ decodeQueueSize     │
          └─────────────┘  └─────────────────────┘
\`\`\`

### FFmpeg Implementation

See \`src/video_decoder.h\` and \`src/ffmpeg_raii.h\`:

\`\`\`cpp
#include <napi.h>
#include <queue>
#include "ffmpeg_raii.h"
#include "error_builder.h"

namespace webcodecs {

class VideoDecoder : public Napi::ObjectWrap<VideoDecoder> {
 private:
  // --- FFmpeg Resources (RAII managed) ---
  raii::AVCodecContextPtr codec_ctx_;

  // --- Thread-Safe State (from ffmpeg_raii.h) ---
  raii::AtomicCodecState state_;

  // --- Synchronization ---
  mutable std::mutex mutex_;

  // --- Decode Queue ---
  std::queue<raii::AVPacketPtr> decode_queue_;
  std::atomic<uint32_t> decode_queue_size_{0};

  // --- Key Chunk Tracking ---
  std::atomic<bool> key_chunk_required_{true};

  // --- Callbacks ---
  Napi::FunctionReference output_callback_;
  Napi::FunctionReference error_callback_;

  Napi::Value Configure(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (state_.IsClosed()) {
      webcodecs::errors::ThrowInvalidStateError(env, "Codec is closed");
      return env.Undefined();
    }

    // Transition to configured
    state_.transition(raii::AtomicCodecState::State::Unconfigured,
                      raii::AtomicCodecState::State::Configured);
    key_chunk_required_.store(true);

    // Queue configure on worker thread
    // ...
    return env.Undefined();
  }

  Napi::Value Decode(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (!state_.IsConfigured()) {
      webcodecs::errors::ThrowInvalidStateError(env, "Codec not configured");
      return env.Undefined();
    }

    if (key_chunk_required_.load() && chunk_type != "key") {
      webcodecs::errors::ThrowDataError(env, "Key frame required");
      return env.Undefined();
    }
    key_chunk_required_.store(false);

    decode_queue_size_++;

    // Queue decode work
    // ...
    return env.Undefined();
  }

  Napi::Value Reset(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (state_.IsClosed()) {
      webcodecs::errors::ThrowInvalidStateError(env, "Codec is closed");
      return env.Undefined();
    }

    // Clear queues
    {
      std::lock_guard<std::mutex> lock(mutex_);
      while (!decode_queue_.empty()) {
        decode_queue_.pop();
      }
    }

    // Reset decode queue size
    uint32_t prev_size = decode_queue_size_.exchange(0);
    if (prev_size > 0) {
      // Schedule dequeue event
    }

    return env.Undefined();
  }

  Napi::Value Close(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    // Force transition to closed
    state_.Close();

    // Release FFmpeg resources (RAII handles cleanup)
    codec_ctx_.reset();

    // Clear decode queue
    {
      std::lock_guard<std::mutex> lock(mutex_);
      while (!decode_queue_.empty()) {
        decode_queue_.pop();
      }
    }

    return env.Undefined();
  }
};

}  // namespace webcodecs
\`\`\`

### Dequeue Event

The \`dequeue\` event fires when \`decodeQueueSize\` or \`encodeQueueSize\` decreases:

\`\`\`cpp
// In video_decoder.cpp
void VideoDecoder::ScheduleDequeueEvent() {
  if (dequeue_event_scheduled_) {
    return;  // Prevent event spam
  }

  dequeue_event_scheduled_ = true;

  // Call ondequeue callback if set
  if (!ondequeue_callback_.IsEmpty()) {
    ondequeue_callback_.Call({});
  }
  dequeue_event_scheduled_ = false;
}
\`\`\`
`;
}

function generatePixelFormatMapping(): string {
  return `---
title: '17. Pixel Format Mapping'
---

> WebCodecs VideoPixelFormat to FFmpeg AVPixelFormat mapping

## VideoPixelFormat to AVPixelFormat Mapping

This document provides the mapping between WebCodecs \`VideoPixelFormat\` values and FFmpeg \`AVPixelFormat\` constants.

Consider adding this as \`src/shared/pixel_format.h\`.

### Complete Mapping Table

| WebCodecs Format | FFmpeg AVPixelFormat | Bits | Planes | Description |
|------------------|---------------------|------|--------|-------------|
| \`I420\` | \`AV_PIX_FMT_YUV420P\` | 8 | 3 | YUV 4:2:0 planar |
| \`I420A\` | \`AV_PIX_FMT_YUVA420P\` | 8 | 4 | YUV 4:2:0 planar + alpha |
| \`I422\` | \`AV_PIX_FMT_YUV422P\` | 8 | 3 | YUV 4:2:2 planar |
| \`I444\` | \`AV_PIX_FMT_YUV444P\` | 8 | 3 | YUV 4:4:4 planar |
| \`NV12\` | \`AV_PIX_FMT_NV12\` | 8 | 2 | YUV 4:2:0 semi-planar |
| \`RGBA\` | \`AV_PIX_FMT_RGBA\` | 8 | 1 | RGBA interleaved |
| \`RGBX\` | \`AV_PIX_FMT_RGB0\` | 8 | 1 | RGBX (alpha ignored) |
| \`BGRA\` | \`AV_PIX_FMT_BGRA\` | 8 | 1 | BGRA interleaved |
| \`BGRX\` | \`AV_PIX_FMT_BGR0\` | 8 | 1 | BGRX (alpha ignored) |
| \`I420P10\` | \`AV_PIX_FMT_YUV420P10LE\` | 10 | 3 | YUV 4:2:0 10-bit |
| \`I420P12\` | \`AV_PIX_FMT_YUV420P12LE\` | 12 | 3 | YUV 4:2:0 12-bit |
| \`I422P10\` | \`AV_PIX_FMT_YUV422P10LE\` | 10 | 3 | YUV 4:2:2 10-bit |
| \`I422P12\` | \`AV_PIX_FMT_YUV422P12LE\` | 12 | 3 | YUV 4:2:2 12-bit |
| \`I444P10\` | \`AV_PIX_FMT_YUV444P10LE\` | 10 | 3 | YUV 4:4:4 10-bit |
| \`I444P12\` | \`AV_PIX_FMT_YUV444P12LE\` | 12 | 3 | YUV 4:4:4 12-bit |
| \`NV12P10\` | \`AV_PIX_FMT_P010LE\` | 10 | 2 | YUV 4:2:0 10-bit semi-planar |
| \`RGBAP10\` | \`AV_PIX_FMT_RGBA64LE\` | 10/16 | 1 | RGBA 10-bit (in 16-bit container) |
| \`RGBAP12\` | \`AV_PIX_FMT_RGBA64LE\` | 12/16 | 1 | RGBA 12-bit (in 16-bit container) |

### C++ Implementation

Suggested file: \`src/shared/pixel_format.h\`

\`\`\`cpp
#pragma once

#include <string>
#include <unordered_map>

extern "C" {
#include <libavutil/pixfmt.h>
}

namespace webcodecs {

/**
 * Convert WebCodecs VideoPixelFormat string to FFmpeg AVPixelFormat.
 * Returns AV_PIX_FMT_NONE if format is not recognized.
 */
inline AVPixelFormat VideoPixelFormatToAv(const std::string& format) {
  static const std::unordered_map<std::string, AVPixelFormat> kFormatMap = {
    {"I420", AV_PIX_FMT_YUV420P},
    {"I420A", AV_PIX_FMT_YUVA420P},
    {"I422", AV_PIX_FMT_YUV422P},
    {"I444", AV_PIX_FMT_YUV444P},
    {"NV12", AV_PIX_FMT_NV12},
    {"RGBA", AV_PIX_FMT_RGBA},
    {"RGBX", AV_PIX_FMT_RGB0},
    {"BGRA", AV_PIX_FMT_BGRA},
    {"BGRX", AV_PIX_FMT_BGR0},
    {"I420P10", AV_PIX_FMT_YUV420P10LE},
    {"I420P12", AV_PIX_FMT_YUV420P12LE},
    {"I422P10", AV_PIX_FMT_YUV422P10LE},
    {"I422P12", AV_PIX_FMT_YUV422P12LE},
    {"I444P10", AV_PIX_FMT_YUV444P10LE},
    {"I444P12", AV_PIX_FMT_YUV444P12LE},
    {"NV12P10", AV_PIX_FMT_P010LE},
  };

  auto it = kFormatMap.find(format);
  if (it != kFormatMap.end()) {
    return it->second;
  }
  return AV_PIX_FMT_NONE;
}

/**
 * Convert FFmpeg AVPixelFormat to WebCodecs VideoPixelFormat string.
 * Returns empty string if format is not supported.
 */
inline std::string AvToVideoPixelFormat(AVPixelFormat format) {
  static const std::unordered_map<AVPixelFormat, std::string> kReverseMap = {
    {AV_PIX_FMT_YUV420P, "I420"},
    {AV_PIX_FMT_YUVA420P, "I420A"},
    {AV_PIX_FMT_YUV422P, "I422"},
    {AV_PIX_FMT_YUV444P, "I444"},
    {AV_PIX_FMT_NV12, "NV12"},
    {AV_PIX_FMT_RGBA, "RGBA"},
    {AV_PIX_FMT_RGB0, "RGBX"},
    {AV_PIX_FMT_BGRA, "BGRA"},
    {AV_PIX_FMT_BGR0, "BGRX"},
    {AV_PIX_FMT_YUV420P10LE, "I420P10"},
    {AV_PIX_FMT_YUV420P12LE, "I420P12"},
    {AV_PIX_FMT_YUV422P10LE, "I422P10"},
    {AV_PIX_FMT_YUV422P12LE, "I422P12"},
    {AV_PIX_FMT_YUV444P10LE, "I444P10"},
    {AV_PIX_FMT_YUV444P12LE, "I444P12"},
    {AV_PIX_FMT_P010LE, "NV12P10"},
  };

  auto it = kReverseMap.find(format);
  if (it != kReverseMap.end()) {
    return it->second;
  }
  return "";  // null in WebCodecs
}

}  // namespace webcodecs
\`\`\`

### Plane Layout

Each format has a specific plane layout:

| Format | Plane 0 | Plane 1 | Plane 2 | Plane 3 |
|--------|---------|---------|---------|---------|
| I420 | Y (full) | U (1/4) | V (1/4) | - |
| I420A | Y (full) | U (1/4) | V (1/4) | A (full) |
| NV12 | Y (full) | UV interleaved (1/2) | - | - |
| RGBA | RGBA interleaved | - | - | - |

### Stride Calculation

\`\`\`cpp
// In src/video_frame.cpp or src/shared/pixel_format.h

namespace webcodecs {

/**
 * Calculate stride for a given format, width, and plane index.
 */
inline size_t CalculateStride(const std::string& format, size_t width, int plane) {
  if (format == "I420" || format == "I420A") {
    // Y plane and Alpha plane have full width, U/V planes have half width
    return plane == 0 || plane == 3 ? width : (width + 1) / 2;
  }
  if (format == "NV12") {
    return width;  // Both planes have same stride
  }
  if (format == "RGBA" || format == "BGRA" || format == "RGBX" || format == "BGRX") {
    return width * 4;
  }
  // Handle 10/12-bit formats
  if (format.find("P10") != std::string::npos ||
      format.find("P12") != std::string::npos) {
    // 10/12-bit formats use 2 bytes per sample
    if (format.substr(0, 4) == "I420") {
      return plane == 0 ? width * 2 : (width + 1) / 2 * 2;
    }
  }
  return 0;  // Unknown format
}

}  // namespace webcodecs
\`\`\`
`;
}

function generateCodecStringRegistry(): string {
  return `---
title: '18. Codec String Registry'
---

> Codec string formats from [WebCodecs Codec Registry](https://www.w3.org/TR/webcodecs-codec-registry/)

## Codec String Format Reference

This document describes the codec string formats used in WebCodecs \`config.codec\` and how to parse them for FFmpeg codec selection.

Consider adding this as \`src/shared/codec_string.h\`.

### Video Codecs

#### AVC (H.264)

**Format:** \`avc1.PPCCLL\` or \`avc3.PPCCLL\`

| Part | Meaning | Example |
|------|---------|---------|
| \`avc1\` / \`avc3\` | Codec identifier | \`avc1\` (with in-band SPS/PPS) |
| \`PP\` | Profile (hex) | \`42\` = Baseline, \`4D\` = Main, \`64\` = High |
| \`CC\` | Constraint flags (hex) | \`00\`, \`40\`, etc. |
| \`LL\` | Level (hex) | \`1E\` = 3.0, \`1F\` = 3.1, \`28\` = 4.0 |

**Examples:**
- \`avc1.42001E\` - Baseline Profile, Level 3.0
- \`avc1.4D0028\` - Main Profile, Level 4.0
- \`avc1.640028\` - High Profile, Level 4.0

**FFmpeg Mapping** (suggested: \`src/shared/codec_string.h\`):
\`\`\`cpp
#pragma once

#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
}

#include "error_builder.h"

namespace webcodecs {

struct H264Config {
  int profile;      // FF_PROFILE_H264_*
  int level;        // Level * 10 (e.g., 30 for 3.0)
  int constraints;
};

/**
 * Parse AVC/H.264 codec string.
 * Throws TypeError if invalid.
 */
inline H264Config ParseAvcCodecString(Napi::Env env, const std::string& codec) {
  // "avc1.PPCCLL" or "avc3.PPCCLL"
  if (codec.size() < 11 ||
      (codec.substr(0, 4) != "avc1" && codec.substr(0, 4) != "avc3")) {
    errors::ThrowTypeError(env, "Invalid AVC codec string: " + codec);
    return {};
  }

  H264Config config;
  try {
    config.profile = std::stoi(codec.substr(5, 2), nullptr, 16);
    config.constraints = std::stoi(codec.substr(7, 2), nullptr, 16);
    config.level = std::stoi(codec.substr(9, 2), nullptr, 16);
  } catch (const std::exception&) {
    errors::ThrowTypeError(env, "Invalid AVC codec string: " + codec);
    return {};
  }

  return config;
}

/**
 * Map H.264 profile_idc to FFmpeg profile constant.
 */
inline int H264ProfileToFfmpeg(int profile_idc) {
  switch (profile_idc) {
    case 66: return FF_PROFILE_H264_BASELINE;
    case 77: return FF_PROFILE_H264_MAIN;
    case 88: return FF_PROFILE_H264_EXTENDED;
    case 100: return FF_PROFILE_H264_HIGH;
    case 110: return FF_PROFILE_H264_HIGH_10;
    case 122: return FF_PROFILE_H264_HIGH_422;
    case 244: return FF_PROFILE_H264_HIGH_444_PREDICTIVE;
    default: return FF_PROFILE_UNKNOWN;
  }
}

}  // namespace webcodecs
\`\`\`

---

#### HEVC (H.265)

**Format:** \`hev1.P.C.TLL.CC\` or \`hvc1.P.C.TLL.CC\`

| Part | Meaning |
|------|---------|
| \`P\` | General profile space + profile_idc |
| \`C\` | General profile compatibility flags (hex, 32-bit) |
| \`T\` | General tier flag (\`L\` = Main, \`H\` = High) |
| \`LL\` | Level (decimal, divided by 30) |
| \`CC\` | Constraint indicator flags |

**Examples:**
- \`hev1.1.6.L93.B0\` - Main Profile, Level 3.1
- \`hev1.2.4.L120.B0\` - Main 10 Profile, Level 4.0

---

#### VP8

**Format:** \`vp8\`

Simple codec string with no parameters.

---

#### VP9

**Format:** \`vp09.PP.LL.DD.CC.CP.TC.MC.FF\`

| Part | Meaning |
|------|---------|
| \`PP\` | Profile (00-03) |
| \`LL\` | Level (10-62) |
| \`DD\` | Bit depth (08, 10, 12) |
| \`CC\` | Chroma subsampling |
| \`CP\` | Color primaries |
| \`TC\` | Transfer characteristics |
| \`MC\` | Matrix coefficients |
| \`FF\` | Full range flag |

**Examples:**
- \`vp09.00.10.08\` - Profile 0, Level 1.0, 8-bit
- \`vp09.02.10.10\` - Profile 2, Level 1.0, 10-bit

---

#### AV1

**Format:** \`av01.P.LLT.DD.M.CCC.CP.TC.MC.F\`

| Part | Meaning |
|------|---------|
| \`P\` | Profile (0 = Main, 1 = High, 2 = Professional) |
| \`LL\` | Level (00-23) |
| \`T\` | Tier (M = Main, H = High) |
| \`DD\` | Bit depth (08, 10, 12) |
| \`M\` | Monochrome flag (0 or 1) |
| \`CCC\` | Chroma subsampling |
| \`CP\`, \`TC\`, \`MC\` | Color info |
| \`F\` | Full range flag |

**Examples:**
- \`av01.0.04M.08\` - Main Profile, Level 3.0, 8-bit
- \`av01.0.08M.10.0.110\` - Main Profile, Level 4.0, 10-bit

---

### Audio Codecs

#### AAC

**Format:** \`mp4a.40.X\`

| \`X\` | AAC Profile |
|-------|-------------|
| \`2\` | AAC-LC (Low Complexity) |
| \`5\` | HE-AAC (SBR) |
| \`29\` | HE-AAC v2 (SBR+PS) |

---

#### Opus

**Format:** \`opus\`

---

#### FLAC

**Format:** \`flac\`

---

#### MP3

**Format:** \`mp3\`

---

#### Vorbis

**Format:** \`vorbis\`

---

### Complete Codec Parser

Suggested file: \`src/shared/codec_string.h\`

\`\`\`cpp
#pragma once

#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace webcodecs {

struct CodecInfo {
  AVCodecID codec_id;
  int profile;
  int level;
  int bit_depth;
};

/**
 * Parse WebCodecs codec string to FFmpeg codec info.
 * Returns codec_id = AV_CODEC_ID_NONE if not recognized.
 */
inline CodecInfo ParseCodecString(const std::string& codec) {
  CodecInfo info = {AV_CODEC_ID_NONE, FF_PROFILE_UNKNOWN, 0, 8};

  // Video codecs
  if (codec.size() >= 4 &&
      (codec.substr(0, 4) == "avc1" || codec.substr(0, 4) == "avc3")) {
    info.codec_id = AV_CODEC_ID_H264;
  }
  else if (codec.size() >= 4 &&
           (codec.substr(0, 4) == "hev1" || codec.substr(0, 4) == "hvc1")) {
    info.codec_id = AV_CODEC_ID_HEVC;
  }
  else if (codec == "vp8") {
    info.codec_id = AV_CODEC_ID_VP8;
  }
  else if (codec.size() >= 4 && codec.substr(0, 4) == "vp09") {
    info.codec_id = AV_CODEC_ID_VP9;
  }
  else if (codec.size() >= 4 && codec.substr(0, 4) == "av01") {
    info.codec_id = AV_CODEC_ID_AV1;
  }
  // Audio codecs
  else if (codec.size() >= 7 && codec.substr(0, 7) == "mp4a.40") {
    info.codec_id = AV_CODEC_ID_AAC;
  }
  else if (codec == "opus") {
    info.codec_id = AV_CODEC_ID_OPUS;
  }
  else if (codec == "flac") {
    info.codec_id = AV_CODEC_ID_FLAC;
  }
  else if (codec == "mp3") {
    info.codec_id = AV_CODEC_ID_MP3;
  }
  else if (codec == "vorbis") {
    info.codec_id = AV_CODEC_ID_VORBIS;
  }

  return info;
}

}  // namespace webcodecs
\`\`\`
`;
}

function generateFFmpegNotes(): string {
  return `---
title: '19. FFmpeg Implementation Notes'
---

> Critical implementation details for FFmpeg-backed WebCodecs

## FFmpeg Implementation Notes

This document covers critical implementation details for building a WebCodecs-compliant API using FFmpeg.

See \`src/ffmpeg_raii.h\` and \`src/error_builder.h\` for core utilities.

### 1. Flush Semantics

**W3C Requirement:**
> "The underlying codec implementation MUST emit all outputs in response to a flush."

**FFmpeg Implementation** (see \`src/video_decoder.cpp\`):
\`\`\`cpp
#include "ffmpeg_raii.h"
#include "error_builder.h"

// In VideoDecoder async flush worker
void FlushInternal() {
  // Send NULL packet to trigger flush
  int ret = avcodec_send_packet(codec_ctx_.get(), nullptr);
  if (ret < 0 && ret != AVERROR_EOF) {
    // Handle error
    return;
  }

  // Drain all pending frames using RAII
  raii::AVFramePtr frame = raii::MakeAvFrame();
  while (true) {
    ret = avcodec_receive_frame(codec_ctx_.get(), frame.get());

    auto error_class = errors::ClassifyFfmpegError(ret);
    if (error_class == errors::FFmpegErrorClass::Again ||
        error_class == errors::FFmpegErrorClass::Eof) {
      break;  // No more frames
    }
    if (error_class == errors::FFmpegErrorClass::Error) {
      // Handle decode error
      return;
    }

    // Output the frame
    OutputFrame(frame.get());
    av_frame_unref(frame.get());
  }

  // Reset key chunk requirement
  key_chunk_required_.store(true);
}
\`\`\`

---

### 2. EAGAIN and EOF Handling

**CRITICAL:** \`AVERROR(EAGAIN)\` and \`AVERROR_EOF\` are NOT errors - they are normal state transitions.

See \`src/error_builder.h\` for \`ClassifyFfmpegError()\`:

\`\`\`cpp
#include "ffmpeg_raii.h"
#include "error_builder.h"

void DecodePacket(raii::AVPacketPtr& packet) {
  int ret = avcodec_send_packet(codec_ctx_.get(), packet.get());

  auto error_class = errors::ClassifyFfmpegError(ret);

  switch (error_class) {
    case errors::FFmpegErrorClass::Success:
      // Packet accepted
      break;

    case errors::FFmpegErrorClass::Again:
      // Codec needs to output frames before accepting more input
      DrainFrames();
      ret = avcodec_send_packet(codec_ctx_.get(), packet.get());
      break;

    case errors::FFmpegErrorClass::Eof:
      // Codec has been flushed - need to reset for new stream
      avcodec_flush_buffers(codec_ctx_.get());
      ret = avcodec_send_packet(codec_ctx_.get(), packet.get());
      break;

    case errors::FFmpegErrorClass::Error:
      // Actual error - close codec and invoke error callback
      errors::ThrowEncodingError(env_, ret, "Decode failed");
      state_.Close();
      break;
  }
}
\`\`\`

---

### 3. Output Order Guarantee

**W3C Requirement:** Outputs MUST be emitted in decode/encode order, not completion order.

\`\`\`cpp
// In src/shared/ or video_decoder.cpp

class OutputQueue {
  uint64_t next_sequence_ = 0;
  uint64_t next_output_ = 0;
  std::map<uint64_t, raii::AVFramePtr> pending_outputs_;
  std::mutex mutex_;

public:
  uint64_t AllocateSequence() { return next_sequence_++; }

  void EnqueueOutput(uint64_t sequence, raii::AVFramePtr frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_outputs_[sequence] = std::move(frame);
    FlushInOrder();
  }

private:
  void FlushInOrder() {
    while (pending_outputs_.count(next_output_)) {
      // Output frame via callback
      OutputCallback(std::move(pending_outputs_[next_output_]));
      pending_outputs_.erase(next_output_);
      next_output_++;
    }
  }
};
\`\`\`

---

### 4. Thread Safety

**W3C Requirement:** \`AVCodecContext\` must never be accessed concurrently.

See \`src/video_decoder.h\` for the pattern:

\`\`\`cpp
#include "ffmpeg_raii.h"

class VideoDecoder : public Napi::ObjectWrap<VideoDecoder> {
  // RAII-managed codec context
  raii::AVCodecContextPtr codec_ctx_;

  // Mutex protects codec context access
  mutable std::mutex mutex_;

  // Decode queue for async processing
  std::queue<raii::AVPacketPtr> decode_queue_;

  Napi::Value Decode(const Napi::CallbackInfo& info) {
    // Queue work - never access codec_ctx_ directly from JS thread
    {
      std::lock_guard<std::mutex> lock(mutex_);
      decode_queue_.push(std::move(packet));
    }
    // ... trigger async worker
  }
};
\`\`\`

---

### 5. RAII Resource Management

**Always use RAII wrappers** from \`src/ffmpeg_raii.h\`:

\`\`\`cpp
#include "ffmpeg_raii.h"

namespace webcodecs {

void Example() {
  // Use factory functions - never raw av_*_alloc()
  raii::AVFramePtr frame = raii::MakeAvFrame();
  raii::AVPacketPtr packet = raii::MakeAvPacket();
  raii::AVCodecContextPtr ctx = raii::MakeAvCodecContext(codec);

  // Check for allocation failure
  if (!frame || !packet || !ctx) {
    // Handle allocation failure
    return;
  }

  // Clone with reference counting
  raii::AVFramePtr frame_copy = raii::CloneAvFrame(frame.get());

  // All resources freed automatically on scope exit
}

}  // namespace webcodecs
\`\`\`

---

### 6. Configuration Deep Copy

**W3C Requirement:** Configurations must be deep-copied before async processing.

\`\`\`cpp
Napi::Value VideoDecoder::Configure(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // Parse config from JS object
  VideoDecoderConfig config = ParseConfig(info[0].As<Napi::Object>());

  // Config is copied by value into lambda capture
  // Queue async work with captured config copy
  QueueConfigureWork([this, config = std::move(config)]() {
    ConfigureInternal(config);
  });

  return env.Undefined();
}
\`\`\`

---

### 7. Backpressure (Saturation)

Implement backpressure to prevent memory exhaustion:

\`\`\`cpp
class VideoDecoder : public Napi::ObjectWrap<VideoDecoder> {
  static constexpr size_t kMaxPendingDecodes = 32;
  std::atomic<bool> codec_saturated_{false};
  std::atomic<uint32_t> decode_queue_size_{0};

  Napi::Value Decode(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (codec_saturated_.load()) {
      // Queue is full - caller should wait for dequeue event
      return env.Undefined();
    }

    uint32_t new_size = ++decode_queue_size_;
    if (new_size >= kMaxPendingDecodes) {
      codec_saturated_.store(true);
    }

    // Queue decode work...
    return env.Undefined();
  }
};
\`\`\`

---

### 8. VideoFrame Rotation and Flip

**W3C Requirement:** Encoders must verify all frames have the same orientation.

\`\`\`cpp
#include "error_builder.h"

class VideoEncoder : public Napi::ObjectWrap<VideoEncoder> {
  std::optional<std::pair<int, bool>> active_orientation_;

  Napi::Value Encode(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    int rotation = frame.rotation();
    bool flip = frame.flip();

    if (active_orientation_.has_value()) {
      auto [expected_rotation, expected_flip] = *active_orientation_;
      if (rotation != expected_rotation || flip != expected_flip) {
        errors::ThrowDataError(env,
            "Frame orientation must match first encoded frame");
        return env.Undefined();
      }
    } else {
      active_orientation_ = {rotation, flip};
    }

    // Continue with encode...
    return env.Undefined();
  }
};
\`\`\`

---

### 9. Error Callback Invocation Rules

| Event | Call error callback? |
|-------|---------------------|
| \`reset()\` | ❌ No |
| \`close()\` | ❌ No |
| Decode/encode error | ✅ Yes |
| Config not supported | ✅ Yes |
| Resource reclaimed | ✅ Yes |
`;
}

function generateAudioFormatMapping(): string {
  return `---
title: '20. Audio Sample Format Mapping'
---

> WebCodecs AudioSampleFormat to FFmpeg AVSampleFormat mapping

## AudioSampleFormat to AVSampleFormat Mapping

Consider adding this as \`src/shared/sample_format.h\`.

### Complete Mapping Table

| WebCodecs Format | FFmpeg AVSampleFormat | Bits | Layout | Description |
|------------------|----------------------|------|--------|-------------|
| \`u8\` | \`AV_SAMPLE_FMT_U8\` | 8 | Interleaved | Unsigned 8-bit |
| \`s16\` | \`AV_SAMPLE_FMT_S16\` | 16 | Interleaved | Signed 16-bit |
| \`s32\` | \`AV_SAMPLE_FMT_S32\` | 32 | Interleaved | Signed 32-bit |
| \`f32\` | \`AV_SAMPLE_FMT_FLT\` | 32 | Interleaved | 32-bit float |
| \`u8-planar\` | \`AV_SAMPLE_FMT_U8P\` | 8 | Planar | Unsigned 8-bit planar |
| \`s16-planar\` | \`AV_SAMPLE_FMT_S16P\` | 16 | Planar | Signed 16-bit planar |
| \`s32-planar\` | \`AV_SAMPLE_FMT_S32P\` | 32 | Planar | Signed 32-bit planar |
| \`f32-planar\` | \`AV_SAMPLE_FMT_FLTP\` | 32 | Planar | 32-bit float planar |

### C++ Implementation

Suggested file: \`src/shared/sample_format.h\`

\`\`\`cpp
#pragma once

#include <string>
#include <unordered_map>

extern "C" {
#include <libavutil/samplefmt.h>
}

namespace webcodecs {

/**
 * Convert WebCodecs AudioSampleFormat string to FFmpeg AVSampleFormat.
 * Returns AV_SAMPLE_FMT_NONE if format is not recognized.
 */
inline AVSampleFormat AudioSampleFormatToAv(const std::string& format) {
  static const std::unordered_map<std::string, AVSampleFormat> kFormatMap = {
    {"u8", AV_SAMPLE_FMT_U8},
    {"s16", AV_SAMPLE_FMT_S16},
    {"s32", AV_SAMPLE_FMT_S32},
    {"f32", AV_SAMPLE_FMT_FLT},
    {"u8-planar", AV_SAMPLE_FMT_U8P},
    {"s16-planar", AV_SAMPLE_FMT_S16P},
    {"s32-planar", AV_SAMPLE_FMT_S32P},
    {"f32-planar", AV_SAMPLE_FMT_FLTP},
  };

  auto it = kFormatMap.find(format);
  return it != kFormatMap.end() ? it->second : AV_SAMPLE_FMT_NONE;
}

/**
 * Convert FFmpeg AVSampleFormat to WebCodecs AudioSampleFormat string.
 * Returns empty string if format is not supported.
 */
inline std::string AvToAudioSampleFormat(AVSampleFormat format) {
  static const std::unordered_map<AVSampleFormat, std::string> kReverseMap = {
    {AV_SAMPLE_FMT_U8, "u8"},
    {AV_SAMPLE_FMT_S16, "s16"},
    {AV_SAMPLE_FMT_S32, "s32"},
    {AV_SAMPLE_FMT_FLT, "f32"},
    {AV_SAMPLE_FMT_U8P, "u8-planar"},
    {AV_SAMPLE_FMT_S16P, "s16-planar"},
    {AV_SAMPLE_FMT_S32P, "s32-planar"},
    {AV_SAMPLE_FMT_FLTP, "f32-planar"},
  };

  auto it = kReverseMap.find(format);
  return it != kReverseMap.end() ? it->second : "";
}

/**
 * Check if format is planar (separate channel buffers).
 */
inline bool IsPlanarFormat(const std::string& format) {
  return format.find("-planar") != std::string::npos;
}

/**
 * Get bytes per sample for a format.
 */
inline size_t BytesPerSample(const std::string& format) {
  if (format.size() >= 2 && format.substr(0, 2) == "u8") return 1;
  if (format.size() >= 3 && format.substr(0, 3) == "s16") return 2;
  if (format.size() >= 3 && format.substr(0, 3) == "s32") return 4;
  if (format.size() >= 3 && format.substr(0, 3) == "f32") return 4;
  return 0;
}

}  // namespace webcodecs
\`\`\`

### Sample Value Ranges

| Format | Min Value | Bias (Silence) | Max Value |
|--------|-----------|----------------|-----------|
| u8 | 0 | 128 | 255 |
| s16 | -32768 | 0 | 32767 |
| s32 | -2147483648 | 0 | 2147483647 |
| f32 | -1.0 | 0.0 | 1.0 |

### 24-bit Audio Handling

WebCodecs does not have a native 24-bit format. 24-bit audio should be stored in \`s32\` (left-shifted by 8 bits) or converted to \`f32\`.

### Conversion Requirements

**W3C Requirement:**
> "Conversion from any AudioSampleFormat to f32-planar MUST always be supported."

Use \`raii::SwrContextPtr\` from \`src/ffmpeg_raii.h\` for format conversion:

\`\`\`cpp
#include "ffmpeg_raii.h"

namespace webcodecs {

raii::SwrContextPtr CreateFormatConverter(
    AVSampleFormat src_fmt, int src_rate, const AVChannelLayout* src_layout,
    AVSampleFormat dst_fmt, int dst_rate, const AVChannelLayout* dst_layout) {

  return raii::MakeSwrContextInitialized(
      dst_layout, dst_fmt, dst_rate,
      src_layout, src_fmt, src_rate);
}

}  // namespace webcodecs
\`\`\`

### Memory Layout

**Interleaved (e.g., \`s16\`):**
\`\`\`
[L0][R0][L1][R1][L2][R2]...
\`\`\`

**Planar (e.g., \`s16-planar\`):**
\`\`\`
Plane 0 (Left):  [L0][L1][L2]...
Plane 1 (Right): [R0][R1][R2]...
\`\`\`
`;
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

  // Generate FFmpeg implementation documentation
  console.log('\nGenerating FFmpeg implementation documentation...');
  await generateFFmpegImplementationDocs();

  console.log('\nDone!');
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
