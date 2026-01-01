# N-API Foundation Design: WebCodecs Native Addon

## Overview

This document defines the foundational build configuration and C++ entry point for a Node.js native addon implementing the WebCodecs API backed by FFmpeg.

**Architecture:** 3-Layer Model
```
TypeScript Public API → N-API Bridge (C++) → FFmpeg (libav*)
```

## Design Decisions

### FFmpeg Path Configuration

**Decision:** Environment variable `FFMPEG_ROOT`

**Rationale:**
- Simple CI/CD integration (no command-line parsing)
- Works across all build systems (GitHub Actions, Jenkins, local)
- Clear error message if unset

**Usage:**
```bash
FFMPEG_ROOT=/usr/local/ffmpeg npm install
```

### FFmpeg Libraries

The addon links against:
| Library | Purpose |
|---------|---------|
| libavcodec | Core codec encoding/decoding |
| libavutil | Common utilities and data structures |
| libavformat | Container format parsing |
| libswscale | Pixel format conversion |

Link order matters for static linking: avformat → avcodec → swscale → avutil

### WebCodecs Class Stubs

Initial exports (all stubs for compile verification):
- `VideoDecoder` - Decodes EncodedVideoChunk → VideoFrame
- `VideoEncoder` - Encodes VideoFrame → EncodedVideoChunk
- `VideoFrame` - Raw frame container with buffer ownership
- `EncodedVideoChunk` - Compressed data container

### Thread Safety Architecture

**Pattern:** `Napi::TypedThreadSafeFunction` with RAII via Finalizer

**Why TypedThreadSafeFunction:**
- Compile-time type checking for callback data
- Cleaner API than untyped ThreadSafeFunction

**RAII Strategy:**
- Use finalizer callback for resource cleanup (joins worker threads)
- `AsyncDecodeContext` struct owns both TSFN and AVCodecContext
- Destructor releases TSFN and frees FFmpeg context

**Non-blocking decode flow:**
```
JS: decoder.decode(chunk)
  → C++: Queue chunk, return immediately
  → Worker: FFmpeg decodes, calls TSFN.NonBlockingCall(frame)
  → Main: TSFN callback wraps frame as VideoFrame, invokes output callback
```

## binding.gyp Specification

### Environment Variable

- `FFMPEG_ROOT` - Required. Path to FFmpeg installation containing:
  - `include/` - Headers (libavcodec/avcodec.h, etc.)
  - `lib/` - Static libraries (.a on Unix, .lib on Windows)

### OS-Specific Configuration

| OS | Library Extension | Additional Frameworks/Libs |
|----|------------------|---------------------------|
| macOS | `.a` | CoreFoundation, CoreMedia, VideoToolbox, Security, bz2, z, iconv |
| Linux | `.a` | pthread, dl, z, lzma |
| Windows | `.lib` | - |

### Compiler Settings

- N-API version: 8 (stable, supports Node 18+)
- C++ standard: C++17
- Exceptions: `NAPI_CPP_EXCEPTIONS` enabled

## addon.cpp Specification

### Module Registration

```cpp
NODE_API_MODULE(webcodecs, Init)
```

### Export Structure

```cpp
Napi::Object Init(Napi::Env env, Napi::Object exports) {
  VideoDecoderInit(env, exports);
  VideoEncoderInit(env, exports);
  VideoFrameInit(env, exports);
  EncodedVideoChunkInit(env, exports);
  return exports;
}
```

### AsyncDecodeContext (RAII)

```cpp
struct AsyncDecodeContext {
  using TSFN = Napi::TypedThreadSafeFunction<AsyncDecodeContext, AVFrame*, CallJs>;

  TSFN tsfn;
  AVCodecContext* codecCtx = nullptr;
  std::thread workerThread;

  ~AsyncDecodeContext() {
    if (tsfn) tsfn.Release();
    if (codecCtx) avcodec_free_context(&codecCtx);
    if (workerThread.joinable()) workerThread.join();
  }
};
```

The destructor is invoked by the TSFN finalizer when all threads have released.

## File Structure

```
node-webcodecs-spec/
├── binding.gyp
├── package.json
├── src/
│   ├── addon.cpp           # Module entry, Init function
│   ├── video_decoder.h     # VideoDecoder class stub
│   ├── video_decoder.cpp
│   ├── video_encoder.h     # VideoEncoder class stub
│   ├── video_encoder.cpp
│   ├── video_frame.h       # VideoFrame class stub
│   ├── video_frame.cpp
│   ├── encoded_video_chunk.h
│   └── encoded_video_chunk.cpp
└── docs/
    └── plans/
```

## References

- [Node.js C++ Addons](https://nodejs.org/api/addons.html)
- [node-addon-api ThreadSafeFunction](https://github.com/nodejs/node-addon-api/blob/main/doc/threadsafe_function.md)
- [node-addon-api TypedThreadSafeFunction](https://github.com/nodejs/node-addon-api/blob/main/doc/typed_threadsafe_function.md)
- [Thread-safe functions guide](https://nodejs.github.io/node-addon-examples/special-topics/thread-safe-functions/)
