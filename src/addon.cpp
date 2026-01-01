#include <napi.h>
#include <thread>

// FFmpeg headers (C linkage)
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
#include <libswscale/swscale.h>
}

// Forward declarations for class registration
#include "video_decoder.h"
#include "video_encoder.h"
#include "video_frame.h"
#include "encoded_video_chunk.h"

namespace webcodecs {

/**
 * AsyncDecodeContext - RAII container for async decode operations.
 *
 * Owns:
 * - TypedThreadSafeFunction for main thread callbacks
 * - AVCodecContext for FFmpeg decode state
 * - Worker thread for non-blocking decode
 *
 * Cleanup is triggered by the TSFN finalizer when all threads Release().
 */
struct AsyncDecodeContext {
  using TSFN = Napi::TypedThreadSafeFunction<AsyncDecodeContext, AVFrame*>;

  TSFN tsfn;
  AVCodecContext* codecCtx = nullptr;
  std::thread workerThread;
  bool shouldExit = false;

  AsyncDecodeContext() = default;

  // Move-only (no copy)
  AsyncDecodeContext(const AsyncDecodeContext&) = delete;
  AsyncDecodeContext& operator=(const AsyncDecodeContext&) = delete;
  AsyncDecodeContext(AsyncDecodeContext&&) = default;
  AsyncDecodeContext& operator=(AsyncDecodeContext&&) = default;

  ~AsyncDecodeContext() {
    shouldExit = true;

    // Release TSFN if valid
    if (tsfn) {
      tsfn.Release();
    }

    // Free FFmpeg codec context
    if (codecCtx) {
      avcodec_free_context(&codecCtx);
      codecCtx = nullptr;
    }

    // Join worker thread
    if (workerThread.joinable()) {
      workerThread.join();
    }
  }
};

/**
 * Module initialization.
 *
 * Registers all WebCodecs classes as exports:
 * - VideoDecoder
 * - VideoEncoder
 * - VideoFrame
 * - EncodedVideoChunk
 */
Napi::Object Init(Napi::Env env, Napi::Object exports) {
  // Register class constructors
  VideoDecoder::Init(env, exports);
  VideoEncoder::Init(env, exports);
  VideoFrame::Init(env, exports);
  EncodedVideoChunk::Init(env, exports);

  return exports;
}

}  // namespace webcodecs

// Module registration macro (N-API stable ABI)
NODE_API_MODULE(webcodecs, webcodecs::Init)
