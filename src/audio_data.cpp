#include "audio_data.h"

#include <cstring>

#include "error_builder.h"

namespace webcodecs {

Napi::FunctionReference AudioData::constructor;

// Helper: Map FFmpeg sample format to WebCodecs AudioSampleFormat
static std::string AvFormatToWebCodecs(AVSampleFormat fmt) {
  switch (fmt) {
    case AV_SAMPLE_FMT_U8:
      return "u8";
    case AV_SAMPLE_FMT_U8P:
      return "u8-planar";
    case AV_SAMPLE_FMT_S16:
      return "s16";
    case AV_SAMPLE_FMT_S16P:
      return "s16-planar";
    case AV_SAMPLE_FMT_S32:
      return "s32";
    case AV_SAMPLE_FMT_S32P:
      return "s32-planar";
    case AV_SAMPLE_FMT_FLT:
      return "f32";
    case AV_SAMPLE_FMT_FLTP:
      return "f32-planar";
    default:
      return "f32";  // Default to float
  }
}

// Helper: Map WebCodecs AudioSampleFormat to FFmpeg sample format
static AVSampleFormat WebCodecsToAvFormat(const std::string& format) {
  if (format == "u8") return AV_SAMPLE_FMT_U8;
  if (format == "u8-planar") return AV_SAMPLE_FMT_U8P;
  if (format == "s16") return AV_SAMPLE_FMT_S16;
  if (format == "s16-planar") return AV_SAMPLE_FMT_S16P;
  if (format == "s32") return AV_SAMPLE_FMT_S32;
  if (format == "s32-planar") return AV_SAMPLE_FMT_S32P;
  if (format == "f32") return AV_SAMPLE_FMT_FLT;
  if (format == "f32-planar") return AV_SAMPLE_FMT_FLTP;
  return AV_SAMPLE_FMT_NONE;
}

// Helper: Get bytes per sample for a format
static int GetBytesPerSample(AVSampleFormat fmt) {
  return av_get_bytes_per_sample(fmt);
}

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
                                        InstanceMethod<&AudioData::SerializeForTransfer>("serializeForTransfer"),
                                    });

  constructor = Napi::Persistent(func);
  constructor.SuppressDestruct();
  exports.Set("AudioData", func);
  return exports;
}

AudioData::AudioData(const Napi::CallbackInfo& info) : Napi::ObjectWrap<AudioData>(info) {
  Napi::Env env = info.Env();

  // [SPEC 9.2.2] AudioData Constructor Algorithm
  // Receives an AudioDataInit object to create from raw audio data

  if (info.Length() < 1) {
    // Default construction - will be populated by CreateFromFrame
    return;
  }

  if (!info[0].IsObject()) {
    errors::ThrowTypeError(env, "AudioDataInit is required");
    return;
  }

  Napi::Object init = info[0].As<Napi::Object>();

  // [SPEC] Step 1: Validate AudioDataInit
  // Required: format
  if (!init.Has("format") || !init.Get("format").IsString()) {
    errors::ThrowTypeError(env, "format is required and must be a string");
    return;
  }
  std::string format_str = init.Get("format").As<Napi::String>().Utf8Value();

  // Validate format string
  AVSampleFormat av_fmt = WebCodecsToAvFormat(format_str);
  if (av_fmt == AV_SAMPLE_FMT_NONE) {
    errors::ThrowTypeError(env, "Invalid AudioSampleFormat: " + format_str);
    return;
  }

  // Required: sampleRate (must be > 0)
  if (!init.Has("sampleRate") || !init.Get("sampleRate").IsNumber()) {
    errors::ThrowTypeError(env, "sampleRate is required and must be a number");
    return;
  }
  int sample_rate = init.Get("sampleRate").As<Napi::Number>().Int32Value();
  if (sample_rate <= 0) {
    errors::ThrowTypeError(env, "sampleRate must be greater than 0");
    return;
  }

  // Required: numberOfFrames (must be > 0)
  if (!init.Has("numberOfFrames") || !init.Get("numberOfFrames").IsNumber()) {
    errors::ThrowTypeError(env, "numberOfFrames is required and must be a number");
    return;
  }
  int num_frames = init.Get("numberOfFrames").As<Napi::Number>().Int32Value();
  if (num_frames <= 0) {
    errors::ThrowTypeError(env, "numberOfFrames must be greater than 0");
    return;
  }

  // Required: numberOfChannels (must be > 0)
  if (!init.Has("numberOfChannels") || !init.Get("numberOfChannels").IsNumber()) {
    errors::ThrowTypeError(env, "numberOfChannels is required and must be a number");
    return;
  }
  int num_channels = init.Get("numberOfChannels").As<Napi::Number>().Int32Value();
  if (num_channels <= 0) {
    errors::ThrowTypeError(env, "numberOfChannels must be greater than 0");
    return;
  }

  // Required: timestamp
  if (!init.Has("timestamp") || !init.Get("timestamp").IsNumber()) {
    errors::ThrowTypeError(env, "timestamp is required and must be a number");
    return;
  }
  int64_t timestamp = init.Get("timestamp").As<Napi::Number>().Int64Value();

  // Required: data (BufferSource)
  if (!init.Has("data")) {
    errors::ThrowTypeError(env, "data is required");
    return;
  }

  Napi::Value data_value = init.Get("data");
  const uint8_t* src_data = nullptr;
  size_t data_size = 0;

  if (data_value.IsArrayBuffer()) {
    Napi::ArrayBuffer ab = data_value.As<Napi::ArrayBuffer>();
    src_data = static_cast<const uint8_t*>(ab.Data());
    data_size = ab.ByteLength();
  } else if (data_value.IsTypedArray()) {
    Napi::TypedArray ta = data_value.As<Napi::TypedArray>();
    Napi::ArrayBuffer ab = ta.ArrayBuffer();
    src_data = static_cast<const uint8_t*>(ab.Data()) + ta.ByteOffset();
    data_size = ta.ByteLength();
  } else if (data_value.IsDataView()) {
    Napi::DataView dv = data_value.As<Napi::DataView>();
    Napi::ArrayBuffer ab = dv.ArrayBuffer();
    src_data = static_cast<const uint8_t*>(ab.Data()) + dv.ByteOffset();
    data_size = dv.ByteLength();
  } else {
    errors::ThrowTypeError(env, "data must be a BufferSource (ArrayBuffer, TypedArray, or DataView)");
    return;
  }

  // [SPEC] Validate data has enough bytes
  // totalSamples = numberOfFrames * numberOfChannels
  // bytesPerSample = bytes per sample for format
  // totalSize = bytesPerSample * totalSamples
  int bytes_per_sample = av_get_bytes_per_sample(av_fmt);
  size_t total_samples = static_cast<size_t>(num_frames) * static_cast<size_t>(num_channels);
  size_t required_size = static_cast<size_t>(bytes_per_sample) * total_samples;

  if (data_size < required_size) {
    errors::ThrowTypeError(env, "data buffer is too small");
    return;
  }

  // [SPEC] Step 4: Create the AudioData
  // Create AVFrame with audio parameters
  frame_ = raii::MakeAvFrame();
  if (!frame_) {
    errors::ThrowTypeError(env, "Failed to allocate audio frame");
    return;
  }

  frame_->format = av_fmt;
  frame_->sample_rate = sample_rate;
  frame_->nb_samples = num_frames;

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(59, 37, 100)
  // FFmpeg 5.1+ uses ch_layout
  av_channel_layout_default(&frame_->ch_layout, num_channels);
#else
  // Older FFmpeg uses channels and channel_layout
  frame_->channels = num_channels;
  frame_->channel_layout = av_get_default_channel_layout(num_channels);
#endif

  // Allocate frame buffer
  int ret = av_frame_get_buffer(frame_.get(), 0);
  if (ret < 0) {
    errors::ThrowTypeError(env, "Failed to allocate audio frame buffer");
    frame_.reset();
    return;
  }

  // Copy data into frame
  bool is_planar = av_sample_fmt_is_planar(av_fmt);

  if (is_planar) {
    // For planar formats, data is arranged as [ch0_samples][ch1_samples]...
    size_t plane_size = static_cast<size_t>(num_frames) * static_cast<size_t>(bytes_per_sample);
    for (int ch = 0; ch < num_channels; ch++) {
      std::memcpy(frame_->data[ch], src_data + ch * plane_size, plane_size);
    }
  } else {
    // For interleaved formats, all data is in data[0]
    std::memcpy(frame_->data[0], src_data, required_size);
  }

  // Set internal state
  format_ = format_str;
  timestamp_ = timestamp;
}

AudioData::~AudioData() { Release(); }

void AudioData::Release() {
  // Mark as closed (atomic, thread-safe)
  closed_.store(true, std::memory_order_release);
  // Release frame (RAII handles av_frame_unref)
  frame_.reset();
}

Napi::Object AudioData::CreateFromFrame(Napi::Env env, const AVFrame* frame, int64_t timestamp_us) {
  if (!frame) {
    return Napi::Object();
  }

  // Create new AudioData instance
  Napi::Object obj = constructor.New({});
  AudioData* audioData = Napi::ObjectWrap<AudioData>::Unwrap(obj);

  // Clone the frame (we don't own the input)
  audioData->frame_ = raii::CloneAvFrame(frame);
  if (!audioData->frame_) {
    return Napi::Object();
  }

  // Set timestamp
  audioData->timestamp_ = timestamp_us;

  // Determine format string
  AVSampleFormat fmt = static_cast<AVSampleFormat>(frame->format);
  audioData->format_ = AvFormatToWebCodecs(fmt);

  return obj;
}

// --- Attributes ---

Napi::Value AudioData::GetFormat(const Napi::CallbackInfo& info) {
  if (closed_.load(std::memory_order_acquire) || !frame_) {
    return info.Env().Null();
  }
  return Napi::String::New(info.Env(), format_);
}

Napi::Value AudioData::GetSampleRate(const Napi::CallbackInfo& info) {
  if (closed_.load(std::memory_order_acquire) || !frame_) {
    return info.Env().Null();
  }
  return Napi::Number::New(info.Env(), frame_->sample_rate);
}

Napi::Value AudioData::GetNumberOfFrames(const Napi::CallbackInfo& info) {
  if (closed_.load(std::memory_order_acquire) || !frame_) {
    return info.Env().Null();
  }
  // In FFmpeg, nb_samples is the number of audio samples per channel
  return Napi::Number::New(info.Env(), frame_->nb_samples);
}

Napi::Value AudioData::GetNumberOfChannels(const Napi::CallbackInfo& info) {
  if (closed_.load(std::memory_order_acquire) || !frame_) {
    return info.Env().Null();
  }
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(59, 37, 100)
  return Napi::Number::New(info.Env(), frame_->ch_layout.nb_channels);
#else
  return Napi::Number::New(info.Env(), frame_->channels);
#endif
}

Napi::Value AudioData::GetDuration(const Napi::CallbackInfo& info) {
  if (closed_.load(std::memory_order_acquire) || !frame_) {
    return info.Env().Null();
  }
  // Duration in microseconds = (nb_samples / sample_rate) * 1_000_000
  if (frame_->sample_rate <= 0) {
    return Napi::Number::New(info.Env(), 0);
  }
  double duration_us = (static_cast<double>(frame_->nb_samples) / frame_->sample_rate) * 1000000.0;
  return Napi::Number::New(info.Env(), static_cast<int64_t>(duration_us));
}

Napi::Value AudioData::GetTimestamp(const Napi::CallbackInfo& info) {
  if (closed_.load(std::memory_order_acquire)) {
    return info.Env().Null();
  }
  return Napi::Number::New(info.Env(), static_cast<double>(timestamp_));
}

// --- Methods ---

/**
 * Helper: Compute Copy Element Count (with options)
 * Per W3C WebCodecs spec section 9.2.5
 *
 * Returns the element count, or -1 on error (throws JS exception).
 */
static int ComputeCopyElementCount(Napi::Env env, const AVFrame* frame,
                                   const std::string& src_format,
                                   const Napi::Object& options,
                                   std::string& out_dest_format) {
  // Get number of channels
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(59, 37, 100)
  int num_channels = frame->ch_layout.nb_channels;
#else
  int num_channels = frame->channels;
#endif
  int num_frames = frame->nb_samples;

  // Step 1-2: destFormat defaults to [[format]], may be overridden by options.format
  out_dest_format = src_format;
  if (options.Has("format") && options.Get("format").IsString()) {
    out_dest_format = options.Get("format").As<Napi::String>().Utf8Value();
  }

  // Determine if dest format is planar or interleaved
  bool dest_is_planar = (out_dest_format.find("-planar") != std::string::npos);

  // Step 3: Validate planeIndex (required)
  if (!options.Has("planeIndex") || !options.Get("planeIndex").IsNumber()) {
    Napi::TypeError::New(env, "planeIndex is required").ThrowAsJavaScriptException();
    return -1;
  }
  int plane_index = options.Get("planeIndex").As<Napi::Number>().Int32Value();

  // Step 3: If destFormat is interleaved and planeIndex > 0, throw RangeError
  if (!dest_is_planar && plane_index > 0) {
    Napi::RangeError::New(env, "planeIndex must be 0 for interleaved format").ThrowAsJavaScriptException();
    return -1;
  }

  // Step 4: If destFormat is planar and planeIndex >= numberOfChannels, throw RangeError
  if (dest_is_planar && plane_index >= num_channels) {
    Napi::RangeError::New(env, "planeIndex exceeds number of channels").ThrowAsJavaScriptException();
    return -1;
  }

  // Step 5: Check format conversion support
  // Per spec: conversion to f32-planar MUST always be supported
  // We support all conversions via libswresample
  AVSampleFormat dest_av_fmt = WebCodecsToAvFormat(out_dest_format);
  if (dest_av_fmt == AV_SAMPLE_FMT_NONE) {
    errors::ThrowNotSupportedError(env, "Unsupported destination format: " + out_dest_format);
    return -1;
  }

  // Step 6: frameCount is the total frames in the source (plane)
  int total_frame_count = num_frames;

  // Step 7: Get frameOffset (default 0)
  int frame_offset = 0;
  if (options.Has("frameOffset") && options.Get("frameOffset").IsNumber()) {
    frame_offset = options.Get("frameOffset").As<Napi::Number>().Int32Value();
  }

  // Step 7: If frameOffset >= frameCount, throw RangeError
  if (frame_offset >= total_frame_count) {
    Napi::RangeError::New(env, "frameOffset exceeds number of frames").ThrowAsJavaScriptException();
    return -1;
  }

  // Step 8: copyFrameCount = frameCount - frameOffset
  int copy_frame_count = total_frame_count - frame_offset;

  // Step 9: If options.frameCount exists
  if (options.Has("frameCount") && options.Get("frameCount").IsNumber()) {
    int requested_frame_count = options.Get("frameCount").As<Napi::Number>().Int32Value();
    // Step 9.1: If requested > available, throw RangeError
    if (requested_frame_count > copy_frame_count) {
      Napi::RangeError::New(env, "frameCount exceeds available frames").ThrowAsJavaScriptException();
      return -1;
    }
    // Step 9.2: Use requested count
    copy_frame_count = requested_frame_count;
  }

  // Step 10: elementCount = copyFrameCount
  int element_count = copy_frame_count;

  // Step 11: If destFormat is interleaved, multiply by numberOfChannels
  if (!dest_is_planar) {
    element_count *= num_channels;
  }

  // Step 12: return elementCount
  return element_count;
}

Napi::Value AudioData::AllocationSize(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (closed_.load(std::memory_order_acquire) || !frame_) {
    errors::ThrowInvalidStateError(env, "AudioData is closed");
    return env.Undefined();
  }

  // [SPEC 9.2.4] allocationSize(options)
  // Step 1: If [[Detached]], throw InvalidStateError (handled above)

  // Parse options
  if (info.Length() < 1 || !info[0].IsObject()) {
    Napi::TypeError::New(env, "options object is required").ThrowAsJavaScriptException();
    return env.Undefined();
  }
  Napi::Object options = info[0].As<Napi::Object>();

  // Step 2: Compute copy element count
  std::string dest_format;
  int element_count = ComputeCopyElementCount(env, frame_.get(), format_, options, dest_format);
  if (element_count < 0) {
    return env.Undefined();  // Exception already thrown
  }

  // Step 3-4: Get dest format (already computed)
  // Step 5: Get bytes per sample
  AVSampleFormat dest_av_fmt = WebCodecsToAvFormat(dest_format);
  int bytes_per_sample = GetBytesPerSample(dest_av_fmt);

  // Step 6: Return bytesPerSample * elementCount
  int64_t total_size = static_cast<int64_t>(bytes_per_sample) * element_count;

  return Napi::Number::New(env, static_cast<double>(total_size));
}

Napi::Value AudioData::CopyTo(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC 9.2.4] copyTo(destination, options)
  // Step 1: If [[Detached]], throw InvalidStateError
  if (closed_.load(std::memory_order_acquire) || !frame_) {
    errors::ThrowInvalidStateError(env, "AudioData is closed");
    return env.Undefined();
  }

  // Validate arguments
  if (info.Length() < 2) {
    Napi::TypeError::New(env, "destination and options are required").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  // Get destination buffer
  if (!info[0].IsArrayBuffer() && !info[0].IsTypedArray()) {
    Napi::TypeError::New(env, "destination must be an ArrayBuffer or TypedArray").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  uint8_t* dest_data = nullptr;
  size_t dest_size = 0;

  if (info[0].IsArrayBuffer()) {
    Napi::ArrayBuffer ab = info[0].As<Napi::ArrayBuffer>();
    dest_data = static_cast<uint8_t*>(ab.Data());
    dest_size = ab.ByteLength();
  } else {
    Napi::TypedArray ta = info[0].As<Napi::TypedArray>();
    dest_data = static_cast<uint8_t*>(ta.ArrayBuffer().Data()) + ta.ByteOffset();
    dest_size = ta.ByteLength();
  }

  // Parse options (required)
  if (!info[1].IsObject()) {
    Napi::TypeError::New(env, "options object is required").ThrowAsJavaScriptException();
    return env.Undefined();
  }
  Napi::Object options = info[1].As<Napi::Object>();

  // Step 2: Compute copy element count
  std::string dest_format;
  int element_count = ComputeCopyElementCount(env, frame_.get(), format_, options, dest_format);
  if (element_count < 0) {
    return env.Undefined();  // Exception already thrown
  }

  // Step 3-5: Get dest format and bytes per sample
  AVSampleFormat dest_av_fmt = WebCodecsToAvFormat(dest_format);
  int dest_bytes_per_sample = GetBytesPerSample(dest_av_fmt);

  // Step 6: Check destination size
  size_t required_size = static_cast<size_t>(dest_bytes_per_sample) * element_count;
  if (dest_size < required_size) {
    Napi::RangeError::New(env, "destination buffer too small").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  // Get copy parameters
  int plane_index = options.Get("planeIndex").As<Napi::Number>().Int32Value();
  int frame_offset = 0;
  if (options.Has("frameOffset") && options.Get("frameOffset").IsNumber()) {
    frame_offset = options.Get("frameOffset").As<Napi::Number>().Int32Value();
  }

  // Determine frame count to copy
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(59, 37, 100)
  int num_channels = frame_->ch_layout.nb_channels;
#else
  int num_channels = frame_->channels;
#endif
  int copy_frame_count = frame_->nb_samples - frame_offset;
  if (options.Has("frameCount") && options.Get("frameCount").IsNumber()) {
    copy_frame_count = options.Get("frameCount").As<Napi::Number>().Int32Value();
  }

  // Source format info
  AVSampleFormat src_av_fmt = static_cast<AVSampleFormat>(frame_->format);
  int src_bytes_per_sample = GetBytesPerSample(src_av_fmt);
  bool src_is_planar = av_sample_fmt_is_planar(src_av_fmt);
  bool dest_is_planar = av_sample_fmt_is_planar(dest_av_fmt);

  // Step 7-9: Copy with optional format conversion
  // Check if format conversion is needed
  bool needs_conversion = (src_av_fmt != dest_av_fmt);

  if (!needs_conversion) {
    // No conversion needed - direct copy with offset handling
    if (dest_is_planar) {
      // Planar destination: copy single plane
      const uint8_t* src_ptr = frame_->data[plane_index] + frame_offset * src_bytes_per_sample;
      std::memcpy(dest_data, src_ptr, copy_frame_count * src_bytes_per_sample);
    } else {
      // Interleaved destination
      if (src_is_planar) {
        // Planar source -> Interleaved destination (interleave)
        uint8_t* dest = dest_data;
        for (int sample = 0; sample < copy_frame_count; sample++) {
          for (int ch = 0; ch < num_channels; ch++) {
            const uint8_t* src_ptr = frame_->data[ch] + (frame_offset + sample) * src_bytes_per_sample;
            std::memcpy(dest, src_ptr, src_bytes_per_sample);
            dest += src_bytes_per_sample;
          }
        }
      } else {
        // Interleaved source -> Interleaved destination (direct copy)
        const uint8_t* src_ptr = frame_->data[0] + frame_offset * src_bytes_per_sample * num_channels;
        std::memcpy(dest_data, src_ptr, copy_frame_count * src_bytes_per_sample * num_channels);
      }
    }
  } else {
    // Format conversion needed - use libswresample
    // Build channel layout
    AVChannelLayout ch_layout;
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(59, 37, 100)
    av_channel_layout_copy(&ch_layout, &frame_->ch_layout);
#else
    av_channel_layout_default(&ch_layout, num_channels);
#endif

    // Create SwrContext for conversion
    raii::SwrContextPtr swr = raii::MakeSwrContextInitialized(
        &ch_layout, dest_av_fmt, frame_->sample_rate,
        &ch_layout, src_av_fmt, frame_->sample_rate);

    if (!swr) {
      errors::ThrowNotSupportedError(env, "Failed to initialize audio format converter");
      av_channel_layout_uninit(&ch_layout);
      return env.Undefined();
    }

    // Prepare input pointers with offset
    const uint8_t* in_data[AV_NUM_DATA_POINTERS];
    for (int i = 0; i < AV_NUM_DATA_POINTERS; i++) {
      if (frame_->data[i]) {
        if (src_is_planar) {
          in_data[i] = frame_->data[i] + frame_offset * src_bytes_per_sample;
        } else if (i == 0) {
          in_data[i] = frame_->data[0] + frame_offset * src_bytes_per_sample * num_channels;
        } else {
          in_data[i] = nullptr;
        }
      } else {
        in_data[i] = nullptr;
      }
    }

    // Prepare output pointers
    uint8_t* out_data[AV_NUM_DATA_POINTERS] = {nullptr};
    if (dest_is_planar) {
      // For planar output, we only populate the requested plane
      // But swr_convert outputs all planes, so we need temporary buffers
      // Actually, for copyTo we only copy ONE plane at a time for planar output
      // So we need to handle this specially

      // Allocate temporary buffer for all planes, then extract the one we need
      int out_linesize;
      uint8_t** temp_out = nullptr;
      int ret = av_samples_alloc_array_and_samples(&temp_out, &out_linesize, num_channels,
                                                    copy_frame_count, dest_av_fmt, 0);
      if (ret < 0) {
        errors::ThrowNotSupportedError(env, "Failed to allocate conversion buffer");
        av_channel_layout_uninit(&ch_layout);
        return env.Undefined();
      }

      // Convert
      ret = swr_convert(swr.get(), temp_out, copy_frame_count,
                        in_data, copy_frame_count);

      if (ret < 0) {
        av_freep(&temp_out[0]);
        av_freep(&temp_out);
        errors::ThrowNotSupportedError(env, "Audio format conversion failed");
        av_channel_layout_uninit(&ch_layout);
        return env.Undefined();
      }

      // Copy the requested plane to destination
      std::memcpy(dest_data, temp_out[plane_index], copy_frame_count * dest_bytes_per_sample);

      av_freep(&temp_out[0]);
      av_freep(&temp_out);
    } else {
      // Interleaved output - output directly to destination
      out_data[0] = dest_data;

      int ret = swr_convert(swr.get(), out_data, copy_frame_count,
                            in_data, copy_frame_count);

      if (ret < 0) {
        errors::ThrowNotSupportedError(env, "Audio format conversion failed");
        av_channel_layout_uninit(&ch_layout);
        return env.Undefined();
      }
    }

    av_channel_layout_uninit(&ch_layout);
  }

  return env.Undefined();
}

Napi::Value AudioData::Clone(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (closed_.load(std::memory_order_acquire) || !frame_) {
    errors::ThrowInvalidStateError(env, "AudioData is closed");
    return env.Undefined();
  }

  // Create a new AudioData with a cloned frame
  return CreateFromFrame(env, frame_.get(), timestamp_);
}

Napi::Value AudioData::Close(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  Release();

  return env.Undefined();
}

Napi::Value AudioData::SerializeForTransfer(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC 9.2.6] Transfer and Serialization
  //
  // Transfer steps:
  // 1. If [[Detached]] is true, throw DataCloneError
  // 2. Serialize all internal slots to dataHolder
  // 3. Run close() on this AudioData (detach source)
  //
  // Serialization steps (clone without detach):
  // 1. If [[Detached]] is true, throw DataCloneError
  // 2. Create new reference to resource (av_frame_ref)
  // 3. Copy all internal slots

  // Step 1: Check if detached (closed)
  if (closed_.load(std::memory_order_acquire)) {
    errors::ThrowDataCloneError(env, "Cannot transfer a closed AudioData");
    return env.Undefined();
  }

  if (!frame_) {
    errors::ThrowDataCloneError(env, "AudioData has no data");
    return env.Undefined();
  }

  // Parse transfer flag (default: false for serialization/clone semantics)
  bool transfer = false;
  if (info.Length() > 0 && info[0].IsBoolean()) {
    transfer = info[0].As<Napi::Boolean>().Value();
  }

  // Step 2: Create clone with new reference to underlying buffers
  // This uses av_frame_ref internally - zero-copy, refcount++
  Napi::Object cloned = CreateFromFrame(env, frame_.get(), timestamp_);
  if (cloned.IsEmpty()) {
    errors::ThrowDataCloneError(env, "Failed to serialize AudioData");
    return env.Undefined();
  }

  // Step 3: If transfer=true, close this AudioData (detach source)
  // The cloned AudioData now owns the only reference to the buffers
  if (transfer) {
    Release();
  }

  return cloned;
}

}  // namespace webcodecs
