# VideoEncoder.encode() Clone Documentation Plan

> **Execution:** Use `/dev-workflow:execute-plan docs/plans/2026-01-04-videoencoder-clone-documentation.md` to implement task-by-task.

**Goal:** Document that VideoEncoder.encode() is spec-compliant by clarifying that the C++ layer handles the Clone VideoFrame step per W3C WebCodecs spec section 6.5.

**Architecture:** Add JSDoc documentation to the TypeScript wrapper's encode() method explaining the clone behavior. The C++ implementation at `src/video_encoder.cpp:505-510` already performs `raii::CloneAvFrame(srcFrame)` which satisfies spec step 5.

**Tech Stack:** TypeScript, JSDoc

---

## Task Group 1: Documentation

### Task 1: Add JSDoc to VideoEncoder.encode()

**Files:**
- Modify: `lib/VideoEncoder.ts:66-70`

**Step 1: Read the current encode method** (30 sec)

Verify the current implementation matches what we expect:

```typescript
encode(frame: VideoFrame, options: VideoEncoderEncodeOptions): void {
  // Extract native object from wrapper if present (for TypeScript wrapper classes)
  const nativeFrame = (frame as { native?: unknown }).native ?? frame;
  this.native.encode(nativeFrame, options);
}
```

**Step 2: Add JSDoc documentation** (2-5 min)

Add a JSDoc block explaining the spec compliance:

```typescript
/**
 * Enqueues a control message to encode the given frame.
 *
 * Per W3C WebCodecs spec section 6.5, the frame is cloned before encoding
 * to allow the caller to close the original frame immediately after this call.
 * The clone operation is performed in the native C++ layer (video_encoder.cpp:505-510)
 * using FFmpeg's av_frame_clone(), ensuring the frame data remains valid throughout
 * the async encoding process.
 *
 * @param frame - The VideoFrame to encode. May be closed after this call returns.
 * @param options - Encoding options, including keyFrame hint.
 * @throws {InvalidStateError} If encoder state is not "configured".
 * @throws {TypeError} If frame is detached.
 * @throws {DataError} If frame orientation doesn't match active orientation.
 *
 * @see https://www.w3.org/TR/webcodecs/#dom-videoencoder-encode
 */
encode(frame: VideoFrame, options: VideoEncoderEncodeOptions): void {
  // Extract native object from wrapper if present (for TypeScript wrapper classes)
  const nativeFrame = (frame as { native?: unknown }).native ?? frame;
  this.native.encode(nativeFrame, options);
}
```

**Step 3: Verify TypeScript compiles** (30 sec)

```bash
npx tsc --noEmit lib/VideoEncoder.ts
```

Expected: No errors

**Step 4: Run existing tests to ensure no regression** (1 min)

```bash
npm test -- --grep "VideoEncoder" --run
```

Expected: All VideoEncoder tests pass

**Step 5: Commit** (30 sec)

```bash
git add lib/VideoEncoder.ts
git commit -m "docs(VideoEncoder): document encode() Clone VideoFrame spec compliance

Per W3C WebCodecs spec section 6.5 step 5, frames are cloned before
encoding. This is implemented in the C++ native layer at video_encoder.cpp:505-510
using raii::CloneAvFrame(). The JSDoc now documents this behavior."
```

---

### Task 2: Add similar documentation to AudioEncoder.encode()

**Files:**
- Modify: `lib/AudioEncoder.ts`

**Step 1: Read the current encode method** (30 sec)

```bash
grep -A 10 "encode(data:" lib/AudioEncoder.ts
```

**Step 2: Add JSDoc documentation** (2-5 min)

Add equivalent JSDoc explaining the spec compliance for AudioData cloning:

```typescript
/**
 * Enqueues a control message to encode the given audio data.
 *
 * Per W3C WebCodecs spec section 5.5, the audio data is cloned before encoding
 * to allow the caller to close the original data immediately after this call.
 * The clone operation is performed in the native C++ layer using FFmpeg's
 * av_frame_clone(), ensuring the audio data remains valid throughout
 * the async encoding process.
 *
 * @param data - The AudioData to encode. May be closed after this call returns.
 * @param options - Encoding options.
 * @throws {InvalidStateError} If encoder state is not "configured".
 * @throws {TypeError} If data is detached.
 *
 * @see https://www.w3.org/TR/webcodecs/#dom-audioencoder-encode
 */
```

**Step 3: Verify TypeScript compiles** (30 sec)

```bash
npx tsc --noEmit lib/AudioEncoder.ts
```

Expected: No errors

**Step 4: Commit** (30 sec)

```bash
git add lib/AudioEncoder.ts
git commit -m "docs(AudioEncoder): document encode() Clone AudioData spec compliance"
```

---

### Task 3: Code Review

Run code review to verify documentation is accurate and complete.

**Step 1: Verify C++ clone implementation exists** (1 min)

```bash
grep -n "CloneAvFrame\|av_frame_clone" src/video_encoder.cpp src/audio_encoder.cpp
```

Expected: Both files show clone calls in their Encode methods.

**Step 2: Run full test suite** (2-5 min)

```bash
npm test
```

Expected: All tests pass

---

## Parallel Groups

| Task Group | Tasks | Rationale |
|------------|-------|-----------|
| Group 1 | 1, 2 | Independent files, no overlap |
| Group 2 | 3 | Review depends on Tasks 1-2 |

---

## Summary

This plan adds documentation to clarify that the Clone VideoFrame/AudioData step per W3C WebCodecs spec is implemented in the native C++ layer, not the TypeScript wrapper. No functional changes are made.

**Files Modified:**
- `lib/VideoEncoder.ts` - Add JSDoc to encode()
- `lib/AudioEncoder.ts` - Add JSDoc to encode()

**Risk:** Low - documentation only, no behavioral changes.
