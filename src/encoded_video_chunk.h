#ifndef WEBCODECS_ENCODED_VIDEO_CHUNK_H
#define WEBCODECS_ENCODED_VIDEO_CHUNK_H

#include <napi.h>

namespace webcodecs {

/**
 * EncodedVideoChunk - WebCodecs EncodedVideoChunk implementation.
 *
 * Represents a chunk of compressed video data (e.g., one H.264 NAL unit).
 * Immutable after construction.
 */
class EncodedVideoChunk : public Napi::ObjectWrap<EncodedVideoChunk> {
 public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  explicit EncodedVideoChunk(const Napi::CallbackInfo& info);
  ~EncodedVideoChunk() override;

 private:
  // WebCodecs API methods
  Napi::Value CopyTo(const Napi::CallbackInfo& info);

  // Properties
  Napi::Value GetType(const Napi::CallbackInfo& info);
  Napi::Value GetTimestamp(const Napi::CallbackInfo& info);
  Napi::Value GetDuration(const Napi::CallbackInfo& info);
  Napi::Value GetByteLength(const Napi::CallbackInfo& info);
};

}  // namespace webcodecs

#endif  // WEBCODECS_ENCODED_VIDEO_CHUNK_H
