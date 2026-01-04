#include "video_frame.h"

#include <string>
#include <vector>

#include "shared/buffer_utils.h"
#include "shared/format_converter.h"
#include "error_builder.h"

namespace webcodecs {

// Helper: Convert WebCodecs VideoPixelFormat string to AVPixelFormat
// Uses format_converter for full W3C spec compliance (21 formats)
static AVPixelFormat StringToPixelFormat(const std::string& format) {
  return format_converter::WebCodecsToFFmpeg(format);
}

Napi::FunctionReference VideoFrame::constructor;

Napi::Object VideoFrame::Init(Napi::Env env, Napi::Object exports) {
  Napi::Function func = DefineClass(env, "VideoFrame",
                                    {
                                        InstanceAccessor<&VideoFrame::GetFormat>("format"),
                                        InstanceAccessor<&VideoFrame::GetCodedWidth>("codedWidth"),
                                        InstanceAccessor<&VideoFrame::GetCodedHeight>("codedHeight"),
                                        InstanceAccessor<&VideoFrame::GetCodedRect>("codedRect"),
                                        InstanceAccessor<&VideoFrame::GetVisibleRect>("visibleRect"),
                                        InstanceAccessor<&VideoFrame::GetRotation>("rotation"),
                                        InstanceAccessor<&VideoFrame::GetFlip>("flip"),
                                        InstanceAccessor<&VideoFrame::GetDisplayWidth>("displayWidth"),
                                        InstanceAccessor<&VideoFrame::GetDisplayHeight>("displayHeight"),
                                        InstanceAccessor<&VideoFrame::GetDuration>("duration"),
                                        InstanceAccessor<&VideoFrame::GetTimestamp>("timestamp"),
                                        InstanceAccessor<&VideoFrame::GetColorSpace>("colorSpace"),
                                        InstanceMethod<&VideoFrame::Metadata>("metadata"),
                                        InstanceMethod<&VideoFrame::AllocationSize>("allocationSize"),
                                        InstanceMethod<&VideoFrame::CopyTo>("copyTo"),
                                        InstanceMethod<&VideoFrame::Clone>("clone"),
                                        InstanceMethod<&VideoFrame::Close>("close"),
                                        InstanceMethod<&VideoFrame::SerializeForTransfer>("serializeForTransfer"),
                                    });

  constructor = Napi::Persistent(func);
  constructor.SuppressDestruct();
  exports.Set("VideoFrame", func);
  return exports;
}

VideoFrame::VideoFrame(const Napi::CallbackInfo& info) : Napi::ObjectWrap<VideoFrame>(info) {
  Napi::Env env = info.Env();

  // [SPEC 9.4.2] VideoFrame constructor can be called with:
  // 1. (data, init) - VideoFrameBufferInit with raw pixel data
  // 2. (image, init?) - CanvasImageSource or VideoFrame
  // 3. No arguments - internal use by CreateFromAVFrame/CloneFrom

  // Initialize metadata to empty object per spec
  // [SPEC] [[metadata]] is always a VideoFrameMetadata dictionary
  metadata_ = Napi::Persistent(Napi::Object::New(env));

  if (info.Length() == 0) {
    // Internal construction - frame_ will be set by CreateFromAVFrame/CloneFrom
    return;
  }

  // [SPEC 9.4.2] Check if first argument is a VideoFrame
  // VideoFrame(image, init?) where image is VideoFrame
  if (info[0].IsObject() && info[0].As<Napi::Object>().InstanceOf(constructor.Value())) {
    VideoFrame* source = Napi::ObjectWrap<VideoFrame>::Unwrap(info[0].As<Napi::Object>());
    if (!source || source->closed_.load(std::memory_order_acquire) || !source->frame_) {
      errors::ThrowInvalidStateError(env, "Source VideoFrame is closed or invalid");
      return;
    }

    // Clone the AVFrame (refcounted)
    frame_ = raii::CloneAvFrame(source->frame_.get());
    if (!frame_) {
      errors::ThrowTypeError(env, "Failed to clone source VideoFrame");
      return;
    }

    // [SPEC] Initialize Frame From Other Frame algorithm
    // Copy internal slots from source as defaults
    rotation_ = source->rotation_;
    flip_ = source->flip_;
    visible_left_ = source->visible_left_;
    visible_top_ = source->visible_top_;
    visible_width_ = source->visible_width_;
    visible_height_ = source->visible_height_;
    display_width_ = source->display_width_;
    display_height_ = source->display_height_;

    // Copy metadata from source as default
    if (!source->metadata_.IsEmpty()) {
      Napi::Object srcMeta = source->metadata_.Value();
      Napi::Object clonedMeta = Napi::Object::New(env);
      Napi::Array keys = srcMeta.GetPropertyNames();
      for (uint32_t i = 0; i < keys.Length(); i++) {
        Napi::Value key = keys.Get(i);
        clonedMeta.Set(key, srcMeta.Get(key));
      }
      metadata_ = Napi::Persistent(clonedMeta);
    }

    // Parse optional init parameter (VideoFrameInit)
    if (info.Length() >= 2 && info[1].IsObject()) {
      Napi::Object init = info[1].As<Napi::Object>();

      // [SPEC] Override timestamp if provided
      if (init.Has("timestamp") && init.Get("timestamp").IsNumber()) {
        frame_->pts = init.Get("timestamp").As<Napi::Number>().Int64Value();
      }

      // [SPEC] Override duration if provided
      if (init.Has("duration") && init.Get("duration").IsNumber()) {
        frame_->duration = init.Get("duration").As<Napi::Number>().Int64Value();
      }

      // [SPEC] Override visibleRect if provided
      if (init.Has("visibleRect") && init.Get("visibleRect").IsObject()) {
        Napi::Object rect = init.Get("visibleRect").As<Napi::Object>();
        if (rect.Has("x")) visible_left_ = rect.Get("x").As<Napi::Number>().Int32Value();
        if (rect.Has("y")) visible_top_ = rect.Get("y").As<Napi::Number>().Int32Value();
        if (rect.Has("width")) visible_width_ = rect.Get("width").As<Napi::Number>().Int32Value();
        if (rect.Has("height")) visible_height_ = rect.Get("height").As<Napi::Number>().Int32Value();

        // Validate bounds
        int width = frame_->width;
        int height = frame_->height;
        if (visible_left_ < 0 || visible_top_ < 0 ||
            visible_width_ <= 0 || visible_height_ <= 0 ||
            visible_width_ > width - visible_left_ ||
            visible_height_ > height - visible_top_) {
          errors::ThrowTypeError(env, "visibleRect out of bounds");
          return;
        }
      }

      // [SPEC] Override rotation if provided (additive per Add Rotations algorithm)
      if (init.Has("rotation") && init.Get("rotation").IsNumber()) {
        int newRotation = init.Get("rotation").As<Napi::Number>().Int32Value();
        if (newRotation != 0 && newRotation != 90 && newRotation != 180 && newRotation != 270) {
          errors::ThrowTypeError(env, "rotation must be 0, 90, 180, or 270");
          return;
        }
        // [SPEC] Add Rotations: if flip, subtract; otherwise add
        int combined;
        if (flip_) {
          combined = rotation_ - newRotation;
        } else {
          combined = rotation_ + newRotation;
        }
        // Normalize to [0, 360)
        rotation_ = ((combined % 360) + 360) % 360;
      }

      // [SPEC] Override flip if provided (XOR with existing)
      if (init.Has("flip") && init.Get("flip").IsBoolean()) {
        bool newFlip = init.Get("flip").As<Napi::Boolean>().Value();
        flip_ = (flip_ != newFlip);  // XOR
      }

      // [SPEC] Override display dimensions if provided
      if (init.Has("displayWidth") && init.Get("displayWidth").IsNumber()) {
        display_width_ = init.Get("displayWidth").As<Napi::Number>().Int32Value();
        if (display_width_ <= 0) {
          errors::ThrowTypeError(env, "displayWidth must be positive");
          return;
        }
      }
      if (init.Has("displayHeight") && init.Get("displayHeight").IsNumber()) {
        display_height_ = init.Get("displayHeight").As<Napi::Number>().Int32Value();
        if (display_height_ <= 0) {
          errors::ThrowTypeError(env, "displayHeight must be positive");
          return;
        }
      }

      // [SPEC] Override metadata if provided
      if (init.Has("metadata") && init.Get("metadata").IsObject()) {
        Napi::Object srcMeta = init.Get("metadata").As<Napi::Object>();
        Napi::Object clonedMeta = Napi::Object::New(env);
        Napi::Array keys = srcMeta.GetPropertyNames();
        for (uint32_t i = 0; i < keys.Length(); i++) {
          Napi::Value key = keys.Get(i);
          clonedMeta.Set(key, srcMeta.Get(key));
        }
        metadata_.Reset();
        metadata_ = Napi::Persistent(clonedMeta);
      }
    }

    return;
  }

  // Check for VideoFrameBufferInit pattern: (data, init)
  if (info.Length() >= 2 && info[1].IsObject()) {
    Napi::Object init = info[1].As<Napi::Object>();

    // Required: format
    if (!init.Has("format") || !init.Get("format").IsString()) {
      errors::ThrowTypeError(env, "format is required");
      return;
    }

    // Required: codedWidth and codedHeight
    if (!init.Has("codedWidth") || !init.Get("codedWidth").IsNumber()) {
      errors::ThrowTypeError(env, "codedWidth is required");
      return;
    }
    if (!init.Has("codedHeight") || !init.Get("codedHeight").IsNumber()) {
      errors::ThrowTypeError(env, "codedHeight is required");
      return;
    }

    std::string format_str = init.Get("format").As<Napi::String>().Utf8Value();
    int width = init.Get("codedWidth").As<Napi::Number>().Int32Value();
    int height = init.Get("codedHeight").As<Napi::Number>().Int32Value();

    if (width <= 0 || height <= 0) {
      errors::ThrowTypeError(env, "codedWidth and codedHeight must be positive");
      return;
    }

    AVPixelFormat pix_fmt = StringToPixelFormat(format_str);
    if (pix_fmt == AV_PIX_FMT_NONE) {
      errors::ThrowTypeError(env, "Unsupported pixel format: " + format_str);
      return;
    }

    // Extract data from first argument
    const uint8_t* data = nullptr;
    size_t size = 0;
    if (!buffer_utils::ExtractBufferData(info[0], &data, &size) || !data || size == 0) {
      errors::ThrowTypeError(env, "data must be an ArrayBuffer or TypedArray");
      return;
    }

    // [SPEC] Parse layout option for custom plane offsets and strides
    std::vector<int> layout_offsets;
    std::vector<int> layout_strides;
    bool has_layout = false;

    if (init.Has("layout") && init.Get("layout").IsArray()) {
      Napi::Array layoutArray = init.Get("layout").As<Napi::Array>();
      has_layout = true;

      for (uint32_t i = 0; i < layoutArray.Length(); i++) {
        if (!layoutArray.Get(i).IsObject()) {
          errors::ThrowTypeError(env, "layout entries must be PlaneLayout objects");
          return;
        }
        Napi::Object planeLayout = layoutArray.Get(i).As<Napi::Object>();

        // [SPEC] PlaneLayout requires both offset and stride
        if (!planeLayout.Has("offset") || !planeLayout.Get("offset").IsNumber()) {
          errors::ThrowTypeError(env, "layout entry missing required 'offset' property");
          return;
        }
        if (!planeLayout.Has("stride") || !planeLayout.Get("stride").IsNumber()) {
          errors::ThrowTypeError(env, "layout entry missing required 'stride' property");
          return;
        }

        int offset = planeLayout.Get("offset").As<Napi::Number>().Int32Value();
        int stride = planeLayout.Get("stride").As<Napi::Number>().Int32Value();

        if (offset < 0) {
          errors::ThrowTypeError(env, "layout offset must be non-negative");
          return;
        }
        if (stride <= 0) {
          errors::ThrowTypeError(env, "layout stride must be positive");
          return;
        }

        layout_offsets.push_back(offset);
        layout_strides.push_back(stride);
      }
    }

    // Create AVFrame from buffer data with optional custom layout
    if (has_layout) {
      frame_ = buffer_utils::CreateFrameFromBufferWithLayout(
          data, size, width, height, static_cast<int>(pix_fmt),
          layout_offsets.data(), layout_strides.data(), static_cast<int>(layout_offsets.size()));
    } else {
      frame_ = buffer_utils::CreateFrameFromBuffer(data, size, width, height, static_cast<int>(pix_fmt));
    }

    if (!frame_) {
      errors::ThrowTypeError(env, "Failed to create frame from data");
      return;
    }

    // Required: timestamp
    if (!init.Has("timestamp") || !init.Get("timestamp").IsNumber()) {
      errors::ThrowTypeError(env, "timestamp is required");
      return;
    }
    frame_->pts = init.Get("timestamp").As<Napi::Number>().Int64Value();

    // Optional: duration
    if (init.Has("duration") && init.Get("duration").IsNumber()) {
      frame_->duration = init.Get("duration").As<Napi::Number>().Int64Value();
    }

    // [SPEC] Parse visibleRect option
    // Default visible rect is the full coded rect
    visible_left_ = 0;
    visible_top_ = 0;
    visible_width_ = width;
    visible_height_ = height;

    if (init.Has("visibleRect") && init.Get("visibleRect").IsObject()) {
      Napi::Object rect = init.Get("visibleRect").As<Napi::Object>();
      if (rect.Has("x") && rect.Get("x").IsNumber()) {
        visible_left_ = rect.Get("x").As<Napi::Number>().Int32Value();
      }
      if (rect.Has("y") && rect.Get("y").IsNumber()) {
        visible_top_ = rect.Get("y").As<Napi::Number>().Int32Value();
      }
      if (rect.Has("width") && rect.Get("width").IsNumber()) {
        visible_width_ = rect.Get("width").As<Napi::Number>().Int32Value();
      }
      if (rect.Has("height") && rect.Get("height").IsNumber()) {
        visible_height_ = rect.Get("height").As<Napi::Number>().Int32Value();
      }

      // Validate visible rect bounds (overflow-safe checks)
      if (visible_left_ < 0 || visible_top_ < 0 ||
          visible_width_ <= 0 || visible_height_ <= 0 ||
          visible_width_ > width - visible_left_ ||
          visible_height_ > height - visible_top_) {
        errors::ThrowTypeError(env, "visibleRect out of bounds");
        return;
      }
    }

    // [SPEC] Parse rotation option (must be 0, 90, 180, or 270)
    if (init.Has("rotation") && init.Get("rotation").IsNumber()) {
      int rotation = init.Get("rotation").As<Napi::Number>().Int32Value();
      if (rotation != 0 && rotation != 90 && rotation != 180 && rotation != 270) {
        errors::ThrowTypeError(env, "rotation must be 0, 90, 180, or 270");
        return;
      }
      rotation_ = rotation;
    }

    // [SPEC] Parse flip option
    if (init.Has("flip") && init.Get("flip").IsBoolean()) {
      flip_ = init.Get("flip").As<Napi::Boolean>().Value();
    }

    // [SPEC] Parse displayWidth and displayHeight options
    // Default is visible dimensions (accounting for rotation)
    if (init.Has("displayWidth") && init.Get("displayWidth").IsNumber()) {
      display_width_ = init.Get("displayWidth").As<Napi::Number>().Int32Value();
      if (display_width_ <= 0) {
        errors::ThrowTypeError(env, "displayWidth must be positive");
        return;
      }
    }
    if (init.Has("displayHeight") && init.Get("displayHeight").IsNumber()) {
      display_height_ = init.Get("displayHeight").As<Napi::Number>().Int32Value();
      if (display_height_ <= 0) {
        errors::ThrowTypeError(env, "displayHeight must be positive");
        return;
      }
    }

    // If display dimensions not explicitly set, compute from visible + rotation
    if (display_width_ == 0 || display_height_ == 0) {
      if (rotation_ == 90 || rotation_ == 270) {
        // Swap dimensions for 90/270 rotation
        display_width_ = (display_width_ == 0) ? visible_height_ : display_width_;
        display_height_ = (display_height_ == 0) ? visible_width_ : display_height_;
      } else {
        display_width_ = (display_width_ == 0) ? visible_width_ : display_width_;
        display_height_ = (display_height_ == 0) ? visible_height_ : display_height_;
      }
    }

    // [SPEC] Parse colorSpace option
    if (init.Has("colorSpace") && init.Get("colorSpace").IsObject()) {
      Napi::Object colorSpace = init.Get("colorSpace").As<Napi::Object>();

      // Map primaries
      if (colorSpace.Has("primaries") && colorSpace.Get("primaries").IsString()) {
        std::string primaries = colorSpace.Get("primaries").As<Napi::String>().Utf8Value();
        if (primaries == "bt709") {
          frame_->color_primaries = AVCOL_PRI_BT709;
        } else if (primaries == "bt470bg") {
          frame_->color_primaries = AVCOL_PRI_BT470BG;
        } else if (primaries == "smpte170m") {
          frame_->color_primaries = AVCOL_PRI_SMPTE170M;
        } else if (primaries == "bt2020") {
          frame_->color_primaries = AVCOL_PRI_BT2020;
        }
      }

      // Map transfer
      if (colorSpace.Has("transfer") && colorSpace.Get("transfer").IsString()) {
        std::string transfer = colorSpace.Get("transfer").As<Napi::String>().Utf8Value();
        if (transfer == "bt709") {
          frame_->color_trc = AVCOL_TRC_BT709;
        } else if (transfer == "smpte170m") {
          frame_->color_trc = AVCOL_TRC_SMPTE170M;
        } else if (transfer == "iec61966-2-1") {
          frame_->color_trc = AVCOL_TRC_IEC61966_2_1;
        } else if (transfer == "pq") {
          frame_->color_trc = AVCOL_TRC_SMPTE2084;
        } else if (transfer == "hlg") {
          frame_->color_trc = AVCOL_TRC_ARIB_STD_B67;
        }
      }

      // Map matrix
      if (colorSpace.Has("matrix") && colorSpace.Get("matrix").IsString()) {
        std::string matrix = colorSpace.Get("matrix").As<Napi::String>().Utf8Value();
        if (matrix == "rgb") {
          frame_->colorspace = AVCOL_SPC_RGB;
        } else if (matrix == "bt709") {
          frame_->colorspace = AVCOL_SPC_BT709;
        } else if (matrix == "smpte170m") {
          frame_->colorspace = AVCOL_SPC_SMPTE170M;
        } else if (matrix == "bt2020-ncl") {
          frame_->colorspace = AVCOL_SPC_BT2020_NCL;
        }
      }

      // Map fullRange
      if (colorSpace.Has("fullRange") && colorSpace.Get("fullRange").IsBoolean()) {
        bool fullRange = colorSpace.Get("fullRange").As<Napi::Boolean>().Value();
        frame_->color_range = fullRange ? AVCOL_RANGE_JPEG : AVCOL_RANGE_MPEG;
      }
    }

    // [SPEC] Copy metadata from init if provided
    // Per "Copy VideoFrame metadata" algorithm, we deep copy (structured clone)
    if (init.Has("metadata") && init.Get("metadata").IsObject()) {
      Napi::Object srcMetadata = init.Get("metadata").As<Napi::Object>();
      Napi::Object clonedMetadata = Napi::Object::New(env);

      // Deep copy all properties from source metadata
      Napi::Array keys = srcMetadata.GetPropertyNames();
      for (uint32_t i = 0; i < keys.Length(); i++) {
        Napi::Value key = keys.Get(i);
        Napi::Value value = srcMetadata.Get(key);
        clonedMetadata.Set(key, value);
      }

      metadata_.Reset(clonedMetadata, 1);
    }

    return;
  }

  // If we get here with arguments but not matching pattern, error
  if (info.Length() > 0) {
    errors::ThrowTypeError(env, "Invalid VideoFrame constructor arguments");
  }
}

VideoFrame::~VideoFrame() { Release(); }

void VideoFrame::Release() {
  // Mark as closed (atomic, thread-safe)
  closed_.store(true, std::memory_order_release);

  // Release the AVFrame (RAII handles av_frame_free)
  // This unrefs the underlying buffers; they're freed when refcount hits 0
  frame_.reset();

  // [SPEC 9.4.6] Close VideoFrame step 6: Assign a new VideoFrameMetadata
  // We reset the reference - a new empty object is created on next metadata() call
  metadata_.Reset();
}

Napi::Object VideoFrame::CreateFromAVFrame(Napi::Env env, const AVFrame* srcFrame) {
  if (!srcFrame) {
    Napi::Error::New(env, "Cannot create VideoFrame from null AVFrame").ThrowAsJavaScriptException();
    return Napi::Object();
  }

  // Create a new JS VideoFrame object
  Napi::Object jsFrame = constructor.New({});
  VideoFrame* frame = Napi::ObjectWrap<VideoFrame>::Unwrap(jsFrame);

  // Clone the AVFrame (refcounted - shares underlying buffers)
  frame->frame_ = raii::CloneAvFrame(srcFrame);
  if (!frame->frame_) {
    Napi::Error::New(env, "Failed to clone AVFrame").ThrowAsJavaScriptException();
    return Napi::Object();
  }

  // [SPEC] Initialize internal slots from AVFrame properties
  // Visible rect from AVFrame crop fields
  frame->visible_left_ = srcFrame->crop_left;
  frame->visible_top_ = srcFrame->crop_top;
  frame->visible_width_ = srcFrame->width - srcFrame->crop_left - srcFrame->crop_right;
  frame->visible_height_ = srcFrame->height - srcFrame->crop_top - srcFrame->crop_bottom;

  // [SPEC] Display dimensions from visible dimensions with SAR correction
  frame->display_width_ = frame->visible_width_;
  frame->display_height_ = frame->visible_height_;

  // Apply sample aspect ratio to display width
  if (srcFrame->sample_aspect_ratio.num > 0 && srcFrame->sample_aspect_ratio.den > 0) {
    frame->display_width_ = frame->visible_width_ * srcFrame->sample_aspect_ratio.num /
                            srcFrame->sample_aspect_ratio.den;
  }

  // rotation_ and flip_ remain at defaults (0 and false) for decoded frames

  return jsFrame;
}

Napi::Object VideoFrame::CloneFrom(Napi::Env env, VideoFrame* source) {
  if (!source || !source->frame_) {
    Napi::Error::New(env, "Cannot clone from null or empty VideoFrame").ThrowAsJavaScriptException();
    return Napi::Object();
  }

  // Create a new JS VideoFrame object
  Napi::Object jsFrame = constructor.New({});
  VideoFrame* frame = Napi::ObjectWrap<VideoFrame>::Unwrap(jsFrame);

  // Clone the AVFrame (refcounted - shares underlying buffers)
  frame->frame_ = raii::CloneAvFrame(source->frame_.get());
  if (!frame->frame_) {
    Napi::Error::New(env, "Failed to clone AVFrame").ThrowAsJavaScriptException();
    return Napi::Object();
  }

  // [SPEC] Copy all internal slots from source VideoFrame
  // These may differ from AVFrame properties for constructed frames
  frame->rotation_ = source->rotation_;
  frame->flip_ = source->flip_;
  frame->visible_left_ = source->visible_left_;
  frame->visible_top_ = source->visible_top_;
  frame->visible_width_ = source->visible_width_;
  frame->visible_height_ = source->visible_height_;
  frame->display_width_ = source->display_width_;
  frame->display_height_ = source->display_height_;

  // [SPEC] Copy metadata via structured clone
  if (!source->metadata_.IsEmpty()) {
    Napi::Object srcMetadata = source->metadata_.Value();
    Napi::Object clonedMetadata = Napi::Object::New(env);
    Napi::Array keys = srcMetadata.GetPropertyNames();
    for (uint32_t i = 0; i < keys.Length(); i++) {
      Napi::Value key = keys.Get(i);
      Napi::Value value = srcMetadata.Get(key);
      clonedMetadata.Set(key, value);
    }
    frame->metadata_ = Napi::Persistent(clonedMetadata);
  }

  return jsFrame;
}

// --- Attributes ---

// Helper: Convert AVPixelFormat to WebCodecs VideoPixelFormat string
// Uses format_converter for full W3C spec compliance (21 formats)
static const char* PixelFormatToString(AVPixelFormat fmt) {
  return format_converter::FFmpegToWebCodecs(fmt);
}

Napi::Value VideoFrame::GetFormat(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (closed_.load(std::memory_order_acquire) || !frame_) {
    return env.Null();
  }

  const char* format = PixelFormatToString(static_cast<AVPixelFormat>(frame_->format));
  if (format) {
    return Napi::String::New(env, format);
  }
  return env.Null();
}

Napi::Value VideoFrame::GetCodedWidth(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (closed_.load(std::memory_order_acquire) || !frame_) {
    return env.Null();
  }
  return Napi::Number::New(env, frame_->width);
}

Napi::Value VideoFrame::GetCodedHeight(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (closed_.load(std::memory_order_acquire) || !frame_) {
    return env.Null();
  }
  return Napi::Number::New(env, frame_->height);
}

Napi::Value VideoFrame::GetCodedRect(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (closed_.load(std::memory_order_acquire) || !frame_) {
    return env.Null();
  }

  // Return DOMRectReadOnly-like object
  Napi::Object rect = Napi::Object::New(env);
  rect.Set("x", Napi::Number::New(env, 0));
  rect.Set("y", Napi::Number::New(env, 0));
  rect.Set("width", Napi::Number::New(env, frame_->width));
  rect.Set("height", Napi::Number::New(env, frame_->height));
  return rect;
}

Napi::Value VideoFrame::GetVisibleRect(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (closed_.load(std::memory_order_acquire) || !frame_) {
    return env.Null();
  }

  // Use our internal slots if set, otherwise derive from AVFrame crop
  int x = visible_left_;
  int y = visible_top_;
  int width = (visible_width_ > 0) ? visible_width_ : (frame_->width - frame_->crop_left - frame_->crop_right);
  int height = (visible_height_ > 0) ? visible_height_ : (frame_->height - frame_->crop_top - frame_->crop_bottom);

  Napi::Object rect = Napi::Object::New(env);
  rect.Set("x", Napi::Number::New(env, x));
  rect.Set("y", Napi::Number::New(env, y));
  rect.Set("width", Napi::Number::New(env, width));
  rect.Set("height", Napi::Number::New(env, height));
  return rect;
}

Napi::Value VideoFrame::GetRotation(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (closed_.load(std::memory_order_acquire) || !frame_) {
    return env.Null();
  }
  // Return our internal slot
  return Napi::Number::New(env, rotation_);
}

Napi::Value VideoFrame::GetFlip(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (closed_.load(std::memory_order_acquire) || !frame_) {
    return env.Null();
  }
  // Return our internal slot
  return Napi::Boolean::New(env, flip_);
}

Napi::Value VideoFrame::GetDisplayWidth(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (closed_.load(std::memory_order_acquire) || !frame_) {
    return env.Null();
  }

  // Use our internal slot if set
  if (display_width_ > 0) {
    return Napi::Number::New(env, display_width_);
  }

  // Otherwise calculate from visible rect and rotation
  int visible_w = (visible_width_ > 0) ? visible_width_ : frame_->width;
  int visible_h = (visible_height_ > 0) ? visible_height_ : frame_->height;

  // Swap for 90/270 rotation
  if (rotation_ == 90 || rotation_ == 270) {
    return Napi::Number::New(env, visible_h);
  }
  return Napi::Number::New(env, visible_w);
}

Napi::Value VideoFrame::GetDisplayHeight(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (closed_.load(std::memory_order_acquire) || !frame_) {
    return env.Null();
  }

  // Use our internal slot if set
  if (display_height_ > 0) {
    return Napi::Number::New(env, display_height_);
  }

  // Otherwise calculate from visible rect and rotation
  int visible_w = (visible_width_ > 0) ? visible_width_ : frame_->width;
  int visible_h = (visible_height_ > 0) ? visible_height_ : frame_->height;

  // Swap for 90/270 rotation
  if (rotation_ == 90 || rotation_ == 270) {
    return Napi::Number::New(env, visible_w);
  }
  return Napi::Number::New(env, visible_h);
}

Napi::Value VideoFrame::GetDuration(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (closed_.load(std::memory_order_acquire) || !frame_) {
    return env.Null();
  }

  if (frame_->duration > 0) {
    return Napi::Number::New(env, static_cast<double>(frame_->duration));
  }
  return env.Null();
}

Napi::Value VideoFrame::GetTimestamp(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (closed_.load(std::memory_order_acquire) || !frame_) {
    return env.Null();
  }

  // pts is the presentation timestamp
  if (frame_->pts != AV_NOPTS_VALUE) {
    return Napi::Number::New(env, static_cast<double>(frame_->pts));
  }
  return Napi::Number::New(env, 0);
}

Napi::Value VideoFrame::GetColorSpace(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (closed_.load(std::memory_order_acquire) || !frame_) {
    return env.Null();
  }

  // Return VideoColorSpace-like object
  Napi::Object colorSpace = Napi::Object::New(env);

  // Map FFmpeg color primaries to WebCodecs
  const char* primaries = nullptr;
  switch (frame_->color_primaries) {
    case AVCOL_PRI_BT709:
      primaries = "bt709";
      break;
    case AVCOL_PRI_BT470BG:
      primaries = "bt470bg";
      break;
    case AVCOL_PRI_SMPTE170M:
      primaries = "smpte170m";
      break;
    case AVCOL_PRI_BT2020:
      primaries = "bt2020";
      break;
    default:
      break;
  }
  if (primaries) {
    colorSpace.Set("primaries", Napi::String::New(env, primaries));
  } else {
    colorSpace.Set("primaries", env.Null());
  }

  // Map FFmpeg transfer characteristics
  const char* transfer = nullptr;
  switch (frame_->color_trc) {
    case AVCOL_TRC_BT709:
      transfer = "bt709";
      break;
    case AVCOL_TRC_SMPTE170M:
      transfer = "smpte170m";
      break;
    case AVCOL_TRC_IEC61966_2_1:
      transfer = "iec61966-2-1";
      break;
    case AVCOL_TRC_SMPTE2084:
      transfer = "pq";
      break;
    case AVCOL_TRC_ARIB_STD_B67:
      transfer = "hlg";
      break;
    default:
      break;
  }
  if (transfer) {
    colorSpace.Set("transfer", Napi::String::New(env, transfer));
  } else {
    colorSpace.Set("transfer", env.Null());
  }

  // Map FFmpeg colorspace to matrix
  const char* matrix = nullptr;
  switch (frame_->colorspace) {
    case AVCOL_SPC_RGB:
      matrix = "rgb";
      break;
    case AVCOL_SPC_BT709:
      matrix = "bt709";
      break;
    case AVCOL_SPC_BT470BG:
    case AVCOL_SPC_SMPTE170M:
      matrix = "smpte170m";
      break;
    case AVCOL_SPC_BT2020_NCL:
      matrix = "bt2020-ncl";
      break;
    default:
      break;
  }
  if (matrix) {
    colorSpace.Set("matrix", Napi::String::New(env, matrix));
  } else {
    colorSpace.Set("matrix", env.Null());
  }

  // Full range flag
  colorSpace.Set("fullRange", Napi::Boolean::New(env, frame_->color_range == AVCOL_RANGE_JPEG));

  return colorSpace;
}

// --- Methods ---

Napi::Value VideoFrame::Metadata(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC 9.4.5] metadata() algorithm:
  // 1. If [[Detached]] is true, throw InvalidStateError
  // 2. Return the result of calling Copy VideoFrame metadata with [[metadata]]

  // Step 1: Check if detached (closed)
  if (closed_.load(std::memory_order_acquire)) {
    errors::ThrowInvalidStateError(env, "VideoFrame is closed");
    return env.Undefined();
  }

  // Step 2: Copy VideoFrame metadata (structured clone)
  // [SPEC 9.4.6] "Copy VideoFrame metadata" algorithm:
  // 1. Let metadataCopySerialized be StructuredSerialize(metadata)
  // 2. Let metadataCopy be StructuredDeserialize(metadataCopySerialized)
  // 3. Return metadataCopy
  //
  // We implement this as a shallow clone of the dictionary object.
  // All VideoFrameMetadata properties are serializable per spec.

  if (metadata_.IsEmpty()) {
    // Return empty object if no metadata set
    return Napi::Object::New(env);
  }

  Napi::Object srcMetadata = metadata_.Value();
  Napi::Object clonedMetadata = Napi::Object::New(env);

  // Deep copy all properties from source metadata
  Napi::Array keys = srcMetadata.GetPropertyNames();
  for (uint32_t i = 0; i < keys.Length(); i++) {
    Napi::Value key = keys.Get(i);
    Napi::Value value = srcMetadata.Get(key);
    clonedMetadata.Set(key, value);
  }

  return clonedMetadata;
}

Napi::Value VideoFrame::AllocationSize(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Returns the number of bytes required to hold the frame data
  // Throws if frame is closed

  if (closed_.load(std::memory_order_acquire) || !frame_) {
    errors::ThrowInvalidStateError(env, "VideoFrame is closed");
    return env.Undefined();
  }

  // Optional options parameter with rect
  // For now, we return the size for the entire frame
  int size = buffer_utils::CalculateFrameBufferSize(frame_->format, frame_->width, frame_->height, 1);

  if (size < 0) {
    errors::ThrowEncodingError(env, "Failed to calculate buffer size");
    return env.Undefined();
  }

  return Napi::Number::New(env, static_cast<double>(size));
}

Napi::Value VideoFrame::CopyTo(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC 9.4.5] copyTo(destination, options)
  // Copies frame data to destination buffer with optional format/rect/layout conversion
  // Returns a Promise that resolves with a PlaneLayout array

  // Check if frame is closed
  if (closed_.load(std::memory_order_acquire) || !frame_) {
    Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
    deferred.Reject(errors::CreateInvalidStateError(env, "VideoFrame is closed").Value());
    return deferred.Promise();
  }

  // Validate destination argument
  if (info.Length() < 1) {
    Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
    deferred.Reject(Napi::TypeError::New(env, "destination buffer is required").Value());
    return deferred.Promise();
  }

  // Extract destination buffer
  uint8_t* dest = nullptr;
  size_t dest_size = 0;

  if (info[0].IsArrayBuffer()) {
    Napi::ArrayBuffer buffer = info[0].As<Napi::ArrayBuffer>();
    dest = static_cast<uint8_t*>(buffer.Data());
    dest_size = buffer.ByteLength();
  } else if (info[0].IsTypedArray()) {
    Napi::TypedArray typedArray = info[0].As<Napi::TypedArray>();
    Napi::ArrayBuffer arrayBuffer = typedArray.ArrayBuffer();
    dest = static_cast<uint8_t*>(arrayBuffer.Data()) + typedArray.ByteOffset();
    dest_size = typedArray.ByteLength();
  } else {
    Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
    deferred.Reject(Napi::TypeError::New(env, "destination must be an ArrayBuffer or TypedArray").Value());
    return deferred.Promise();
  }

  // Parse VideoFrameCopyToOptions
  std::string dst_format;
  std::string color_space = "srgb";  // Default per spec
  int rect_x = 0, rect_y = 0;
  int rect_width = frame_->width - frame_->crop_left - frame_->crop_right;
  int rect_height = frame_->height - frame_->crop_top - frame_->crop_bottom;
  bool has_rect = false;
  bool has_format = false;
  std::vector<int> layout_offsets;
  std::vector<int> layout_strides;
  bool has_layout = false;

  if (info.Length() >= 2 && info[1].IsObject()) {
    Napi::Object options = info[1].As<Napi::Object>();

    // Parse rect option
    if (options.Has("rect") && options.Get("rect").IsObject()) {
      Napi::Object rect = options.Get("rect").As<Napi::Object>();
      if (rect.Has("x")) rect_x = rect.Get("x").As<Napi::Number>().Int32Value();
      if (rect.Has("y")) rect_y = rect.Get("y").As<Napi::Number>().Int32Value();
      if (rect.Has("width")) rect_width = rect.Get("width").As<Napi::Number>().Int32Value();
      if (rect.Has("height")) rect_height = rect.Get("height").As<Napi::Number>().Int32Value();
      has_rect = true;

      // Validate rect bounds
      if (rect_x < 0 || rect_y < 0 || rect_width <= 0 || rect_height <= 0 ||
          rect_x + rect_width > frame_->width || rect_y + rect_height > frame_->height) {
        Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
        deferred.Reject(Napi::TypeError::New(env, "rect out of bounds").Value());
        return deferred.Promise();
      }
    }

    // Parse format option (for pixel format conversion)
    if (options.Has("format") && options.Get("format").IsString()) {
      dst_format = options.Get("format").As<Napi::String>().Utf8Value();
      has_format = true;

      // Validate format is supported for conversion
      if (!format_converter::IsRGBFormat(dst_format)) {
        Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
        deferred.Reject(errors::CreateNotSupportedError(env,
            "Format conversion only supports RGBA, RGBX, BGRA, BGRX").Value());
        return deferred.Promise();
      }
    }

    // Parse colorSpace option
    if (options.Has("colorSpace") && options.Get("colorSpace").IsString()) {
      color_space = options.Get("colorSpace").As<Napi::String>().Utf8Value();
      if (color_space != "srgb" && color_space != "display-p3") {
        Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
        deferred.Reject(errors::CreateNotSupportedError(env,
            "Unsupported colorSpace: " + color_space).Value());
        return deferred.Promise();
      }
    }

    // Parse layout option
    if (options.Has("layout") && options.Get("layout").IsArray()) {
      Napi::Array layoutArray = options.Get("layout").As<Napi::Array>();
      has_layout = true;

      for (uint32_t i = 0; i < layoutArray.Length(); i++) {
        if (!layoutArray.Get(i).IsObject()) {
          Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
          deferred.Reject(Napi::TypeError::New(env, "layout entries must be objects").Value());
          return deferred.Promise();
        }
        Napi::Object planeLayout = layoutArray.Get(i).As<Napi::Object>();
        int offset = 0, stride = 0;
        if (planeLayout.Has("offset")) offset = planeLayout.Get("offset").As<Napi::Number>().Int32Value();
        if (planeLayout.Has("stride")) stride = planeLayout.Get("stride").As<Napi::Number>().Int32Value();
        layout_offsets.push_back(offset);
        layout_strides.push_back(stride);
      }
    }
  }

  // Determine source frame (possibly converted)
  const AVFrame* src_frame = frame_.get();
  raii::AVFramePtr converted_frame;

  // Apply format/colorspace conversion or rect cropping if needed
  if (has_format || has_rect) {
    format_converter::FormatConverter converter;

    if (has_rect) {
      // Convert with rect (crop + optional format conversion)
      converted_frame = converter.ConvertRect(
          frame_.get(), rect_x, rect_y, rect_width, rect_height,
          has_format ? dst_format : std::string(PixelFormatToString(static_cast<AVPixelFormat>(frame_->format))),
          color_space);
    } else {
      // Full frame format conversion
      converted_frame = converter.Convert(frame_.get(), dst_format, color_space);
    }

    if (!converted_frame) {
      Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
      deferred.Reject(errors::CreateEncodingError(env, "Format conversion failed").Value());
      return deferred.Promise();
    }
    src_frame = converted_frame.get();
  }

  // Calculate required size
  int required;
  if (has_layout) {
    required = format_converter::CalculateSizeWithLayout(
        src_frame->format, src_frame->width, src_frame->height,
        layout_offsets.data(), layout_strides.data(), static_cast<int>(layout_offsets.size()));
  } else {
    required = buffer_utils::CalculateFrameBufferSize(src_frame->format, src_frame->width, src_frame->height, 1);
  }

  if (required < 0) {
    Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
    deferred.Reject(errors::CreateEncodingError(env, "Failed to calculate buffer size").Value());
    return deferred.Promise();
  }

  if (dest_size < static_cast<size_t>(required)) {
    Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
    deferred.Reject(Napi::TypeError::New(env, "destination buffer is too small").Value());
    return deferred.Promise();
  }

  // Copy frame data to destination
  int ret;
  if (has_layout) {
    ret = format_converter::CopyFrameWithLayout(
        src_frame, dest, dest_size,
        layout_offsets.data(), layout_strides.data(), static_cast<int>(layout_offsets.size()));
  } else {
    ret = buffer_utils::CopyFrameToBuffer(src_frame, dest, dest_size, 1);
  }

  if (ret < 0) {
    Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
    deferred.Reject(errors::CreateEncodingError(env, "Failed to copy frame data").Value());
    return deferred.Promise();
  }

  // Build PlaneLayout array
  Napi::Array planeLayouts = Napi::Array::New(env);
  int numPlanes = buffer_utils::GetPlaneCount(src_frame->format);
  size_t offset = 0;

  for (int i = 0; i < numPlanes; i++) {
    Napi::Object layout = Napi::Object::New(env);

    if (has_layout && i < static_cast<int>(layout_offsets.size())) {
      layout.Set("offset", Napi::Number::New(env, static_cast<double>(layout_offsets[i])));
      layout.Set("stride", Napi::Number::New(env, layout_strides[i]));
    } else {
      size_t planeSize = buffer_utils::GetPlaneSize(src_frame, i);
      layout.Set("offset", Napi::Number::New(env, static_cast<double>(offset)));
      layout.Set("stride", Napi::Number::New(env, src_frame->linesize[i]));
      offset += planeSize;
    }

    planeLayouts.Set(static_cast<uint32_t>(i), layout);
  }

  Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
  deferred.Resolve(planeLayouts);
  return deferred.Promise();
}

Napi::Value VideoFrame::Clone(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC 9.4.5] clone() algorithm:
  // 1. If [[Detached]] is true, throw InvalidStateError
  // 2. Return the result of running Clone VideoFrame with this

  if (closed_.load(std::memory_order_acquire)) {
    errors::ThrowInvalidStateError(env, "VideoFrame is closed");
    return env.Undefined();
  }

  if (!frame_) {
    errors::ThrowInvalidStateError(env, "VideoFrame has no data");
    return env.Undefined();
  }

  // [SPEC] Clone VideoFrame copies all internal slots, not just AVFrame
  return CloneFrom(env, this);
}

Napi::Value VideoFrame::Close(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Close releases the media resource immediately
  // After close(), all accessors return null and clone() throws

  // Release is idempotent - safe to call multiple times
  Release();

  return env.Undefined();
}

Napi::Value VideoFrame::SerializeForTransfer(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC 9.4.7] Transfer and Serialization
  //
  // Transfer steps:
  // 1. If [[Detached]] is true, throw DataCloneError
  // 2. Serialize all internal slots to dataHolder
  // 3. Run close() on this VideoFrame (detach source)
  //
  // Serialization steps (clone without detach):
  // 1. If [[Detached]] is true, throw DataCloneError
  // 2. Create new reference to resource (av_frame_ref)
  // 3. Copy all internal slots

  // Step 1: Check if detached (closed)
  if (closed_.load(std::memory_order_acquire)) {
    errors::ThrowDataCloneError(env, "Cannot transfer a closed VideoFrame");
    return env.Undefined();
  }

  if (!frame_) {
    errors::ThrowDataCloneError(env, "VideoFrame has no data");
    return env.Undefined();
  }

  // Parse transfer flag (default: false for serialization/clone semantics)
  bool transfer = false;
  if (info.Length() > 0 && info[0].IsBoolean()) {
    transfer = info[0].As<Napi::Boolean>().Value();
  }

  // Step 2: Create clone with new reference to underlying buffers
  // This copies all internal slots and uses av_frame_ref - zero-copy, refcount++
  Napi::Object cloned = CloneFrom(env, this);
  if (cloned.IsEmpty()) {
    errors::ThrowDataCloneError(env, "Failed to serialize VideoFrame");
    return env.Undefined();
  }

  // Step 3: If transfer=true, close this frame (detach source)
  // The cloned frame now owns the only reference to the buffers
  if (transfer) {
    Release();
  }

  return cloned;
}

}  // namespace webcodecs
