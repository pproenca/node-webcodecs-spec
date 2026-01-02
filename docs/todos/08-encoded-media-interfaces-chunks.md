# Task: Encoded Media Interfaces (Chunks) Implementation

> **Created:** 2026-01-02
> **Spec:** [docs/specs/08-encoded-media-interfaces-chunks.md](../specs/08-encoded-media-interfaces-chunks.md)
> **Branch:** feat/encoded-chunks

## Success Criteria
- [ ] All tests pass (`npm test`)
- [ ] Type check passes (`npm run typecheck`)
- [ ] Linting clean (`npm run lint`)
- [ ] All checklist items below marked complete
- [ ] PR description created

---

## Phase 1: Investigation (NO CODING)
- [ ] Read relevant files: `lib/EncodedAudioChunk.ts`, `lib/EncodedVideoChunk.ts`
- [ ] Document current patterns in NOTES.md
- [ ] Identify integration points with encoders/decoders
- [ ] List dependencies and constraints
- [ ] Update plan.md with findings

## Phase 2: Planning
- [ ] Create detailed implementation plan
- [ ] Define interface contracts per WebIDL
- [ ] Identify parallelizable work units
- [ ] Get human approval on plan
- [ ] Create task packets for subagents (if applicable)

## Phase 3: Implementation

### 3.1 EncodedAudioChunk Interface
- [ ] Write failing tests for EncodedAudioChunk
- [ ] Confirm tests fail (RED)
- [ ] Implement EncodedAudioChunk:
  - Constructor(init: EncodedAudioChunkInit)
  - Internal slots:
    - `[[type]]` (EncodedAudioChunkType)
    - `[[timestamp]]` (long long, microseconds)
    - `[[duration]]` (unsigned long long, microseconds)
    - `[[internal data]]` (byte sequence)
  - Readonly attributes:
    - `type` -> "key" | "delta"
    - `timestamp`
    - `duration`
    - `byteLength`
  - Methods:
    - `copyTo(destination: AllowSharedBufferSource)`
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.2 EncodedAudioChunkInit Dictionary
- [ ] Write failing tests for init validation
- [ ] Confirm tests fail (RED)
- [ ] Implement EncodedAudioChunkInit:
  - `type` (required EncodedAudioChunkType)
  - `timestamp` (required long long)
  - `duration` (optional unsigned long long)
  - `data` (required BufferSource)
  - `transfer` (sequence<ArrayBuffer>)
- [ ] Implement validation algorithm
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.3 EncodedVideoChunk Interface
- [ ] Write failing tests for EncodedVideoChunk
- [ ] Confirm tests fail (RED)
- [ ] Implement EncodedVideoChunk:
  - Constructor(init: EncodedVideoChunkInit)
  - Internal slots:
    - `[[type]]` (EncodedVideoChunkType)
    - `[[timestamp]]` (long long, microseconds)
    - `[[duration]]` (unsigned long long, microseconds)
    - `[[internal data]]` (byte sequence)
  - Readonly attributes:
    - `type` -> "key" | "delta"
    - `timestamp`
    - `duration`
    - `byteLength`
  - Methods:
    - `copyTo(destination: AllowSharedBufferSource)`
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.4 EncodedVideoChunkInit Dictionary
- [ ] Write failing tests for init validation
- [ ] Confirm tests fail (RED)
- [ ] Implement EncodedVideoChunkInit:
  - `type` (required EncodedVideoChunkType)
  - `timestamp` (required long long)
  - `duration` (optional unsigned long long)
  - `data` (required BufferSource)
  - `transfer` (sequence<ArrayBuffer>)
- [ ] Implement validation algorithm
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.5 Chunk Type Enums
- [ ] Write failing tests for enums
- [ ] Confirm tests fail (RED)
- [ ] Implement:
  - `EncodedAudioChunkType`: "key" | "delta"
  - `EncodedVideoChunkType`: "key" | "delta"
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.6 Metadata Dictionaries
- [ ] Write failing tests for metadata
- [ ] Confirm tests fail (RED)
- [ ] Implement:
  - `EncodedAudioChunkMetadata`:
    - `decoderConfig` (AudioDecoderConfig)
  - `EncodedVideoChunkMetadata`:
    - `decoderConfig` (VideoDecoderConfig)
    - `svc` (SvcOutputMetadata)
    - `alphaSideData` (BufferSource)
  - `SvcOutputMetadata`:
    - `temporalLayerId` (unsigned long)
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.7 copyTo() Algorithm
- [ ] Write failing tests for copyTo
- [ ] Confirm tests fail (RED)
- [ ] Implement copyTo algorithm:
  - Validate destination size >= byteLength
  - Copy internal data to destination
  - Handle SharedArrayBuffer
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

### 3.8 Transfer Handling
- [ ] Write failing tests for transfer semantics
- [ ] Confirm tests fail (RED)
- [ ] Implement ArrayBuffer transfer:
  - Detach transferred buffers from source
  - Validate transferred buffers are not detached
- [ ] Confirm tests pass (GREEN)
- [ ] Refactor if needed (BLUE)
- [ ] Write artifact summary

## Phase 4: Integration
- [ ] Verify all components work together
- [ ] Run full test suite
- [ ] Run integration tests
- [ ] Test with encoder/decoder pipelines
- [ ] Update documentation

## Phase 5: Verification
- [ ] Code review checklist complete
- [ ] No hardcoded test values
- [ ] Edge cases handled:
  - Empty data
  - Zero timestamp/duration
  - Detached ArrayBuffers
  - SharedArrayBuffer support
- [ ] Error handling complete
- [ ] Types are strict (no `any`)

## Phase 6: Finalize
- [ ] All success criteria met
- [ ] PR description written
- [ ] Ready for human review

---

## Blockers
<!-- Add any blockers encountered -->

## Notes
- Chunks are immutable after construction
- Key chunks are IDR/keyframes, delta chunks are P/B frames
- Metadata is emitted by encoders alongside chunks
- Transfer semantics optimize memory for worker communication
