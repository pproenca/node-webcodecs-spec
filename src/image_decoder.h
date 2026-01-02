#pragma once
/**
 * image_decoder.h - W3C WebCodecs ImageDecoder implementation
 *
 * Unlike VideoDecoder/AudioDecoder which use configure() + decode(),
 * ImageDecoder receives all data in the constructor and uses
 * promise-based decode() calls.
 *
 * @see https://www.w3.org/TR/webcodecs/#imagedecoder-interface
 */

#include <napi.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "ffmpeg_raii.h"
#include "shared/control_message_queue.h"
#include "shared/safe_tsfn.h"

namespace webcodecs {

// Forward declarations
class ImageDecoderWorker;
class ImageTrackList;
class ImageTrack;
struct ImageTrackInfo;
struct ImageDecodeResult;

/**
 * ImageDecoder - W3C WebCodecs ImageDecoder implementation
 *
 * Architecture:
 * - JS main thread: handles API calls, creates promises
 * - Worker thread: processes FFmpeg image demuxing and decoding
 * - Output via SafeThreadSafeFunction back to JS thread
 *
 * Thread Safety:
 * - All public methods are called from the JS main thread
 * - Decode operations run on dedicated worker thread
 * - Promise resolution occurs on JS main thread via TSFN
 */
class ImageDecoder : public Napi::ObjectWrap<ImageDecoder> {
 public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  explicit ImageDecoder(const Napi::CallbackInfo& info);
  ~ImageDecoder() override;

  // Non-copyable, non-movable (Google C++ Style Guide)
  ImageDecoder(const ImageDecoder&) = delete;
  ImageDecoder& operator=(const ImageDecoder&) = delete;
  ImageDecoder(ImageDecoder&&) = delete;
  ImageDecoder& operator=(ImageDecoder&&) = delete;

  // RAII Release - cleans up all resources
  void Release();

  /**
   * Called by ImageTrackList when track selection changes.
   * Enqueues message to worker to update selected track.
   */
  void OnTrackSelectionChanged(int32_t selected_index);

  // Access constructor for static methods
  static Napi::FunctionReference& GetConstructor() { return constructor_; }

 private:
  static Napi::FunctionReference constructor_;

  // --- Message Queue ---
  ImageControlQueue queue_;

  // --- Worker Thread ---
  std::unique_ptr<ImageDecoderWorker> worker_;

  // --- Thread-Safe State ---
  std::atomic<bool> closed_{false};
  std::atomic<bool> complete_{false};
  std::atomic<bool> tracks_established_{false};

  // --- Data structs for TSFN callbacks ---
  struct DecodeResultData {
    uint32_t promise_id;
    AVFrame* frame;  // Raw pointer, ownership transferred
    int64_t timestamp;
    int64_t duration;
    bool complete;
  };

  struct ErrorData {
    uint32_t promise_id;  // 0 if not decode-related
    int error_code;
    std::string message;
  };

  struct TracksReadyData {
    std::vector<ImageTrackInfo> tracks;
    int32_t selected_index;
  };

  // --- TSFN Callback Handlers ---
  static void OnDecodeResult(Napi::Env env, Napi::Function jsCallback,
                              ImageDecoder* context, DecodeResultData* data);
  static void OnError(Napi::Env env, Napi::Function jsCallback,
                      ImageDecoder* context, ErrorData* data);
  static void OnTracksReady(Napi::Env env, Napi::Function jsCallback,
                             ImageDecoder* context, TracksReadyData* data);
  static void OnCompleted(Napi::Env env, Napi::Function jsCallback,
                           ImageDecoder* context, void** data);

  // --- TSFN types ---
  using DecodeResultTSFN =
      SafeThreadSafeFunction<ImageDecoder, DecodeResultData, &ImageDecoder::OnDecodeResult>;
  DecodeResultTSFN decode_result_tsfn_;

  using ErrorTSFN = SafeThreadSafeFunction<ImageDecoder, ErrorData, &ImageDecoder::OnError>;
  ErrorTSFN error_tsfn_;

  using TracksReadyTSFN =
      SafeThreadSafeFunction<ImageDecoder, TracksReadyData, &ImageDecoder::OnTracksReady>;
  TracksReadyTSFN tracks_ready_tsfn_;

  using CompletedTSFN = SafeThreadSafeFunction<ImageDecoder, void*, &ImageDecoder::OnCompleted>;
  CompletedTSFN completed_tsfn_;

  // --- Pending decode promises ---
  std::unordered_map<uint32_t, Napi::Promise::Deferred> pending_decodes_;
  uint32_t next_promise_id_{1};  // Start at 1, 0 means "not decode-related"
  std::mutex promise_mutex_;

  // --- Completed promise ---
  std::optional<Napi::Promise::Deferred> completed_deferred_;
  Napi::Reference<Napi::Promise> completed_promise_ref_;

  // --- Child objects ---
  Napi::ObjectReference tracks_ref_;

  // --- Configuration (deep copy from init) ---
  std::string type_;

  // Attributes
  Napi::Value GetType(const Napi::CallbackInfo& info);
  Napi::Value GetComplete(const Napi::CallbackInfo& info);
  Napi::Value GetCompleted(const Napi::CallbackInfo& info);
  Napi::Value GetTracks(const Napi::CallbackInfo& info);

  // Methods
  Napi::Value Decode(const Napi::CallbackInfo& info);
  void Reset(const Napi::CallbackInfo& info);
  void Close(const Napi::CallbackInfo& info);
  static Napi::Value IsTypeSupported(const Napi::CallbackInfo& info);

  // --- Internal Helpers ---
  void InitializeTSFNs(Napi::Env env);
  void ReleaseTSFNs();
  bool ValidateInit(Napi::Env env, const Napi::Object& init);
  void SetupWorkerCallbacks();

  // Friend class for worker access
  friend class ImageDecoderWorker;
  friend class ImageTrackList;
};

}  // namespace webcodecs
