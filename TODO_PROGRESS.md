# TODO Progress

This document tracks the progress of implementing W3C WebCodecs spec compliance.

## Completed

### VideoFrame Internal Slots per W3C Spec (2026-01-04)
**Commit:** `2af45e5`

Implemented WebCodecs internal slots for VideoFrame per W3C spec section 9.4.1 and constructor options per section 9.4.2.

Changes:
- Added internal slots to `video_frame.h`:
  - `rotation_` (0, 90, 180, 270 degrees per spec)
  - `flip_` (boolean for vertical flip)
  - `visible_left_`, `visible_top_`, `visible_width_`, `visible_height_` (visible rect)
  - `display_width_`, `display_height_` (display dimensions)
- Updated constructor to parse VideoFrameBufferInit options:
  - `visibleRect` (DOMRectInit) - validates bounds against coded dimensions (overflow-safe)
  - `rotation` (VideoRotation) - validates 0, 90, 180, or 270
  - `flip` (boolean) - sets flip state
  - `displayWidth`/`displayHeight` - stores custom display dimensions
  - `colorSpace` (VideoColorSpaceInit) - maps to AVFrame color fields
- Updated `CreateFromAVFrame()` factory to initialize internal slots:
  - Sets visible rect from AVFrame crop fields
  - Applies sample aspect ratio (SAR) correction to display width
  - Maintains backward compatibility with decoded frames
- Updated getters to use internal slots with proper null checks:
  - `GetVisibleRect()` returns visible rect or coded rect as fallback
  - `GetRotation()` returns rotation value (with closed/null frame checks)
  - `GetFlip()` returns flip state (with closed/null frame checks)
  - `GetDisplayWidth()`/`GetDisplayHeight()` account for rotation swap

### VideoFrame Clone Copies Internal Slots (2026-01-04)
**Commit:** `2417f2d`

Fixed `clone()`, `serializeForTransfer()` to properly copy internal slots.

Changes:
- Added `CloneFrom(VideoFrame*)` factory method that copies all internal slots
- Updated `clone()` to use CloneFrom instead of CreateFromAVFrame
- Updated `serializeForTransfer()` to use CloneFrom
- Copies rotation, flip, visible rect, display dimensions, and metadata

### VideoFrame(VideoFrame, init) Constructor (2026-01-04)
**Commit:** `c51ab16`

Implemented `VideoFrame(image, init?)` constructor overload for VideoFrame input.

Per W3C spec section 9.4.2 and "Initialize Frame From Other Frame" algorithm:
- Detects when first argument is a VideoFrame via InstanceOf check
- Clones AVFrame (refcounted) and copies all internal slots from source
- Supports optional `VideoFrameInit` parameter with overrides:
  - `timestamp`, `duration` - override source values
  - `visibleRect` - override visible rect with bounds validation
  - `rotation` - additive per "Add Rotations" algorithm (respects flip)
  - `flip` - XOR with source flip value
  - `displayWidth`, `displayHeight` - override display dimensions
  - `metadata` - override with new metadata dictionary

### VideoFrame Constructor Layout Option (2026-01-04)
**Commit:** `af60389`

Implemented `layout` option in VideoFrameBufferInit for custom plane layouts.

Per W3C spec section 9.4.2 and PlaneLayout interface (section 9.7):
- Added `CreateFrameFromBufferWithLayout()` to buffer_utils.h
- Parses `layout` array of PlaneLayout objects from constructor init
- Each PlaneLayout has required `offset` and `stride` properties
- Validates offsets are non-negative and strides are positive
- Validates plane data fits within source buffer bounds
- Handles chroma subsampling for multi-plane formats (I420, NV12, etc.)
- Falls back to tightly-packed default when layout not specified

### AudioData copyTo with Full Options Support (2026-01-04)
**Commit:** `c04a028`

Implemented full `AudioDataCopyToOptions` support per W3C WebCodecs spec sections 9.2.4 (methods) and 9.2.5 (algorithms).

Changes:
- Added `ComputeCopyElementCount()` helper implementing spec algorithm
- Updated `allocationSize(options)` to properly parse `AudioDataCopyToOptions`:
  - `planeIndex` (required) - validates for interleaved vs planar formats
  - `frameOffset` (optional, default 0) - validates against available frames
  - `frameCount` (optional) - validates against remaining frames after offset
  - `format` (optional) - calculates size for destination format
- Updated `copyTo(destination, options)` with full options support:
  - Validates destination buffer size against computed allocation
  - Handles frameOffset/frameCount for partial copies
  - Implements format conversion via libswresample
  - Supports interleaved <-> planar conversions
  - Supports sample type conversions (u8, s16, s32, f32)
- Added comprehensive test suite (29 tests):
  - Constructor tests for various formats
  - allocationSize with planeIndex, frameOffset, frameCount, format
  - copyTo with all option combinations
  - Format conversion tests (f32<->s16, interleaved<->planar)
  - Error handling (RangeError, InvalidStateError)

### ImageDecoder ReadableStream Support (2026-01-03)
**Commit:** `9d569d5`

Implemented ReadableStream support for ImageDecoder per W3C WebCodecs spec section 10.2.2 (constructor) and 10.2.5 (Fetch Stream Data Loop algorithm).

Changes:
- Added `IsReadableStream()` helper to detect WHATWG ReadableStream input
- Added `IsStreamDisturbedOrLocked()` to validate stream state per spec
- Added `ImageStreamDataMessage`, `ImageStreamEndMessage`, `ImageStreamErrorMessage` to control_message_queue.h
- Modified `ImageConfigureMessage` to support streaming mode (`is_streaming` flag)
- Implemented `StartStreamReadLoop()` and `ContinueStreamRead()` in ImageDecoder
  - Gets reader from stream via `getReader()`
  - Reads chunks asynchronously via `reader.read().then()`
  - Validates chunk is Uint8Array per spec
  - Sends `ImageStreamDataMessage` to worker for each chunk
  - Sends `ImageStreamEndMessage` when stream closes
  - Sends `ImageStreamErrorMessage` on stream error
- Modified `ImageDecoderWorker` to handle streaming:
  - `OnStreamData()`: Appends chunk to accumulated buffer, tries to configure decoder
  - `OnStreamEnd()`: Marks stream complete, signals completion
  - `OnStreamError()`: Signals error to decoder
  - `TryConfigureFromBuffer()`: Attempts to configure decoder from accumulated data
- Added streaming mode state tracking in ImageDecoderWorker
- Added tests for ReadableStream input:
  - Accept ReadableStream as data source
  - Reject locked ReadableStream
  - Handle stream completion

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

### Spec Compliance Gaps
Run `npm run specs` to regenerate spec infrastructure and identify any remaining gaps.

### VideoFrame
- Constructor from CanvasImageSource (not applicable in Node.js - browser-only)
- ~~Constructor from BufferSource with layout option~~ ✅ DONE
- ~~Constructor from VideoFrame with init overrides~~ ✅ DONE
- ~~Internal slots (rotation, flip, visibleRect, displayWidth/Height)~~ ✅ DONE
- 12 additional pixel formats (I420P10, I420P12, I422P10, I422P12, I444P10, I444P12, NV12P10, RGB565, RGBF16, BGRF16, RGBAF16, BGRAF16)
