#pragma once
/**
 * format_converter.h - Format Conversion Utilities for WebCodecs
 *
 * Provides pixel format and color space conversion using libswscale.
 * Used by VideoFrame.copyTo() and related methods.
 *
 * @see https://www.w3.org/TR/webcodecs/#videoframe-copyto
 */

#include <string>
#include <unordered_map>
#include "../ffmpeg_raii.h"

extern "C" {
#include <libswscale/swscale.h>
#include <libavutil/pixdesc.h>
}

namespace webcodecs {
namespace format_converter {

// =============================================================================
// PIXEL FORMAT MAPPINGS
// =============================================================================

/**
 * Convert WebCodecs VideoPixelFormat string to FFmpeg AVPixelFormat.
 *
 * Supports all W3C WebCodecs pixel formats per section 9.8.
 * @see https://www.w3.org/TR/webcodecs/#enumdef-videopixelformat
 */
inline AVPixelFormat WebCodecsToFFmpeg(const std::string& format) {
  // YUV 4:2:0 formats (8-bit)
  if (format == "I420") return AV_PIX_FMT_YUV420P;
  if (format == "I420A") return AV_PIX_FMT_YUVA420P;
  // YUV 4:2:0 formats (10/12-bit)
  if (format == "I420P10") return AV_PIX_FMT_YUV420P10LE;
  if (format == "I420P12") return AV_PIX_FMT_YUV420P12LE;
  if (format == "I420AP10") return AV_PIX_FMT_YUVA420P10LE;
  // Note: I420AP12 not supported by FFmpeg (no YUVA420P12 format exists)
  if (format == "I420AP12") return AV_PIX_FMT_NONE;
  // NV12 (semi-planar 4:2:0)
  if (format == "NV12") return AV_PIX_FMT_NV12;

  // YUV 4:2:2 formats (8-bit)
  if (format == "I422") return AV_PIX_FMT_YUV422P;
  if (format == "I422A") return AV_PIX_FMT_YUVA422P;
  // YUV 4:2:2 formats (10/12-bit)
  if (format == "I422P10") return AV_PIX_FMT_YUV422P10LE;
  if (format == "I422P12") return AV_PIX_FMT_YUV422P12LE;
  if (format == "I422AP10") return AV_PIX_FMT_YUVA422P10LE;
  if (format == "I422AP12") return AV_PIX_FMT_YUVA422P12LE;

  // YUV 4:4:4 formats (8-bit)
  if (format == "I444") return AV_PIX_FMT_YUV444P;
  if (format == "I444A") return AV_PIX_FMT_YUVA444P;
  // YUV 4:4:4 formats (10/12-bit)
  if (format == "I444P10") return AV_PIX_FMT_YUV444P10LE;
  if (format == "I444P12") return AV_PIX_FMT_YUV444P12LE;
  if (format == "I444AP10") return AV_PIX_FMT_YUVA444P10LE;
  if (format == "I444AP12") return AV_PIX_FMT_YUVA444P12LE;

  // RGB/RGBA formats (packed, 8-bit per component)
  if (format == "RGBA") return AV_PIX_FMT_RGBA;
  if (format == "RGBX") return AV_PIX_FMT_RGB0;     // RGB with padding byte
  if (format == "BGRA") return AV_PIX_FMT_BGRA;
  if (format == "BGRX") return AV_PIX_FMT_BGR0;     // BGR with padding byte

  return AV_PIX_FMT_NONE;
}

/**
 * Convert FFmpeg AVPixelFormat to WebCodecs VideoPixelFormat string.
 *
 * Returns nullptr for unsupported formats.
 */
inline const char* FFmpegToWebCodecs(AVPixelFormat format) {
  switch (format) {
    // YUV 4:2:0 formats (8-bit)
    case AV_PIX_FMT_YUV420P:      return "I420";
    case AV_PIX_FMT_YUVA420P:     return "I420A";
    // YUV 4:2:0 formats (10/12-bit)
    case AV_PIX_FMT_YUV420P10LE:  return "I420P10";
    case AV_PIX_FMT_YUV420P12LE:  return "I420P12";
    case AV_PIX_FMT_YUVA420P10LE: return "I420AP10";
    // Note: I420AP12 not available - FFmpeg has no YUVA420P12 format
    // NV12 (semi-planar 4:2:0)
    case AV_PIX_FMT_NV12:         return "NV12";

    // YUV 4:2:2 formats (8-bit)
    case AV_PIX_FMT_YUV422P:      return "I422";
    case AV_PIX_FMT_YUVA422P:     return "I422A";
    // YUV 4:2:2 formats (10/12-bit)
    case AV_PIX_FMT_YUV422P10LE:  return "I422P10";
    case AV_PIX_FMT_YUV422P12LE:  return "I422P12";
    case AV_PIX_FMT_YUVA422P10LE: return "I422AP10";
    case AV_PIX_FMT_YUVA422P12LE: return "I422AP12";

    // YUV 4:4:4 formats (8-bit)
    case AV_PIX_FMT_YUV444P:      return "I444";
    case AV_PIX_FMT_YUVA444P:     return "I444A";
    // YUV 4:4:4 formats (10/12-bit)
    case AV_PIX_FMT_YUV444P10LE:  return "I444P10";
    case AV_PIX_FMT_YUV444P12LE:  return "I444P12";
    case AV_PIX_FMT_YUVA444P10LE: return "I444AP10";
    case AV_PIX_FMT_YUVA444P12LE: return "I444AP12";

    // RGB/RGBA formats (packed, 8-bit per component)
    case AV_PIX_FMT_RGBA:         return "RGBA";
    case AV_PIX_FMT_RGB0:         return "RGBX";
    case AV_PIX_FMT_BGRA:         return "BGRA";
    case AV_PIX_FMT_BGR0:         return "BGRX";

    // Fallback for common decoder output formats (closest WebCodecs match)
    case AV_PIX_FMT_RGB24:        return "RGBX";
    case AV_PIX_FMT_BGR24:        return "BGRX";

    default:
      return nullptr;
  }
}

/**
 * Check if a pixel format is an RGB variant (supports colorSpace conversion).
 *
 * Per W3C WebCodecs spec section 9.8, RGB formats are:
 * - RGBA, RGBX, BGRA, BGRX
 */
inline bool IsRGBFormat(const std::string& format) {
  return format == "RGBA" || format == "RGBX" ||
         format == "BGRA" || format == "BGRX";
}

// =============================================================================
// COLOR SPACE MAPPINGS
// =============================================================================

/**
 * Get FFmpeg color range from WebCodecs PredefinedColorSpace.
 */
inline AVColorRange GetColorRange(const std::string& colorSpace) {
  // Both srgb and display-p3 use full range (0-255)
  (void)colorSpace;  // Unused for now, both use same range
  return AVCOL_RANGE_JPEG;
}

/**
 * Get FFmpeg color primaries from WebCodecs PredefinedColorSpace.
 */
inline AVColorPrimaries GetColorPrimaries(const std::string& colorSpace) {
  if (colorSpace == "srgb") {
    return AVCOL_PRI_BT709;  // sRGB uses BT.709 primaries
  }
  if (colorSpace == "display-p3") {
    return AVCOL_PRI_SMPTE432;  // Display P3 primaries
  }
  return AVCOL_PRI_BT709;  // Default to sRGB
}

/**
 * Get FFmpeg transfer characteristics from WebCodecs PredefinedColorSpace.
 */
inline AVColorTransferCharacteristic GetTransferCharacteristics(const std::string& colorSpace) {
  // sRGB uses IEC 61966-2-1 transfer
  // display-p3 also uses sRGB transfer curve
  (void)colorSpace;  // Unused for now, both use same transfer
  return AVCOL_TRC_IEC61966_2_1;
}

// =============================================================================
// FORMAT CONVERTER CLASS
// =============================================================================

/**
 * Cached pixel format converter using libswscale.
 *
 * Creates and caches SwsContext for repeated conversions between the same
 * format pairs. Thread-safe for concurrent read operations on a single context,
 * but creation and destruction must be externally synchronized.
 */
class FormatConverter {
 public:
  FormatConverter() = default;
  ~FormatConverter() = default;

  // Non-copyable, movable
  FormatConverter(const FormatConverter&) = delete;
  FormatConverter& operator=(const FormatConverter&) = delete;
  FormatConverter(FormatConverter&&) = default;
  FormatConverter& operator=(FormatConverter&&) = default;

  /**
   * Convert frame to a different pixel format.
   *
   * @param src_frame Source frame
   * @param dst_format Destination pixel format string (WebCodecs format)
   * @param colorSpace Target color space (srgb or display-p3)
   * @return New frame in destination format, or nullptr on error
   */
  raii::AVFramePtr Convert(const AVFrame* src_frame,
                           const std::string& dst_format,
                           const std::string& colorSpace = "srgb") {
    if (!src_frame) return nullptr;

    AVPixelFormat dst_pix_fmt = WebCodecsToFFmpeg(dst_format);
    if (dst_pix_fmt == AV_PIX_FMT_NONE) {
      return nullptr;
    }

    AVPixelFormat src_pix_fmt = static_cast<AVPixelFormat>(src_frame->format);

    // No conversion needed if same format and no color space conversion
    if (src_pix_fmt == dst_pix_fmt && !IsRGBFormat(dst_format)) {
      return raii::CloneAvFrame(src_frame);
    }

    // Create destination frame
    raii::AVFramePtr dst_frame = raii::MakeAvFrame();
    if (!dst_frame) return nullptr;

    dst_frame->width = src_frame->width;
    dst_frame->height = src_frame->height;
    dst_frame->format = dst_pix_fmt;
    dst_frame->pts = src_frame->pts;
    dst_frame->duration = src_frame->duration;

    // Set color properties for RGB output
    if (IsRGBFormat(dst_format)) {
      dst_frame->color_range = GetColorRange(colorSpace);
      dst_frame->color_primaries = GetColorPrimaries(colorSpace);
      dst_frame->color_trc = GetTransferCharacteristics(colorSpace);
    }

    // Allocate destination buffer
    int ret = av_frame_get_buffer(dst_frame.get(), 0);
    if (ret < 0) return nullptr;

    // Get or create SwsContext
    sws_ctx_.reset(sws_getCachedContext(
        sws_ctx_.release(),
        src_frame->width, src_frame->height, src_pix_fmt,
        dst_frame->width, dst_frame->height, dst_pix_fmt,
        SWS_BILINEAR, nullptr, nullptr, nullptr));

    if (!sws_ctx_) return nullptr;

    // Perform conversion
    ret = sws_scale(sws_ctx_.get(),
                    src_frame->data, src_frame->linesize,
                    0, src_frame->height,
                    dst_frame->data, dst_frame->linesize);

    if (ret < 0) return nullptr;

    return dst_frame;
  }

  /**
   * Convert a sub-region (rect) of the frame.
   *
   * @param src_frame Source frame
   * @param x X offset in source
   * @param y Y offset in source
   * @param width Width of region
   * @param height Height of region
   * @param dst_format Destination pixel format string
   * @param colorSpace Target color space
   * @return New frame containing the converted region
   */
  raii::AVFramePtr ConvertRect(const AVFrame* src_frame,
                                int x, int y, int width, int height,
                                const std::string& dst_format,
                                const std::string& colorSpace = "srgb") {
    if (!src_frame) return nullptr;
    if (x < 0 || y < 0 || width <= 0 || height <= 0) return nullptr;
    if (x + width > src_frame->width || y + height > src_frame->height) return nullptr;

    AVPixelFormat src_pix_fmt = static_cast<AVPixelFormat>(src_frame->format);
    AVPixelFormat dst_pix_fmt = WebCodecsToFFmpeg(dst_format);
    if (dst_pix_fmt == AV_PIX_FMT_NONE) dst_pix_fmt = src_pix_fmt;

    // Create destination frame for the cropped region
    raii::AVFramePtr dst_frame = raii::MakeAvFrame();
    if (!dst_frame) return nullptr;

    dst_frame->width = width;
    dst_frame->height = height;
    dst_frame->format = dst_pix_fmt;
    dst_frame->pts = src_frame->pts;
    dst_frame->duration = src_frame->duration;

    if (IsRGBFormat(dst_format)) {
      dst_frame->color_range = GetColorRange(colorSpace);
      dst_frame->color_primaries = GetColorPrimaries(colorSpace);
      dst_frame->color_trc = GetTransferCharacteristics(colorSpace);
    }

    int ret = av_frame_get_buffer(dst_frame.get(), 0);
    if (ret < 0) return nullptr;

    // Calculate source pointers with offset
    const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(src_pix_fmt);
    if (!desc) return nullptr;

    // For the source, we need to adjust the data pointers to start at (x, y)
    const uint8_t* src_data[4];
    int src_linesize[4];

    for (int i = 0; i < 4 && src_frame->data[i]; i++) {
      int plane_x = x;
      int plane_y = y;

      // Adjust for chroma subsampling
      if (i > 0 && desc->log2_chroma_w > 0) {
        plane_x >>= desc->log2_chroma_w;
      }
      if (i > 0 && desc->log2_chroma_h > 0) {
        plane_y >>= desc->log2_chroma_h;
      }

      int bytes_per_sample = (desc->comp[i].depth + 7) / 8;
      src_data[i] = src_frame->data[i] + plane_y * src_frame->linesize[i] + plane_x * bytes_per_sample;
      src_linesize[i] = src_frame->linesize[i];
    }

    // Get or create SwsContext for the cropped size
    sws_ctx_.reset(sws_getCachedContext(
        sws_ctx_.release(),
        width, height, src_pix_fmt,
        width, height, dst_pix_fmt,
        SWS_BILINEAR, nullptr, nullptr, nullptr));

    if (!sws_ctx_) return nullptr;

    ret = sws_scale(sws_ctx_.get(),
                    src_data, src_linesize,
                    0, height,
                    dst_frame->data, dst_frame->linesize);

    if (ret < 0) return nullptr;

    return dst_frame;
  }

 private:
  raii::SwsContextPtr sws_ctx_;
};

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

/**
 * Copy frame data to buffer with custom layout.
 *
 * @param frame Source frame
 * @param dest Destination buffer
 * @param dest_size Size of destination buffer
 * @param layout PlaneLayout array (offsets and strides per plane)
 * @param layout_count Number of planes in layout
 * @return Number of bytes written, or negative error code
 */
inline int CopyFrameWithLayout(const AVFrame* frame, uint8_t* dest, size_t dest_size,
                                const int* offsets, const int* strides, int layout_count) {
  if (!frame || !dest || !offsets || !strides) {
    return AVERROR(EINVAL);
  }

  AVPixelFormat pix_fmt = static_cast<AVPixelFormat>(frame->format);
  const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(pix_fmt);
  if (!desc) return AVERROR(EINVAL);

  int num_planes = av_pix_fmt_count_planes(pix_fmt);
  if (layout_count != num_planes) {
    return AVERROR(EINVAL);
  }

  size_t total_written = 0;

  for (int plane = 0; plane < num_planes; plane++) {
    if (!frame->data[plane]) continue;

    int height = frame->height;
    if (plane > 0 && desc->log2_chroma_h > 0) {
      height = AV_CEIL_RSHIFT(height, desc->log2_chroma_h);
    }

    int width = frame->width;
    if (plane > 0 && desc->log2_chroma_w > 0) {
      width = AV_CEIL_RSHIFT(width, desc->log2_chroma_w);
    }

    // Calculate bytes per pixel for this plane
    int bytes_per_sample = (desc->comp[plane].depth + 7) / 8;
    int row_bytes = width * bytes_per_sample;

    int dst_offset = offsets[plane];
    int dst_stride = strides[plane];

    // Validate layout (use size_t arithmetic to prevent overflow)
    if (dst_offset < 0 || dst_stride < row_bytes) {
      return AVERROR(EINVAL);  // Stride too small or negative offset
    }
    size_t required_size = static_cast<size_t>(dst_offset) +
                           static_cast<size_t>(height - 1) * static_cast<size_t>(dst_stride) +
                           static_cast<size_t>(row_bytes);
    if (required_size > dest_size) {
      return AVERROR(ENOSPC);  // Buffer too small
    }

    // Copy row by row with custom stride
    for (int y = 0; y < height; y++) {
      uint8_t* dst_row = dest + dst_offset + y * dst_stride;
      const uint8_t* src_row = frame->data[plane] + y * frame->linesize[plane];
      memcpy(dst_row, src_row, row_bytes);
    }

    total_written = std::max(total_written, required_size);
  }

  return static_cast<int>(total_written);
}

/**
 * Calculate required buffer size for frame with custom layout.
 */
inline int CalculateSizeWithLayout(int format, int width, int height,
                                    const int* offsets, const int* strides, int layout_count) {
  AVPixelFormat pix_fmt = static_cast<AVPixelFormat>(format);
  const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(pix_fmt);
  if (!desc) return AVERROR(EINVAL);

  int num_planes = av_pix_fmt_count_planes(pix_fmt);
  if (layout_count != num_planes) {
    return AVERROR(EINVAL);
  }

  size_t max_size = 0;
  for (int plane = 0; plane < num_planes; plane++) {
    int plane_height = height;
    int plane_width = width;

    if (plane > 0) {
      if (desc->log2_chroma_h > 0) {
        plane_height = AV_CEIL_RSHIFT(height, desc->log2_chroma_h);
      }
      if (desc->log2_chroma_w > 0) {
        plane_width = AV_CEIL_RSHIFT(width, desc->log2_chroma_w);
      }
    }

    int bytes_per_sample = (desc->comp[plane].depth + 7) / 8;
    size_t row_bytes = static_cast<size_t>(plane_width) * bytes_per_sample;

    // Use size_t arithmetic to prevent integer overflow with large strides
    size_t plane_size = static_cast<size_t>(offsets[plane]) +
                        static_cast<size_t>(plane_height - 1) * static_cast<size_t>(strides[plane]) +
                        row_bytes;

    // Check for overflow: result must fit in int for return value
    if (plane_size > static_cast<size_t>(INT32_MAX)) {
      return AVERROR(EINVAL);  // Size would overflow int
    }

    max_size = std::max(max_size, plane_size);
  }

  // Final check: max_size must fit in int
  if (max_size > static_cast<size_t>(INT32_MAX)) {
    return AVERROR(EINVAL);
  }

  return static_cast<int>(max_size);
}

}  // namespace format_converter
}  // namespace webcodecs
