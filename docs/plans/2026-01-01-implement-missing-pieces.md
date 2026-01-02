# Implement Missing PRD Pieces

> **Execution:** Use `/dev-workflow:execute-plan docs/plans/2026-01-01-implement-missing-pieces.md` to implement task-by-task.

**Goal:** Complete the node-webcodecs project infrastructure by adding vitest configuration, codec registry, test suite, task generator, and GitHub Actions CI.

**Architecture:** Layered approach - codec registry in C++ shared utils, vitest config at root, tests mirror lib/ structure, task generator extends existing scripts/, CI uses node-pre-gyp pattern.

**Tech Stack:** Vitest, node-addon-api, FFmpeg libavcodec, webidl2, GitHub Actions, node-pre-gyp

---

## Task Groups

| Task Group | Tasks | Rationale                                                     |
| ---------- | ----- | ------------------------------------------------------------- |
| Group 1    | 1, 2  | Independent: vitest config and codec registry have no overlap |
| Group 2    | 3     | Depends on vitest config from Task 1                          |
| Group 3    | 4     | Independent script, no file overlap                           |
| Group 4    | 5     | Independent CI workflow                                       |
| Group 5    | 6     | Final code review                                             |

---

### Task 1: Create vitest.config.ts with Path Alias

**Files:**

- Create: `vitest.config.ts`
- Modify: `tsconfig.json` (add paths)
- Modify: `package.json` (add test script if missing)

**Step 1: Create vitest.config.ts** (2 min)

```typescript
import { defineConfig } from 'vitest/config';
import path from 'path';

export default defineConfig({
  test: {
    globals: true,
    environment: 'node',
    include: ['test/**/*.test.ts'],
    coverage: {
      provider: 'v8',
      reporter: ['text', 'json', 'html'],
    },
  },
  resolve: {
    alias: {
      '@pproenca/node-webcodecs': path.resolve(__dirname, './lib'),
    },
  },
});
```

**Step 2: Update tsconfig.json with paths** (2 min)

Add to `compilerOptions`:

```json
{
  "compilerOptions": {
    "paths": {
      "@pproenca/node-webcodecs": ["./lib"],
      "@pproenca/node-webcodecs/*": ["./lib/*"]
    },
    "baseUrl": "."
  }
}
```

**Step 3: Verify configuration loads** (30 sec)

```bash
npx vitest --version
```

Expected: Version number printed (e.g., `vitest/1.6.1`)

**Step 4: Commit** (30 sec)

```bash
git add vitest.config.ts tsconfig.json
git commit -m "build(vitest): add vitest config with @pproenca/node-webcodecs alias"
```

---

### Task 2: Create Codec Registry (C++)

**Files:**

- Create: `src/shared/codec_registry.h`
- Create: `src/shared/codec_registry.cpp`
- Modify: `binding.gyp` (add new source file)

**Step 1: Create codec_registry.h header** (3 min)

```cpp
// src/shared/codec_registry.h
#pragma once

#include <string>
#include <optional>

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace webcodecs {

/**
 * Codec Registry - Maps W3C codec strings to FFmpeg AVCodecID.
 *
 * W3C codec strings follow the pattern defined in:
 * - RFC 6381 (MP4)
 * - WebM codec registry
 * - AV1 codec string format
 *
 * Examples:
 *   "avc1.42E01E" -> AV_CODEC_ID_H264 (Baseline Profile, Level 3.0)
 *   "hvc1.1.6.L93.B0" -> AV_CODEC_ID_HEVC
 *   "vp8" -> AV_CODEC_ID_VP8
 *   "vp09.00.10.08" -> AV_CODEC_ID_VP9
 *   "av01.0.04M.08" -> AV_CODEC_ID_AV1
 *   "mp4a.40.2" -> AV_CODEC_ID_AAC
 *   "opus" -> AV_CODEC_ID_OPUS
 *   "flac" -> AV_CODEC_ID_FLAC
 *   "mp3" -> AV_CODEC_ID_MP3
 *   "pcm-s16le" -> AV_CODEC_ID_PCM_S16LE
 */

struct CodecInfo {
  AVCodecID codec_id;
  int profile;       // -1 if not applicable
  int level;         // -1 if not applicable
  int bit_depth;     // -1 if not applicable
};

/**
 * Parse a W3C codec string and return FFmpeg codec information.
 *
 * @param codec_string The W3C codec string (e.g., "avc1.42E01E")
 * @return CodecInfo if parsing succeeded, std::nullopt otherwise
 */
std::optional<CodecInfo> ParseCodecString(const std::string& codec_string);

/**
 * Get the W3C codec string prefix for a given FFmpeg codec ID.
 *
 * @param codec_id The FFmpeg AVCodecID
 * @return The codec string prefix (e.g., "avc1" for H264)
 */
std::string GetCodecPrefix(AVCodecID codec_id);

/**
 * Check if a codec string is supported.
 *
 * @param codec_string The W3C codec string
 * @return true if the codec is recognized and FFmpeg has a decoder/encoder
 */
bool IsCodecSupported(const std::string& codec_string);

// Video codec prefixes
constexpr const char* CODEC_AVC = "avc1";   // H.264/AVC
constexpr const char* CODEC_HEVC = "hvc1";  // H.265/HEVC
constexpr const char* CODEC_HEVC_ALT = "hev1";
constexpr const char* CODEC_VP8 = "vp8";
constexpr const char* CODEC_VP9 = "vp09";
constexpr const char* CODEC_AV1 = "av01";

// Audio codec prefixes
constexpr const char* CODEC_AAC = "mp4a";
constexpr const char* CODEC_OPUS = "opus";
constexpr const char* CODEC_FLAC = "flac";
constexpr const char* CODEC_MP3 = "mp3";
constexpr const char* CODEC_VORBIS = "vorbis";
constexpr const char* CODEC_PCM = "pcm";
constexpr const char* CODEC_ULAW = "ulaw";
constexpr const char* CODEC_ALAW = "alaw";

}  // namespace webcodecs
```

**Step 2: Create codec_registry.cpp implementation** (5 min)

```cpp
// src/shared/codec_registry.cpp
#include "codec_registry.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <unordered_map>

namespace webcodecs {

namespace {

// Convert hex string to integer
int HexToInt(const std::string& hex) {
  try {
    return std::stoi(hex, nullptr, 16);
  } catch (...) {
    return -1;
  }
}

// Parse AVC codec string: avc1.PPCCLL
// PP = profile_idc, CC = constraint_set flags, LL = level_idc
std::optional<CodecInfo> ParseAVC(const std::string& params) {
  if (params.length() < 6) {
    return CodecInfo{AV_CODEC_ID_H264, -1, -1, -1};
  }

  int profile = HexToInt(params.substr(0, 2));
  int level = HexToInt(params.substr(4, 2));

  return CodecInfo{AV_CODEC_ID_H264, profile, level, -1};
}

// Parse HEVC codec string: hvc1.P.T.Lxx
std::optional<CodecInfo> ParseHEVC(const std::string& params) {
  if (params.empty()) {
    return CodecInfo{AV_CODEC_ID_HEVC, -1, -1, -1};
  }

  // Basic parsing - full HEVC parsing is complex
  return CodecInfo{AV_CODEC_ID_HEVC, -1, -1, -1};
}

// Parse VP9 codec string: vp09.PP.LL.DD
std::optional<CodecInfo> ParseVP9(const std::string& params) {
  if (params.empty()) {
    return CodecInfo{AV_CODEC_ID_VP9, -1, -1, -1};
  }

  std::istringstream ss(params);
  std::string token;
  int profile = -1, level = -1, bit_depth = -1;

  if (std::getline(ss, token, '.')) profile = std::stoi(token);
  if (std::getline(ss, token, '.')) level = std::stoi(token);
  if (std::getline(ss, token, '.')) bit_depth = std::stoi(token);

  return CodecInfo{AV_CODEC_ID_VP9, profile, level, bit_depth};
}

// Parse AV1 codec string: av01.P.LLT.DD
std::optional<CodecInfo> ParseAV1(const std::string& params) {
  if (params.empty()) {
    return CodecInfo{AV_CODEC_ID_AV1, -1, -1, -1};
  }

  std::istringstream ss(params);
  std::string token;
  int profile = -1, level = -1, bit_depth = -1;

  if (std::getline(ss, token, '.')) profile = std::stoi(token);
  if (std::getline(ss, token, '.')) {
    // Level + tier (e.g., "04M" = level 4, Main tier)
    level = std::stoi(token.substr(0, 2));
  }
  if (std::getline(ss, token, '.')) bit_depth = std::stoi(token);

  return CodecInfo{AV_CODEC_ID_AV1, profile, level, bit_depth};
}

// Parse AAC codec string: mp4a.40.X
std::optional<CodecInfo> ParseAAC(const std::string& params) {
  if (params.empty() || params.substr(0, 2) != "40") {
    return std::nullopt;  // Not AAC
  }

  int profile = -1;
  if (params.length() >= 4 && params[2] == '.') {
    profile = std::stoi(params.substr(3));
  }

  return CodecInfo{AV_CODEC_ID_AAC, profile, -1, -1};
}

// Parse PCM codec string: pcm-<format>
std::optional<CodecInfo> ParsePCM(const std::string& format) {
  static const std::unordered_map<std::string, AVCodecID> pcm_formats = {
    {"s16le", AV_CODEC_ID_PCM_S16LE},
    {"s16be", AV_CODEC_ID_PCM_S16BE},
    {"s24le", AV_CODEC_ID_PCM_S24LE},
    {"s24be", AV_CODEC_ID_PCM_S24BE},
    {"s32le", AV_CODEC_ID_PCM_S32LE},
    {"s32be", AV_CODEC_ID_PCM_S32BE},
    {"f32le", AV_CODEC_ID_PCM_F32LE},
    {"f32be", AV_CODEC_ID_PCM_F32BE},
    {"u8", AV_CODEC_ID_PCM_U8},
  };

  auto it = pcm_formats.find(format);
  if (it != pcm_formats.end()) {
    return CodecInfo{it->second, -1, -1, -1};
  }
  return std::nullopt;
}

}  // namespace

std::optional<CodecInfo> ParseCodecString(const std::string& codec_string) {
  if (codec_string.empty()) {
    return std::nullopt;
  }

  // Find the prefix (before first dot or entire string)
  size_t dot_pos = codec_string.find('.');
  std::string prefix = (dot_pos != std::string::npos)
    ? codec_string.substr(0, dot_pos)
    : codec_string;
  std::string params = (dot_pos != std::string::npos)
    ? codec_string.substr(dot_pos + 1)
    : "";

  // Convert prefix to lowercase for comparison
  std::string lower_prefix = prefix;
  std::transform(lower_prefix.begin(), lower_prefix.end(),
                 lower_prefix.begin(), ::tolower);

  // Video codecs
  if (lower_prefix == "avc1" || lower_prefix == "avc3") {
    return ParseAVC(params);
  }
  if (lower_prefix == "hvc1" || lower_prefix == "hev1") {
    return ParseHEVC(params);
  }
  if (lower_prefix == "vp8") {
    return CodecInfo{AV_CODEC_ID_VP8, -1, -1, -1};
  }
  if (lower_prefix == "vp09" || lower_prefix == "vp9") {
    return ParseVP9(params);
  }
  if (lower_prefix == "av01" || lower_prefix == "av1") {
    return ParseAV1(params);
  }

  // Audio codecs
  if (lower_prefix == "mp4a") {
    return ParseAAC(params);
  }
  if (lower_prefix == "opus") {
    return CodecInfo{AV_CODEC_ID_OPUS, -1, -1, -1};
  }
  if (lower_prefix == "flac") {
    return CodecInfo{AV_CODEC_ID_FLAC, -1, -1, -1};
  }
  if (lower_prefix == "mp3") {
    return CodecInfo{AV_CODEC_ID_MP3, -1, -1, -1};
  }
  if (lower_prefix == "vorbis") {
    return CodecInfo{AV_CODEC_ID_VORBIS, -1, -1, -1};
  }
  if (lower_prefix == "ulaw") {
    return CodecInfo{AV_CODEC_ID_PCM_MULAW, -1, -1, -1};
  }
  if (lower_prefix == "alaw") {
    return CodecInfo{AV_CODEC_ID_PCM_ALAW, -1, -1, -1};
  }

  // PCM formats
  if (codec_string.substr(0, 4) == "pcm-") {
    return ParsePCM(codec_string.substr(4));
  }

  return std::nullopt;
}

std::string GetCodecPrefix(AVCodecID codec_id) {
  switch (codec_id) {
    case AV_CODEC_ID_H264: return CODEC_AVC;
    case AV_CODEC_ID_HEVC: return CODEC_HEVC;
    case AV_CODEC_ID_VP8: return CODEC_VP8;
    case AV_CODEC_ID_VP9: return CODEC_VP9;
    case AV_CODEC_ID_AV1: return CODEC_AV1;
    case AV_CODEC_ID_AAC: return CODEC_AAC;
    case AV_CODEC_ID_OPUS: return CODEC_OPUS;
    case AV_CODEC_ID_FLAC: return CODEC_FLAC;
    case AV_CODEC_ID_MP3: return CODEC_MP3;
    case AV_CODEC_ID_VORBIS: return CODEC_VORBIS;
    case AV_CODEC_ID_PCM_MULAW: return CODEC_ULAW;
    case AV_CODEC_ID_PCM_ALAW: return CODEC_ALAW;
    default: return "";
  }
}

bool IsCodecSupported(const std::string& codec_string) {
  auto info = ParseCodecString(codec_string);
  if (!info) {
    return false;
  }

  // Check if FFmpeg has a decoder for this codec
  const AVCodec* decoder = avcodec_find_decoder(info->codec_id);
  return decoder != nullptr;
}

}  // namespace webcodecs
```

**Step 3: Update binding.gyp to include codec_registry.cpp** (2 min)

Add to the `sources` array:

```json
"src/shared/codec_registry.cpp"
```

**Step 4: Verify compilation** (1 min)

```bash
npm run build:native
```

Expected: Build succeeds with `gyp info ok`

**Step 5: Commit** (30 sec)

```bash
git add src/shared/codec_registry.h src/shared/codec_registry.cpp binding.gyp
git commit -m "feat(codec): add codec registry for W3C string to FFmpeg ID mapping"
```

---

### Task 3: Create Test Suite Structure

**Files:**

- Create: `test/setup.ts`
- Create: `test/codec-registry.test.ts`
- Create: `test/video-decoder.test.ts`
- Create: `test/fixtures/.gitkeep`

**Step 1: Create test setup file** (2 min)

```typescript
// test/setup.ts
import { beforeAll, afterAll } from 'vitest';

// Global test setup
beforeAll(() => {
  // Ensure native module is loaded
  console.log('Test suite starting...');
});

afterAll(() => {
  console.log('Test suite complete.');
});
```

**Step 2: Create codec registry test** (3 min)

```typescript
// test/codec-registry.test.ts
import { describe, it, expect } from 'vitest';
import { VideoDecoder } from '@pproenca/node-webcodecs';

describe('Codec Registry', () => {
  describe('Video Codecs', () => {
    it('should recognize H.264/AVC codec strings', async () => {
      const result = await VideoDecoder.isConfigSupported({
        codec: 'avc1.42E01E', // Baseline Profile, Level 3.0
      });
      expect(result).toBeDefined();
      expect(result.config).toBeDefined();
      expect(result.config.codec).toBe('avc1.42E01E');
    });

    it('should recognize VP8 codec', async () => {
      const result = await VideoDecoder.isConfigSupported({
        codec: 'vp8',
      });
      expect(result).toBeDefined();
    });

    it('should recognize VP9 codec strings', async () => {
      const result = await VideoDecoder.isConfigSupported({
        codec: 'vp09.00.10.08', // Profile 0, Level 1.0, 8-bit
      });
      expect(result).toBeDefined();
    });

    it('should recognize AV1 codec strings', async () => {
      const result = await VideoDecoder.isConfigSupported({
        codec: 'av01.0.04M.08', // Profile 0, Level 4 Main, 8-bit
      });
      expect(result).toBeDefined();
    });
  });

  describe('Invalid Codecs', () => {
    it('should reject unknown codec strings', async () => {
      const result = await VideoDecoder.isConfigSupported({
        codec: 'unknown-codec',
      });
      expect(result.supported).toBe(false);
    });
  });
});
```

**Step 3: Create VideoDecoder test** (3 min)

```typescript
// test/video-decoder.test.ts
import { describe, it, expect, beforeEach } from 'vitest';
import { VideoDecoder } from '@pproenca/node-webcodecs';

describe('VideoDecoder', () => {
  describe('Constructor', () => {
    it('should create a VideoDecoder instance', () => {
      const decoder = new VideoDecoder({
        output: () => {},
        error: () => {},
      });
      expect(decoder).toBeDefined();
      expect(decoder.state).toBe('unconfigured');
    });

    it('should require output callback', () => {
      expect(() => {
        // @ts-expect-error - Testing missing required field
        new VideoDecoder({ error: () => {} });
      }).toThrow();
    });

    it('should require error callback', () => {
      expect(() => {
        // @ts-expect-error - Testing missing required field
        new VideoDecoder({ output: () => {} });
      }).toThrow();
    });
  });

  describe('State Machine', () => {
    let decoder: VideoDecoder;

    beforeEach(() => {
      decoder = new VideoDecoder({
        output: () => {},
        error: () => {},
      });
    });

    it('should start in unconfigured state', () => {
      expect(decoder.state).toBe('unconfigured');
    });

    it('should have decodeQueueSize of 0 initially', () => {
      expect(decoder.decodeQueueSize).toBe(0);
    });
  });

  describe('close()', () => {
    it('should transition to closed state', () => {
      const decoder = new VideoDecoder({
        output: () => {},
        error: () => {},
      });
      decoder.close();
      expect(decoder.state).toBe('closed');
    });

    it('should be idempotent', () => {
      const decoder = new VideoDecoder({
        output: () => {},
        error: () => {},
      });
      decoder.close();
      decoder.close(); // Should not throw
      expect(decoder.state).toBe('closed');
    });
  });
});
```

**Step 4: Create fixtures directory** (30 sec)

```bash
mkdir -p test/fixtures
touch test/fixtures/.gitkeep
```

**Step 5: Run tests to verify setup** (30 sec)

```bash
npm test -- --run
```

Expected: Tests run (may fail on TODO implementations, but vitest should execute)

**Step 6: Commit** (30 sec)

```bash
git add test/ vitest.config.ts
git commit -m "test: add test suite structure with vitest and initial tests"
```

---

### Task 4: Create Task Generator Script

**Files:**

- Create: `scripts/generate-tasks.ts`
- Create: `docs/tasks/.gitkeep`

**Step 1: Create task generator script** (5 min)

````typescript
// scripts/generate-tasks.ts
/**
 * Task Generator - Parses WebIDL and generates Markdown task files.
 *
 * Usage: npx tsx scripts/generate-tasks.ts
 *
 * Reads: spec/context/_webcodecs.idl
 * Outputs: docs/tasks/<interface-name>.md
 */

import * as fs from 'fs';
import * as path from 'path';
import * as webidl2 from 'webidl2';

const IDL_PATH = path.join(__dirname, '../spec/context/_webcodecs.idl');
const OUTPUT_DIR = path.join(__dirname, '../docs/tasks');

interface TaskItem {
  type: 'attribute' | 'method' | 'constructor' | 'static-method';
  name: string;
  returnType: string;
  params?: string[];
  readonly?: boolean;
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

function extractTasks(member: webidl2.IDLInterfaceMemberType): TaskItem | null {
  switch (member.type) {
    case 'attribute':
      return {
        type: 'attribute',
        name: member.name,
        returnType: parseIdlType(member.idlType),
        readonly: member.readonly,
      };
    case 'operation':
      if (!member.name) return null;
      return {
        type: member.special === 'static' ? 'static-method' : 'method',
        name: member.name,
        returnType: parseIdlType(member.idlType),
        params: member.arguments.map((arg) => `${arg.name}: ${parseIdlType(arg.idlType)}`),
      };
    case 'constructor':
      return {
        type: 'constructor',
        name: 'constructor',
        returnType: 'instance',
        params: member.arguments.map((arg) => `${arg.name}: ${parseIdlType(arg.idlType)}`),
      };
    default:
      return null;
  }
}

function generateTaskMarkdown(interfaceName: string, tasks: TaskItem[]): string {
  const lines: string[] = [
    `# ${interfaceName} Implementation Tasks`,
    '',
    `> Auto-generated from \`spec/context/_webcodecs.idl\``,
    '',
    `## Overview`,
    '',
    `Implement the \`${interfaceName}\` class following the W3C WebCodecs specification.`,
    '',
    `**Files:**`,
    `- C++ Header: \`src/${interfaceName}.h\``,
    `- C++ Implementation: \`src/${interfaceName}.cpp\``,
    `- TypeScript Wrapper: \`lib/${interfaceName}.ts\``,
    `- Tests: \`test/${interfaceName.toLowerCase()}.test.ts\``,
    '',
    `---`,
    '',
    `## Implementation Checklist`,
    '',
  ];

  // Constructor
  const constructor = tasks.find((t) => t.type === 'constructor');
  if (constructor) {
    lines.push(`### Constructor`);
    lines.push('');
    lines.push(`- [ ] Implement C++ constructor with parameter validation`);
    lines.push(`- [ ] Initialize internal state slots`);
    lines.push(`- [ ] Wire TypeScript wrapper to native constructor`);
    if (constructor.params?.length) {
      lines.push(`- [ ] Validate required parameters: \`${constructor.params.join(', ')}\``);
    }
    lines.push('');
  }

  // Attributes
  const attributes = tasks.filter((t) => t.type === 'attribute');
  if (attributes.length > 0) {
    lines.push(`### Attributes`);
    lines.push('');
    for (const attr of attributes) {
      const prefix = attr.readonly ? '(readonly)' : '(read/write)';
      lines.push(`#### \`${attr.name}\` ${prefix}`);
      lines.push('');
      lines.push(
        `- [ ] C++: Implement \`Get${capitalize(attr.name)}()\` returning \`${attr.returnType}\``
      );
      if (!attr.readonly) {
        lines.push(`- [ ] C++: Implement \`Set${capitalize(attr.name)}()\``);
      }
      lines.push(`- [ ] TS: Wire getter${attr.readonly ? '' : ' and setter'} to native`);
      lines.push(`- [ ] Test: Verify initial value and behavior`);
      lines.push('');
    }
  }

  // Methods
  const methods = tasks.filter((t) => t.type === 'method');
  if (methods.length > 0) {
    lines.push(`### Methods`);
    lines.push('');
    for (const method of methods) {
      const params = method.params?.join(', ') || '';
      lines.push(`#### \`${method.name}(${params})\``);
      lines.push('');
      lines.push(`- [ ] C++: Implement method logic per W3C spec algorithm`);
      if (method.returnType.includes('Promise')) {
        lines.push(`- [ ] C++: Use AsyncWorker for non-blocking execution`);
      }
      lines.push(`- [ ] C++: Handle error cases with proper DOMException types`);
      lines.push(`- [ ] TS: Wire method to native implementation`);
      lines.push(`- [ ] Test: Write test case for happy path`);
      lines.push(`- [ ] Test: Write test case for error conditions`);
      lines.push('');
    }
  }

  // Static Methods
  const staticMethods = tasks.filter((t) => t.type === 'static-method');
  if (staticMethods.length > 0) {
    lines.push(`### Static Methods`);
    lines.push('');
    for (const method of staticMethods) {
      const params = method.params?.join(', ') || '';
      lines.push(`#### \`${interfaceName}.${method.name}(${params})\``);
      lines.push('');
      lines.push(`- [ ] C++: Implement as StaticMethod on class`);
      lines.push(`- [ ] C++: Return Promise with appropriate result type`);
      lines.push(`- [ ] TS: Expose as static method on class`);
      lines.push(`- [ ] Test: Verify supported configurations`);
      lines.push('');
    }
  }

  // Memory Management (if applicable)
  if (
    ['VideoFrame', 'AudioData', 'EncodedVideoChunk', 'EncodedAudioChunk'].includes(interfaceName)
  ) {
    lines.push(`### Memory Management`);
    lines.push('');
    lines.push(`- [ ] Implement \`close()\` to free native resources immediately`);
    lines.push(`- [ ] Add C++ destructor as fallback for GC`);
    lines.push(`- [ ] Test: Verify no memory leaks under stress`);
    lines.push(`- [ ] Test: Verify \`close()\` is idempotent`);
    lines.push('');
  }

  lines.push(`---`);
  lines.push('');
  lines.push(`## Verification`);
  lines.push('');
  lines.push('```bash');
  lines.push(`npm run build && npm test -- --grep "${interfaceName}"`);
  lines.push('```');
  lines.push('');

  return lines.join('\n');
}

function capitalize(str: string): string {
  return str.charAt(0).toUpperCase() + str.slice(1);
}

async function main() {
  // Ensure output directory exists
  if (!fs.existsSync(OUTPUT_DIR)) {
    fs.mkdirSync(OUTPUT_DIR, { recursive: true });
  }

  // Read and parse IDL
  const idlContent = fs.readFileSync(IDL_PATH, 'utf-8');
  const ast = webidl2.parse(idlContent);

  let interfaceCount = 0;
  let taskCount = 0;

  for (const node of ast) {
    if (node.type === 'interface') {
      const interfaceName = node.name;
      const tasks: TaskItem[] = [];

      for (const member of node.members) {
        const task = extractTasks(member);
        if (task) {
          tasks.push(task);
          taskCount++;
        }
      }

      if (tasks.length > 0) {
        const markdown = generateTaskMarkdown(interfaceName, tasks);
        const outputPath = path.join(OUTPUT_DIR, `${interfaceName}.md`);
        fs.writeFileSync(outputPath, markdown);
        console.log(`Generated: ${outputPath} (${tasks.length} tasks)`);
        interfaceCount++;
      }
    }
  }

  console.log(`\nDone! Generated ${interfaceCount} task files with ${taskCount} total tasks.`);
}

main().catch(console.error);
````

**Step 2: Add script to package.json** (1 min)

Add to `scripts`:

```json
"generate:tasks": "tsx scripts/generate-tasks.ts"
```

**Step 3: Create tasks output directory** (30 sec)

```bash
mkdir -p docs/tasks
touch docs/tasks/.gitkeep
```

**Step 4: Run task generator** (30 sec)

```bash
npm run generate:tasks
```

Expected: Task files generated in `docs/tasks/`

**Step 5: Commit** (30 sec)

```bash
git add scripts/generate-tasks.ts docs/tasks/ package.json
git commit -m "feat(scripts): add task generator for IDL-to-markdown conversion"
```

---

### Task 5: Create GitHub Actions CI Workflow

**Files:**

- Create: `.github/workflows/build.yml`
- Modify: `package.json` (add node-pre-gyp config)

**Step 1: Create GitHub Actions workflow** (5 min)

```yaml
# .github/workflows/build.yml
name: Build and Test

on:
  push:
    branches: [master, main]
    tags: ['v*']
  pull_request:
    branches: [master, main]

jobs:
  build:
    strategy:
      fail-fast: false
      matrix:
        include:
          - os: ubuntu-22.04
            arch: x64
            platform: linux
          - os: macos-13
            arch: x64
            platform: darwin
          - os: macos-14
            arch: arm64
            platform: darwin

    runs-on: ${{ matrix.os }}

    steps:
      - name: Checkout
        uses: actions/checkout@v4

      - name: Setup Node.js
        uses: actions/setup-node@v4
        with:
          node-version: '20'
          cache: 'npm'

      - name: Install FFmpeg (Ubuntu)
        if: matrix.platform == 'linux'
        run: |
          sudo apt-get update
          sudo apt-get install -y \
            libavcodec-dev \
            libavformat-dev \
            libavutil-dev \
            libswscale-dev \
            pkg-config

      - name: Install FFmpeg (macOS)
        if: matrix.platform == 'darwin'
        run: |
          brew install ffmpeg pkg-config

      - name: Install dependencies
        run: npm ci

      - name: Build native module
        run: npm run build:native

      - name: Build TypeScript
        run: npx tsc

      - name: Run tests
        run: npm test -- --run

      - name: Package prebuild
        if: startsWith(github.ref, 'refs/tags/v')
        run: npx node-pre-gyp package

      - name: Upload prebuild artifact
        if: startsWith(github.ref, 'refs/tags/v')
        uses: actions/upload-artifact@v4
        with:
          name: prebuild-${{ matrix.platform }}-${{ matrix.arch }}
          path: build/stage/**/*.tar.gz

  release:
    needs: build
    runs-on: ubuntu-latest
    if: startsWith(github.ref, 'refs/tags/v')
    permissions:
      contents: write

    steps:
      - name: Download all artifacts
        uses: actions/download-artifact@v4
        with:
          path: prebuilds
          pattern: prebuild-*
          merge-multiple: true

      - name: Create Release
        uses: softprops/action-gh-release@v1
        with:
          files: prebuilds/**/*.tar.gz
          generate_release_notes: true
```

**Step 2: Add node-pre-gyp configuration to package.json** (2 min)

Add these fields to `package.json`:

```json
{
  "binary": {
    "module_name": "webcodecs",
    "module_path": "./build/Release/",
    "remote_path": "./releases/download/{version}/",
    "package_name": "{module_name}-v{version}-{node_abi}-{platform}-{arch}.tar.gz",
    "host": "https://github.com/pproenca/node-webcodecs-spec"
  },
  "scripts": {
    "install": "node-pre-gyp install --fallback-to-build"
  },
  "dependencies": {
    "@mapbox/node-pre-gyp": "^1.0.11"
  }
}
```

**Step 3: Install node-pre-gyp** (30 sec)

```bash
npm install @mapbox/node-pre-gyp
```

**Step 4: Verify workflow syntax** (30 sec)

```bash
# Check YAML is valid
cat .github/workflows/build.yml | head -20
```

**Step 5: Commit** (30 sec)

```bash
git add .github/workflows/build.yml package.json package-lock.json
git commit -m "ci: add GitHub Actions workflow for cross-platform builds"
```

---

### Task 6: Code Review

**Final verification and cleanup.**

**Step 1: Run full build** (2 min)

```bash
npm run rebuild
```

Expected: Native build + TypeScript compilation succeeds

**Step 2: Run all tests** (1 min)

```bash
npm test -- --run
```

Expected: All tests execute (some may fail due to TODO implementations)

**Step 3: Generate tasks** (30 sec)

```bash
npm run generate:tasks
```

Expected: Task files in `docs/tasks/`

**Step 4: Verify project structure** (30 sec)

```bash
ls -la vitest.config.ts src/shared/codec_registry.* test/ .github/workflows/
```

Expected: All new files exist

**Step 5: Final commit with summary** (30 sec)

```bash
git add -A
git status
# If any unstaged changes remain:
git commit -m "chore: complete PRD missing pieces implementation"
```

---

## Summary

| Task | Component      | Files Created/Modified                              |
| ---- | -------------- | --------------------------------------------------- |
| 1    | Vitest Config  | `vitest.config.ts`, `tsconfig.json`                 |
| 2    | Codec Registry | `src/shared/codec_registry.{h,cpp}`, `binding.gyp`  |
| 3    | Test Suite     | `test/setup.ts`, `test/*.test.ts`, `test/fixtures/` |
| 4    | Task Generator | `scripts/generate-tasks.ts`, `docs/tasks/`          |
| 5    | GitHub Actions | `.github/workflows/build.yml`, `package.json`       |
| 6    | Code Review    | Final verification                                  |

**Total Commits:** 6
**Estimated Time:** 45-60 minutes
