#pragma once
/**
 * buffer_utils.h - Buffer Operations for WebCodecs
 *
 * Provides utilities for copying data between JavaScript ArrayBuffers
 * and FFmpeg's AVFrame/AVPacket data structures.
 *
 * Design choice: COPY, not reference
 * - This implementation copies data rather than sharing pointers
 * - This is spec-compliant and prevents use-after-free bugs
 * - Zero-copy can be added later for internal transfers if needed
 */

// Only include napi.h when not in pure C++ testing mode
#ifndef WEBCODECS_TESTING
#include <napi.h>
#endif

#include <cstdint>
#include <cstring>
#include "../ffmpeg_raii.h"

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
#include <libavutil/samplefmt.h>
#include <libavutil/channel_layout.h>
}

namespace webcodecs {
namespace buffer_utils {

// =============================================================================
// VIDEO FRAME BUFFER UTILITIES
// =============================================================================

/**
 * Calculate the buffer size needed for a video frame.
 *
 * @param format The pixel format (AVPixelFormat)
 * @param width Frame width in pixels
 * @param height Frame height in pixels
 * @param align Buffer alignment (1 for packed, 32 for SIMD optimization)
 * @return Required buffer size in bytes, or negative on error
 */
inline int CalculateFrameBufferSize(int format, int width, int height, int align = 1) {
  if (width <= 0 || height <= 0) {
    return AVERROR(EINVAL);
  }

  AVPixelFormat pix_fmt = static_cast<AVPixelFormat>(format);
  if (pix_fmt == AV_PIX_FMT_NONE) {
    return AVERROR(EINVAL);
  }

  return av_image_get_buffer_size(pix_fmt, width, height, align);
}

/**
 * Copy video frame data to a destination buffer.
 *
 * Copies all planes of the frame to a contiguous buffer in planar format.
 * This is the spec-compliant way to implement VideoFrame.copyTo().
 *
 * @param frame Source AVFrame
 * @param dest Destination buffer
 * @param dest_size Size of destination buffer in bytes
 * @param align Buffer alignment (1 for packed)
 * @return Number of bytes written, or negative error code
 */
inline int CopyFrameToBuffer(const AVFrame* frame, uint8_t* dest, size_t dest_size, int align = 1) {
  if (!frame || !dest || dest_size == 0) {
    return AVERROR(EINVAL);
  }

  AVPixelFormat pix_fmt = static_cast<AVPixelFormat>(frame->format);
  if (pix_fmt == AV_PIX_FMT_NONE) {
    return AVERROR(EINVAL);
  }

  int required = av_image_get_buffer_size(pix_fmt, frame->width, frame->height, align);
  if (required < 0) {
    return required;
  }

  if (static_cast<size_t>(required) > dest_size) {
    return AVERROR(ENOSPC);
  }

  return av_image_copy_to_buffer(dest, static_cast<int>(dest_size), frame->data, frame->linesize, pix_fmt, frame->width,
                                 frame->height, align);
}

/**
 * Create an AVFrame from buffer data.
 *
 * Allocates a new frame and copies the buffer data into it.
 * The returned frame owns its own buffer (not a reference to source).
 *
 * @param data Source buffer
 * @param size Size of source buffer
 * @param width Frame width
 * @param height Frame height
 * @param format Pixel format (AVPixelFormat)
 * @return New AVFrame, or nullptr on error
 */
inline raii::AVFramePtr CreateFrameFromBuffer(const uint8_t* data, size_t size, int width, int height, int format) {
  if (!data || size == 0 || width <= 0 || height <= 0) {
    return nullptr;
  }

  AVPixelFormat pix_fmt = static_cast<AVPixelFormat>(format);
  if (pix_fmt == AV_PIX_FMT_NONE) {
    return nullptr;
  }

  // Calculate required size
  int required = av_image_get_buffer_size(pix_fmt, width, height, 1);
  if (required < 0 || static_cast<size_t>(required) > size) {
    return nullptr;
  }

  // Allocate frame
  raii::AVFramePtr frame = raii::MakeAvFrame();
  if (!frame) {
    return nullptr;
  }

  frame->width = width;
  frame->height = height;
  frame->format = format;

  // Allocate frame buffer
  int ret = av_frame_get_buffer(frame.get(), 0);
  if (ret < 0) {
    return nullptr;
  }

  // Make frame writable (in case of reference counting)
  ret = av_frame_make_writable(frame.get());
  if (ret < 0) {
    return nullptr;
  }

  // Copy data from source buffer to frame planes
  ret = av_image_fill_arrays(frame->data, frame->linesize, data, pix_fmt, width, height, 1);
  if (ret < 0) {
    return nullptr;
  }

  // Actually copy the data (av_image_fill_arrays just sets pointers)
  // We need to manually copy since the frame has its own buffer
  const uint8_t* src_data[4] = {nullptr};
  int src_linesize[4] = {0};

  ret = av_image_fill_arrays(const_cast<uint8_t**>(src_data), src_linesize, data, pix_fmt, width, height, 1);
  if (ret < 0) {
    return nullptr;
  }

  av_image_copy(frame->data, frame->linesize, src_data, src_linesize, pix_fmt, width, height);

  return frame;
}

/**
 * Get the number of planes for a pixel format.
 *
 * @param format Pixel format
 * @return Number of planes, or 0 if unknown
 */
inline int GetPlaneCount(int format) {
  AVPixelFormat pix_fmt = static_cast<AVPixelFormat>(format);
  const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(pix_fmt);
  if (!desc) {
    return 0;
  }
  return av_pix_fmt_count_planes(pix_fmt);
}

/**
 * Get the size of a specific plane in bytes.
 *
 * @param frame The AVFrame
 * @param plane Plane index (0-3)
 * @return Size in bytes, or 0 on error
 */
inline size_t GetPlaneSize(const AVFrame* frame, int plane) {
  if (!frame || plane < 0 || plane >= AV_NUM_DATA_POINTERS) {
    return 0;
  }

  if (!frame->data[plane]) {
    return 0;
  }

  AVPixelFormat pix_fmt = static_cast<AVPixelFormat>(frame->format);
  const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(pix_fmt);
  if (!desc) {
    return 0;
  }

  int height = frame->height;
  if (plane > 0 && (desc->log2_chroma_h > 0)) {
    height = AV_CEIL_RSHIFT(height, desc->log2_chroma_h);
  }

  return static_cast<size_t>(frame->linesize[plane]) * static_cast<size_t>(height);
}

// =============================================================================
// AUDIO DATA BUFFER UTILITIES
// =============================================================================

/**
 * Calculate buffer size for audio data.
 *
 * @param nb_samples Number of samples
 * @param nb_channels Number of channels
 * @param format Sample format (AVSampleFormat)
 * @param align Buffer alignment
 * @return Required buffer size, or negative on error
 */
inline int CalculateAudioBufferSize(int nb_samples, int nb_channels, int format, int align = 1) {
  if (nb_samples <= 0 || nb_channels <= 0) {
    return AVERROR(EINVAL);
  }

  AVSampleFormat sample_fmt = static_cast<AVSampleFormat>(format);
  if (sample_fmt == AV_SAMPLE_FMT_NONE) {
    return AVERROR(EINVAL);
  }

  int linesize = 0;
  return av_samples_get_buffer_size(&linesize, nb_channels, nb_samples, sample_fmt, align);
}

/**
 * Copy audio frame data to a destination buffer.
 *
 * @param frame Source audio AVFrame
 * @param dest Destination buffer
 * @param dest_size Size of destination buffer
 * @return Number of bytes written, or negative error code
 */
inline int CopyAudioToBuffer(const AVFrame* frame, uint8_t* dest, size_t dest_size) {
  if (!frame || !dest || dest_size == 0) {
    return AVERROR(EINVAL);
  }

  AVSampleFormat sample_fmt = static_cast<AVSampleFormat>(frame->format);
  if (sample_fmt == AV_SAMPLE_FMT_NONE) {
    return AVERROR(EINVAL);
  }

  int channels = frame->ch_layout.nb_channels;
  if (channels <= 0) {
    return AVERROR(EINVAL);
  }

  int linesize = 0;
  int required = av_samples_get_buffer_size(&linesize, channels, frame->nb_samples, sample_fmt, 1);
  if (required < 0) {
    return required;
  }

  if (static_cast<size_t>(required) > dest_size) {
    return AVERROR(ENOSPC);
  }

  // For planar formats, we need to interleave or copy each plane
  if (av_sample_fmt_is_planar(sample_fmt)) {
    // Copy each plane sequentially
    uint8_t* dst = dest;
    int plane_size = required / channels;
    for (int ch = 0; ch < channels && frame->data[ch]; ch++) {
      std::memcpy(dst, frame->data[ch], plane_size);
      dst += plane_size;
    }
  } else {
    // Interleaved format - single plane
    std::memcpy(dest, frame->data[0], required);
  }

  return required;
}

// =============================================================================
// PACKET BUFFER UTILITIES
// =============================================================================

/**
 * Copy packet data to a destination buffer.
 *
 * @param packet Source AVPacket
 * @param dest Destination buffer
 * @param dest_size Size of destination buffer
 * @return Number of bytes written, or negative error code
 */
inline int CopyPacketToBuffer(const AVPacket* packet, uint8_t* dest, size_t dest_size) {
  if (!packet || !dest || dest_size == 0) {
    return AVERROR(EINVAL);
  }

  if (!packet->data || packet->size <= 0) {
    return 0;
  }

  if (static_cast<size_t>(packet->size) > dest_size) {
    return AVERROR(ENOSPC);
  }

  std::memcpy(dest, packet->data, packet->size);
  return packet->size;
}

/**
 * Create an AVPacket from buffer data.
 *
 * @param data Source buffer
 * @param size Size of source buffer
 * @return New AVPacket, or nullptr on error
 */
inline raii::AVPacketPtr CreatePacketFromBuffer(const uint8_t* data, size_t size) {
  if (!data || size == 0) {
    return nullptr;
  }

  raii::AVPacketPtr packet = raii::MakeAvPacket();
  if (!packet) {
    return nullptr;
  }

  // Allocate packet buffer and copy data
  int ret = av_new_packet(packet.get(), static_cast<int>(size));
  if (ret < 0) {
    return nullptr;
  }

  std::memcpy(packet->data, data, size);

  return packet;
}

// =============================================================================
// NAPI BUFFER HELPERS (only available when not in testing mode)
// =============================================================================

#ifndef WEBCODECS_TESTING

/**
 * Create a Napi::ArrayBuffer containing a copy of frame data.
 *
 * @param env Napi environment
 * @param frame Source AVFrame
 * @return ArrayBuffer with copied frame data, or empty on error
 */
inline Napi::ArrayBuffer FrameToArrayBuffer(Napi::Env env, const AVFrame* frame) {
  if (!frame) {
    return Napi::ArrayBuffer::New(env, 0);
  }

  int size = CalculateFrameBufferSize(frame->format, frame->width, frame->height, 1);
  if (size <= 0) {
    return Napi::ArrayBuffer::New(env, 0);
  }

  Napi::ArrayBuffer buffer = Napi::ArrayBuffer::New(env, static_cast<size_t>(size));
  uint8_t* dest = static_cast<uint8_t*>(buffer.Data());

  int ret = CopyFrameToBuffer(frame, dest, buffer.ByteLength(), 1);
  if (ret < 0) {
    return Napi::ArrayBuffer::New(env, 0);
  }

  return buffer;
}

/**
 * Create a Napi::ArrayBuffer containing a copy of packet data.
 *
 * @param env Napi environment
 * @param packet Source AVPacket
 * @return ArrayBuffer with copied packet data, or empty on error
 */
inline Napi::ArrayBuffer PacketToArrayBuffer(Napi::Env env, const AVPacket* packet) {
  if (!packet || !packet->data || packet->size <= 0) {
    return Napi::ArrayBuffer::New(env, 0);
  }

  Napi::ArrayBuffer buffer = Napi::ArrayBuffer::New(env, static_cast<size_t>(packet->size));
  std::memcpy(buffer.Data(), packet->data, packet->size);

  return buffer;
}

/**
 * Extract data from a Napi::TypedArray or ArrayBuffer.
 *
 * @param value The Napi value (TypedArray or ArrayBuffer)
 * @param[out] data Pointer to the data
 * @param[out] size Size of the data in bytes
 * @return true if extraction succeeded, false otherwise
 */
inline bool ExtractBufferData(Napi::Value value, const uint8_t** data, size_t* size) {
  if (!data || !size) {
    return false;
  }

  *data = nullptr;
  *size = 0;

  if (value.IsTypedArray()) {
    Napi::TypedArray typedArray = value.As<Napi::TypedArray>();
    Napi::ArrayBuffer arrayBuffer = typedArray.ArrayBuffer();
    *data = static_cast<const uint8_t*>(arrayBuffer.Data()) + typedArray.ByteOffset();
    *size = typedArray.ByteLength();
    return true;
  }

  if (value.IsArrayBuffer()) {
    Napi::ArrayBuffer arrayBuffer = value.As<Napi::ArrayBuffer>();
    *data = static_cast<const uint8_t*>(arrayBuffer.Data());
    *size = arrayBuffer.ByteLength();
    return true;
  }

  if (value.IsDataView()) {
    Napi::DataView dataView = value.As<Napi::DataView>();
    Napi::ArrayBuffer arrayBuffer = dataView.ArrayBuffer();
    *data = static_cast<const uint8_t*>(arrayBuffer.Data()) + dataView.ByteOffset();
    *size = dataView.ByteLength();
    return true;
  }

  return false;
}

#endif  // WEBCODECS_TESTING

}  // namespace buffer_utils
}  // namespace webcodecs
