#ifndef WEBCODECS_VIDEO_ENCODER_H
#define WEBCODECS_VIDEO_ENCODER_H

#include <napi.h>

namespace webcodecs {

/**
 * VideoEncoder - WebCodecs VideoEncoder implementation.
 *
 * Encodes VideoFrame instances into EncodedVideoChunk instances.
 * Uses FFmpeg libavcodec for actual encoding.
 */
class VideoEncoder : public Napi::ObjectWrap<VideoEncoder> {
 public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  explicit VideoEncoder(const Napi::CallbackInfo& info);
  ~VideoEncoder() override;

 private:
  // WebCodecs API methods
  Napi::Value Configure(const Napi::CallbackInfo& info);
  Napi::Value Encode(const Napi::CallbackInfo& info);
  Napi::Value Flush(const Napi::CallbackInfo& info);
  Napi::Value Reset(const Napi::CallbackInfo& info);
  void Close(const Napi::CallbackInfo& info);

  // Properties
  Napi::Value GetState(const Napi::CallbackInfo& info);
  Napi::Value GetEncodeQueueSize(const Napi::CallbackInfo& info);

  // Static methods
  static Napi::Value IsConfigSupported(const Napi::CallbackInfo& info);

  // Instance state
  enum class State { Unconfigured, Configured, Closed };
  State state_ = State::Unconfigured;
};

}  // namespace webcodecs

#endif  // WEBCODECS_VIDEO_ENCODER_H
