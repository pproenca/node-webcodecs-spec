#include "image_track.h"

#include <limits>

#include "image_decoder.h"
#include "image_track_list.h"

namespace webcodecs {

Napi::FunctionReference ImageTrack::constructor_;

Napi::Object ImageTrack::Init(Napi::Env env, Napi::Object exports) {
  Napi::Function func = DefineClass(
      env, "ImageTrack",
      {
          InstanceAccessor<&ImageTrack::GetAnimated>("animated"),
          InstanceAccessor<&ImageTrack::GetFrameCount>("frameCount"),
          InstanceAccessor<&ImageTrack::GetRepetitionCount>("repetitionCount"),
          InstanceAccessor<&ImageTrack::GetSelected, &ImageTrack::SetSelected>("selected"),
      });

  constructor_ = Napi::Persistent(func);
  constructor_.SuppressDestruct();
  exports.Set("ImageTrack", func);
  return exports;
}

ImageTrack::ImageTrack(const Napi::CallbackInfo& info) : Napi::ObjectWrap<ImageTrack>(info) {
  // ImageTrack should only be created via Create() factory method
  // Direct construction from JS is not allowed per spec
}

ImageTrack::~ImageTrack() {
  // Parent references are weak (decoder owns the track, not vice versa)
  decoder_ = nullptr;
  track_list_ = nullptr;
}

Napi::Object ImageTrack::Create(Napi::Env env, bool animated, uint32_t frame_count,
                                 float repetition_count, ImageDecoder* decoder,
                                 ImageTrackList* track_list, uint32_t track_index) {
  // Create instance via constructor
  Napi::Object obj = constructor_.New({});
  ImageTrack* track = ImageTrack::Unwrap(obj);

  // Initialize internal slots
  track->animated_ = animated;
  track->frame_count_.store(frame_count, std::memory_order_release);
  track->repetition_count_ = repetition_count;
  track->selected_.store(false, std::memory_order_release);
  track->decoder_ = decoder;
  track->track_list_ = track_list;
  track->track_index_ = track_index;

  return obj;
}

void ImageTrack::SetFrameCount(uint32_t count) {
  frame_count_.store(count, std::memory_order_release);
}

void ImageTrack::SetSelectedInternal(bool selected) {
  selected_.store(selected, std::memory_order_release);
}

// =============================================================================
// Attribute Getters
// =============================================================================

Napi::Value ImageTrack::GetAnimated(const Napi::CallbackInfo& info) {
  return Napi::Boolean::New(info.Env(), animated_);
}

Napi::Value ImageTrack::GetFrameCount(const Napi::CallbackInfo& info) {
  return Napi::Number::New(info.Env(), frame_count_.load(std::memory_order_acquire));
}

Napi::Value ImageTrack::GetRepetitionCount(const Napi::CallbackInfo& info) {
  // Per spec: unrestricted float, can be Infinity
  if (std::isinf(repetition_count_)) {
    return Napi::Number::New(info.Env(), std::numeric_limits<double>::infinity());
  }
  return Napi::Number::New(info.Env(), static_cast<double>(repetition_count_));
}

Napi::Value ImageTrack::GetSelected(const Napi::CallbackInfo& info) {
  return Napi::Boolean::New(info.Env(), selected_.load(std::memory_order_acquire));
}

// =============================================================================
// Selected Setter (per spec 10.7.2)
// =============================================================================

void ImageTrack::SetSelected(const Napi::CallbackInfo& info, const Napi::Value& value) {
  Napi::Env env = info.Env();

  // 1. If [[ImageDecoder]]'s [[closed]] slot is true, abort
  if (decoder_ == nullptr) {
    return;
  }

  // Get new value
  if (!value.IsBoolean()) {
    Napi::TypeError::New(env, "selected must be a boolean").ThrowAsJavaScriptException();
    return;
  }
  bool new_value = value.As<Napi::Boolean>().Value();

  // 3. If newValue equals [[selected]], abort
  bool current = selected_.load(std::memory_order_acquire);
  if (new_value == current) {
    return;
  }

  // 4. Assign newValue to [[selected]]
  selected_.store(new_value, std::memory_order_release);

  // 5-9. Update track list and decoder via track list
  if (track_list_ != nullptr) {
    track_list_->OnTrackSelectedChanged(this, new_value);
  }
}

}  // namespace webcodecs
