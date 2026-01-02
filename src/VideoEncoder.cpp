#include "VideoEncoder.h"

namespace webcodecs {

Napi::FunctionReference VideoEncoder::constructor;

Napi::Object VideoEncoder::Init(Napi::Env env, Napi::Object exports) {
  Napi::Function func = DefineClass(env, "VideoEncoder", {
    InstanceAccessor<&VideoEncoder::GetState>("state"),
    InstanceAccessor<&VideoEncoder::GetEncodeQueueSize>("encodeQueueSize"),
    InstanceAccessor<&VideoEncoder::GetOndequeue, &VideoEncoder::SetOndequeue>("ondequeue"),
    InstanceMethod<&VideoEncoder::Configure>("configure"),
    InstanceMethod<&VideoEncoder::Encode>("encode"),
    InstanceMethod<&VideoEncoder::Flush>("flush"),
    InstanceMethod<&VideoEncoder::Reset>("reset"),
    InstanceMethod<&VideoEncoder::Close>("close"),
    StaticMethod<&VideoEncoder::IsConfigSupported>("isConfigSupported"),
  });

  constructor = Napi::Persistent(func);
  constructor.SuppressDestruct();
  exports.Set("VideoEncoder", func);
  return exports;
}

VideoEncoder::VideoEncoder(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<VideoEncoder>(info) {
  Napi::Env env = info.Env();

  // [SPEC] Constructor Algorithm
  /*
   * Initialize internal slots.
   */

  // TODO(impl): Implement Constructor & Resource Allocation
}

VideoEncoder::~VideoEncoder() {
  Release();
}

void VideoEncoder::Release() {
  // RAII cleanup - codecCtx_ is automatically freed
  // Clear the encode queue
  {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!encodeQueue_.empty()) {
      encodeQueue_.pop();
    }
  }
  encodeQueueSize_.store(0, std::memory_order_release);

  // Transition to closed state
  state_.close();

  // codecCtx_ is freed automatically by RAII destructor
  codecCtx_.reset();
}

// --- Attributes ---

Napi::Value VideoEncoder::GetState(const Napi::CallbackInfo& info) {
  return Napi::String::New(info.Env(), state_.to_string());
}

Napi::Value VideoEncoder::GetEncodeQueueSize(const Napi::CallbackInfo& info) {
  return Napi::Number::New(info.Env(),
      encodeQueueSize_.load(std::memory_order_acquire));
}

Napi::Value VideoEncoder::GetOndequeue(const Napi::CallbackInfo& info) {
  if (ondequeueCallback_.IsEmpty()) {
    return info.Env().Null();
  }
  return ondequeueCallback_.Value();
}

void VideoEncoder::SetOndequeue(const Napi::CallbackInfo& info, const Napi::Value& value) {
  Napi::Env env = info.Env();
  if (value.IsNull() || value.IsUndefined()) {
    ondequeueCallback_.Reset();
  } else if (value.IsFunction()) {
    ondequeueCallback_.Reset(value.As<Napi::Function>(), 1);
  } else {
    Napi::TypeError::New(env, "ondequeue must be a function or null")
        .ThrowAsJavaScriptException();
  }
}


// --- Methods ---

Napi::Value VideoEncoder::Configure(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Algorithm
  /*
   * \[=Enqueues a control message=\] to configure the video encoder for encoding video frames as described by |config|. NOTE: This method will trigger a {{NotSupportedError}} if the User Agent does not support |config|. Authors are encouraged to first check support by calling {{VideoEncoder/isConfigSupported()}} with |config|. User Agents don't have to support any particular codec type or configuration. When invoked, run these steps: 1. If |config| is not a \[=valid VideoEncoderConfig=\], throw a {{TypeError}}. 2. If {{VideoEncoder/\[\[state\]\]}} is \`"closed"\`, throw an {{InvalidStateError}}. 3. Set {{VideoEncoder/\[\[state\]\]}} to \`"configured"\`. 4. Set {{VideoEncoder/\[\[active orientation\]\]}} to \`null\`. 5. \[=Queue a control message=\] to configure the encoder using |config|. 6. \[=Process the control message queue=\]. \[=Running a control message=\] to configure the encoder means performing these steps: 1. Assign \`true\` to {{VideoEncoder/\[\[message queue blocked\]\]}}. 2. Enqueue the following steps to {{VideoEncoder/\[\[codec work queue\]\]}}: 1. Let |supported| be the result of running the Check Configuration Support algorithm with |config|. 2. If |supported| is \`false\`, \[=queue a task=\] to run the Close VideoEncoder algorithm with {{NotSupportedError}} and abort these steps. 3. If needed, assign {{VideoEncoder/\[\[codec implementation\]\]}} with an implementation supporting |config|. 4. Configure {{VideoEncoder/\[\[codec implementation\]\]}} with |config|. 5. \[=queue a task=\] to run the following steps: 1. Assign \`false\` to {{VideoEncoder/\[\[message queue blocked\]\]}}. 2. \[=Queue a task=\] to \[=Process the control message queue=\]. 3. Return \`"processed"\`.
   */

  // TODO(impl): Implement method logic
  return env.Undefined();
}

Napi::Value VideoEncoder::Encode(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Algorithm
  /*
   * \[=Enqueues a control message=\] to encode the given |frame|. When invoked, run these steps: 1. If the value of |frame|'s {{platform object/\[\[Detached\]\]}} internal slot is \`true\`, throw a {{TypeError}}. 2. If {{VideoEncoder/\[\[state\]\]}} is not \`"configured"\`, throw an {{InvalidStateError}}. 3. If {{VideoEncoder/\[\[active orientation\]\]}} is not \`null\` and does not match |frame|'s {{VideoFrame/\[\[rotation\]\]}} and {{VideoFrame/\[\[flip\]\]}} throw a {{DataError}}. 4. If {{VideoEncoder/\[\[active orientation\]\]}} is \`null\`, set it to |frame|'s {{VideoFrame/\[\[rotation\]\]}} and {{VideoFrame/\[\[flip\]\]}}. 5. Let |frameClone| hold the result of running the \[=Clone VideoFrame=\] algorithm with |frame|. 6. Increment {{VideoEncoder/\[\[encodeQueueSize\]\]}}. 7. \[=Queue a control message=\] to encode |frameClone|. 8. \[=Process the control message queue=\]. \[=Running a control message=\] to encode the frame means performing these steps: 1. If {{VideoEncoder/\[\[codec saturated\]\]}} equals \`true\`, return \`"not processed"\`. 2. If encoding |frame| will cause the {{VideoEncoder/\[\[codec implementation\]\]}} to become \[=saturated=\], assign \`true\` to {{VideoEncoder/\[\[codec saturated\]\]}}. 3. Decrement {{VideoEncoder/\[\[encodeQueueSize\]\]}} and run the \[=VideoEncoder/Schedule Dequeue Event=\] algorithm. 4. Enqueue the following steps to the {{VideoEncoder/\[\[codec work queue\]\]}}: 1. Attempt to use {{VideoEncoder/\[\[codec implementation\]\]}} to encode the |frameClone| according to |options|. 2. If encoding results in an error, \[=queue a task=\] to run the \[=Close VideoEncoder=\] algorithm with {{EncodingError}} and return. 3. If {{VideoEncoder/\[\[codec saturated\]\]}} equals \`true\` and {{VideoEncoder/\[\[codec implementation\]\]}} is no longer \[=saturated=\], \[=queue a task=\] to perform the following steps: 1. Assign \`false\` to {{VideoEncoder/\[\[codec saturated\]\]}}. 2. \[=Process the control message queue=\]. 4. Let |encoded outputs| be a \[=list=\] of encoded video data outputs emitted by {{VideoEncoder/\[\[codec implementation\]\]}}. 5. If |encoded outputs| is not empty, \[=queue a task=\] to run the \[=Output EncodedVideoChunks=\] algorithm with |encoded outputs|. 5. Return \`"processed"\`.
   */

  // TODO(impl): Implement method logic
  return env.Undefined();
}

Napi::Value VideoEncoder::Flush(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Algorithm
  /*
   * Completes all \[=control messages=\] in the \[=control message queue=\] and emits all outputs. When invoked, run these steps: 1. If {{VideoEncoder/\[\[state\]\]}} is not \`"configured"\`, return \[=a promise rejected with=\] {{InvalidStateError}} {{DOMException}}. 2. Let |promise| be a new Promise. 3. Append |promise| to {{VideoEncoder/\[\[pending flush promises\]\]}}. 4. \[=Queue a control message=\] to flush the codec with |promise|. 5. \[=Process the control message queue=\]. 6. Return |promise|. \[=Running a control message=\] to flush the codec means performing these steps with |promise|: 1. Enqueue the following steps to the {{VideoEncoder/\[\[codec work queue\]\]}}: 1. Signal {{VideoEncoder/\[\[codec implementation\]\]}} to emit all \[=internal pending outputs=\]. 2. Let |encoded outputs| be a \[=list=\] of encoded video data outputs emitted by {{VideoEncoder/\[\[codec implementation\]\]}}. 3. \[=Queue a task=\] to perform these steps: 1. If |encoded outputs| is not empty, run the \[=Output EncodedVideoChunks=\] algorithm with |encoded outputs|. 2. Remove |promise| from {{VideoEncoder/\[\[pending flush promises\]\]}}. 3. Resolve |promise|. 2. Return \`"processed"\`.
   */

  // TODO(impl): Implement method logic
  return env.Undefined();
}

Napi::Value VideoEncoder::Reset(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Algorithm
  /*
   * Immediately resets all state including configuration, \[=control messages=\] in the \[=control message queue=\], and all pending callbacks. When invoked, run the \[=Reset VideoEncoder=\] algorithm with an {{AbortError}} {{DOMException}}.
   */

  // TODO(impl): Implement method logic
  return env.Undefined();
}

Napi::Value VideoEncoder::Close(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Algorithm
  /*
   * Immediately aborts all pending work and releases \[=system resources=\]. Close is final. When invoked, run the \[=Close VideoEncoder=\] algorithm with an {{AbortError}} {{DOMException}}.
   */

  // TODO(impl): Implement method logic
  return env.Undefined();
}

Napi::Value VideoEncoder::IsConfigSupported(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Algorithm
  /*
   * Returns a promise indicating whether the provided |config| is supported by the User Agent. NOTE: The returned {{VideoEncoderSupport}} {{VideoEncoderSupport/config}} will contain only the dictionary members that User Agent recognized. Unrecognized dictionary members will be ignored. Authors can detect unrecognized dictionary members by comparing {{VideoEncoderSupport/config}} to their provided |config|. When invoked, run these steps: 1. If |config| is not a valid VideoEncoderConfig, return \[=a promise rejected with=\] {{TypeError}}. 2. Let |p| be a new Promise. 3. Let |checkSupportQueue| be the result of starting a new parallel queue. 4. Enqueue the following steps to |checkSupportQueue|: 1. Let |supported| be the result of running the Check Configuration Support algorithm with |config|. 2. \[=Queue a task=\] to run the following steps: 1. Let |encoderSupport| be a newly constructed {{VideoEncoderSupport}}, initialized as follows: 1. Set {{VideoEncoderSupport/config}} to the result of running the Clone Configuration algorithm with |config|. 2. Set {{VideoEncoderSupport/supported}} to |supported|. 2. Resolve |p| with |encoderSupport|. 5. Return |p|.
   */

  // TODO(impl): Implement method logic
  return env.Undefined();
}

}  // namespace webcodecs
