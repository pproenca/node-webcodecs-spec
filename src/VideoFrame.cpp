#include "VideoFrame.h"
#include "shared/buffer_utils.h"
#include "error_builder.h"

namespace webcodecs {

// Helper: Convert WebCodecs VideoPixelFormat string to AVPixelFormat
static AVPixelFormat StringToPixelFormat(const std::string& format) {
  if (format == "I420") return AV_PIX_FMT_YUV420P;
  if (format == "I422") return AV_PIX_FMT_YUV422P;
  if (format == "I444") return AV_PIX_FMT_YUV444P;
  if (format == "NV12") return AV_PIX_FMT_NV12;
  if (format == "NV21") return AV_PIX_FMT_NV21;
  if (format == "RGBA") return AV_PIX_FMT_RGBA;
  if (format == "BGRA") return AV_PIX_FMT_BGRA;
  if (format == "RGBX") return AV_PIX_FMT_RGB24;
  if (format == "BGRX") return AV_PIX_FMT_BGR24;
  return AV_PIX_FMT_NONE;
}

Napi::FunctionReference VideoFrame::constructor;

Napi::Object VideoFrame::Init(Napi::Env env, Napi::Object exports) {
  Napi::Function func = DefineClass(env, "VideoFrame", {
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

  });

  constructor = Napi::Persistent(func);
  constructor.SuppressDestruct();
  exports.Set("VideoFrame", func);
  return exports;
}

VideoFrame::VideoFrame(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<VideoFrame>(info) {
  Napi::Env env = info.Env();

  // [SPEC] VideoFrame constructor can be called with:
  // 1. (data, init) - VideoFrameBufferInit with raw pixel data
  // 2. (image) - CanvasImageSource (not supported in Node.js)
  // 3. No arguments - internal use by CreateFromAVFrame

  if (info.Length() == 0) {
    // Internal construction - frame_ will be set by CreateFromAVFrame
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

    // Create AVFrame from buffer data
    frame_ = buffer_utils::CreateFrameFromBuffer(data, size, width, height,
                                                  static_cast<int>(pix_fmt));
    if (!frame_) {
      errors::ThrowTypeError(env, "Failed to create frame from data");
      return;
    }

    // Set timestamp if provided
    if (init.Has("timestamp") && init.Get("timestamp").IsNumber()) {
      frame_->pts = init.Get("timestamp").As<Napi::Number>().Int64Value();
    }

    // Set duration if provided
    if (init.Has("duration") && init.Get("duration").IsNumber()) {
      frame_->duration = init.Get("duration").As<Napi::Number>().Int64Value();
    }

    return;
  }

  // If we get here with arguments but not matching pattern, error
  if (info.Length() > 0) {
    errors::ThrowTypeError(env, "Invalid VideoFrame constructor arguments");
  }
}

VideoFrame::~VideoFrame() {
  Release();
}

void VideoFrame::Release() {
  // Mark as closed (atomic, thread-safe)
  closed_.store(true, std::memory_order_release);

  // Release the AVFrame (RAII handles av_frame_free)
  // This unrefs the underlying buffers; they're freed when refcount hits 0
  frame_.reset();
}

Napi::Object VideoFrame::CreateFromAVFrame(Napi::Env env, const AVFrame* srcFrame) {
  if (!srcFrame) {
    Napi::Error::New(env, "Cannot create VideoFrame from null AVFrame")
        .ThrowAsJavaScriptException();
    return Napi::Object();
  }

  // Create a new JS VideoFrame object
  Napi::Object jsFrame = constructor.New({});
  VideoFrame* frame = Napi::ObjectWrap<VideoFrame>::Unwrap(jsFrame);

  // Clone the AVFrame (refcounted - shares underlying buffers)
  frame->frame_ = raii::CloneAvFrame(srcFrame);
  if (!frame->frame_) {
    Napi::Error::New(env, "Failed to clone AVFrame")
        .ThrowAsJavaScriptException();
    return Napi::Object();
  }

  return jsFrame;
}

// --- Attributes ---

// Helper: Convert AVPixelFormat to WebCodecs VideoPixelFormat string
static const char* PixelFormatToString(AVPixelFormat fmt) {
  switch (fmt) {
    case AV_PIX_FMT_YUV420P: return "I420";
    case AV_PIX_FMT_YUV422P: return "I422";
    case AV_PIX_FMT_YUV444P: return "I444";
    case AV_PIX_FMT_NV12: return "NV12";
    case AV_PIX_FMT_NV21: return "NV21";
    case AV_PIX_FMT_RGBA: return "RGBA";
    case AV_PIX_FMT_BGRA: return "BGRA";
    case AV_PIX_FMT_RGB24: return "RGBX";
    case AV_PIX_FMT_BGR24: return "BGRX";
    default: return nullptr;  // Unsupported format
  }
}

Napi::Value VideoFrame::GetFormat(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (closed_.load(std::memory_order_acquire) || !frame_) {
    return env.Null();
  }

  const char* format = PixelFormatToString(
      static_cast<AVPixelFormat>(frame_->format));
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

  // For now, visible rect equals coded rect (no cropping)
  // AVFrame has crop_top, crop_bottom, crop_left, crop_right for actual crop
  int x = frame_->crop_left;
  int y = frame_->crop_top;
  int width = frame_->width - frame_->crop_left - frame_->crop_right;
  int height = frame_->height - frame_->crop_top - frame_->crop_bottom;

  Napi::Object rect = Napi::Object::New(env);
  rect.Set("x", Napi::Number::New(env, x));
  rect.Set("y", Napi::Number::New(env, y));
  rect.Set("width", Napi::Number::New(env, width));
  rect.Set("height", Napi::Number::New(env, height));
  return rect;
}

Napi::Value VideoFrame::GetRotation(const Napi::CallbackInfo& info) {
  // WebCodecs rotation is 0, 90, 180, 270
  // FFmpeg stores rotation in side data, default to 0
  return Napi::Number::New(info.Env(), 0);
}

Napi::Value VideoFrame::GetFlip(const Napi::CallbackInfo& info) {
  // WebCodecs flip is boolean
  return Napi::Boolean::New(info.Env(), false);
}

Napi::Value VideoFrame::GetDisplayWidth(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (closed_.load(std::memory_order_acquire) || !frame_) {
    return env.Null();
  }

  // Display width accounts for sample aspect ratio
  int display_width = frame_->width;
  if (frame_->sample_aspect_ratio.num > 0 && frame_->sample_aspect_ratio.den > 0) {
    display_width = frame_->width * frame_->sample_aspect_ratio.num /
                    frame_->sample_aspect_ratio.den;
  }
  return Napi::Number::New(env, display_width);
}

Napi::Value VideoFrame::GetDisplayHeight(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (closed_.load(std::memory_order_acquire) || !frame_) {
    return env.Null();
  }
  return Napi::Number::New(env, frame_->height);
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
    case AVCOL_PRI_BT709: primaries = "bt709"; break;
    case AVCOL_PRI_BT470BG: primaries = "bt470bg"; break;
    case AVCOL_PRI_SMPTE170M: primaries = "smpte170m"; break;
    case AVCOL_PRI_BT2020: primaries = "bt2020"; break;
    default: break;
  }
  if (primaries) {
    colorSpace.Set("primaries", Napi::String::New(env, primaries));
  } else {
    colorSpace.Set("primaries", env.Null());
  }

  // Map FFmpeg transfer characteristics
  const char* transfer = nullptr;
  switch (frame_->color_trc) {
    case AVCOL_TRC_BT709: transfer = "bt709"; break;
    case AVCOL_TRC_SMPTE170M: transfer = "smpte170m"; break;
    case AVCOL_TRC_IEC61966_2_1: transfer = "iec61966-2-1"; break;
    case AVCOL_TRC_SMPTE2084: transfer = "pq"; break;
    case AVCOL_TRC_ARIB_STD_B67: transfer = "hlg"; break;
    default: break;
  }
  if (transfer) {
    colorSpace.Set("transfer", Napi::String::New(env, transfer));
  } else {
    colorSpace.Set("transfer", env.Null());
  }

  // Map FFmpeg colorspace to matrix
  const char* matrix = nullptr;
  switch (frame_->colorspace) {
    case AVCOL_SPC_RGB: matrix = "rgb"; break;
    case AVCOL_SPC_BT709: matrix = "bt709"; break;
    case AVCOL_SPC_BT470BG:
    case AVCOL_SPC_SMPTE170M: matrix = "smpte170m"; break;
    case AVCOL_SPC_BT2020_NCL: matrix = "bt2020-ncl"; break;
    default: break;
  }
  if (matrix) {
    colorSpace.Set("matrix", Napi::String::New(env, matrix));
  } else {
    colorSpace.Set("matrix", env.Null());
  }

  // Full range flag
  colorSpace.Set("fullRange", Napi::Boolean::New(env,
      frame_->color_range == AVCOL_RANGE_JPEG));

  return colorSpace;
}


// --- Methods ---

Napi::Value VideoFrame::Metadata(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Algorithm
  /*
   * See spec/context file.
   */

  // TODO(impl): Implement method logic
  return env.Undefined();
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
  int size = buffer_utils::CalculateFrameBufferSize(
      frame_->format, frame_->width, frame_->height, 1);

  if (size < 0) {
    errors::ThrowEncodingError(env, "Failed to calculate buffer size");
    return env.Undefined();
  }

  return Napi::Number::New(env, static_cast<double>(size));
}

Napi::Value VideoFrame::CopyTo(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Copies the frame data to the provided destination buffer
  // Returns a Promise that resolves with a PlaneLayout array

  // Check if frame is closed
  if (closed_.load(std::memory_order_acquire) || !frame_) {
    Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
    Napi::Error error = Napi::Error::New(env, "InvalidStateError: VideoFrame is closed");
    error.Set("name", Napi::String::New(env, "InvalidStateError"));
    deferred.Reject(error.Value());
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

  // Calculate required size
  int required = buffer_utils::CalculateFrameBufferSize(
      frame_->format, frame_->width, frame_->height, 1);

  if (required < 0) {
    Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
    Napi::Error error = Napi::Error::New(env, "EncodingError: Failed to calculate buffer size");
    error.Set("name", Napi::String::New(env, "EncodingError"));
    deferred.Reject(error.Value());
    return deferred.Promise();
  }

  if (dest_size < static_cast<size_t>(required)) {
    Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
    deferred.Reject(Napi::TypeError::New(env, "destination buffer is too small").Value());
    return deferred.Promise();
  }

  // Copy frame data to destination
  int ret = buffer_utils::CopyFrameToBuffer(frame_.get(), dest, dest_size, 1);
  if (ret < 0) {
    Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
    Napi::Error error = Napi::Error::New(env, "EncodingError: Failed to copy frame data");
    error.Set("name", Napi::String::New(env, "EncodingError"));
    deferred.Reject(error.Value());
    return deferred.Promise();
  }

  // Build PlaneLayout array
  Napi::Array planeLayouts = Napi::Array::New(env);
  int numPlanes = buffer_utils::GetPlaneCount(frame_->format);
  size_t offset = 0;

  for (int i = 0; i < numPlanes; i++) {
    size_t planeSize = buffer_utils::GetPlaneSize(frame_.get(), i);
    Napi::Object layout = Napi::Object::New(env);
    layout.Set("offset", Napi::Number::New(env, static_cast<double>(offset)));
    layout.Set("stride", Napi::Number::New(env, frame_->linesize[i]));
    planeLayouts.Set(static_cast<uint32_t>(i), layout);
    offset += planeSize;
  }

  Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
  deferred.Resolve(planeLayouts);
  return deferred.Promise();
}

Napi::Value VideoFrame::Clone(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Clone creates a new VideoFrame referencing the same media resource
  // The clone shares the underlying buffer memory via refcounting

  // Check if this frame is closed
  if (closed_.load(std::memory_order_acquire)) {
    Napi::Error::New(env, "InvalidStateError: VideoFrame is closed")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }

  if (!frame_) {
    Napi::Error::New(env, "InvalidStateError: VideoFrame has no data")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }

  // Create a new VideoFrame that references the same underlying AVFrame buffers
  // This uses av_frame_ref internally, which increments buffer refcounts
  return CreateFromAVFrame(env, frame_.get());
}

Napi::Value VideoFrame::Close(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Close releases the media resource immediately
  // After close(), all accessors return null and clone() throws

  // Release is idempotent - safe to call multiple times
  Release();

  return env.Undefined();
}

}  // namespace webcodecs
