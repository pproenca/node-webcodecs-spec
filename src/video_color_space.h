#pragma once
#include <napi.h>
#include <optional>
#include <string>
#include "shared/utils.h"

namespace webcodecs {

/**
 * VideoColorSpace - W3C WebCodecs VideoColorSpace implementation
 * @see spec/context/VideoColorSpace.md
 *
 * Represents the color space properties of a video frame.
 * All properties are optional (nullable in WebIDL terms).
 */
class VideoColorSpace : public Napi::ObjectWrap<VideoColorSpace> {
 public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  explicit VideoColorSpace(const Napi::CallbackInfo& info);
  ~VideoColorSpace() override = default;

  // Factory: Create from VideoColorSpaceInit dictionary
  static Napi::Object CreateFromInit(Napi::Env env, Napi::Object init);

  // Factory: Create with explicit values (for internal use)
  static Napi::Object Create(
      Napi::Env env,
      const std::optional<std::string>& primaries,
      const std::optional<std::string>& transfer,
      const std::optional<std::string>& matrix,
      const std::optional<bool>& fullRange);

  // Public constructor reference for InstanceOf checks
  static Napi::FunctionReference constructor;

 private:
  // --- Internal Slots (per spec 9.9.1) ---
  // All optional/nullable per WebIDL
  std::optional<std::string> primaries_;  // [[primaries]]
  std::optional<std::string> transfer_;   // [[transfer]]
  std::optional<std::string> matrix_;     // [[matrix]]
  std::optional<bool> fullRange_;         // [[full range]]

  // Attributes
  Napi::Value GetPrimaries(const Napi::CallbackInfo& info);
  Napi::Value GetTransfer(const Napi::CallbackInfo& info);
  Napi::Value GetMatrix(const Napi::CallbackInfo& info);
  Napi::Value GetFullRange(const Napi::CallbackInfo& info);

  // Methods
  Napi::Value ToJSON(const Napi::CallbackInfo& info);
};

}  // namespace webcodecs
