# AudioDecoder Implementation Tasks

> Auto-generated from `spec/context/_webcodecs.idl`

## Overview

Implement the `AudioDecoder` class following the W3C WebCodecs specification.

**Files:**
- C++ Header: `src/AudioDecoder.h`
- C++ Implementation: `src/AudioDecoder.cpp`
- TypeScript Wrapper: `lib/AudioDecoder.ts`
- Tests: `test/audiodecoder.test.ts`

---

## Implementation Checklist

### Constructor

- [ ] Implement C++ constructor with parameter validation
- [ ] Initialize internal state slots
- [ ] Wire TypeScript wrapper to native constructor
- [ ] Validate required parameters: `init: AudioDecoderInit`

### Attributes

#### `state` (readonly)

- [ ] C++: Implement `GetState()` returning `CodecState`
- [ ] TS: Wire getter to native
- [ ] Test: Verify initial value and behavior

#### `decodeQueueSize` (readonly)

- [ ] C++: Implement `GetDecodeQueueSize()` returning `unsigned long`
- [ ] TS: Wire getter to native
- [ ] Test: Verify initial value and behavior

#### `ondequeue` (read/write)

- [ ] C++: Implement `GetOndequeue()` returning `EventHandler`
- [ ] C++: Implement `SetOndequeue()`
- [ ] TS: Wire getter and setter to native
- [ ] Test: Verify initial value and behavior

### Methods

#### `configure(config: AudioDecoderConfig)`

- [ ] C++: Implement method logic per W3C spec algorithm
- [ ] C++: Handle error cases with proper DOMException types
- [ ] TS: Wire method to native implementation
- [ ] Test: Write test case for happy path
- [ ] Test: Write test case for error conditions

#### `decode(chunk: EncodedAudioChunk)`

- [ ] C++: Implement method logic per W3C spec algorithm
- [ ] C++: Handle error cases with proper DOMException types
- [ ] TS: Wire method to native implementation
- [ ] Test: Write test case for happy path
- [ ] Test: Write test case for error conditions

#### `flush()`

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

#### `AudioDecoder.isConfigSupported(config: AudioDecoderConfig)`

- [ ] C++: Implement as StaticMethod on class
- [ ] C++: Return Promise with appropriate result type
- [ ] TS: Expose as static method on class
- [ ] Test: Verify supported configurations

---

## Verification

```bash
npm run build && npm test -- --grep "AudioDecoder"
```
