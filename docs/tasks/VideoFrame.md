# VideoFrame Implementation Tasks

> Auto-generated from `spec/context/_webcodecs.idl`

## Overview

Implement the `VideoFrame` class following the W3C WebCodecs specification.

**Files:**
- C++ Header: `src/VideoFrame.h`
- C++ Implementation: `src/VideoFrame.cpp`
- TypeScript Wrapper: `lib/VideoFrame.ts`
- Tests: `test/videoframe.test.ts`

---

## Implementation Checklist

### Constructor

- [ ] Implement C++ constructor with parameter validation
- [ ] Initialize internal state slots
- [ ] Wire TypeScript wrapper to native constructor
- [ ] Validate required parameters: `image: CanvasImageSource, init: VideoFrameInit`

### Attributes

#### `format` (readonly)

- [ ] C++: Implement `GetFormat()` returning `VideoPixelFormat`
- [ ] TS: Wire getter to native
- [ ] Test: Verify initial value and behavior

#### `codedWidth` (readonly)

- [ ] C++: Implement `GetCodedWidth()` returning `unsigned long`
- [ ] TS: Wire getter to native
- [ ] Test: Verify initial value and behavior

#### `codedHeight` (readonly)

- [ ] C++: Implement `GetCodedHeight()` returning `unsigned long`
- [ ] TS: Wire getter to native
- [ ] Test: Verify initial value and behavior

#### `codedRect` (readonly)

- [ ] C++: Implement `GetCodedRect()` returning `DOMRectReadOnly`
- [ ] TS: Wire getter to native
- [ ] Test: Verify initial value and behavior

#### `visibleRect` (readonly)

- [ ] C++: Implement `GetVisibleRect()` returning `DOMRectReadOnly`
- [ ] TS: Wire getter to native
- [ ] Test: Verify initial value and behavior

#### `rotation` (readonly)

- [ ] C++: Implement `GetRotation()` returning `double`
- [ ] TS: Wire getter to native
- [ ] Test: Verify initial value and behavior

#### `flip` (readonly)

- [ ] C++: Implement `GetFlip()` returning `boolean`
- [ ] TS: Wire getter to native
- [ ] Test: Verify initial value and behavior

#### `displayWidth` (readonly)

- [ ] C++: Implement `GetDisplayWidth()` returning `unsigned long`
- [ ] TS: Wire getter to native
- [ ] Test: Verify initial value and behavior

#### `displayHeight` (readonly)

- [ ] C++: Implement `GetDisplayHeight()` returning `unsigned long`
- [ ] TS: Wire getter to native
- [ ] Test: Verify initial value and behavior

#### `duration` (readonly)

- [ ] C++: Implement `GetDuration()` returning `unsigned long long`
- [ ] TS: Wire getter to native
- [ ] Test: Verify initial value and behavior

#### `timestamp` (readonly)

- [ ] C++: Implement `GetTimestamp()` returning `long long`
- [ ] TS: Wire getter to native
- [ ] Test: Verify initial value and behavior

#### `colorSpace` (readonly)

- [ ] C++: Implement `GetColorSpace()` returning `VideoColorSpace`
- [ ] TS: Wire getter to native
- [ ] Test: Verify initial value and behavior

### Methods

#### `metadata()`

- [ ] C++: Implement method logic per W3C spec algorithm
- [ ] C++: Handle error cases with proper DOMException types
- [ ] TS: Wire method to native implementation
- [ ] Test: Write test case for happy path
- [ ] Test: Write test case for error conditions

#### `allocationSize(options: VideoFrameCopyToOptions)`

- [ ] C++: Implement method logic per W3C spec algorithm
- [ ] C++: Handle error cases with proper DOMException types
- [ ] TS: Wire method to native implementation
- [ ] Test: Write test case for happy path
- [ ] Test: Write test case for error conditions

#### `copyTo(destination: AllowSharedBufferSource, options: VideoFrameCopyToOptions)`

- [ ] C++: Implement method logic per W3C spec algorithm
- [ ] C++: Handle error cases with proper DOMException types
- [ ] TS: Wire method to native implementation
- [ ] Test: Write test case for happy path
- [ ] Test: Write test case for error conditions

#### `clone()`

- [ ] C++: Implement method logic per W3C spec algorithm
- [ ] C++: Handle error cases with proper DOMException types
- [ ] TS: Wire method to native implementation
- [ ] Test: Write test case for happy path
- [ ] Test: Write test case for error conditions

#### `close()`

- [ ] C++: Implement method logic per W3C spec algorithm
- [ ] C++: Handle error cases with proper DOMException types
- [ ] TS: Wire method to native implementation
- [ ] Test: Write test case for happy path
- [ ] Test: Write test case for error conditions

### Memory Management

- [ ] Implement `close()` to free native resources immediately
- [ ] Add C++ destructor as fallback for GC
- [ ] Test: Verify no memory leaks under stress
- [ ] Test: Verify `close()` is idempotent

---

## Verification

```bash
npm run build && npm test -- --grep "VideoFrame"
```
