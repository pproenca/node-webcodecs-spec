# ImageTrackList Implementation Tasks

> Auto-generated from `spec/context/_webcodecs.idl`

## Overview

Implement the `ImageTrackList` class following the W3C WebCodecs specification.

**Files:**
- C++ Header: `src/ImageTrackList.h`
- C++ Implementation: `src/ImageTrackList.cpp`
- TypeScript Wrapper: `lib/ImageTrackList.ts`
- Tests: `test/imagetracklist.test.ts`

---

## Implementation Checklist

### Attributes

#### `ready` (readonly)

- [ ] C++: Implement `GetReady()` returning `undefined`
- [ ] TS: Wire getter to native
- [ ] Test: Verify initial value and behavior

#### `length` (readonly)

- [ ] C++: Implement `GetLength()` returning `unsigned long`
- [ ] TS: Wire getter to native
- [ ] Test: Verify initial value and behavior

#### `selectedIndex` (readonly)

- [ ] C++: Implement `GetSelectedIndex()` returning `long`
- [ ] TS: Wire getter to native
- [ ] Test: Verify initial value and behavior

#### `selectedTrack` (readonly)

- [ ] C++: Implement `GetSelectedTrack()` returning `ImageTrack`
- [ ] TS: Wire getter to native
- [ ] Test: Verify initial value and behavior

---

## Verification

```bash
npm run build && npm test -- --grep "ImageTrackList"
```
