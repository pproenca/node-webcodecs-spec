#include "audio_data.h"

namespace webcodecs {

Napi::FunctionReference AudioData::constructor;

Napi::Object AudioData::Init(Napi::Env env, Napi::Object exports) {
  Napi::Function func = DefineClass(env, "AudioData",
                                    {
                                        InstanceAccessor<&AudioData::GetFormat>("format"),
                                        InstanceAccessor<&AudioData::GetSampleRate>("sampleRate"),
                                        InstanceAccessor<&AudioData::GetNumberOfFrames>("numberOfFrames"),
                                        InstanceAccessor<&AudioData::GetNumberOfChannels>("numberOfChannels"),
                                        InstanceAccessor<&AudioData::GetDuration>("duration"),
                                        InstanceAccessor<&AudioData::GetTimestamp>("timestamp"),
                                        InstanceMethod<&AudioData::AllocationSize>("allocationSize"),
                                        InstanceMethod<&AudioData::CopyTo>("copyTo"),
                                        InstanceMethod<&AudioData::Clone>("clone"),
                                        InstanceMethod<&AudioData::Close>("close"),
                                    });

  constructor = Napi::Persistent(func);
  constructor.SuppressDestruct();
  exports.Set("AudioData", func);
  return exports;
}

AudioData::AudioData(const Napi::CallbackInfo& info) : Napi::ObjectWrap<AudioData>(info) {
  Napi::Env env = info.Env();

  // [SPEC] Constructor Algorithm
  /*
   * Initialize internal slots.
   */

  // TODO(impl): Implement Constructor & Resource Allocation
}

AudioData::~AudioData() { Release(); }

void AudioData::Release() {
  // Release frame (RAII handles av_frame_unref)
  frame_.reset();
  closed_ = true;
}

// --- Attributes ---

Napi::Value AudioData::GetFormat(const Napi::CallbackInfo& info) {
  // TODO(impl): Return format
  return info.Env().Null();
}

Napi::Value AudioData::GetSampleRate(const Napi::CallbackInfo& info) {
  // TODO(impl): Return sampleRate
  return info.Env().Null();
}

Napi::Value AudioData::GetNumberOfFrames(const Napi::CallbackInfo& info) {
  // TODO(impl): Return numberOfFrames
  return info.Env().Null();
}

Napi::Value AudioData::GetNumberOfChannels(const Napi::CallbackInfo& info) {
  // TODO(impl): Return numberOfChannels
  return info.Env().Null();
}

Napi::Value AudioData::GetDuration(const Napi::CallbackInfo& info) {
  // TODO(impl): Return duration
  return info.Env().Null();
}

Napi::Value AudioData::GetTimestamp(const Napi::CallbackInfo& info) {
  // TODO(impl): Return timestamp
  return info.Env().Null();
}

// --- Methods ---

Napi::Value AudioData::AllocationSize(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Algorithm
  /*
   * See spec/context file.
   */

  // TODO(impl): Implement method logic
  return env.Undefined();
}

Napi::Value AudioData::CopyTo(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Algorithm
  /*
   * See spec/context file.
   */

  // TODO(impl): Implement method logic
  return env.Undefined();
}

Napi::Value AudioData::Clone(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Algorithm
  /*
   * See spec/context file.
   */

  // TODO(impl): Implement method logic
  return env.Undefined();
}

Napi::Value AudioData::Close(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Algorithm
  /*
   * See spec/context file.
   */

  // TODO(impl): Implement method logic
  return env.Undefined();
}

}  // namespace webcodecs
