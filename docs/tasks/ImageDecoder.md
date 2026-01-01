# ImageDecoder Implementation Tasks

> Auto-generated from `spec/context/_webcodecs.idl`

## Overview

Implement the `ImageDecoder` class following the W3C WebCodecs specification.

**Files:**
- C++ Header: `src/ImageDecoder.h`
- C++ Implementation: `src/ImageDecoder.cpp`
- TypeScript Wrapper: `lib/ImageDecoder.ts`
- Tests: `test/imagedecoder.test.ts`

---

## Implementation Checklist

### Constructor

- [ ] Implement C++ constructor with parameter validation
- [ ] Initialize internal state slots
- [ ] Wire TypeScript wrapper to native constructor
- [ ] Validate required parameters: `init: ImageDecoderInit`

### Attributes

#### `type` (readonly)

- [ ] C++: Implement `GetType()` returning `DOMString`
- [ ] TS: Wire getter to native
- [ ] Test: Verify initial value and behavior

#### `complete` (readonly)

- [ ] C++: Implement `GetComplete()` returning `boolean`
- [ ] TS: Wire getter to native
- [ ] Test: Verify initial value and behavior

#### `completed` (readonly)

- [ ] C++: Implement `GetCompleted()` returning `undefined`
- [ ] TS: Wire getter to native
- [ ] Test: Verify initial value and behavior

#### `tracks` (readonly)

- [ ] C++: Implement `GetTracks()` returning `ImageTrackList`
- [ ] TS: Wire getter to native
- [ ] Test: Verify initial value and behavior

### Methods

#### `decode(options: ImageDecodeOptions)`

- [ ] C++: Implement method logic per W3C spec algorithm
- [ ] C++: Handle error cases with proper DOMException types
- [ ] TS: Wire method to native implementation
- [ ] Test: Write test case for happy path
- [ ] Test: Write test case for error conditions

#### `reset()`

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

### Static Methods

#### `ImageDecoder.isTypeSupported(type: DOMString)`

- [ ] C++: Implement as StaticMethod on class
- [ ] C++: Return Promise with appropriate result type
- [ ] TS: Expose as static method on class
- [ ] Test: Verify supported configurations

---

## Verification

```bash
npm run build && npm test -- --grep "ImageDecoder"
```
