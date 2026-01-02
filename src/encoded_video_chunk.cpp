#include "encoded_video_chunk.h"

namespace webcodecs {

Napi::FunctionReference EncodedVideoChunk::constructor;

Napi::Object EncodedVideoChunk::Init(Napi::Env env, Napi::Object exports) {
  Napi::Function func = DefineClass(env, "EncodedVideoChunk",
                                    {
                                        InstanceAccessor<&EncodedVideoChunk::GetType>("type"),
                                        InstanceAccessor<&EncodedVideoChunk::GetTimestamp>("timestamp"),
                                        InstanceAccessor<&EncodedVideoChunk::GetDuration>("duration"),
                                        InstanceAccessor<&EncodedVideoChunk::GetByteLength>("byteLength"),
                                        InstanceMethod<&EncodedVideoChunk::CopyTo>("copyTo"),
                                    });

  constructor = Napi::Persistent(func);
  constructor.SuppressDestruct();
  exports.Set("EncodedVideoChunk", func);
  return exports;
}

EncodedVideoChunk::EncodedVideoChunk(const Napi::CallbackInfo& info) : Napi::ObjectWrap<EncodedVideoChunk>(info) {
  Napi::Env env = info.Env();

  // [SPEC] Constructor Algorithm
  /*
   * Initialize internal slots.
   */

  // TODO(impl): Implement Constructor & Resource Allocation
}

EncodedVideoChunk::~EncodedVideoChunk() { Release(); }

void EncodedVideoChunk::Release() {
  // Release packet (RAII handles av_packet_unref)
  packet_.reset();
}

// --- Attributes ---

Napi::Value EncodedVideoChunk::GetType(const Napi::CallbackInfo& info) {
  // TODO(impl): Return type
  return info.Env().Null();
}

Napi::Value EncodedVideoChunk::GetTimestamp(const Napi::CallbackInfo& info) {
  // TODO(impl): Return timestamp
  return info.Env().Null();
}

Napi::Value EncodedVideoChunk::GetDuration(const Napi::CallbackInfo& info) {
  // TODO(impl): Return duration
  return info.Env().Null();
}

Napi::Value EncodedVideoChunk::GetByteLength(const Napi::CallbackInfo& info) {
  // TODO(impl): Return byteLength
  return info.Env().Null();
}

// --- Methods ---

Napi::Value EncodedVideoChunk::CopyTo(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Algorithm
  /*
   * See spec/context file.
   */

  // TODO(impl): Implement method logic
  return env.Undefined();
}

}  // namespace webcodecs
