# ImageTrack Implementation Tasks

> Auto-generated from `spec/context/_webcodecs.idl`

## Overview

Implement the `ImageTrack` class following the W3C WebCodecs specification.

**Files:**
- C++ Header: `src/ImageTrack.h`
- C++ Implementation: `src/ImageTrack.cpp`
- TypeScript Wrapper: `lib/ImageTrack.ts`
- Tests: `test/imagetrack.test.ts`

---

## Implementation Checklist

### Attributes

#### `animated` (readonly)

- [ ] C++: Implement `GetAnimated()` returning `boolean`
- [ ] TS: Wire getter to native
- [ ] Test: Verify initial value and behavior

#### `frameCount` (readonly)

- [ ] C++: Implement `GetFrameCount()` returning `unsigned long`
- [ ] TS: Wire getter to native
- [ ] Test: Verify initial value and behavior

#### `repetitionCount` (readonly)

- [ ] C++: Implement `GetRepetitionCount()` returning `unrestricted float`
- [ ] TS: Wire getter to native
- [ ] Test: Verify initial value and behavior

#### `selected` (read/write)

- [ ] C++: Implement `GetSelected()` returning `boolean`
- [ ] C++: Implement `SetSelected()`
- [ ] TS: Wire getter and setter to native
- [ ] Test: Verify initial value and behavior

---

## Verification

```bash
npm run build && npm test -- --grep "ImageTrack"
```
