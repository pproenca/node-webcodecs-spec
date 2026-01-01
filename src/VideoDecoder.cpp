#include "VideoDecoder.h"

namespace webcodecs {

Napi::FunctionReference VideoDecoder::constructor;

Napi::Object VideoDecoder::Init(Napi::Env env, Napi::Object exports) {
  Napi::Function func = DefineClass(env, "VideoDecoder", {
    InstanceAccessor<&VideoDecoder::GetState>("state"),
    InstanceAccessor<&VideoDecoder::GetDecodeQueueSize>("decodeQueueSize"),
    InstanceAccessor<&VideoDecoder::GetOndequeue, &VideoDecoder::SetOndequeue>("ondequeue"),
    InstanceMethod<&VideoDecoder::Configure>("configure"),
    InstanceMethod<&VideoDecoder::Decode>("decode"),
    InstanceMethod<&VideoDecoder::Flush>("flush"),
    InstanceMethod<&VideoDecoder::Reset>("reset"),
    InstanceMethod<&VideoDecoder::Close>("close"),
    StaticMethod<&VideoDecoder::IsConfigSupported>("isConfigSupported"),
  });

  constructor = Napi::Persistent(func);
  constructor.SuppressDestruct();
  exports.Set("VideoDecoder", func);
  return exports;
}

VideoDecoder::VideoDecoder(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<VideoDecoder>(info) {
  Napi::Env env = info.Env();

  // [SPEC] Constructor Algorithm
  /*
   * Initialize internal slots.
   */

  // TODO(impl): Implement Constructor & Resource Allocation
}

VideoDecoder::~VideoDecoder() {
  Release();
}

void VideoDecoder::Release() {
  // Thread-safe close: transition to Closed state
  state_.close();

  // Lock to safely clear resources
  std::lock_guard<std::mutex> lock(mutex_);

  // Clear decode queue (RAII handles packet cleanup)
  while (!decodeQueue_.empty()) {
    decodeQueue_.pop();
  }
  decodeQueueSize_.store(0, std::memory_order_release);

  // Release codec context (RAII handles avcodec_free_context)
  codecCtx_.reset();

  // Release JS callbacks
  if (!outputCallback_.IsEmpty()) {
    outputCallback_.Reset();
  }
  if (!errorCallback_.IsEmpty()) {
    errorCallback_.Reset();
  }
  if (!ondequeueCallback_.IsEmpty()) {
    ondequeueCallback_.Reset();
  }
}

// --- Attributes ---

Napi::Value VideoDecoder::GetState(const Napi::CallbackInfo& info) {
  return Napi::String::New(info.Env(), state_.to_string());
}

Napi::Value VideoDecoder::GetDecodeQueueSize(const Napi::CallbackInfo& info) {
  return Napi::Number::New(info.Env(),
      static_cast<double>(decodeQueueSize_.load(std::memory_order_acquire)));
}

Napi::Value VideoDecoder::GetOndequeue(const Napi::CallbackInfo& info) {
  // TODO(impl): Return ondequeue
  return info.Env().Null();
}

void VideoDecoder::SetOndequeue(const Napi::CallbackInfo& info, const Napi::Value& value) {
  // TODO(impl): Set ondequeue
}


// --- Methods ---

Napi::Value VideoDecoder::Configure(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Algorithm
  /*
   * \[=Enqueues a control message=\] to configure the video decoder for decoding chunks as described by |config|. NOTE: This method will trigger a {{NotSupportedError}} if the User Agent does not support |config|. Authors are encouraged to first check support by calling {{VideoDecoder/isConfigSupported()}} with |config|. User Agents don't have to support any particular codec type or configuration. When invoked, run these steps: 1. If |config| is not a \[=valid VideoDecoderConfig=\], throw a {{TypeError}}. 2. If {{VideoDecoder/\[\[state\]\]}} is \`“closed”\`, throw an {{InvalidStateError}}. 3. Set {{VideoDecoder/\[\[state\]\]}} to \`"configured"\`. 4. Set {{VideoDecoder/\[\[key chunk required\]\]}} to \`true\`. 5. \[=Queue a control message=\] to configure the decoder with |config|. 6. \[=Process the control message queue=\]. \[=Running a control message=\] to configure the decoder means running these steps: 1. Assign \`true\` to {{VideoDecoder/\[\[message queue blocked\]\]}}. 2. Enqueue the following steps to {{VideoDecoder/\[\[codec work queue\]\]}}: 1. Let |supported| be the result of running the Check Configuration Support algorithm with |config|. 2. If |supported| is \`false\`, \[=queue a task=\] to run the Close VideoDecoder algorithm with {{NotSupportedError}} and abort these steps. 3. If needed, assign {{VideoDecoder/\[\[codec implementation\]\]}} with an implementation supporting |config|. 4. Configure {{VideoDecoder/\[\[codec implementation\]\]}} with |config|. 5. \[=queue a task=\] to run the following steps: 1. Assign \`false\` to {{VideoDecoder/\[\[message queue blocked\]\]}}. 2. \[=Queue a task=\] to \[=Process the control message queue=\]. 3. Return \`"processed"\`.
   */

  // TODO(impl): Implement method logic
  return env.Undefined();
}

Napi::Value VideoDecoder::Decode(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Algorithm
  /*
   * \[=Enqueues a control message=\] to decode the given |chunk|. NOTE: Authors are encouraged to call {{VideoFrame/close()}} on output {{VideoFrame}}s immediately when frames are no longer needed. The underlying \[=media resource=\]s are owned by the {{VideoDecoder}} and failing to release them (or waiting for garbage collection) can cause decoding to stall. NOTE: {{VideoDecoder}} requires that frames are output in the order they expect to be presented, commonly known as presentation order. When using some {{VideoDecoder/\[\[codec implementation\]\]}}s the User Agent will have to reorder outputs into presentation order. When invoked, run these steps: 1. If {{VideoDecoder/\[\[state\]\]}} is not \`"configured"\`, throw an {{InvalidStateError}}. 2. If {{VideoDecoder/\[\[key chunk required\]\]}} is \`true\`: 1. If |chunk|.{{EncodedVideoChunk/type}} is not {{EncodedVideoChunkType/key}}, throw a {{DataError}}. 2. Implementers _SHOULD_ inspect the |chunk|'s {{EncodedVideoChunk/\[\[internal data\]\]}} to verify that it is truly a \[=key chunk=\]. If a mismatch is detected, throw a {{DataError}}. 3. Otherwise, assign \`false\` to {{VideoDecoder/\[\[key chunk required\]\]}}. 3. Increment {{VideoDecoder/\[\[decodeQueueSize\]\]}}. 4. \[=Queue a control message=\] to decode the |chunk|. 5. \[=Process the control message queue=\]. \[=Running a control message=\] to decode the chunk means performing these steps: 1. If {{VideoDecoder/\[\[codec saturated\]\]}} equals \`true\`, return \`"not processed"\`. 2. If decoding chunk will cause the {{VideoDecoder/\[\[codec implementation\]\]}} to become \[=saturated=\], assign \`true\` to {{VideoDecoder/\[\[codec saturated\]\]}}. 3. Decrement {{VideoDecoder/\[\[decodeQueueSize\]\]}} and run the \[=VideoDecoder/Schedule Dequeue Event=\] algorithm. 4. Enqueue the following steps to the {{VideoDecoder/\[\[codec work queue\]\]}}: 1. Attempt to use {{VideoDecoder/\[\[codec implementation\]\]}} to decode the chunk. 2. If decoding results in an error, \[=queue a task=\] to run the \[=Close VideoDecoder=\] algorithm with {{EncodingError}} and return. 3. If {{VideoDecoder/\[\[codec saturated\]\]}} equals \`true\` and {{VideoDecoder/\[\[codec implementation\]\]}} is no longer \[=saturated=\], \[=queue a task=\] to perform the following steps: 1. Assign \`false\` to {{VideoDecoder/\[\[codec saturated\]\]}}. 2. \[=Process the control message queue=\]. 4. Let |decoded outputs| be a \[=list=\] of decoded video data outputs emitted by {{VideoDecoder/\[\[codec implementation\]\]}} in presentation order. 5. If |decoded outputs| is not empty, \[=queue a task=\] to run the \[=Output VideoFrame=\] algorithm with |decoded outputs|. 5. Return \`"processed"\`.
   */

  // TODO(impl): Implement method logic
  return env.Undefined();
}

Napi::Value VideoDecoder::Flush(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Algorithm
  /*
   * Completes all \[=control messages=\] in the \[=control message queue=\] and emits all outputs. When invoked, run these steps: 1. If {{VideoDecoder/\[\[state\]\]}} is not \`"configured"\`, return \[=a promise rejected with=\] {{InvalidStateError}} {{DOMException}}. 2. Set {{VideoDecoder/\[\[key chunk required\]\]}} to \`true\`. 3. Let |promise| be a new Promise. 4. Append |promise| to {{VideoDecoder/\[\[pending flush promises\]\]}}. 5. \[=Queue a control message=\] to flush the codec with |promise|. 6. \[=Process the control message queue=\]. 7. Return |promise|. \[=Running a control message=\] to flush the codec means performing these steps with |promise|. 1. Enqueue the following steps to the {{VideoDecoder/\[\[codec work queue\]\]}}: 1. Signal {{VideoDecoder/\[\[codec implementation\]\]}} to emit all \[=internal pending outputs=\]. 2. Let |decoded outputs| be a \[=list=\] of decoded video data outputs emitted by {{VideoDecoder/\[\[codec implementation\]\]}}. 3. \[=Queue a task=\] to perform these steps: 1. If |decoded outputs| is not empty, run the \[=Output VideoFrame=\] algorithm with |decoded outputs|. 2. Remove |promise| from {{VideoDecoder/\[\[pending flush promises\]\]}}. 3. Resolve |promise|. 2. Return \`"processed"\`.
   */

  // TODO(impl): Implement method logic
  return env.Undefined();
}

Napi::Value VideoDecoder::Reset(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Algorithm
  /*
   * Immediately resets all state including configuration, \[=control messages=\] in the \[=control message queue=\], and all pending callbacks. When invoked, run the \[=Reset VideoDecoder=\] algorithm with an {{AbortError}} {{DOMException}}.
   */

  // TODO(impl): Implement method logic
  return env.Undefined();
}

Napi::Value VideoDecoder::Close(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Immediately aborts all pending work and releases system resources.
  // Close is final - after close(), the decoder cannot be used again.

  // Release all resources and transition to closed state
  Release();

  return env.Undefined();
}

Napi::Value VideoDecoder::IsConfigSupported(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Algorithm
  /*
   * Returns a promise indicating whether the provided |config| is supported by the User Agent. NOTE: The returned {{VideoDecoderSupport}} {{VideoDecoderSupport/config}} will contain only the dictionary members that User Agent recognized. Unrecognized dictionary members will be ignored. Authors can detect unrecognized dictionary members by comparing {{VideoDecoderSupport/config}} to their provided |config|. When invoked, run these steps: 1. If |config| is not a valid VideoDecoderConfig, return \[=a promise rejected with=\] {{TypeError}}. 2. Let |p| be a new Promise. 3. Let |checkSupportQueue| be the result of starting a new parallel queue. 4. Enqueue the following steps to |checkSupportQueue|: 1. Let |supported| be the result of running the Check Configuration Support algorithm with |config|. 2. \[=Queue a task=\] to run the following steps: 1. Let |decoderSupport| be a newly constructed {{VideoDecoderSupport}}, initialized as follows: 1. Set {{VideoDecoderSupport/config}} to the result of running the Clone Configuration algorithm with |config|. 2. Set {{VideoDecoderSupport/supported}} to |supported|. 2. Resolve |p| with |decoderSupport|. 5. Return |p|.
   */

  // TODO(impl): Implement method logic
  return env.Undefined();
}

}  // namespace webcodecs
