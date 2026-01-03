#include "encoded_audio_chunk.h"

#include <cstring>

#include "error_builder.h"
#include "shared/buffer_utils.h"

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
                                        InstanceMethod<&EncodedAudioChunk::SerializeForTransfer>("serializeForTransfer"),
                                    });

  constructor = Napi::Persistent(func);
  constructor.SuppressDestruct();
  exports.Set("EncodedAudioChunk", func);
  return exports;
}

EncodedAudioChunk::EncodedAudioChunk(const Napi::CallbackInfo& info) : Napi::ObjectWrap<EncodedAudioChunk>(info) {
  Napi::Env env = info.Env();

  // [SPEC] Constructor accepts EncodedAudioChunkInit
  // { type: "key"|"delta", timestamp: number, duration?: number, data: BufferSource }

  // Internal construction - packet_ will be set by CreateFromPacket
  if (info.Length() == 0) {
    return;
  }

  if (!info[0].IsObject()) {
    Napi::TypeError::New(env, "EncodedAudioChunkInit is required").ThrowAsJavaScriptException();
    return;
  }

  Napi::Object init = info[0].As<Napi::Object>();

  // Required: type
  if (!init.Has("type") || !init.Get("type").IsString()) {
    Napi::TypeError::New(env, "type is required and must be a string").ThrowAsJavaScriptException();
    return;
  }
  type_ = init.Get("type").As<Napi::String>().Utf8Value();
  if (type_ != "key" && type_ != "delta") {
    Napi::TypeError::New(env, "type must be 'key' or 'delta'").ThrowAsJavaScriptException();
    return;
  }

  // Required: timestamp
  if (!init.Has("timestamp") || !init.Get("timestamp").IsNumber()) {
    Napi::TypeError::New(env, "timestamp is required and must be a number").ThrowAsJavaScriptException();
    return;
  }
  timestamp_ = init.Get("timestamp").As<Napi::Number>().Int64Value();

  // Optional: duration
  if (init.Has("duration") && init.Get("duration").IsNumber()) {
    duration_ = init.Get("duration").As<Napi::Number>().Int64Value();
  }

  // Required: data
  if (!init.Has("data")) {
    Napi::TypeError::New(env, "data is required").ThrowAsJavaScriptException();
    return;
  }

  const uint8_t* data = nullptr;
  size_t size = 0;
  if (!buffer_utils::ExtractBufferData(init.Get("data"), &data, &size) || !data || size == 0) {
    Napi::TypeError::New(env, "data must be a non-empty BufferSource").ThrowAsJavaScriptException();
    return;
  }

  // Create packet and copy data
  packet_ = raii::MakeAvPacket();
  if (!packet_) {
    Napi::Error::New(env, "Failed to allocate packet").ThrowAsJavaScriptException();
    return;
  }

  if (av_new_packet(packet_.get(), static_cast<int>(size)) < 0) {
    Napi::Error::New(env, "Failed to allocate packet data").ThrowAsJavaScriptException();
    packet_.reset();
    return;
  }

  std::memcpy(packet_->data, data, size);
  packet_->pts = timestamp_;
  packet_->dts = timestamp_;
  if (duration_.has_value()) {
    packet_->duration = duration_.value();
  }
  if (type_ == "key") {
    packet_->flags |= AV_PKT_FLAG_KEY;
  }
}

EncodedAudioChunk::~EncodedAudioChunk() { Release(); }

void EncodedAudioChunk::Release() {
  // Release packet (RAII handles av_packet_unref)
  packet_.reset();
}

// =============================================================================
// FACTORY METHOD
// =============================================================================

Napi::Object EncodedAudioChunk::CreateFromPacket(Napi::Env env, const AVPacket* pkt,
                                                  bool is_key_frame, int64_t timestamp_us) {
  if (!pkt || !pkt->data || pkt->size <= 0) {
    return Napi::Object();
  }

  // Create new instance via constructor reference
  Napi::Object obj = constructor.New({});
  if (obj.IsEmpty()) {
    return Napi::Object();
  }

  EncodedAudioChunk* chunk = Napi::ObjectWrap<EncodedAudioChunk>::Unwrap(obj);
  if (!chunk) {
    return Napi::Object();
  }

  // Clone the packet (we don't own the input)
  chunk->packet_ = raii::CloneAvPacket(pkt);
  if (!chunk->packet_) {
    return Napi::Object();
  }

  // Set properties
  chunk->type_ = is_key_frame ? "key" : "delta";
  chunk->timestamp_ = timestamp_us;

  // Duration from packet if available
  if (pkt->duration > 0) {
    chunk->duration_ = pkt->duration;
  }

  return obj;
}

// =============================================================================
// ATTRIBUTE ACCESSORS
// =============================================================================

Napi::Value EncodedAudioChunk::GetType(const Napi::CallbackInfo& info) {
  return Napi::String::New(info.Env(), type_);
}

Napi::Value EncodedAudioChunk::GetTimestamp(const Napi::CallbackInfo& info) {
  return Napi::Number::New(info.Env(), static_cast<double>(timestamp_));
}

Napi::Value EncodedAudioChunk::GetDuration(const Napi::CallbackInfo& info) {
  if (duration_.has_value()) {
    return Napi::Number::New(info.Env(), static_cast<double>(duration_.value()));
  }
  return info.Env().Null();
}

Napi::Value EncodedAudioChunk::GetByteLength(const Napi::CallbackInfo& info) {
  if (!packet_ || !packet_->data) {
    return Napi::Number::New(info.Env(), 0);
  }
  return Napi::Number::New(info.Env(), static_cast<double>(packet_->size));
}

// =============================================================================
// METHODS
// =============================================================================

Napi::Value EncodedAudioChunk::CopyTo(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Copies the encoded audio data to the destination buffer.

  if (!packet_ || !packet_->data) {
    Napi::Error error = Napi::Error::New(env, "InvalidStateError: chunk is closed or empty");
    error.Set("name", Napi::String::New(env, "InvalidStateError"));
    error.ThrowAsJavaScriptException();
    return env.Undefined();
  }

  if (info.Length() < 1) {
    Napi::TypeError::New(env, "destination is required").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  // Extract destination buffer
  uint8_t* dest_data = nullptr;
  size_t dest_size = 0;

  Napi::Value dest = info[0];
  if (dest.IsArrayBuffer()) {
    Napi::ArrayBuffer ab = dest.As<Napi::ArrayBuffer>();
    dest_data = static_cast<uint8_t*>(ab.Data());
    dest_size = ab.ByteLength();
  } else if (dest.IsTypedArray()) {
    Napi::TypedArray ta = dest.As<Napi::TypedArray>();
    dest_data = static_cast<uint8_t*>(ta.ArrayBuffer().Data()) + ta.ByteOffset();
    dest_size = ta.ByteLength();
  } else if (dest.IsDataView()) {
    Napi::DataView dv = dest.As<Napi::DataView>();
    dest_data = static_cast<uint8_t*>(dv.ArrayBuffer().Data()) + dv.ByteOffset();
    dest_size = dv.ByteLength();
  } else {
    Napi::TypeError::New(env, "destination must be an ArrayBuffer, TypedArray, or DataView")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }

  size_t required = static_cast<size_t>(packet_->size);
  if (dest_size < required) {
    Napi::TypeError::New(env, "destination buffer is too small").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  std::memcpy(dest_data, packet_->data, required);
  return env.Undefined();
}

Napi::Value EncodedAudioChunk::SerializeForTransfer(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // EncodedAudioChunk is immutable - it doesn't have close() or [[Detached]]
  // Transfer/serialization both just create a clone (via av_packet_ref)
  //
  // The 'transfer' parameter is accepted for API consistency but has no effect
  // since EncodedAudioChunk doesn't support detachment.

  if (!packet_ || !packet_->data) {
    errors::ThrowDataCloneError(env, "EncodedAudioChunk has no data");
    return env.Undefined();
  }

  // Clone via CreateFromPacket (which uses av_packet_ref)
  bool is_key = (packet_->flags & AV_PKT_FLAG_KEY) != 0;
  return CreateFromPacket(env, packet_.get(), is_key, timestamp_);
}

}  // namespace webcodecs
