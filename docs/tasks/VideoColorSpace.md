# VideoColorSpace Implementation Tasks

> Auto-generated from `spec/context/_webcodecs.idl`

## Overview

Implement the `VideoColorSpace` class following the W3C WebCodecs specification.

**Files:**
- C++ Header: `src/VideoColorSpace.h`
- C++ Implementation: `src/VideoColorSpace.cpp`
- TypeScript Wrapper: `lib/VideoColorSpace.ts`
- Tests: `test/videocolorspace.test.ts`

---

## Implementation Checklist

### Constructor

- [ ] Implement C++ constructor with parameter validation
- [ ] Initialize internal state slots
- [ ] Wire TypeScript wrapper to native constructor
- [ ] Validate required parameters: `init: VideoColorSpaceInit`

### Attributes

#### `primaries` (readonly)

- [ ] C++: Implement `GetPrimaries()` returning `VideoColorPrimaries`
- [ ] TS: Wire getter to native
- [ ] Test: Verify initial value and behavior

#### `transfer` (readonly)

- [ ] C++: Implement `GetTransfer()` returning `VideoTransferCharacteristics`
- [ ] TS: Wire getter to native
- [ ] Test: Verify initial value and behavior

#### `matrix` (readonly)

- [ ] C++: Implement `GetMatrix()` returning `VideoMatrixCoefficients`
- [ ] TS: Wire getter to native
- [ ] Test: Verify initial value and behavior

#### `fullRange` (readonly)

- [ ] C++: Implement `GetFullRange()` returning `boolean`
- [ ] TS: Wire getter to native
- [ ] Test: Verify initial value and behavior

### Methods

#### `toJSON()`

- [ ] C++: Implement method logic per W3C spec algorithm
- [ ] C++: Handle error cases with proper DOMException types
- [ ] TS: Wire method to native implementation
- [ ] Test: Write test case for happy path
- [ ] Test: Write test case for error conditions

---

## Verification

```bash
npm run build && npm test -- --grep "VideoColorSpace"
```
