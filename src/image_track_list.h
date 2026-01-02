#pragma once
/**
 * image_track_list.h - W3C WebCodecs ImageTrackList implementation
 *
 * A live list of ImageTrack objects representing tracks in an image file.
 * Provides the ready promise and track selection mechanism.
 *
 * @see https://www.w3.org/TR/webcodecs/#imagetracklist
 */

#include <napi.h>

#include <atomic>
#include <vector>

namespace webcodecs {

// Forward declarations
class ImageDecoder;
class ImageTrack;

/**
 * ImageTrackList - List of tracks in an image file.
 *
 * Internal slots per spec 10.6.1:
 * - [[ready promise]]: Promise that resolves when tracks are established
 * - [[track list]]: List of ImageTrack objects
 * - [[selected index]]: Index of selected track (-1 if none)
 */
class ImageTrackList : public Napi::ObjectWrap<ImageTrackList> {
 public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  explicit ImageTrackList(const Napi::CallbackInfo& info);
  ~ImageTrackList() override;

  // Non-copyable, non-movable (Google C++ Style)
  ImageTrackList(const ImageTrackList&) = delete;
  ImageTrackList& operator=(const ImageTrackList&) = delete;
  ImageTrackList(ImageTrackList&&) = delete;
  ImageTrackList& operator=(ImageTrackList&&) = delete;

  /**
   * Factory method for creating ImageTrackList from C++.
   * Used by ImageDecoder during construction.
   */
  static Napi::Object Create(Napi::Env env, ImageDecoder* decoder);

  /**
   * Add a track to the list.
   * Called by ImageDecoder when establishing tracks.
   */
  void AddTrack(Napi::Object track);

  /**
   * Clear all tracks.
   * Called during close().
   */
  void ClearTracks();

  /**
   * Set the selected index.
   * Thread-safe.
   */
  void SetSelectedIndex(int32_t index);

  /**
   * Get the selected index.
   */
  [[nodiscard]] int32_t GetSelectedIndexValue() const;

  /**
   * Resolve the ready promise.
   * Called when tracks are established.
   */
  void ResolveReady();

  /**
   * Reject the ready promise.
   * Called on error or close before tracks established.
   */
  void RejectReady(const Napi::Error& error);

  /**
   * Called by ImageTrack when its selected property changes.
   * Handles the track selection logic per spec.
   */
  void OnTrackSelectedChanged(ImageTrack* track, bool new_value);

  // Access to constructor for Create()
  static Napi::FunctionReference& GetConstructor() { return constructor_; }

 private:
  static Napi::FunctionReference constructor_;

  // Indexed getter (tracks[index])
  Napi::Value GetTrackByIndex(const Napi::CallbackInfo& info);

  // Attribute getters
  Napi::Value GetReady(const Napi::CallbackInfo& info);
  Napi::Value GetLength(const Napi::CallbackInfo& info);
  Napi::Value GetSelectedIndex(const Napi::CallbackInfo& info);
  Napi::Value GetSelectedTrack(const Napi::CallbackInfo& info);

  // Internal slots (per spec 10.6.1)
  std::vector<Napi::ObjectReference> tracks_;   // [[track list]]
  std::atomic<int32_t> selected_index_{-1};     // [[selected index]]

  // Ready promise management
  Napi::Promise::Deferred ready_deferred_;
  Napi::Reference<Napi::Promise> ready_promise_ref_;
  bool ready_resolved_{false};
  bool ready_rejected_{false};

  // Parent decoder reference (weak)
  ImageDecoder* decoder_{nullptr};

  // Allow ImageDecoder access to internal slots
  friend class ImageDecoder;
};

}  // namespace webcodecs
