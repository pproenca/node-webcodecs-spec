#pragma once
#include <napi.h>
#include <thread>
#include <vector>
#include <string>
#include <memory>

// --- ENGINEERING EXCELLENCE: Guardrails ---

// Macro: Ensure condition is met or throw JS Error
#define ENSURE(env, condition, message) \
    if (!(condition)) { \
        Napi::Error::New(env, message).ThrowAsJavaScriptException(); \
        return env.Null(); \
    }

// Macro: Check argument count
#define CHECK_ARGS(env, info, count) \
    ENSURE(env, info.Length() >= count, "Invalid number of arguments")

// Macro: Assert state matches expected
#define ENSURE_STATE(env, actual, expected, message) \
    ENSURE(env, actual == expected, message)

// --- ASYNC WORKER TEMPLATE ---
// Use this for all heavy FFmpeg tasks to avoid blocking the Event Loop.
class BaseWorker : public Napi::AsyncWorker {
public:
    explicit BaseWorker(const Napi::Function& callback)
        : Napi::AsyncWorker(callback) {}

    // Subclasses must implement Execute()

    void OnOK() override {
        // Default: Call callback with (null, result)
        Callback().Call({Env().Null()});
    }

    void OnError(const Napi::Error& e) override {
        Callback().Call({e.Value()});
    }
};

// --- RAII HANDLE WRAPPER ---
// Template for automatic cleanup of native handles
template<typename T, void(*Deleter)(T**)>
class ScopedHandle {
public:
    ScopedHandle() : handle_(nullptr) {}
    explicit ScopedHandle(T* handle) : handle_(handle) {}

    ~ScopedHandle() {
        if (handle_) {
            Deleter(&handle_);
        }
    }

    // Move-only semantics
    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;

    ScopedHandle(ScopedHandle&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }

    ScopedHandle& operator=(ScopedHandle&& other) noexcept {
        if (this != &other) {
            if (handle_) Deleter(&handle_);
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    T* get() const { return handle_; }
    T* release() {
        T* tmp = handle_;
        handle_ = nullptr;
        return tmp;
    }
    void reset(T* handle = nullptr) {
        if (handle_) Deleter(&handle_);
        handle_ = handle;
    }

    explicit operator bool() const { return handle_ != nullptr; }
    T* operator->() const { return handle_; }
    T& operator*() const { return *handle_; }

private:
    T* handle_;
};

// --- TYPE CONVERSION HELPERS ---

// JS -> Native
inline std::string ToStdString(const Napi::Value& value) {
    if (!value.IsString()) return "";
    return value.As<Napi::String>().Utf8Value();
}

inline int64_t ToInt64(const Napi::Value& value) {
    if (!value.IsNumber()) return 0;
    return value.As<Napi::Number>().Int64Value();
}

inline double ToDouble(const Napi::Value& value) {
    if (!value.IsNumber()) return 0.0;
    return value.As<Napi::Number>().DoubleValue();
}

inline bool ToBool(const Napi::Value& value) {
    if (!value.IsBoolean()) return false;
    return value.As<Napi::Boolean>().Value();
}

// Native -> JS
inline Napi::String FromStdString(Napi::Env env, const std::string& str) {
    return Napi::String::New(env, str);
}

inline Napi::Number FromInt64(Napi::Env env, int64_t value) {
    return Napi::Number::New(env, static_cast<double>(value));
}

inline Napi::Number FromDouble(Napi::Env env, double value) {
    return Napi::Number::New(env, value);
}

inline Napi::Boolean FromBool(Napi::Env env, bool value) {
    return Napi::Boolean::New(env, value);
}

// --- COMMON TYPES ---

// WebCodecs state enum (shared across Decoder/Encoder)
enum class CodecState {
    Unconfigured,
    Configured,
    Closed
};

inline const char* CodecStateToString(CodecState state) {
    switch (state) {
        case CodecState::Unconfigured: return "unconfigured";
        case CodecState::Configured: return "configured";
        case CodecState::Closed: return "closed";
    }
    return "unknown";
}
