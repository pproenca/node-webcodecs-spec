#pragma once
/**
 * napi_guards.h - N-API Handle Scope and Resource Management Guards
 *
 * Provides RAII utilities for coordinating N-API handle scopes with
 * FFmpeg resource lifetimes, ensuring proper cleanup ordering.
 *
 * Key guarantees:
 * - FFmpeg resources are freed BEFORE handle scopes close
 * - Type-safe unwrapping with runtime type checking
 * - Exception-safe cleanup on all code paths
 */

#include <napi.h>

#include <string>
#include <utility>

#include "../ffmpeg_raii.h"

namespace webcodecs {
namespace napi_guards {

// =============================================================================
// TYPE TAGS - Runtime type identification for WebCodecs objects
// =============================================================================

/**
 * Type tags for safe runtime type checking of wrapped objects.
 * Each WebCodecs class gets a unique tag to prevent type confusion attacks.
 *
 * Usage:
 *   napi_type_tag_object(env, jsObject, &VIDEO_FRAME_TAG);
 *   // Later:
 *   bool matches;
 *   napi_check_object_type_tag(env, jsObject, &VIDEO_FRAME_TAG, &matches);
 */
// "WEBCODEC" "VIDEOFRA"
constexpr napi_type_tag VIDEO_FRAME_TAG = {0x574542434F444543ULL, 0x564944454F465241ULL};
// "WEBCODEC" "AUDIODAT"
constexpr napi_type_tag AUDIO_DATA_TAG = {0x574542434F444543ULL, 0x415544494F444154ULL};
// "WEBCODEC" "ENVVIDC"
constexpr napi_type_tag ENCODED_VIDEO_CHUNK_TAG = {0x574542434F444543ULL, 0x454E5656494443ULL};
// "WEBCODEC" "ENAUDIC"
constexpr napi_type_tag ENCODED_AUDIO_CHUNK_TAG = {0x574542434F444543ULL, 0x454E4155444943ULL};
// "WEBCODEC" "VIDDECOD"
constexpr napi_type_tag VIDEO_DECODER_TAG = {0x574542434F444543ULL, 0x5649444445434F44ULL};
// "WEBCODEC" "VIDENCODE"
constexpr napi_type_tag VIDEO_ENCODER_TAG = {0x574542434F444543ULL, 0x564944454E434F44ULL};
// "WEBCODEC" "AUDDECOD"
constexpr napi_type_tag AUDIO_DECODER_TAG = {0x574542434F444543ULL, 0x4155444445434F44ULL};
// "WEBCODEC" "AUDENCODE"
constexpr napi_type_tag AUDIO_ENCODER_TAG = {0x574542434F444543ULL, 0x415544454E434F44ULL};
// "WEBCODEC" "IMGDECOD"
constexpr napi_type_tag IMAGE_DECODER_TAG = {0x574542434F444543ULL, 0x494D474445434F44ULL};

// =============================================================================
// HANDLE SCOPE GUARDS
// =============================================================================

/**
 * HandleScopeGuard - RAII wrapper for napi_handle_scope.
 *
 * Automatically opens a handle scope on construction and closes it on
 * destruction. Useful for limiting the lifetime of JS handles.
 *
 * Usage:
 *   void ProcessFrames(Napi::Env env) {
 *     HandleScopeGuard guard(env);
 *     // Create temporary JS objects...
 *   }  // All handles released here
 */
class HandleScopeGuard {
 public:
  explicit HandleScopeGuard(napi_env env) : env_(env), scope_(nullptr) {
    napi_status status = napi_open_handle_scope(env_, &scope_);
    if (status != napi_ok) {
      scope_ = nullptr;
    }
  }

  ~HandleScopeGuard() {
    if (scope_) {
      napi_close_handle_scope(env_, scope_);
    }
  }

  // Non-copyable, non-movable
  HandleScopeGuard(const HandleScopeGuard&) = delete;
  HandleScopeGuard& operator=(const HandleScopeGuard&) = delete;
  HandleScopeGuard(HandleScopeGuard&&) = delete;
  HandleScopeGuard& operator=(HandleScopeGuard&&) = delete;

  bool valid() const { return scope_ != nullptr; }

 private:
  napi_env env_;
  napi_handle_scope scope_;
};

/**
 * EscapableHandleScopeGuard - RAII wrapper for napi_escapable_handle_scope.
 *
 * Use when you need to create temporary handles but return one of them
 * to the caller. Call escape() on the value you want to return.
 *
 * Usage:
 *   napi_value CreateFrame(napi_env env) {
 *     EscapableHandleScopeGuard guard(env);
 *     napi_value frame = CreateFrameObject(env);
 *     return guard.escape(frame);  // This value survives scope close
 *   }
 */
class EscapableHandleScopeGuard {
 public:
  explicit EscapableHandleScopeGuard(napi_env env) : env_(env), scope_(nullptr) {
    napi_status status = napi_open_escapable_handle_scope(env_, &scope_);
    if (status != napi_ok) {
      scope_ = nullptr;
    }
  }

  ~EscapableHandleScopeGuard() {
    if (scope_) {
      napi_close_escapable_handle_scope(env_, scope_);
    }
  }

  // Non-copyable, non-movable
  EscapableHandleScopeGuard(const EscapableHandleScopeGuard&) = delete;
  EscapableHandleScopeGuard& operator=(const EscapableHandleScopeGuard&) = delete;
  EscapableHandleScopeGuard(EscapableHandleScopeGuard&&) = delete;
  EscapableHandleScopeGuard& operator=(EscapableHandleScopeGuard&&) = delete;

  bool valid() const { return scope_ != nullptr; }

  /**
   * Escape a handle from this scope so it survives after scope closes.
   * Can only be called once per scope.
   */
  napi_value escape(napi_value value) {
    if (!scope_) return nullptr;
    napi_value escaped = nullptr;
    napi_escape_handle(env_, scope_, value, &escaped);
    return escaped;
  }

 private:
  napi_env env_;
  napi_escapable_handle_scope scope_;
};

// =============================================================================
// SCOPED RESOURCE + HANDLE SCOPE
// =============================================================================

/**
 * ScopedResourceWithHandleScope - Combines RAII resource with handle scope.
 *
 * CRITICAL: Ensures FFmpeg resources are freed BEFORE the handle scope closes.
 * This prevents use-after-free when JS objects reference native data.
 *
 * Destruction order:
 * 1. FFmpeg resource (unique_ptr reset)
 * 2. Handle scope close
 *
 * Usage:
 *   void ProcessFrame(Napi::Env env, raii::AVFramePtr frame) {
 *     ScopedResourceWithHandleScope<raii::AVFramePtr> guard(env, std::move(frame));
 *     // Use guard.get() to access the frame
 *     // Create JS objects that reference frame data
 *   }  // Frame freed, then handle scope closed
 */
template <typename FFmpegPtr>
class ScopedResourceWithHandleScope {
 public:
  ScopedResourceWithHandleScope(napi_env env, FFmpegPtr resource)
      : env_(env), resource_(std::move(resource)), scope_(nullptr) {
    napi_status status = napi_open_handle_scope(env_, &scope_);
    if (status != napi_ok) {
      scope_ = nullptr;
    }
  }

  ~ScopedResourceWithHandleScope() {
    // Order matters:
    // 1. Release FFmpeg resource (while JS refs still valid)
    resource_.reset();
    // 2. Close handle scope
    if (scope_) {
      napi_close_handle_scope(env_, scope_);
    }
  }

  // Non-copyable, non-movable
  ScopedResourceWithHandleScope(const ScopedResourceWithHandleScope&) = delete;
  ScopedResourceWithHandleScope& operator=(const ScopedResourceWithHandleScope&) = delete;
  ScopedResourceWithHandleScope(ScopedResourceWithHandleScope&&) = delete;
  ScopedResourceWithHandleScope& operator=(ScopedResourceWithHandleScope&&) = delete;

  bool valid() const { return scope_ != nullptr && resource_ != nullptr; }

  auto* get() { return resource_.get(); }
  const auto* get() const { return resource_.get(); }
  auto* operator->() { return resource_.get(); }
  const auto* operator->() const { return resource_.get(); }

  /**
   * Release ownership of the resource.
   * Use when transferring ownership to a JS wrapper.
   */
  [[nodiscard]] auto release() { return resource_.release(); }

 private:
  napi_env env_;
  FFmpegPtr resource_;
  napi_handle_scope scope_;
};

// =============================================================================
// TYPE-SAFE UNWRAPPING
// =============================================================================

/**
 * Check if a JS object has the expected type tag.
 *
 * @param env The N-API environment
 * @param object The JS object to check
 * @param tag The expected type tag
 * @return true if the object has the matching tag, false otherwise
 */
inline bool CheckTypeTag(napi_env env, napi_value object, const napi_type_tag* tag) {
  if (!object) return false;
  bool matches = false;
  napi_status status = napi_check_object_type_tag(env, object, tag, &matches);
  return status == napi_ok && matches;
}

/**
 * Tag a JS object with a type tag for later verification.
 *
 * @param env The N-API environment
 * @param object The JS object to tag
 * @param tag The type tag to apply
 * @return true if tagging succeeded, false otherwise
 */
inline bool TagObject(napi_env env, napi_value object, const napi_type_tag* tag) {
  if (!object) return false;
  napi_status status = napi_type_tag_object(env, object, tag);
  return status == napi_ok;
}

/**
 * SafeUnwrap - Type-safe unwrapping with runtime type checking.
 *
 * Verifies the object has the expected type tag before unwrapping.
 * Throws TypeError if the type doesn't match.
 *
 * Usage:
 *   VideoFrame* frame = SafeUnwrap<VideoFrame>(env, jsValue, VIDEO_FRAME_TAG);
 *   if (!frame) { /* handle error */ }
**@tparam T The C++ wrapper class(e.g., VideoFrame) * @param env The N - API environment *@param value The JS value to
    unwrap *@param tag The expected type tag *@ return Pointer to the unwrapped object,
    or nullptr on failure * / template <typename T>
                              T *SafeUnwrap(Napi::Env env, Napi::Value value, const napi_type_tag& tag) {
  if (!value.IsObject()) {
    Napi::TypeError::New(env, "Expected an object").ThrowAsJavaScriptException();
    return nullptr;
  }

  napi_value nvalue = value;
  if (!CheckTypeTag(env, nvalue, &tag)) {
    std::string msg = "Expected ";
    msg += typeid(T).name();
    Napi::TypeError::New(env, msg).ThrowAsJavaScriptException();
    return nullptr;
  }

  return Napi::ObjectWrap<T>::Unwrap(value.As<Napi::Object>());
}

/**
 * SafeUnwrap variant that doesn't throw, returns nullptr on failure.
 */
template <typename T>
T* SafeUnwrapNoThrow(Napi::Env env, Napi::Value value, const napi_type_tag& tag) {
  if (!value.IsObject()) {
    return nullptr;
  }

  napi_value nvalue = value;
  if (!CheckTypeTag(env, nvalue, &tag)) {
    return nullptr;
  }

  return Napi::ObjectWrap<T>::Unwrap(value.As<Napi::Object>());
}

// =============================================================================
// PERSISTENT REFERENCE GUARD
// =============================================================================

/**
 * PersistentRef - RAII wrapper for Napi::Reference with automatic cleanup.
 *
 * Ensures references are properly released on destruction, preventing leaks.
 *
 * Usage:
 *   class VideoDecoder {
 *     PersistentRef<Napi::Function> outputCallback_;
 *
 *     void SetCallback(Napi::Value value) {
 *       outputCallback_.Reset(value.As<Napi::Function>());
 *     }
 *   };
 */
template <typename T>
class PersistentRef {
 public:
  PersistentRef() = default;

  explicit PersistentRef(const T& value) { ref_.Reset(value, 1); }

  ~PersistentRef() { Reset(); }

  // Move-only
  PersistentRef(const PersistentRef&) = delete;
  PersistentRef& operator=(const PersistentRef&) = delete;

  PersistentRef(PersistentRef&& other) noexcept : ref_(std::move(other.ref_)) {}

  PersistentRef& operator=(PersistentRef&& other) noexcept {
    if (this != &other) {
      Reset();
      ref_ = std::move(other.ref_);
    }
    return *this;
  }

  void Reset() {
    if (!ref_.IsEmpty()) {
      ref_.Reset();
    }
  }

  void Reset(const T& value, uint32_t refcount = 1) {
    Reset();
    ref_.Reset(value, refcount);
  }

  bool IsEmpty() const { return ref_.IsEmpty(); }

  T Value() const { return ref_.Value(); }

  explicit operator bool() const { return !ref_.IsEmpty(); }

 private:
  Napi::Reference<T> ref_;
};

}  // namespace napi_guards
}  // namespace webcodecs
