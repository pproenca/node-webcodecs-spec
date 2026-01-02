#pragma once
#include <napi.h>
#include <optional>
#include <string>
#include "shared/utils.h"
#include "ffmpeg_raii.h"

namespace webcodecs {

/**
 * EncodedAudioChunk - W3C WebCodecs EncodedAudioChunk implementation
 * @see spec/context/EncodedAudioChunk.md
 *
 * Represents a chunk of encoded audio data. The chunk is immutable after
 * construction. Ownership of the underlying AVPacket data is managed via RAII.
 */
class EncodedAudioChunk : public Napi::ObjectWrap<EncodedAudioChunk> {
 public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  explicit EncodedAudioChunk(const Napi::CallbackInfo& info);
  ~EncodedAudioChunk() override;

  // RAII Release - cleans up all resources
  void Release();

  // Factory: Create from AVPacket (clones, caller retains ownership of source)
  static Napi::Object CreateFromPacket(Napi::Env env, const AVPacket* pkt, bool is_key_frame, int64_t timestamp_us);

  // Access underlying packet (for decoders)
  const AVPacket* packet() const { return packet_.get(); }

 private:
  static Napi::FunctionReference constructor;

  // --- Typed Storage (NO void*) ---
  raii::AVPacketPtr packet_;         // Owns encoded data
  std::string type_;                 // "key" or "delta"
  int64_t timestamp_;                // WebCodecs timestamp (microseconds)
  std::optional<int64_t> duration_;  // WebCodecs duration (microseconds, optional)

  // Attributes
  Napi::Value GetType(const Napi::CallbackInfo& info);
  Napi::Value GetTimestamp(const Napi::CallbackInfo& info);
  Napi::Value GetDuration(const Napi::CallbackInfo& info);
  Napi::Value GetByteLength(const Napi::CallbackInfo& info);

  // Methods
  Napi::Value CopyTo(const Napi::CallbackInfo& info);
};

}  // namespace webcodecs
