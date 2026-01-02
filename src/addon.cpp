#include <napi.h>
#include "shared/utils.h"  // Brings in FFmpeg headers and AsyncDecodeContext

// WebCodecs class headers
#include "video_decoder.h"
#include "video_encoder.h"
#include "video_frame.h"
#include "encoded_video_chunk.h"
#include "video_color_space.h"
#include "audio_decoder.h"
#include "audio_encoder.h"
#include "audio_data.h"
#include "encoded_audio_chunk.h"
#include "image_decoder.h"
#include "image_track.h"
#include "image_track_list.h"

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

  // Audio codec classes (AudioData must be initialized before AudioEncoder)
  webcodecs::AudioData::Init(env, exports);
  webcodecs::EncodedAudioChunk::Init(env, exports);
  webcodecs::AudioDecoder::Init(env, exports);
  webcodecs::AudioEncoder::Init(env, exports);

  // Image codec classes
  webcodecs::ImageDecoder::Init(env, exports);
  webcodecs::ImageTrack::Init(env, exports);
  webcodecs::ImageTrackList::Init(env, exports);

  return exports;
}

// Module registration macro (N-API stable ABI)
NODE_API_MODULE(webcodecs, Init)
