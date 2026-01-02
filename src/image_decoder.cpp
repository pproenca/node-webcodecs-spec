#include "image_decoder.h"

#include <cstring>
#include <string>
#include <unordered_set>
#include <utility>

#include "image_decoder_worker.h"
#include "image_track.h"
#include "image_track_list.h"
#include "video_frame.h"
#include "error_builder.h"
#include "shared/buffer_utils.h"

namespace webcodecs {

Napi::FunctionReference ImageDecoder::constructor_;

// =============================================================================
// Supported MIME Types
// =============================================================================

static const std::unordered_set<std::string> kSupportedTypes = {
    "image/jpeg",
    "image/png",
    "image/webp",
    "image/gif",
    "image/avif",
    "image/bmp",
};

static bool IsSupportedType(const std::string& type) {
  return kSupportedTypes.count(type) > 0;
}

// =============================================================================
// IMAGEDECODER IMPLEMENTATION
// =============================================================================

Napi::Object ImageDecoder::Init(Napi::Env env, Napi::Object exports) {
  Napi::Function func = DefineClass(
      env, "ImageDecoder",
      {
          InstanceAccessor<&ImageDecoder::GetType>("type"),
          InstanceAccessor<&ImageDecoder::GetComplete>("complete"),
          InstanceAccessor<&ImageDecoder::GetCompleted>("completed"),
          InstanceAccessor<&ImageDecoder::GetTracks>("tracks"),
          InstanceMethod<&ImageDecoder::Decode>("decode"),
          InstanceMethod<&ImageDecoder::Reset>("reset"),
          InstanceMethod<&ImageDecoder::Close>("close"),
          StaticMethod<&ImageDecoder::IsTypeSupported>("isTypeSupported"),
      });

  constructor_ = Napi::Persistent(func);
  constructor_.SuppressDestruct();
  exports.Set("ImageDecoder", func);
  return exports;
}

ImageDecoder::ImageDecoder(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<ImageDecoder>(info) {
  Napi::Env env = info.Env();

  // [SPEC 10.2.3] Constructor Algorithm
  // 1. If init.data is not provided, throw TypeError
  // 2. If init.type is not provided, throw TypeError

  if (info.Length() < 1 || !info[0].IsObject()) {
    Napi::TypeError::New(env, "ImageDecoderInit is required").ThrowAsJavaScriptException();
    return;
  }

  Napi::Object init = info[0].As<Napi::Object>();

  if (!ValidateInit(env, init)) {
    return;  // Exception already thrown
  }

  // Extract type
  type_ = init.Get("type").As<Napi::String>().Utf8Value();

  // Create completed promise
  completed_deferred_.emplace(Napi::Promise::Deferred::New(env));
  completed_promise_ref_ =
      Napi::Reference<Napi::Promise>::New(completed_deferred_->Promise(), 1);

  // Create ImageTrackList
  Napi::Object track_list = ImageTrackList::Create(env, this);
  tracks_ref_ = Napi::Reference<Napi::Object>::New(track_list, 1);

  // Initialize TSFNs
  InitializeTSFNs(env);

  // Create worker with queue reference and set up callbacks
  worker_ = std::make_unique<ImageDecoderWorker>(queue_);
  SetupWorkerCallbacks();

  // Start the worker thread
  if (!worker_->Start()) {
    Napi::Error::New(env, "Failed to start worker thread").ThrowAsJavaScriptException();
    return;
  }

  // Extract data and enqueue configure message
  Napi::Value data_value = init.Get("data");
  std::vector<uint8_t> encoded_data;

  if (data_value.IsArrayBuffer()) {
    Napi::ArrayBuffer buffer = data_value.As<Napi::ArrayBuffer>();
    encoded_data.resize(buffer.ByteLength());
    std::memcpy(encoded_data.data(), buffer.Data(), buffer.ByteLength());
  } else if (data_value.IsTypedArray()) {
    Napi::TypedArray typed_array = data_value.As<Napi::TypedArray>();
    Napi::ArrayBuffer buffer = typed_array.ArrayBuffer();
    size_t offset = typed_array.ByteOffset();
    size_t length = typed_array.ByteLength();
    encoded_data.resize(length);
    std::memcpy(encoded_data.data(), static_cast<uint8_t*>(buffer.Data()) + offset, length);
  } else if (data_value.IsBuffer()) {
    Napi::Buffer<uint8_t> buffer = data_value.As<Napi::Buffer<uint8_t>>();
    encoded_data.resize(buffer.Length());
    std::memcpy(encoded_data.data(), buffer.Data(), buffer.Length());
  } else {
    // TODO: Handle ReadableStream
    Napi::TypeError::New(env, "data must be ArrayBuffer, TypedArray, or Buffer")
        .ThrowAsJavaScriptException();
    return;
  }

  // Build configure message
  ImageConfigureMessage msg;
  msg.type = type_;
  msg.data = std::move(encoded_data);

  // Optional: colorSpaceConversion
  if (init.Has("colorSpaceConversion") && init.Get("colorSpaceConversion").IsString()) {
    msg.color_space_conversion = init.Get("colorSpaceConversion").As<Napi::String>().Utf8Value();
  }

  // Optional: desiredWidth
  if (init.Has("desiredWidth") && init.Get("desiredWidth").IsNumber()) {
    msg.desired_width = init.Get("desiredWidth").As<Napi::Number>().Uint32Value();
  }

  // Optional: desiredHeight
  if (init.Has("desiredHeight") && init.Get("desiredHeight").IsNumber()) {
    msg.desired_height = init.Get("desiredHeight").As<Napi::Number>().Uint32Value();
  }

  // Optional: preferAnimation
  if (init.Has("preferAnimation") && init.Get("preferAnimation").IsBoolean()) {
    msg.prefer_animation = init.Get("preferAnimation").As<Napi::Boolean>().Value();
  }

  // Enqueue configure message
  (void)queue_.Enqueue(std::move(msg));
}

ImageDecoder::~ImageDecoder() {
  Release();
}

void ImageDecoder::Release() {
  // Mark as closed
  closed_.store(true, std::memory_order_release);

  // Stop the worker thread first
  if (worker_) {
    worker_->Stop();
    worker_.reset();
  }

  // Shutdown the queue
  queue_.Shutdown();

  // Release TSFNs
  ReleaseTSFNs();

  // Reject pending decode promises
  {
    std::lock_guard<std::mutex> lock(promise_mutex_);
    pending_decodes_.clear();
  }

  // Clear track list reference
  if (!tracks_ref_.IsEmpty()) {
    ImageTrackList* track_list = ImageTrackList::Unwrap(tracks_ref_.Value());
    if (track_list) {
      track_list->ClearTracks();
    }
    tracks_ref_.Reset();
  }
}

bool ImageDecoder::ValidateInit(Napi::Env env, const Napi::Object& init) {
  // Check data is provided
  if (!init.Has("data")) {
    Napi::TypeError::New(env, "data is required in ImageDecoderInit").ThrowAsJavaScriptException();
    return false;
  }

  Napi::Value data = init.Get("data");
  if (!data.IsArrayBuffer() && !data.IsTypedArray() && !data.IsBuffer()) {
    // TODO: Also allow ReadableStream
    Napi::TypeError::New(env, "data must be BufferSource or ReadableStream")
        .ThrowAsJavaScriptException();
    return false;
  }

  // Check type is provided
  if (!init.Has("type") || !init.Get("type").IsString()) {
    Napi::TypeError::New(env, "type is required in ImageDecoderInit").ThrowAsJavaScriptException();
    return false;
  }

  std::string type = init.Get("type").As<Napi::String>().Utf8Value();

  // [SPEC 10.2.4] If type is not a valid image MIME type, throw NotSupportedError
  if (!IsSupportedType(type)) {
    errors::ThrowNotSupportedError(env, "Unsupported image type: " + type);
    return false;
  }

  return true;
}

void ImageDecoder::InitializeTSFNs(Napi::Env env) {
  // Create dummy callback (we handle logic in the TSFN handlers)
  Napi::Function dummyFn = Napi::Function::New(env, [](const Napi::CallbackInfo&) {});

  // Decode result TSFN
  using DecodeResultTSFNType =
      Napi::TypedThreadSafeFunction<ImageDecoder, DecodeResultData, &ImageDecoder::OnDecodeResult>;
  auto decode_result_tsfn = DecodeResultTSFNType::New(
      env, dummyFn, "ImageDecoder::decodeResult", 0, 1, this);
  decode_result_tsfn_.Init(std::move(decode_result_tsfn));

  // Error TSFN
  using ErrorTSFNType =
      Napi::TypedThreadSafeFunction<ImageDecoder, ErrorData, &ImageDecoder::OnError>;
  auto error_tsfn = ErrorTSFNType::New(
      env, dummyFn, "ImageDecoder::error", 0, 1, this);
  error_tsfn_.Init(std::move(error_tsfn));

  // Tracks ready TSFN
  using TracksReadyTSFNType =
      Napi::TypedThreadSafeFunction<ImageDecoder, TracksReadyData, &ImageDecoder::OnTracksReady>;
  auto tracks_ready_tsfn = TracksReadyTSFNType::New(
      env, dummyFn, "ImageDecoder::tracksReady", 0, 1, this);
  tracks_ready_tsfn_.Init(std::move(tracks_ready_tsfn));

  // Completed TSFN
  using CompletedTSFNType =
      Napi::TypedThreadSafeFunction<ImageDecoder, void*, &ImageDecoder::OnCompleted>;
  auto completed_tsfn = CompletedTSFNType::New(
      env, dummyFn, "ImageDecoder::completed", 0, 1, this);
  completed_tsfn_.Init(std::move(completed_tsfn));

  // Unref all TSFNs to allow Node.js to exit cleanly
  decode_result_tsfn_.Unref(env);
  error_tsfn_.Unref(env);
  tracks_ready_tsfn_.Unref(env);
  completed_tsfn_.Unref(env);
}

void ImageDecoder::ReleaseTSFNs() {
  decode_result_tsfn_.Release();
  error_tsfn_.Release();
  tracks_ready_tsfn_.Release();
  completed_tsfn_.Release();
}

void ImageDecoder::SetupWorkerCallbacks() {
  if (!worker_) return;

  // Track info callback
  worker_->SetTrackInfoCallback(
      [this](const std::vector<ImageTrackInfo>& tracks, int32_t selected_index) {
        auto* data = new TracksReadyData{tracks, selected_index};
        if (!tracks_ready_tsfn_.Call(data)) {
          delete data;
        }
      });

  // Decode result callback
  worker_->SetDecodeResultCallback(
      [this](uint32_t promise_id, ImageDecodeResult result) {
        auto* data = new DecodeResultData{
            promise_id,
            result.frame.release(),  // Transfer ownership
            result.timestamp,
            result.duration,
            result.complete,
        };
        if (!decode_result_tsfn_.Call(data)) {
          if (data->frame) {
            av_frame_free(&data->frame);
          }
          delete data;
        }
      });

  // Error callback
  worker_->SetErrorCallback(
      [this](uint32_t promise_id, int error_code, const std::string& message) {
        auto* data = new ErrorData{promise_id, error_code, message};
        if (!error_tsfn_.Call(data)) {
          delete data;
        }
      });

  // Completed callback
  worker_->SetCompletedCallback([this]() {
    if (!completed_tsfn_.Call(nullptr)) {
      // Failed to deliver, but nothing to clean up
    }
  });
}

// =============================================================================
// TSFN Callback Handlers
// =============================================================================

void ImageDecoder::OnDecodeResult(Napi::Env env, Napi::Function /*jsCallback*/,
                                   ImageDecoder* context, DecodeResultData* data) {
  if (!data) return;

  // Take ownership of data
  std::unique_ptr<DecodeResultData> data_ptr(data);

  if (!context || context->closed_.load(std::memory_order_acquire)) {
    if (data->frame) {
      av_frame_free(&data->frame);
    }
    return;
  }

  // Find the pending promise and extract it
  Napi::Promise::Deferred deferred(env);
  {
    std::lock_guard<std::mutex> lock(context->promise_mutex_);
    auto it = context->pending_decodes_.find(data->promise_id);
    if (it == context->pending_decodes_.end()) {
      if (data->frame) {
        av_frame_free(&data->frame);
      }
      return;
    }
    deferred = std::move(it->second);
    context->pending_decodes_.erase(it);
  }

  // Create VideoFrame from AVFrame
  Napi::Object frame_obj;
  if (data->frame) {
    frame_obj = VideoFrame::CreateFromAVFrame(env, data->frame);
  }

  // Create ImageDecodeResult object
  Napi::Object result = Napi::Object::New(env);
  if (!frame_obj.IsEmpty()) {
    result.Set("image", frame_obj);
  } else {
    result.Set("image", env.Null());
  }
  result.Set("complete", Napi::Boolean::New(env, data->complete));

  deferred.Resolve(result);
}

void ImageDecoder::OnError(Napi::Env env, Napi::Function /*jsCallback*/,
                            ImageDecoder* context, ErrorData* data) {
  if (!data) return;

  std::unique_ptr<ErrorData> data_ptr(data);

  if (!context || context->closed_.load(std::memory_order_acquire)) {
    return;
  }

  // If promise_id > 0, reject that specific decode promise
  if (data->promise_id > 0) {
    std::lock_guard<std::mutex> lock(context->promise_mutex_);
    auto it = context->pending_decodes_.find(data->promise_id);
    if (it != context->pending_decodes_.end()) {
      Napi::Error error = Napi::Error::New(env, data->message);
      it->second.Reject(error.Value());
      context->pending_decodes_.erase(it);
    }
  } else {
    // General error - reject tracks.ready if not established
    if (!context->tracks_established_.load(std::memory_order_acquire)) {
      ImageTrackList* track_list = ImageTrackList::Unwrap(context->tracks_ref_.Value());
      if (track_list) {
        track_list->RejectReady(Napi::Error::New(env, data->message));
      }
    }

    // Also reject completed promise if not resolved
    if (!context->complete_.load(std::memory_order_acquire) && context->completed_deferred_) {
      context->completed_deferred_->Reject(Napi::Error::New(env, data->message).Value());
      context->completed_deferred_.reset();
    }
  }
}

void ImageDecoder::OnTracksReady(Napi::Env env, Napi::Function /*jsCallback*/,
                                  ImageDecoder* context, TracksReadyData* data) {
  if (!data) return;

  std::unique_ptr<TracksReadyData> data_ptr(data);

  if (!context || context->closed_.load(std::memory_order_acquire)) {
    return;
  }

  // Get track list
  ImageTrackList* track_list = ImageTrackList::Unwrap(context->tracks_ref_.Value());
  if (!track_list) {
    return;
  }

  // Create ImageTrack objects and add to track list
  for (size_t i = 0; i < data->tracks.size(); ++i) {
    const ImageTrackInfo& info = data->tracks[i];
    Napi::Object track = ImageTrack::Create(
        env,
        info.animated,
        info.frame_count,
        info.repetition_count,
        context,
        track_list,
        static_cast<uint32_t>(i));
    track_list->AddTrack(track);
  }

  // Set selected index and mark selected track
  if (data->selected_index >= 0 && data->selected_index < static_cast<int32_t>(data->tracks.size())) {
    track_list->SetSelectedIndex(data->selected_index);

    // Mark the selected track as selected (access via tracks_ vector directly)
    if (static_cast<size_t>(data->selected_index) < track_list->tracks_.size()) {
      Napi::Object selected_track_obj = track_list->tracks_[data->selected_index].Value();
      if (!selected_track_obj.IsEmpty()) {
        ImageTrack* selected_track = ImageTrack::Unwrap(selected_track_obj);
        if (selected_track) {
          selected_track->SetSelectedInternal(true);
        }
      }
    }
  }

  // Mark tracks as established and resolve ready promise
  context->tracks_established_.store(true, std::memory_order_release);
  track_list->ResolveReady();
}

void ImageDecoder::OnCompleted(Napi::Env env, Napi::Function /*jsCallback*/,
                                ImageDecoder* context, void** /*data*/) {
  if (!context || context->closed_.load(std::memory_order_acquire)) {
    return;
  }

  // Mark as complete
  context->complete_.store(true, std::memory_order_release);

  // Resolve completed promise
  if (context->completed_deferred_) {
    context->completed_deferred_->Resolve(env.Undefined());
    context->completed_deferred_.reset();
  }
}

// =============================================================================
// Attributes
// =============================================================================

Napi::Value ImageDecoder::GetType(const Napi::CallbackInfo& info) {
  return Napi::String::New(info.Env(), type_);
}

Napi::Value ImageDecoder::GetComplete(const Napi::CallbackInfo& info) {
  return Napi::Boolean::New(info.Env(), complete_.load(std::memory_order_acquire));
}

Napi::Value ImageDecoder::GetCompleted(const Napi::CallbackInfo& info) {
  return completed_promise_ref_.Value();
}

Napi::Value ImageDecoder::GetTracks(const Napi::CallbackInfo& info) {
  return tracks_ref_.Value();
}

// =============================================================================
// Methods
// =============================================================================

Napi::Value ImageDecoder::Decode(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC 10.4.4] decode() Algorithm
  // 1. If [[closed]] is true, return a promise rejected with InvalidStateError

  if (closed_.load(std::memory_order_acquire)) {
    Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
    deferred.Reject(errors::CreateInvalidStateError(env, "ImageDecoder is closed").Value());
    return deferred.Promise();
  }

  // 2. Parse options
  uint32_t frame_index = 0;
  bool complete_frames_only = true;

  if (info.Length() > 0 && info[0].IsObject()) {
    Napi::Object options = info[0].As<Napi::Object>();

    if (options.Has("frameIndex") && options.Get("frameIndex").IsNumber()) {
      frame_index = options.Get("frameIndex").As<Napi::Number>().Uint32Value();
    }

    if (options.Has("completeFramesOnly") && options.Get("completeFramesOnly").IsBoolean()) {
      complete_frames_only = options.Get("completeFramesOnly").As<Napi::Boolean>().Value();
    }
  }

  // 3. Create promise and store it
  Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
  uint32_t promise_id;
  {
    std::lock_guard<std::mutex> lock(promise_mutex_);
    promise_id = next_promise_id_++;
    pending_decodes_.emplace(promise_id, deferred);
  }

  // 4. Queue decode request
  ImageDecodeMessage msg;
  msg.frame_index = frame_index;
  msg.complete_frames_only = complete_frames_only;
  msg.promise_id = promise_id;

  if (!queue_.Enqueue(std::move(msg))) {
    std::lock_guard<std::mutex> lock(promise_mutex_);
    pending_decodes_.erase(promise_id);
    deferred.Reject(errors::CreateInvalidStateError(env, "Failed to enqueue decode").Value());
  }

  return deferred.Promise();
}

void ImageDecoder::Reset(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC 10.4.5] reset() Algorithm
  // 1. If [[closed]] is true, throw InvalidStateError

  if (closed_.load(std::memory_order_acquire)) {
    errors::ThrowInvalidStateError(env, "ImageDecoder is closed");
    return;
  }

  // 2. Abort pending decode operations
  std::vector<uint32_t> cancelled_ids = queue_.ClearDecodes();

  // Reject cancelled promises with AbortError
  {
    std::lock_guard<std::mutex> lock(promise_mutex_);
    for (uint32_t id : cancelled_ids) {
      auto it = pending_decodes_.find(id);
      if (it != pending_decodes_.end()) {
        it->second.Reject(errors::CreateAbortError(env, "Decode aborted due to reset").Value());
        pending_decodes_.erase(it);
      }
    }
  }

  // 3. Enqueue reset message
  (void)queue_.Enqueue(ImageResetMessage{});
}

void ImageDecoder::Close(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC 10.4.6] close() Algorithm
  // 1. If [[closed]] is true, return

  if (closed_.load(std::memory_order_acquire)) {
    return;
  }

  // 2. Set [[closed]] to true
  closed_.store(true, std::memory_order_release);

  // 3. Abort pending decode operations
  std::vector<uint32_t> cancelled_ids = queue_.ClearDecodes();

  // Reject cancelled promises with AbortError
  {
    std::lock_guard<std::mutex> lock(promise_mutex_);
    for (uint32_t id : cancelled_ids) {
      auto it = pending_decodes_.find(id);
      if (it != pending_decodes_.end()) {
        it->second.Reject(errors::CreateAbortError(env, "ImageDecoder closed").Value());
        pending_decodes_.erase(it);
      }
    }
  }

  // 4. Reject ready promise if not resolved
  if (!tracks_established_.load(std::memory_order_acquire)) {
    ImageTrackList* track_list = ImageTrackList::Unwrap(tracks_ref_.Value());
    if (track_list) {
      track_list->RejectReady(errors::CreateAbortError(env, "ImageDecoder closed"));
    }
  }

  // 5. Reject completed promise if not resolved
  if (!complete_.load(std::memory_order_acquire) && completed_deferred_) {
    completed_deferred_->Reject(errors::CreateAbortError(env, "ImageDecoder closed").Value());
    completed_deferred_.reset();
  }

  // 6. Enqueue close message
  (void)queue_.Enqueue(ImageCloseMessage{});
}

void ImageDecoder::OnTrackSelectionChanged(int32_t selected_index) {
  if (closed_.load(std::memory_order_acquire)) {
    return;
  }

  // Enqueue track update message
  ImageUpdateTrackMessage msg;
  msg.selected_index = selected_index;
  (void)queue_.Enqueue(std::move(msg));
}

// =============================================================================
// Static Methods
// =============================================================================

Napi::Value ImageDecoder::IsTypeSupported(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC 10.3] isTypeSupported() Algorithm
  // Returns a Promise that resolves with a boolean

  if (info.Length() < 1 || !info[0].IsString()) {
    Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
    deferred.Reject(Napi::TypeError::New(env, "type argument is required").Value());
    return deferred.Promise();
  }

  std::string type = info[0].As<Napi::String>().Utf8Value();

  Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
  deferred.Resolve(Napi::Boolean::New(env, IsSupportedType(type)));
  return deferred.Promise();
}

}  // namespace webcodecs
