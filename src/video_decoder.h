#ifndef WEBCODECS_VIDEO_DECODER_H
#define WEBCODECS_VIDEO_DECODER_H

#include <napi.h>

namespace webcodecs {

/**
 * VideoDecoder - WebCodecs VideoDecoder implementation.
 *
 * Decodes EncodedVideoChunk instances into VideoFrame instances.
 * Uses FFmpeg libavcodec for actual decoding.
 *
 * Lifecycle:
 *   1. new VideoDecoder(init) - Registers output/error callbacks
 *   2. configure(config) - Sets up codec (e.g., "avc1.42E01E" for H.264)
 *   3. decode(chunk) - Queues chunk for async decode
 *   4. flush() - Drains decoder, returns Promise
 *   5. close() - Releases resources
 *
 * Threading: decode() is non-blocking; FFmpeg runs on worker thread.
 */
class VideoDecoder : public Napi::ObjectWrap<VideoDecoder> {
 public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  explicit VideoDecoder(const Napi::CallbackInfo& info);
  ~VideoDecoder() override;

 private:
  // WebCodecs API methods
  Napi::Value Configure(const Napi::CallbackInfo& info);
  Napi::Value Decode(const Napi::CallbackInfo& info);
  Napi::Value Flush(const Napi::CallbackInfo& info);
  Napi::Value Reset(const Napi::CallbackInfo& info);
  void Close(const Napi::CallbackInfo& info);

  // Properties
  Napi::Value GetState(const Napi::CallbackInfo& info);
  Napi::Value GetDecodeQueueSize(const Napi::CallbackInfo& info);

  // Static methods
  static Napi::Value IsConfigSupported(const Napi::CallbackInfo& info);

  // Instance state
  enum class State { Unconfigured, Configured, Closed };
  State state_ = State::Unconfigured;
};

}  // namespace webcodecs

#endif  // WEBCODECS_VIDEO_DECODER_H
