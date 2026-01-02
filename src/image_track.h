#pragma once
/**
 * image_track.h - W3C WebCodecs ImageTrack implementation
 *
 * Represents a single track within an image file.
 * For animated images (GIF, WebP, APNG), each track can have multiple frames.
 *
 * @see https://www.w3.org/TR/webcodecs/#imagetrack
 */

#include <napi.h>

#include <atomic>
#include <cmath>

namespace webcodecs {

// Forward declarations
class ImageDecoder;
class ImageTrackList;

/**
 * ImageTrack - Represents a track in an image file.
 *
 * Internal slots per spec:
 * - [[ImageDecoder]]: Parent decoder
 * - [[ImageTrackList]]: Parent track list
 * - [[animated]]: Whether track contains animation
 * - [[frame count]]: Number of frames
 * - [[repetition count]]: Loop count (Infinity for infinite)
 * - [[selected]]: Whether this track is selected for decoding
 */
class ImageTrack : public Napi::ObjectWrap<ImageTrack> {
 public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  explicit ImageTrack(const Napi::CallbackInfo& info);
  ~ImageTrack() override;

  // Non-copyable, non-movable (Google C++ Style)
  ImageTrack(const ImageTrack&) = delete;
  ImageTrack& operator=(const ImageTrack&) = delete;
  ImageTrack(ImageTrack&&) = delete;
  ImageTrack& operator=(ImageTrack&&) = delete;

  /**
   * Factory method for creating ImageTrack from C++.
   * Used by ImageDecoder when establishing tracks.
   */
  static Napi::Object Create(Napi::Env env, bool animated, uint32_t frame_count,
                              float repetition_count, ImageDecoder* decoder,
                              ImageTrackList* track_list, uint32_t track_index);

  /**
   * Update frame count (called when more data arrives via ReadableStream).
   * Thread-safe.
   */
  void SetFrameCount(uint32_t count);

  /**
   * Set selected state without triggering callbacks.
   * Used internally by ImageTrackList when changing selection.
   */
  void SetSelectedInternal(bool selected);

  /**
   * Get the track index within the track list.
   */
  [[nodiscard]] uint32_t GetTrackIndex() const { return track_index_; }

  // Access to constructor for Create()
  static Napi::FunctionReference& GetConstructor() { return constructor_; }

 private:
  static Napi::FunctionReference constructor_;

  // Attribute getters
  Napi::Value GetAnimated(const Napi::CallbackInfo& info);
  Napi::Value GetFrameCount(const Napi::CallbackInfo& info);
  Napi::Value GetRepetitionCount(const Napi::CallbackInfo& info);
  Napi::Value GetSelected(const Napi::CallbackInfo& info);
  void SetSelected(const Napi::CallbackInfo& info, const Napi::Value& value);

  // Internal slots (per spec 10.7.1)
  ImageDecoder* decoder_{nullptr};        // [[ImageDecoder]]
  ImageTrackList* track_list_{nullptr};   // [[ImageTrackList]]
  bool animated_{false};                  // [[animated]]
  std::atomic<uint32_t> frame_count_{0};  // [[frame count]]
  float repetition_count_{0.0f};          // [[repetition count]]
  std::atomic<bool> selected_{false};     // [[selected]]
  uint32_t track_index_{0};               // Position in track list

  // Allow ImageDecoder and ImageTrackList to access internal slots
  friend class ImageDecoder;
  friend class ImageTrackList;
};

}  // namespace webcodecs
