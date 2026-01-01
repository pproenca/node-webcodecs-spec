#include <napi.h>
#include <thread>

// FFmpeg headers (C linkage)
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
#include <libswscale/swscale.h>
}

// WebCodecs class headers
#include "VideoDecoder.h"
#include "VideoEncoder.h"
#include "VideoFrame.h"
#include "EncodedVideoChunk.h"
#include "VideoColorSpace.h"
#include "AudioDecoder.h"
#include "AudioEncoder.h"
#include "AudioData.h"
#include "EncodedAudioChunk.h"
#include "ImageDecoder.h"
#include "ImageTrack.h"
#include "ImageTrackList.h"

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

}  // namespace webcodecs

/**
 * Module initialization.
 *
 * Registers all WebCodecs classes as exports.
 */
Napi::Object Init(Napi::Env env, Napi::Object exports) {
  // Video codec classes
  webcodecs::VideoDecoder::Init(env, exports);
  webcodecs::VideoEncoder::Init(env, exports);
  webcodecs::VideoFrame::Init(env, exports);
  webcodecs::EncodedVideoChunk::Init(env, exports);
  webcodecs::VideoColorSpace::Init(env, exports);

  // Audio codec classes
  webcodecs::AudioDecoder::Init(env, exports);
  webcodecs::AudioEncoder::Init(env, exports);
  webcodecs::AudioData::Init(env, exports);
  webcodecs::EncodedAudioChunk::Init(env, exports);

  // Image codec classes
  webcodecs::ImageDecoder::Init(env, exports);
  webcodecs::ImageTrack::Init(env, exports);
  webcodecs::ImageTrackList::Init(env, exports);

  return exports;
}

// Module registration macro (N-API stable ABI)
NODE_API_MODULE(webcodecs, Init)
