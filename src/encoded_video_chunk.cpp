#include "encoded_video_chunk.h"

#include <cstring>

#include "error_builder.h"

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
  // EncodedVideoChunkInit requires: type, timestamp, data
  // Optional: duration

  if (info.Length() < 1 || !info[0].IsObject()) {
    Napi::TypeError::New(env, "EncodedVideoChunkInit is required").ThrowAsJavaScriptException();
    return;
  }

  Napi::Object init = info[0].As<Napi::Object>();

  // Validate type
  if (!init.Has("type") || !init.Get("type").IsString()) {
    Napi::TypeError::New(env, "type is required and must be 'key' or 'delta'").ThrowAsJavaScriptException();
    return;
  }
  type_ = init.Get("type").As<Napi::String>().Utf8Value();
  if (type_ != "key" && type_ != "delta") {
    Napi::TypeError::New(env, "type must be 'key' or 'delta'").ThrowAsJavaScriptException();
    return;
  }

  // Validate timestamp
  if (!init.Has("timestamp") || !init.Get("timestamp").IsNumber()) {
    Napi::TypeError::New(env, "timestamp is required and must be a number").ThrowAsJavaScriptException();
    return;
  }
  timestamp_ = init.Get("timestamp").As<Napi::Number>().Int64Value();

  // Validate data (BufferSource)
  if (!init.Has("data")) {
    Napi::TypeError::New(env, "data is required").ThrowAsJavaScriptException();
    return;
  }

  Napi::Value dataValue = init.Get("data");
  const uint8_t* src_data = nullptr;
  size_t src_size = 0;

  if (dataValue.IsArrayBuffer()) {
    Napi::ArrayBuffer ab = dataValue.As<Napi::ArrayBuffer>();
    src_data = static_cast<const uint8_t*>(ab.Data());
    src_size = ab.ByteLength();
  } else if (dataValue.IsTypedArray()) {
    Napi::TypedArray ta = dataValue.As<Napi::TypedArray>();
    Napi::ArrayBuffer ab = ta.ArrayBuffer();
    src_data = static_cast<const uint8_t*>(ab.Data()) + ta.ByteOffset();
    src_size = ta.ByteLength();
  } else if (dataValue.IsDataView()) {
    Napi::DataView dv = dataValue.As<Napi::DataView>();
    Napi::ArrayBuffer ab = dv.ArrayBuffer();
    src_data = static_cast<const uint8_t*>(ab.Data()) + dv.ByteOffset();
    src_size = dv.ByteLength();
  } else {
    Napi::TypeError::New(env, "data must be a BufferSource (ArrayBuffer, TypedArray, or DataView)")
        .ThrowAsJavaScriptException();
    return;
  }

  // Optional: duration
  if (init.Has("duration") && init.Get("duration").IsNumber()) {
    duration_ = init.Get("duration").As<Napi::Number>().Int64Value();
  }

  // Create AVPacket and copy data
  packet_ = raii::MakeAvPacket();
  if (!packet_) {
    Napi::Error::New(env, "Failed to allocate packet").ThrowAsJavaScriptException();
    return;
  }

  // Allocate buffer and copy data into packet
  int ret = av_new_packet(packet_.get(), static_cast<int>(src_size));
  if (ret < 0) {
    Napi::Error::New(env, "Failed to allocate packet buffer").ThrowAsJavaScriptException();
    packet_.reset();
    return;
  }

  std::memcpy(packet_->data, src_data, src_size);

  // Set packet flags
  if (type_ == "key") {
    packet_->flags |= AV_PKT_FLAG_KEY;
  }

  // Set timestamps (in timebase units, assuming microseconds)
  packet_->pts = timestamp_;
  packet_->dts = timestamp_;
  if (duration_.has_value()) {
    packet_->duration = duration_.value();
  }
}

EncodedVideoChunk::~EncodedVideoChunk() { Release(); }

void EncodedVideoChunk::Release() {
  // Release packet (RAII handles av_packet_unref)
  packet_.reset();
}

// --- Factory Method ---

Napi::Object EncodedVideoChunk::CreateFromPacket(Napi::Env env, const AVPacket* pkt,
                                                  bool is_key_frame, int64_t timestamp_us) {
  if (!pkt || !pkt->data || pkt->size <= 0) {
    Napi::Error::New(env, "Invalid packet").ThrowAsJavaScriptException();
    return Napi::Object();
  }

  // Create init object with required fields
  Napi::Object init = Napi::Object::New(env);
  init.Set("type", Napi::String::New(env, is_key_frame ? "key" : "delta"));
  init.Set("timestamp", Napi::Number::New(env, static_cast<double>(timestamp_us)));

  // Copy packet data into ArrayBuffer
  Napi::ArrayBuffer dataBuffer = Napi::ArrayBuffer::New(env, pkt->size);
  std::memcpy(dataBuffer.Data(), pkt->data, pkt->size);
  init.Set("data", Napi::Uint8Array::New(env, pkt->size, dataBuffer, 0));

  // Optional: duration
  if (pkt->duration > 0) {
    init.Set("duration", Napi::Number::New(env, static_cast<double>(pkt->duration)));
  }

  // Create instance via constructor
  Napi::Object instance = constructor.New({init});
  return instance;
}

// --- Attributes ---

Napi::Value EncodedVideoChunk::GetType(const Napi::CallbackInfo& info) {
  // [SPEC] The type attribute indicates whether the chunk is a key chunk.
  return Napi::String::New(info.Env(), type_);
}

Napi::Value EncodedVideoChunk::GetTimestamp(const Napi::CallbackInfo& info) {
  // [SPEC] The timestamp attribute indicates the timestamp of the chunk in microseconds.
  return Napi::Number::New(info.Env(), static_cast<double>(timestamp_));
}

Napi::Value EncodedVideoChunk::GetDuration(const Napi::CallbackInfo& info) {
  // [SPEC] The duration attribute indicates the duration of the chunk in microseconds.
  // Returns null if duration was not provided.
  if (!duration_.has_value()) {
    return info.Env().Null();
  }
  return Napi::Number::New(info.Env(), static_cast<double>(duration_.value()));
}

Napi::Value EncodedVideoChunk::GetByteLength(const Napi::CallbackInfo& info) {
  // [SPEC] The byteLength attribute indicates the byte length of the chunk.
  if (!packet_ || !packet_->data) {
    return Napi::Number::New(info.Env(), 0);
  }
  return Napi::Number::New(info.Env(), static_cast<double>(packet_->size));
}

// --- Methods ---

Napi::Value EncodedVideoChunk::CopyTo(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] copyTo(destination)
  // Copies the encoded data into the destination BufferSource.

  if (info.Length() < 1) {
    errors::ThrowTypeError(env, "destination is required");
    return env.Undefined();
  }

  if (!packet_ || !packet_->data) {
    errors::ThrowInvalidStateError(env, "EncodedVideoChunk is closed");
    return env.Undefined();
  }

  Napi::Value destValue = info[0];
  uint8_t* dest_data = nullptr;
  size_t dest_size = 0;

  if (destValue.IsArrayBuffer()) {
    Napi::ArrayBuffer ab = destValue.As<Napi::ArrayBuffer>();
    dest_data = static_cast<uint8_t*>(ab.Data());
    dest_size = ab.ByteLength();
  } else if (destValue.IsTypedArray()) {
    Napi::TypedArray ta = destValue.As<Napi::TypedArray>();
    Napi::ArrayBuffer ab = ta.ArrayBuffer();
    dest_data = static_cast<uint8_t*>(ab.Data()) + ta.ByteOffset();
    dest_size = ta.ByteLength();
  } else if (destValue.IsDataView()) {
    Napi::DataView dv = destValue.As<Napi::DataView>();
    Napi::ArrayBuffer ab = dv.ArrayBuffer();
    dest_data = static_cast<uint8_t*>(ab.Data()) + dv.ByteOffset();
    dest_size = dv.ByteLength();
  } else {
    errors::ThrowTypeError(env, "destination must be a BufferSource (ArrayBuffer, TypedArray, or DataView)");
    return env.Undefined();
  }

  // Check destination size
  size_t required_size = static_cast<size_t>(packet_->size);
  if (dest_size < required_size) {
    errors::ThrowTypeError(env, "destination buffer is too small");
    return env.Undefined();
  }

  // Copy data
  std::memcpy(dest_data, packet_->data, required_size);

  return env.Undefined();
}

}  // namespace webcodecs
