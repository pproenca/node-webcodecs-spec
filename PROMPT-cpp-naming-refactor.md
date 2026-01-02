# C++ Google Style Guide Naming Refactor

You are applying Google C++ Style Guide naming conventions to this codebase.

## Rules

1. **Functions**: `PascalCase` (e.g., `MakeSafeTsfn`, not `make_safe_tsfn`)
2. **Constants** (`constexpr`, `static const`): `kPascalCase` (e.g., `kVideoFrameTag`, not `VIDEO_FRAME_TAG`)
3. **Class data members**: `snake_case_` with trailing underscore (e.g., `codec_ctx_`, not `codecCtx_`)

## Tasks (in order)

### Task 1: Rename `make_safe_tsfn` → `MakeSafeTsfn`

**File:** `src/shared/safe_tsfn.h:203`

- Rename the function template `make_safe_tsfn` to `MakeSafeTsfn`
- Search for any usages and update them

### Task 2: Rename type tag constants in `napi_guards.h`

**File:** `src/shared/napi_guards.h:39-55`

Rename all `SCREAMING_SNAKE_CASE` constants to `kPascalCase`:

- `VIDEO_FRAME_TAG` → `kVideoFrameTag`
- `AUDIO_DATA_TAG` → `kAudioDataTag`
- `ENCODED_VIDEO_CHUNK_TAG` → `kEncodedVideoChunkTag`
- `ENCODED_AUDIO_CHUNK_TAG` → `kEncodedAudioChunkTag`
- `VIDEO_DECODER_TAG` → `kVideoDecoderTag`
- `VIDEO_ENCODER_TAG` → `kVideoEncoderTag`
- `AUDIO_DECODER_TAG` → `kAudioDecoderTag`
- `AUDIO_ENCODER_TAG` → `kAudioEncoderTag`
- `IMAGE_DECODER_TAG` → `kImageDecoderTag`

Update all references in the same file (SafeUnwrap calls, etc.)

### Task 3: Rename member variables in VideoDecoder

**Files:** `src/video_decoder.h`, `src/video_decoder.cpp`

Rename `camelCase_` to `snake_case_`:

- `codecCtx_` → `codec_ctx_`
- `decodeQueue_` → `decode_queue_`
- `decodeQueueSize_` → `decode_queue_size_`
- `keyChunkRequired_` → `key_chunk_required_`
- `outputCallback_` → `output_callback_`
- `errorCallback_` → `error_callback_`
- `ondequeueCallback_` → `ondequeue_callback_`

### Task 4: Rename member variables in VideoEncoder

**Files:** `src/video_encoder.h`, `src/video_encoder.cpp`

Rename `camelCase_` to `snake_case_`:

- `codecCtx_` → `codec_ctx_`
- `encodeQueue_` → `encode_queue_`
- `encodeQueueSize_` → `encode_queue_size_`
- `outputCallback_` → `output_callback_`
- `errorCallback_` → `error_callback_`
- `ondequeueCallback_` → `ondequeue_callback_`

### Task 5: Rename member variables in AudioDecoder

**Files:** `src/audio_decoder.h`, `src/audio_decoder.cpp`

Rename `camelCase_` to `snake_case_`:

- `codecCtx_` → `codec_ctx_`
- `swrCtx_` → `swr_ctx_`
- `decodeQueue_` → `decode_queue_`
- `decodeQueueSize_` → `decode_queue_size_`
- `outputCallback_` → `output_callback_`
- `errorCallback_` → `error_callback_`
- `ondequeueCallback_` → `ondequeue_callback_`

## Verification

After each task:

1. Run `npm run build:native` to verify compilation
2. If build fails, fix the issue before proceeding

## Completion

When ALL tasks are complete and the build passes:

<promise>ALL NAMING CONVENTIONS FIXED</promise>
