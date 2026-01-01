#include <napi.h>
#include "shared/Utils.h"  // Brings in FFmpeg headers and AsyncDecodeContext

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
