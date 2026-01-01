# AudioData Implementation Tasks

> Auto-generated from `spec/context/_webcodecs.idl`

## Overview

Implement the `AudioData` class following the W3C WebCodecs specification.

**Files:**
- C++ Header: `src/AudioData.h`
- C++ Implementation: `src/AudioData.cpp`
- TypeScript Wrapper: `lib/AudioData.ts`
- Tests: `test/audiodata.test.ts`

---

## Implementation Checklist

### Constructor

- [ ] Implement C++ constructor with parameter validation
- [ ] Initialize internal state slots
- [ ] Wire TypeScript wrapper to native constructor
- [ ] Validate required parameters: `init: AudioDataInit`

### Attributes

#### `format` (readonly)

- [ ] C++: Implement `GetFormat()` returning `AudioSampleFormat`
- [ ] TS: Wire getter to native
- [ ] Test: Verify initial value and behavior

#### `sampleRate` (readonly)

- [ ] C++: Implement `GetSampleRate()` returning `float`
- [ ] TS: Wire getter to native
- [ ] Test: Verify initial value and behavior

#### `numberOfFrames` (readonly)

- [ ] C++: Implement `GetNumberOfFrames()` returning `unsigned long`
- [ ] TS: Wire getter to native
- [ ] Test: Verify initial value and behavior

#### `numberOfChannels` (readonly)

- [ ] C++: Implement `GetNumberOfChannels()` returning `unsigned long`
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

### Methods

#### `allocationSize(options: AudioDataCopyToOptions)`

- [ ] C++: Implement method logic per W3C spec algorithm
- [ ] C++: Handle error cases with proper DOMException types
- [ ] TS: Wire method to native implementation
- [ ] Test: Write test case for happy path
- [ ] Test: Write test case for error conditions

#### `copyTo(destination: AllowSharedBufferSource, options: AudioDataCopyToOptions)`

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
npm run build && npm test -- --grep "AudioData"
```
