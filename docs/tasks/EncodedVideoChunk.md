# EncodedVideoChunk Implementation Tasks

> Auto-generated from `spec/context/_webcodecs.idl`

## Overview

Implement the `EncodedVideoChunk` class following the W3C WebCodecs specification.

**Files:**
- C++ Header: `src/EncodedVideoChunk.h`
- C++ Implementation: `src/EncodedVideoChunk.cpp`
- TypeScript Wrapper: `lib/EncodedVideoChunk.ts`
- Tests: `test/encodedvideochunk.test.ts`

---

## Implementation Checklist

### Constructor

- [ ] Implement C++ constructor with parameter validation
- [ ] Initialize internal state slots
- [ ] Wire TypeScript wrapper to native constructor
- [ ] Validate required parameters: `init: EncodedVideoChunkInit`

### Attributes

#### `type` (readonly)

- [ ] C++: Implement `GetType()` returning `EncodedVideoChunkType`
- [ ] TS: Wire getter to native
- [ ] Test: Verify initial value and behavior

#### `timestamp` (readonly)

- [ ] C++: Implement `GetTimestamp()` returning `long long`
- [ ] TS: Wire getter to native
- [ ] Test: Verify initial value and behavior

#### `duration` (readonly)

- [ ] C++: Implement `GetDuration()` returning `unsigned long long`
- [ ] TS: Wire getter to native
- [ ] Test: Verify initial value and behavior

#### `byteLength` (readonly)

- [ ] C++: Implement `GetByteLength()` returning `unsigned long`
- [ ] TS: Wire getter to native
- [ ] Test: Verify initial value and behavior

### Methods

#### `copyTo(destination: AllowSharedBufferSource)`

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
npm run build && npm test -- --grep "EncodedVideoChunk"
```
