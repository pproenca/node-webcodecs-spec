# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Server-side WebCodecs implementation for Node.js backed by FFmpeg. Brings the W3C WebCodecs API to Node.js through native C++ addons wrapping FFmpeg libraries.

**Key Components:**
- Video/Audio encoding and decoding (VideoEncoder, VideoDecoder, AudioEncoder, AudioDecoder)
- Image decoding (ImageDecoder)
- Data containers (VideoFrame, AudioData, EncodedVideoChunk, EncodedAudioChunk)
- Auto-generated WebCodecs type definitions from W3C spec

## Build Commands

```bash
npm run build           # Full build: native C++ + TypeScript + copy types
npm run build:native    # Build C++ addon only (node-gyp)
npm run clean           # Clean build artifacts
npm run rebuild         # Clean + full build
```

**Requirements:** FFmpeg 5.0+ (libavcodec 59+), Node.js 18+, C++17 compiler

## Testing

```bash
npm test                  # Run JS/TS tests (Vitest)
npm run test:native       # Run C++ tests (CMake)
npm run test:native:tsan  # C++ tests with ThreadSanitizer
npm run test:native:asan  # C++ tests with AddressSanitizer
npm run test:native:ubsan # C++ tests with UndefinedBehaviorSanitizer
npm run test:all          # All tests (C++ + JS)
```

Test files: `test/**/*.test.ts`, C++ tests in `test/cpp/`

## Code Formatting

```bash
npm run format        # Format with Prettier
npm run format:check  # Check formatting
```

## Architecture

**Hybrid TypeScript + C++ Design:**

```
lib/                    TypeScript wrappers around native bindings
  └─ VideoDecoder.ts    Load native addon via bindings, implement WebCodecs interfaces

src/                    C++ native addon (Node-API / NAPI)
  ├─ addon.cpp          Module initialization, exports all classes
  ├─ VideoDecoder.cpp   FFmpeg decoder integration
  ├─ ffmpeg_raii.h      RAII wrappers (AVFramePtr, AVPacketPtr, etc.)
  ├─ error_builder.h    Spec-compliant DOMException builders
  └─ shared/            Utilities (frame_pool, packet_pool, safe_tsfn, etc.)

types/
  └─ webcodecs.d.ts     Complete W3C WebCodecs type definitions
```

**Key Patterns:**

1. **RAII for FFmpeg Resources**: All FFmpeg structs wrapped in `std::unique_ptr` with custom deleters. Use wrappers from `ffmpeg_raii.h`: `ffmpeg::make_frame()`, `ffmpeg::make_packet()`, `ffmpeg::make_codec_context()`.

2. **Threading Model**: `AVCodecContext` never accessed concurrently between JS main thread and AsyncWorker threads. Long operations use Napi::AsyncWorker.

3. **Error Handling**: C++ uses `error_builder.h` for spec-compliant errors. Always handle `AVERROR(EAGAIN)` and `AVERROR_EOF` as state transitions, not errors.

## C++ Development Guidelines

When writing C++ code that interfaces with FFmpeg, the **ffmpeg-cpp-sentinel** agent should be used. Key rules:

- **NEVER** use raw pointers for ownership
- **NEVER** ignore FFmpeg return values
- **ALWAYS** use RAII wrappers from `ffmpeg_raii.h`
- **ALWAYS** match allocators with deallocators (`av_frame_free` for `av_frame_alloc`, etc.)
- **ALWAYS** flush codec at stream end (send nullptr packet)

## Scripts

```bash
npm run scaffold    # Generate WebCodecs spec infrastructure from W3C spec
npm run issues      # Generate GitHub issues from spec
```
