#pragma once
/**
 * error_builder.h - FFmpeg Error Handling and DOMException Builders
 *
 * Provides utilities for:
 * - Converting FFmpeg error codes to human-readable strings
 * - Classifying FFmpeg errors (EAGAIN, EOF, actual errors)
 * - Building WebCodecs-spec DOMExceptions for JavaScript
 *
 * Usage:
 *   int ret = avcodec_send_packet(ctx, pkt);
 *   if (ret < 0) {
 *     auto classification = ClassifyFfmpegError(ret);
 *     if (classification == FFmpegErrorClass::Error) {
 *       ThrowEncodingError(env, ret, "Failed to send packet");
 *     }
 *   }
 */

#include <napi.h>
#include <string>

extern "C" {
#include <libavutil/error.h>
}

namespace webcodecs {
namespace errors {

// =============================================================================
// FFMPEG ERROR STRING UTILITIES
// =============================================================================

/**
 * Convert an FFmpeg error code to a human-readable string.
 * Uses av_strerror internally.
 */
inline std::string FfmpegErrorString(int errnum) {
  char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
  av_strerror(errnum, errbuf, sizeof(errbuf));
  return std::string(errbuf);
}

/**
 * Create a detailed error message with FFmpeg error info.
 * Format: "context: ffmpeg_error (errno)"
 */
inline std::string MakeErrorMessage(const char* context, int errnum) {
  std::string msg = context;
  msg += ": ";
  msg += FfmpegErrorString(errnum);
  msg += " (";
  msg += std::to_string(errnum);
  msg += ")";
  return msg;
}

// =============================================================================
// FFMPEG ERROR CLASSIFICATION
// =============================================================================

/**
 * Classification of FFmpeg error codes for control flow.
 */
enum class FFmpegErrorClass {
  Success,    // ret >= 0: Operation succeeded
  Again,      // AVERROR(EAGAIN): Need more input/output not ready
  Eof,        // AVERROR_EOF: End of stream reached
  Error       // All other negative values: Actual error
};

/**
 * Classify an FFmpeg return value for control flow decisions.
 */
inline FFmpegErrorClass ClassifyFfmpegError(int ret) {
  if (ret >= 0) {
    return FFmpegErrorClass::Success;
  }
  if (ret == AVERROR(EAGAIN)) {
    return FFmpegErrorClass::Again;
  }
  if (ret == AVERROR_EOF) {
    return FFmpegErrorClass::Eof;
  }
  return FFmpegErrorClass::Error;
}

/**
 * Check if error is recoverable (EAGAIN or EOF).
 */
inline bool IsRecoverableError(int ret) {
  auto cls = ClassifyFfmpegError(ret);
  return cls == FFmpegErrorClass::Again || cls == FFmpegErrorClass::Eof;
}

// =============================================================================
// DOMEXCEPTION BUILDERS (WebCodecs Spec)
// =============================================================================

/**
 * Throw a NotSupportedError DOMException.
 * Used when: codec/config is not supported by the implementation.
 */
inline void ThrowNotSupportedError(Napi::Env env, const std::string& message) {
  Napi::Error error = Napi::Error::New(env, "NotSupportedError: " + message);
  error.Set("name", Napi::String::New(env, "NotSupportedError"));
  error.ThrowAsJavaScriptException();
}

/**
 * Throw an InvalidStateError DOMException.
 * Used when: method called in wrong state (e.g., decode before configure).
 */
inline void ThrowInvalidStateError(Napi::Env env, const std::string& message) {
  Napi::Error error = Napi::Error::New(env, "InvalidStateError: " + message);
  error.Set("name", Napi::String::New(env, "InvalidStateError"));
  error.ThrowAsJavaScriptException();
}

/**
 * Throw a DataError DOMException.
 * Used when: input data is malformed or invalid.
 */
inline void ThrowDataError(Napi::Env env, const std::string& message) {
  Napi::Error error = Napi::Error::New(env, "DataError: " + message);
  error.Set("name", Napi::String::New(env, "DataError"));
  error.ThrowAsJavaScriptException();
}

/**
 * Throw an EncodingError DOMException.
 * Used when: encode/decode operation fails due to codec error.
 */
inline void ThrowEncodingError(Napi::Env env, const std::string& message) {
  Napi::Error error = Napi::Error::New(env, "EncodingError: " + message);
  error.Set("name", Napi::String::New(env, "EncodingError"));
  error.ThrowAsJavaScriptException();
}

/**
 * Throw an EncodingError with FFmpeg error details.
 */
inline void ThrowEncodingError(Napi::Env env, int ffmpeg_err,
                                  const char* context) {
  std::string message = MakeErrorMessage(context, ffmpeg_err);
  ThrowEncodingError(env, message);
}

/**
 * Throw an AbortError DOMException.
 * Used when: operation was aborted by user (reset/close).
 */
inline void ThrowAbortError(Napi::Env env, const std::string& message) {
  Napi::Error error = Napi::Error::New(env, "AbortError: " + message);
  error.Set("name", Napi::String::New(env, "AbortError"));
  error.ThrowAsJavaScriptException();
}

/**
 * Throw a TypeError.
 * Used when: argument type/value is invalid.
 */
inline void ThrowTypeError(Napi::Env env, const std::string& message) {
  Napi::TypeError::New(env, message).ThrowAsJavaScriptException();
}

// =============================================================================
// HELPER MACROS
// =============================================================================

/**
 * Check FFmpeg return value and throw EncodingError on failure.
 * Usage: WEBCODECS_FFMPEG_CHECK(env, avcodec_send_packet(ctx, pkt), "send packet");
 */
#define WEBCODECS_FFMPEG_CHECK(env, expr, context) \
  do { \
    int __ret = (expr); \
    if (__ret < 0 && !::webcodecs::errors::IsRecoverableError(__ret)) { \
      ::webcodecs::errors::ThrowEncodingError(env, __ret, context); \
      return env.Undefined(); \
    } \
  } while (0)

/**
 * Check FFmpeg return value and return the error class for control flow.
 */
#define WEBCODECS_FFMPEG_CLASSIFY(expr) \
  ::webcodecs::errors::ClassifyFfmpegError(expr)

// =============================================================================
// STATE VALIDATION HELPERS
// =============================================================================

/**
 * Check if codec is in configured state, throw InvalidStateError if not.
 */
template<typename StateType>
inline bool RequireConfiguredState(Napi::Env env, const StateType& state,
                                      const char* method_name) {
  if (!state.is_configured()) {
    std::string msg = std::string(method_name) +
        " called on " + state.to_string() + " decoder";
    ThrowInvalidStateError(env, msg);
    return false;
  }
  return true;
}

/**
 * Check if codec is not closed, throw InvalidStateError if closed.
 */
template<typename StateType>
inline bool RequireNotClosed(Napi::Env env, const StateType& state,
                                const char* method_name) {
  if (state.is_closed()) {
    std::string msg = std::string(method_name) + " called on closed codec";
    ThrowInvalidStateError(env, msg);
    return false;
  }
  return true;
}

}  // namespace errors
}  // namespace webcodecs
