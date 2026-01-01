#include "VideoFrame.h"

namespace webcodecs {

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

  // [SPEC] Constructor Algorithm
  /*
   * Initialize internal slots.
   */

  // TODO(impl): Implement Constructor & Resource Allocation
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
  frame->frame_ = raii::clone_av_frame(srcFrame);
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
  // TODO(impl): Return codedRect
  return info.Env().Null();
}

Napi::Value VideoFrame::GetVisibleRect(const Napi::CallbackInfo& info) {
  // TODO(impl): Return visibleRect
  return info.Env().Null();
}

Napi::Value VideoFrame::GetRotation(const Napi::CallbackInfo& info) {
  // TODO(impl): Return rotation
  return info.Env().Null();
}

Napi::Value VideoFrame::GetFlip(const Napi::CallbackInfo& info) {
  // TODO(impl): Return flip
  return info.Env().Null();
}

Napi::Value VideoFrame::GetDisplayWidth(const Napi::CallbackInfo& info) {
  // TODO(impl): Return displayWidth
  return info.Env().Null();
}

Napi::Value VideoFrame::GetDisplayHeight(const Napi::CallbackInfo& info) {
  // TODO(impl): Return displayHeight
  return info.Env().Null();
}

Napi::Value VideoFrame::GetDuration(const Napi::CallbackInfo& info) {
  // TODO(impl): Return duration
  return info.Env().Null();
}

Napi::Value VideoFrame::GetTimestamp(const Napi::CallbackInfo& info) {
  // TODO(impl): Return timestamp
  return info.Env().Null();
}

Napi::Value VideoFrame::GetColorSpace(const Napi::CallbackInfo& info) {
  // TODO(impl): Return colorSpace
  return info.Env().Null();
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

  // [SPEC] Algorithm
  /*
   * See spec/context file.
   */

  // TODO(impl): Implement method logic
  return env.Undefined();
}

Napi::Value VideoFrame::CopyTo(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Algorithm
  /*
   * See spec/context file.
   */

  // TODO(impl): Implement method logic
  return env.Undefined();
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
