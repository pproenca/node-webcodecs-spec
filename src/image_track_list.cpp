#include "image_track_list.h"

#include "image_decoder.h"
#include "image_track.h"

namespace webcodecs {

Napi::FunctionReference ImageTrackList::constructor_;

Napi::Object ImageTrackList::Init(Napi::Env env, Napi::Object exports) {
  Napi::Function func = DefineClass(
      env, "ImageTrackList",
      {
          InstanceAccessor<&ImageTrackList::GetReady>("ready"),
          InstanceAccessor<&ImageTrackList::GetLength>("length"),
          InstanceAccessor<&ImageTrackList::GetSelectedIndex>("selectedIndex"),
          InstanceAccessor<&ImageTrackList::GetSelectedTrack>("selectedTrack"),
      });

  constructor_ = Napi::Persistent(func);
  constructor_.SuppressDestruct();
  exports.Set("ImageTrackList", func);
  return exports;
}

ImageTrackList::ImageTrackList(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<ImageTrackList>(info),
      ready_deferred_(Napi::Promise::Deferred::New(info.Env())) {
  // Store reference to the ready promise
  ready_promise_ref_ = Napi::Reference<Napi::Promise>::New(ready_deferred_.Promise(), 1);
}

ImageTrackList::~ImageTrackList() {
  // Clear all track references
  tracks_.clear();
  decoder_ = nullptr;
}

Napi::Object ImageTrackList::Create(Napi::Env env, ImageDecoder* decoder) {
  fprintf(stderr, "ImageTrackList::Create - start\n");
  fflush(stderr);

  // Verify constructor is initialized
  fprintf(stderr, "ImageTrackList::Create - checking constructor\n");
  fflush(stderr);
  if (constructor_.IsEmpty()) {
    fprintf(stderr, "ImageTrackList::Create - constructor is empty!\n");
    fflush(stderr);
    Napi::Error::New(env, "ImageTrackList constructor not initialized").ThrowAsJavaScriptException();
    return Napi::Object();
  }

  fprintf(stderr, "ImageTrackList::Create - calling constructor_.New\n");
  fflush(stderr);
  Napi::Object obj = constructor_.New({});
  fprintf(stderr, "ImageTrackList::Create - constructor_.New returned\n");
  fflush(stderr);

  if (obj.IsEmpty()) {
    fprintf(stderr, "ImageTrackList::Create - obj is empty\n");
    fflush(stderr);
    return Napi::Object();
  }

  ImageTrackList* list = ImageTrackList::Unwrap(obj);
  if (list) {
    list->decoder_ = decoder;
  }
  fprintf(stderr, "ImageTrackList::Create - done\n");
  fflush(stderr);
  return obj;
}

void ImageTrackList::AddTrack(Napi::Object track) {
  // Store persistent reference to prevent GC
  tracks_.push_back(Napi::Reference<Napi::Object>::New(track, 1));
}

void ImageTrackList::ClearTracks() {
  // Release all track references
  tracks_.clear();
  selected_index_.store(-1, std::memory_order_release);
}

void ImageTrackList::SetSelectedIndex(int32_t index) {
  selected_index_.store(index, std::memory_order_release);
}

int32_t ImageTrackList::GetSelectedIndexValue() const {
  return selected_index_.load(std::memory_order_acquire);
}

void ImageTrackList::ResolveReady() {
  if (ready_resolved_ || ready_rejected_) {
    return;
  }
  ready_resolved_ = true;
  ready_deferred_.Resolve(ready_deferred_.Env().Undefined());
}

void ImageTrackList::RejectReady(const Napi::Error& error) {
  if (ready_resolved_ || ready_rejected_) {
    return;
  }
  ready_rejected_ = true;
  ready_deferred_.Reject(error.Value());
}

void ImageTrackList::OnTrackSelectedChanged(ImageTrack* track, bool new_value) {
  // Per spec 10.7.2 (selected setter):
  // 5. Let parentTrackList be [[ImageTrackList]]
  // 6. Let oldSelectedIndex be parentTrackList.[[selected index]]
  int32_t old_selected_index = selected_index_.load(std::memory_order_acquire);

  // 7. If oldSelectedIndex is not -1:
  //    a. Let oldSelectedTrack be the track at oldSelectedIndex
  //    b. Assign false to oldSelectedTrack.[[selected]]
  if (old_selected_index >= 0 && old_selected_index < static_cast<int32_t>(tracks_.size())) {
    ImageTrack* old_track = ImageTrack::Unwrap(tracks_[old_selected_index].Value());
    if (old_track != track) {
      old_track->SetSelectedInternal(false);
    }
  }

  // 8. If newValue is true, let selectedIndex be the track index.
  //    Otherwise, let selectedIndex be -1.
  int32_t selected_index = new_value ? static_cast<int32_t>(track->GetTrackIndex()) : -1;

  // 9. Assign selectedIndex to parentTrackList.[[selected index]]
  selected_index_.store(selected_index, std::memory_order_release);

  // 10. Run the Reset ImageDecoder algorithm
  // 11. Queue a control message to update internal selected track index
  // These are handled by the decoder
  if (decoder_ != nullptr) {
    decoder_->OnTrackSelectionChanged(selected_index);
  }
}

// =============================================================================
// Attribute Getters
// =============================================================================

Napi::Value ImageTrackList::GetReady(const Napi::CallbackInfo& info) {
  return ready_promise_ref_.Value();
}

Napi::Value ImageTrackList::GetLength(const Napi::CallbackInfo& info) {
  return Napi::Number::New(info.Env(), static_cast<uint32_t>(tracks_.size()));
}

Napi::Value ImageTrackList::GetSelectedIndex(const Napi::CallbackInfo& info) {
  return Napi::Number::New(info.Env(), selected_index_.load(std::memory_order_acquire));
}

Napi::Value ImageTrackList::GetSelectedTrack(const Napi::CallbackInfo& info) {
  int32_t index = selected_index_.load(std::memory_order_acquire);

  // Per spec: If [[selected index]] is -1, return null
  if (index < 0 || index >= static_cast<int32_t>(tracks_.size())) {
    return info.Env().Null();
  }

  return tracks_[index].Value();
}

// =============================================================================
// Indexed Getter (for tracks[index] access)
// =============================================================================

Napi::Value ImageTrackList::GetTrackByIndex(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1 || !info[0].IsNumber()) {
    return env.Undefined();
  }

  uint32_t index = info[0].As<Napi::Number>().Uint32Value();
  if (index >= tracks_.size()) {
    return env.Undefined();
  }

  return tracks_[index].Value();
}

}  // namespace webcodecs
