# Task: Encoded Media Interfaces (Chunks) Implementation

> **Created:** 2026-01-02
> **Spec:** [docs/specs/08-encoded-media-interfaces-chunks.md](../specs/08-encoded-media-interfaces-chunks.md)
> **Branch:** feat/encoded-chunks

## Success Criteria
- [ ] All tests pass (`npm test`) - **Note: ImageDecoder crash blocks full suite**
- [x] Type check passes (`npm run typecheck`)
- [x] Linting clean (`npm run lint`)
- [ ] All checklist items below marked complete
- [ ] PR description created

## Audit Status (2026-01-02)
**Compliance:** ~85%
**See:** [docs/audit-report.md](../audit-report.md)

---

## Phase 1: Investigation (NO CODING)
- [x] Read relevant files: `lib/EncodedAudioChunk.ts`, `lib/EncodedVideoChunk.ts`
- [x] Document current patterns in NOTES.md
- [x] Identify integration points with encoders/decoders
- [x] List dependencies and constraints
- [x] Update plan.md with findings

## Phase 2: Planning
- [x] Create detailed implementation plan
- [x] Define interface contracts per WebIDL
- [x] Identify parallelizable work units
- [x] Get human approval on plan
- [x] Create task packets for subagents (if applicable)

## Phase 3: Implementation

### 3.1 EncodedAudioChunk Interface
- [x] Write failing tests for EncodedAudioChunk
- [x] Confirm tests fail (RED)
- [x] Implement EncodedAudioChunk:
  - [x] Constructor(init: EncodedAudioChunkInit)
  - [x] Internal slots:
    - [x] `[[type]]` (EncodedAudioChunkType)
    - [x] `[[timestamp]]` (long long, microseconds)
    - [x] `[[duration]]` (unsigned long long, microseconds)
    - [x] `[[internal data]]` (byte sequence)
  - [x] Readonly attributes:
    - [x] `type` -> "key" | "delta"
    - [x] `timestamp`
    - [x] `duration`
    - [x] `byteLength`
  - [x] Methods:
    - [x] `copyTo(destination: AllowSharedBufferSource)`
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.2 EncodedAudioChunkInit Dictionary
- [x] Write failing tests for init validation
- [x] Confirm tests fail (RED)
- [x] Implement EncodedAudioChunkInit:
  - [x] `type` (required EncodedAudioChunkType)
  - [x] `timestamp` (required long long)
  - [x] `duration` (optional unsigned long long)
  - [x] `data` (required BufferSource)
  - [ ] `transfer` (sequence<ArrayBuffer>) - **NOT IMPLEMENTED**
- [x] Implement validation algorithm
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.3 EncodedVideoChunk Interface
- [x] Write failing tests for EncodedVideoChunk
- [x] Confirm tests fail (RED)
- [x] Implement EncodedVideoChunk:
  - [x] Constructor(init: EncodedVideoChunkInit)
  - [x] Internal slots:
    - [x] `[[type]]` (EncodedVideoChunkType)
    - [x] `[[timestamp]]` (long long, microseconds)
    - [x] `[[duration]]` (unsigned long long, microseconds)
    - [x] `[[internal data]]` (byte sequence)
  - [x] Readonly attributes:
    - [x] `type` -> "key" | "delta"
    - [x] `timestamp`
    - [x] `duration`
    - [x] `byteLength`
  - [x] Methods:
    - [x] `copyTo(destination: AllowSharedBufferSource)`
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.4 EncodedVideoChunkInit Dictionary
- [x] Write failing tests for init validation
- [x] Confirm tests fail (RED)
- [x] Implement EncodedVideoChunkInit:
  - [x] `type` (required EncodedVideoChunkType)
  - [x] `timestamp` (required long long)
  - [x] `duration` (optional unsigned long long)
  - [x] `data` (required BufferSource)
  - [ ] `transfer` (sequence<ArrayBuffer>) - **NOT IMPLEMENTED**
- [x] Implement validation algorithm
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.5 Chunk Type Enums
- [x] Write failing tests for enums
- [x] Confirm tests fail (RED)
- [x] Implement:
  - [x] `EncodedAudioChunkType`: "key" | "delta"
  - [x] `EncodedVideoChunkType`: "key" | "delta"
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.6 Metadata Dictionaries
- [x] Write failing tests for metadata
- [x] Confirm tests fail (RED)
- [x] Implement:
  - [x] `EncodedAudioChunkMetadata`:
    - [x] `decoderConfig` (AudioDecoderConfig)
  - [x] `EncodedVideoChunkMetadata`:
    - [x] `decoderConfig` (VideoDecoderConfig)
    - [ ] `svc` (SvcOutputMetadata) - **NOT IMPLEMENTED**
    - [ ] `alphaSideData` (BufferSource) - **NOT IMPLEMENTED**
  - [ ] `SvcOutputMetadata`:
    - [ ] `temporalLayerId` (unsigned long) - **NOT IMPLEMENTED**
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.7 copyTo() Algorithm
- [x] Write failing tests for copyTo
- [x] Confirm tests fail (RED)
- [x] Implement copyTo algorithm:
  - [x] Validate destination size >= byteLength
  - [x] Copy internal data to destination
  - [x] Handle SharedArrayBuffer
- [x] Confirm tests pass (GREEN)
- [x] Refactor if needed (BLUE)
- [x] Write artifact summary

### 3.8 Transfer Handling
- [ ] Write failing tests for transfer semantics
- [ ] Confirm tests fail (RED)
- [ ] Implement ArrayBuffer transfer:
  - [ ] Detach transferred buffers from source - **NOT IMPLEMENTED**
  - [ ] Validate transferred buffers are not detached
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

## Phase 4: Integration
- [ ] Verify all components work together
- [ ] Run full test suite
- [ ] Run integration tests
- [x] Test with encoder/decoder pipelines
- [ ] Update documentation

## Phase 5: Verification
- [ ] Code review checklist complete
- [x] No hardcoded test values
- [x] Edge cases handled:
  - [x] Empty data
  - [x] Zero timestamp/duration
  - [ ] Detached ArrayBuffers - **NEEDS TESTING**
  - [x] SharedArrayBuffer support
- [x] Error handling complete
- [x] Types are strict (no `any`)

## Phase 6: Finalize
- [ ] All success criteria met
- [ ] PR description written
- [ ] Ready for human review

---

## Blockers
- ImageDecoder crash blocks full TypeScript test suite

## Missing Features (P3)
- ArrayBuffer transfer semantics
- SvcOutputMetadata (svc.temporalLayerId)
- alphaSideData in EncodedVideoChunkMetadata
- Serialization support

## Notes
- Chunks are immutable after construction
- Key chunks are IDR/keyframes, delta chunks are P/B frames
- Metadata is emitted by encoders alongside chunks
- Transfer semantics optimize memory for worker communication
