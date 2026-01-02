#pragma once
#include <napi.h>
#include <optional>
#include <string>
#include "shared/Utils.h"
#include "ffmpeg_raii.h"

namespace webcodecs {

/**
 * AudioData - W3C WebCodecs AudioData implementation
 * @see spec/context/AudioData.md
 *
 * Represents decoded audio sample data. The frame owns the audio buffer data
 * via RAII. Close() releases the underlying memory.
 */
class AudioData : public Napi::ObjectWrap<AudioData> {
 public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  explicit AudioData(const Napi::CallbackInfo& info);
  ~AudioData() override;

  // RAII Release - cleans up all resources
  void Release();

  // Factory: Create from AVFrame (clones, caller retains ownership of source)
  static Napi::Object CreateFromFrame(Napi::Env env, const AVFrame* frame, int64_t timestamp_us);

  // Access underlying frame (for encoders)
  const AVFrame* frame() const { return frame_.get(); }

  // Check if closed
  bool IsClosed() const { return closed_; }

 private:
  static Napi::FunctionReference constructor;

  // --- Typed Storage (NO void*) ---
  raii::AVFramePtr frame_;  // Owns decoded audio samples
  std::string format_;      // WebCodecs AudioSampleFormat
  int64_t timestamp_;       // WebCodecs timestamp (microseconds)
  bool closed_{false};      // Whether Close() has been called

  // Attributes
  Napi::Value GetFormat(const Napi::CallbackInfo& info);
  Napi::Value GetSampleRate(const Napi::CallbackInfo& info);
  Napi::Value GetNumberOfFrames(const Napi::CallbackInfo& info);
  Napi::Value GetNumberOfChannels(const Napi::CallbackInfo& info);
  Napi::Value GetDuration(const Napi::CallbackInfo& info);
  Napi::Value GetTimestamp(const Napi::CallbackInfo& info);

  // Methods
  Napi::Value AllocationSize(const Napi::CallbackInfo& info);
  Napi::Value CopyTo(const Napi::CallbackInfo& info);
  Napi::Value Clone(const Napi::CallbackInfo& info);
  Napi::Value Close(const Napi::CallbackInfo& info);
};

}  // namespace webcodecs
