#include "audio_data.h"

#include <cstring>

namespace webcodecs {

Napi::FunctionReference AudioData::constructor;

// Helper: Map FFmpeg sample format to WebCodecs AudioSampleFormat
static std::string AvFormatToWebCodecs(AVSampleFormat fmt) {
  switch (fmt) {
    case AV_SAMPLE_FMT_U8:
    case AV_SAMPLE_FMT_U8P:
      return "u8";
    case AV_SAMPLE_FMT_S16:
    case AV_SAMPLE_FMT_S16P:
      return "s16";
    case AV_SAMPLE_FMT_S32:
    case AV_SAMPLE_FMT_S32P:
      return "s32";
    case AV_SAMPLE_FMT_FLT:
    case AV_SAMPLE_FMT_FLTP:
      return "f32";
    default:
      return "f32";  // Default to float
  }
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
                                    });

  constructor = Napi::Persistent(func);
  constructor.SuppressDestruct();
  exports.Set("AudioData", func);
  return exports;
}

AudioData::AudioData(const Napi::CallbackInfo& info) : Napi::ObjectWrap<AudioData>(info) {
  Napi::Env env = info.Env();

  // [SPEC] Constructor Algorithm
  // The constructor can receive an AudioDataInit object to create from raw data
  // For now, we only support creation via CreateFromFrame factory

  if (info.Length() < 1) {
    // Default construction - will be populated by CreateFromFrame
    return;
  }

  // TODO(impl): Implement full constructor with AudioDataInit
}

AudioData::~AudioData() { Release(); }

void AudioData::Release() {
  // Release frame (RAII handles av_frame_unref)
  frame_.reset();
  closed_ = true;
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
  if (closed_ || !frame_) {
    return info.Env().Null();
  }
  return Napi::String::New(info.Env(), format_);
}

Napi::Value AudioData::GetSampleRate(const Napi::CallbackInfo& info) {
  if (closed_ || !frame_) {
    return info.Env().Null();
  }
  return Napi::Number::New(info.Env(), frame_->sample_rate);
}

Napi::Value AudioData::GetNumberOfFrames(const Napi::CallbackInfo& info) {
  if (closed_ || !frame_) {
    return info.Env().Null();
  }
  // In FFmpeg, nb_samples is the number of audio samples per channel
  return Napi::Number::New(info.Env(), frame_->nb_samples);
}

Napi::Value AudioData::GetNumberOfChannels(const Napi::CallbackInfo& info) {
  if (closed_ || !frame_) {
    return info.Env().Null();
  }
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(59, 37, 100)
  return Napi::Number::New(info.Env(), frame_->ch_layout.nb_channels);
#else
  return Napi::Number::New(info.Env(), frame_->channels);
#endif
}

Napi::Value AudioData::GetDuration(const Napi::CallbackInfo& info) {
  if (closed_ || !frame_) {
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
  if (closed_) {
    return info.Env().Null();
  }
  return Napi::Number::New(info.Env(), static_cast<double>(timestamp_));
}

// --- Methods ---

Napi::Value AudioData::AllocationSize(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (closed_ || !frame_) {
    Napi::Error::New(env, "InvalidStateError: AudioData is closed").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  // Calculate total size needed for the audio data
  AVSampleFormat fmt = static_cast<AVSampleFormat>(frame_->format);
  int bytes_per_sample = GetBytesPerSample(fmt);
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(59, 37, 100)
  int channels = frame_->ch_layout.nb_channels;
#else
  int channels = frame_->channels;
#endif

  // For interleaved formats, size = nb_samples * channels * bytes_per_sample
  // For planar formats, same total size but organized differently
  int64_t total_size = static_cast<int64_t>(frame_->nb_samples) * channels * bytes_per_sample;

  return Napi::Number::New(env, static_cast<double>(total_size));
}

Napi::Value AudioData::CopyTo(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (closed_ || !frame_) {
    Napi::Error::New(env, "InvalidStateError: AudioData is closed").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  if (info.Length() < 1) {
    Napi::TypeError::New(env, "destination is required").ThrowAsJavaScriptException();
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

  // Calculate required size
  AVSampleFormat fmt = static_cast<AVSampleFormat>(frame_->format);
  int bytes_per_sample = GetBytesPerSample(fmt);
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(59, 37, 100)
  int channels = frame_->ch_layout.nb_channels;
#else
  int channels = frame_->channels;
#endif

  size_t required_size = static_cast<size_t>(frame_->nb_samples) * channels * bytes_per_sample;

  if (dest_size < required_size) {
    Napi::RangeError::New(env, "destination buffer too small").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  // Copy data based on format (planar vs interleaved)
  bool is_planar = av_sample_fmt_is_planar(fmt);

  if (is_planar) {
    // For planar formats, data is in frame_->data[channel][sample]
    // We need to interleave it
    int plane_size = frame_->nb_samples * bytes_per_sample;
    uint8_t* dest = dest_data;

    for (int sample = 0; sample < frame_->nb_samples; sample++) {
      for (int ch = 0; ch < channels; ch++) {
        std::memcpy(dest, frame_->data[ch] + sample * bytes_per_sample, bytes_per_sample);
        dest += bytes_per_sample;
      }
    }
  } else {
    // For interleaved formats, just copy directly
    std::memcpy(dest_data, frame_->data[0], required_size);
  }

  return env.Undefined();
}

Napi::Value AudioData::Clone(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (closed_ || !frame_) {
    Napi::Error::New(env, "InvalidStateError: AudioData is closed").ThrowAsJavaScriptException();
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

}  // namespace webcodecs
