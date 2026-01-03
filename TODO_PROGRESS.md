# TODO Progress

This document tracks the progress of implementing W3C WebCodecs spec compliance.

## Completed

### VideoFrame.metadata() (2026-01-03)
**Commit:** `199d01a`

Implemented the `[[metadata]]` internal slot and `metadata()` method according to W3C WebCodecs spec sections 9.4.5 and 9.4.6.

Changes:
- Added `metadata_` member (`Napi::Reference<Napi::Object>`) to store VideoFrameMetadata dictionary
- Initialize metadata to empty object in constructor
- Copy metadata from `VideoFrameInit` if provided (structured clone)
- Implement `metadata()` to return a deep copy of stored metadata
- Throw `InvalidStateError` if called on closed frame
- Reset metadata reference on `close()` per spec algorithm

### AudioData Constructor with AudioDataInit (2026-01-03)
**Commit:** `50b0c2b`

Implemented the AudioData constructor that accepts an `AudioDataInit` object per W3C WebCodecs specification section 9.2.2.

Changes:
- Add `WebCodecsToAvFormat` helper for AudioSampleFormat conversion
- Fix `AvFormatToWebCodecs` to properly distinguish planar vs interleaved formats
- Implement full AudioDataInit validation per spec:
  - `sampleRate > 0`
  - `numberOfFrames > 0`
  - `numberOfChannels > 0`
  - data buffer has sufficient size
- Create AVFrame from provided raw audio data
- Support all 8 AudioSampleFormat values (u8, s16, s32, f32 + planar variants)
- Copy data correctly for both interleaved and planar formats

### VideoColorSpace (2026-01-03)
**Commit:** `e1b5bdb`

Implemented VideoColorSpace per W3C WebCodecs specification section 9.9.

Changes:
- Implement constructor with `VideoColorSpaceInit` dictionary
- Add internal slots: `[[primaries]]`, `[[transfer]]`, `[[matrix]]`, `[[full range]]`
- Implement all attribute getters returning nullable values
- Implement `toJSON()` method returning VideoColorSpaceInit dictionary
- Add factory methods `Create()` and `CreateFromInit()` for internal use
- Removed unused `void* handle_` member

## Previously Completed

### Transfer/Serialization Support
Implemented transfer and serialization support for all container types:
- VideoFrame: `serializeForTransfer(transfer: boolean)`
- AudioData: `serializeForTransfer(transfer: boolean)`
- EncodedVideoChunk: `serializeForTransfer()`
- EncodedAudioChunk: `serializeForTransfer()`

Key changes:
- Added `DataCloneError` support in `error_builder.h`
- Made `closed_` atomic for thread-safe detached state checking
- Transfer mode closes/detaches the source object
- Serialization mode creates a clone without closing source

### ImageDecoder P0 Crash Fix
Fixed race condition in TSFN callbacks that caused crashes when ImageDecoder was used.

### VideoFrame/AudioData Format Conversion
Implemented format conversion using libswscale for VideoFrame and libswresample for AudioData.

### Missing Pixel Formats
Added support for all 21 W3C WebCodecs pixel formats in `format_converter.h`.

## Remaining TODOs

### ImageDecoder ReadableStream Support
**Files:** `src/image_decoder.cpp:124,217`

The ImageDecoder constructor currently does not support ReadableStream input. Only static image data (ArrayBuffer/TypedArray) is supported.

### Spec Compliance Gaps
Run `npm run scaffold` to regenerate spec infrastructure and identify any remaining gaps.
