#include "EncodedAudioChunk.h"

namespace webcodecs {

Napi::FunctionReference EncodedAudioChunk::constructor;

Napi::Object EncodedAudioChunk::Init(Napi::Env env, Napi::Object exports) {
  Napi::Function func = DefineClass(env, "EncodedAudioChunk",
                                    {
                                        InstanceAccessor<&EncodedAudioChunk::GetType>("type"),
                                        InstanceAccessor<&EncodedAudioChunk::GetTimestamp>("timestamp"),
                                        InstanceAccessor<&EncodedAudioChunk::GetDuration>("duration"),
                                        InstanceAccessor<&EncodedAudioChunk::GetByteLength>("byteLength"),
                                        InstanceMethod<&EncodedAudioChunk::CopyTo>("copyTo"),
                                    });

  constructor = Napi::Persistent(func);
  constructor.SuppressDestruct();
  exports.Set("EncodedAudioChunk", func);
  return exports;
}

EncodedAudioChunk::EncodedAudioChunk(const Napi::CallbackInfo& info) : Napi::ObjectWrap<EncodedAudioChunk>(info) {
  Napi::Env env = info.Env();

  // [SPEC] Constructor Algorithm
  /*
   * Initialize internal slots.
   */

  // TODO(impl): Implement Constructor & Resource Allocation
}

EncodedAudioChunk::~EncodedAudioChunk() { Release(); }

void EncodedAudioChunk::Release() {
  // Release packet (RAII handles av_packet_unref)
  packet_.reset();
}

// --- Attributes ---

Napi::Value EncodedAudioChunk::GetType(const Napi::CallbackInfo& info) {
  // TODO(impl): Return type
  return info.Env().Null();
}

Napi::Value EncodedAudioChunk::GetTimestamp(const Napi::CallbackInfo& info) {
  // TODO(impl): Return timestamp
  return info.Env().Null();
}

Napi::Value EncodedAudioChunk::GetDuration(const Napi::CallbackInfo& info) {
  // TODO(impl): Return duration
  return info.Env().Null();
}

Napi::Value EncodedAudioChunk::GetByteLength(const Napi::CallbackInfo& info) {
  // TODO(impl): Return byteLength
  return info.Env().Null();
}

// --- Methods ---

Napi::Value EncodedAudioChunk::CopyTo(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Algorithm
  /*
   * See spec/context file.
   */

  // TODO(impl): Implement method logic
  return env.Undefined();
}

}  // namespace webcodecs
