#include "video_color_space.h"

namespace webcodecs {

Napi::FunctionReference VideoColorSpace::constructor;

Napi::Object VideoColorSpace::Init(Napi::Env env, Napi::Object exports) {
  Napi::Function func = DefineClass(env, "VideoColorSpace",
                                    {
                                        InstanceAccessor<&VideoColorSpace::GetPrimaries>("primaries"),
                                        InstanceAccessor<&VideoColorSpace::GetTransfer>("transfer"),
                                        InstanceAccessor<&VideoColorSpace::GetMatrix>("matrix"),
                                        InstanceAccessor<&VideoColorSpace::GetFullRange>("fullRange"),
                                        InstanceMethod<&VideoColorSpace::ToJSON>("toJSON"),
                                    });

  constructor = Napi::Persistent(func);
  constructor.SuppressDestruct();
  exports.Set("VideoColorSpace", func);
  return exports;
}

VideoColorSpace::VideoColorSpace(const Napi::CallbackInfo& info) : Napi::ObjectWrap<VideoColorSpace>(info) {
  // [SPEC 9.9.2] VideoColorSpace(init) Constructor Algorithm
  // 1. Let c be a new VideoColorSpace object, initialized as follows:
  //    a. Assign init.primaries to [[primaries]]
  //    b. Assign init.transfer to [[transfer]]
  //    c. Assign init.matrix to [[matrix]]
  //    d. Assign init.fullRange to [[full range]]
  // 2. Return c

  // Default construction - all values are null (per WebIDL defaults)
  if (info.Length() < 1 || !info[0].IsObject()) {
    // Empty init = all properties null
    return;
  }

  Napi::Object init = info[0].As<Napi::Object>();

  // primaries (optional, nullable)
  if (init.Has("primaries") && !init.Get("primaries").IsNull() && !init.Get("primaries").IsUndefined()) {
    if (init.Get("primaries").IsString()) {
      primaries_ = init.Get("primaries").As<Napi::String>().Utf8Value();
    }
  }

  // transfer (optional, nullable)
  if (init.Has("transfer") && !init.Get("transfer").IsNull() && !init.Get("transfer").IsUndefined()) {
    if (init.Get("transfer").IsString()) {
      transfer_ = init.Get("transfer").As<Napi::String>().Utf8Value();
    }
  }

  // matrix (optional, nullable)
  if (init.Has("matrix") && !init.Get("matrix").IsNull() && !init.Get("matrix").IsUndefined()) {
    if (init.Get("matrix").IsString()) {
      matrix_ = init.Get("matrix").As<Napi::String>().Utf8Value();
    }
  }

  // fullRange (optional, nullable)
  if (init.Has("fullRange") && !init.Get("fullRange").IsNull() && !init.Get("fullRange").IsUndefined()) {
    if (init.Get("fullRange").IsBoolean()) {
      fullRange_ = init.Get("fullRange").As<Napi::Boolean>().Value();
    }
  }
}

// --- Factory Methods ---

Napi::Object VideoColorSpace::CreateFromInit(Napi::Env env, Napi::Object init) {
  return constructor.New({init});
}

Napi::Object VideoColorSpace::Create(
    Napi::Env env,
    const std::optional<std::string>& primaries,
    const std::optional<std::string>& transfer,
    const std::optional<std::string>& matrix,
    const std::optional<bool>& fullRange) {

  // Create init object from parameters
  Napi::Object init = Napi::Object::New(env);

  if (primaries.has_value()) {
    init.Set("primaries", Napi::String::New(env, primaries.value()));
  }
  if (transfer.has_value()) {
    init.Set("transfer", Napi::String::New(env, transfer.value()));
  }
  if (matrix.has_value()) {
    init.Set("matrix", Napi::String::New(env, matrix.value()));
  }
  if (fullRange.has_value()) {
    init.Set("fullRange", Napi::Boolean::New(env, fullRange.value()));
  }

  return constructor.New({init});
}

// --- Attributes ---

Napi::Value VideoColorSpace::GetPrimaries(const Napi::CallbackInfo& info) {
  // [SPEC 9.9.3] Return the value of [[primaries]]
  if (!primaries_.has_value()) {
    return info.Env().Null();
  }
  return Napi::String::New(info.Env(), primaries_.value());
}

Napi::Value VideoColorSpace::GetTransfer(const Napi::CallbackInfo& info) {
  // [SPEC 9.9.3] Return the value of [[transfer]]
  if (!transfer_.has_value()) {
    return info.Env().Null();
  }
  return Napi::String::New(info.Env(), transfer_.value());
}

Napi::Value VideoColorSpace::GetMatrix(const Napi::CallbackInfo& info) {
  // [SPEC 9.9.3] Return the value of [[matrix]]
  if (!matrix_.has_value()) {
    return info.Env().Null();
  }
  return Napi::String::New(info.Env(), matrix_.value());
}

Napi::Value VideoColorSpace::GetFullRange(const Napi::CallbackInfo& info) {
  // [SPEC 9.9.3] Return the value of [[full range]]
  if (!fullRange_.has_value()) {
    return info.Env().Null();
  }
  return Napi::Boolean::New(info.Env(), fullRange_.value());
}

// --- Methods ---

Napi::Value VideoColorSpace::ToJSON(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] toJSON() returns a VideoColorSpaceInit dictionary
  // This is the [Default] toJSON behavior per WebIDL
  Napi::Object result = Napi::Object::New(env);

  // Include all properties, even if null
  if (primaries_.has_value()) {
    result.Set("primaries", Napi::String::New(env, primaries_.value()));
  } else {
    result.Set("primaries", env.Null());
  }

  if (transfer_.has_value()) {
    result.Set("transfer", Napi::String::New(env, transfer_.value()));
  } else {
    result.Set("transfer", env.Null());
  }

  if (matrix_.has_value()) {
    result.Set("matrix", Napi::String::New(env, matrix_.value()));
  } else {
    result.Set("matrix", env.Null());
  }

  if (fullRange_.has_value()) {
    result.Set("fullRange", Napi::Boolean::New(env, fullRange_.value()));
  } else {
    result.Set("fullRange", env.Null());
  }

  return result;
}

}  // namespace webcodecs
