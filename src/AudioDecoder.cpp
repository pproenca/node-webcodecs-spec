#include "AudioDecoder.h"

namespace webcodecs {

Napi::FunctionReference AudioDecoder::constructor;

Napi::Object AudioDecoder::Init(Napi::Env env, Napi::Object exports) {
  Napi::Function func =
      DefineClass(env, "AudioDecoder",
                  {
                      InstanceAccessor<&AudioDecoder::GetState>("state"),
                      InstanceAccessor<&AudioDecoder::GetDecodeQueueSize>("decodeQueueSize"),
                      InstanceAccessor<&AudioDecoder::GetOndequeue, &AudioDecoder::SetOndequeue>("ondequeue"),
                      InstanceMethod<&AudioDecoder::Configure>("configure"),
                      InstanceMethod<&AudioDecoder::Decode>("decode"),
                      InstanceMethod<&AudioDecoder::Flush>("flush"),
                      InstanceMethod<&AudioDecoder::Reset>("reset"),
                      InstanceMethod<&AudioDecoder::Close>("close"),
                      StaticMethod<&AudioDecoder::IsConfigSupported>("isConfigSupported"),
                  });

  constructor = Napi::Persistent(func);
  constructor.SuppressDestruct();
  exports.Set("AudioDecoder", func);
  return exports;
}

AudioDecoder::AudioDecoder(const Napi::CallbackInfo& info) : Napi::ObjectWrap<AudioDecoder>(info) {
  Napi::Env env = info.Env();

  // [SPEC] Constructor Algorithm
  /*
   * Initialize internal slots.
   */

  // TODO(impl): Implement Constructor & Resource Allocation
}

AudioDecoder::~AudioDecoder() { Release(); }

void AudioDecoder::Release() {
  // Thread-safe close: transition to Closed state
  state_.Close();

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

Napi::Value AudioDecoder::GetState(const Napi::CallbackInfo& info) {
  // TODO(impl): Return state
  return info.Env().Null();
}

Napi::Value AudioDecoder::GetDecodeQueueSize(const Napi::CallbackInfo& info) {
  // TODO(impl): Return decodeQueueSize
  return info.Env().Null();
}

Napi::Value AudioDecoder::GetOndequeue(const Napi::CallbackInfo& info) {
  // TODO(impl): Return ondequeue
  return info.Env().Null();
}

void AudioDecoder::SetOndequeue(const Napi::CallbackInfo& info, const Napi::Value& value) {
  // TODO(impl): Set ondequeue
}

// --- Methods ---

Napi::Value AudioDecoder::Configure(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Algorithm
  /*
   * \[=Enqueues a control message=\] to configure the audio decoder for decoding chunks as described by |config|. NOTE:
   * This method will trigger a {{NotSupportedError}} if the User Agent does not support |config|. Authors are
   * encouraged to first check support by calling {{AudioDecoder/isConfigSupported()}} with |config|. User Agents don't
   * have to support any particular codec type or configuration. When invoked, run these steps: 1. If |config| is not a
   * \[=valid AudioDecoderConfig=\], throw a {{TypeError}}. 2. If {{AudioDecoder/\[\[state\]\]}} is \`“closed”\`, throw
   * an {{InvalidStateError}}. 3. Set {{AudioDecoder/\[\[state\]\]}} to \`"configured"\`. 4. Set {{AudioDecoder/\[\[key
   * chunk required\]\]}} to \`true\`. 5. \[=Queue a control message=\] to configure the decoder with |config|. 6.
   * \[=Process the control message queue=\]. \[=Running a control message=\] to configure the decoder means running
   * these steps: 1. Assign \`true\` to {{AudioDecoder/\[\[message queue blocked\]\]}}. 2. Enqueue the following steps
   * to {{AudioDecoder/\[\[codec work queue\]\]}}: 1. Let |supported| be the result of running the Check Configuration
   * Support algorithm with |config|. 2. If |supported| is \`false\`, \[=queue a task=\] to run the Close AudioDecoder
   * algorithm with {{NotSupportedError}} and abort these steps. 3. If needed, assign {{AudioDecoder/\[\[codec
   * implementation\]\]}} with an implementation supporting |config|. 4. Configure {{AudioDecoder/\[\[codec
   * implementation\]\]}} with |config|. 5. \[=queue a task=\] to run the following steps: 1. Assign \`false\` to
   * {{AudioDecoder/\[\[message queue blocked\]\]}}. 2. \[=Queue a task=\] to \[=Process the control message
   * queue=\]. 3. Return \`"processed"\`.
   */

  // TODO(impl): Implement method logic
  return env.Undefined();
}

Napi::Value AudioDecoder::Decode(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Algorithm
  /*
   * \[=Enqueues a control message=\] to decode the given |chunk|. When invoked, run these steps: 1. If
   * {{AudioDecoder/\[\[state\]\]}} is not \`"configured"\`, throw an {{InvalidStateError}}. 2. If
   * {{AudioDecoder/\[\[key chunk required\]\]}} is \`true\`: 1. If |chunk|.{{EncodedAudioChunk/\[\[type\]\]}} is not
   * {{EncodedAudioChunkType/key}}, throw a {{DataError}}. 2. Implementers _SHOULD_ inspect the |chunk|'s
   * {{EncodedAudioChunk/\[\[internal data\]\]}} to verify that it is truly a \[=key chunk=\]. If a mismatch is
   * detected, throw a {{DataError}}. 3. Otherwise, assign \`false\` to {{AudioDecoder/\[\[key chunk required\]\]}}. 3.
   * Increment {{AudioDecoder/\[\[decodeQueueSize\]\]}}. 4. \[=Queue a control message=\] to decode the |chunk|. 5.
   * \[=Process the control message queue=\]. \[=Running a control message=\] to decode the chunk means performing these
   * steps: 1. If {{AudioDecoder/\[\[codec saturated\]\]}} equals \`true\`, return \`"not processed"\`. 2. If decoding
   * chunk will cause the {{AudioDecoder/\[\[codec implementation\]\]}} to become \[=saturated=\], assign \`true\` to
   * {{AudioDecoder/\[\[codec saturated\]\]}}. 3. Decrement {{AudioDecoder/\[\[decodeQueueSize\]\]}} and run the
   * \[=AudioDecoder/Schedule Dequeue Event=\] algorithm. 4. Enqueue the following steps to the {{AudioDecoder/\[\[codec
   * work queue\]\]}}: 1. Attempt to use {{AudioDecoder/\[\[codec implementation\]\]}} to decode the chunk. 2. If
   * decoding results in an error, \[=queue a task=\] to run the \[=Close AudioDecoder=\] algorithm with
   * {{EncodingError}} and return. 3. If {{AudioDecoder/\[\[codec saturated\]\]}} equals \`true\` and
   * {{AudioDecoder/\[\[codec implementation\]\]}} is no longer \[=saturated=\], \[=queue a task=\] to perform the
   * following steps: 1. Assign \`false\` to {{AudioDecoder/\[\[codec saturated\]\]}}. 2. \[=Process the control message
   * queue=\]. 4. Let |decoded outputs| be a \[=list=\] of decoded audio data outputs emitted by
   * {{AudioDecoder/\[\[codec implementation\]\]}}. 5. If |decoded outputs| is not empty, \[=queue a task=\] to run the
   * \[=Output AudioData=\] algorithm with |decoded outputs|. 5. Return \`"processed"\`.
   */

  // TODO(impl): Implement method logic
  return env.Undefined();
}

Napi::Value AudioDecoder::Flush(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Algorithm
  /*
   * Completes all \[=control messages=\] in the \[=control message queue=\] and emits all outputs. When invoked, run
   * these steps: 1. If {{AudioDecoder/\[\[state\]\]}} is not \`"configured"\`, return \[=a promise rejected with=\]
   * {{InvalidStateError}} {{DOMException}}. 2. Set {{AudioDecoder/\[\[key chunk required\]\]}} to \`true\`. 3. Let
   * |promise| be a new Promise. 4. Append |promise| to {{AudioDecoder/\[\[pending flush promises\]\]}}. 5. \[=Queue a
   * control message=\] to flush the codec with |promise|. 6. \[=Process the control message queue=\]. 7. Return
   * |promise|. \[=Running a control message=\] to flush the codec means performing these steps with |promise|. 1.
   * Enqueue the following steps to the {{AudioDecoder/\[\[codec work queue\]\]}}: 1. Signal {{AudioDecoder/\[\[codec
   * implementation\]\]}} to emit all \[=internal pending outputs=\]. 2. Let |decoded outputs| be a \[=list=\] of
   * decoded audio data outputs emitted by {{AudioDecoder/\[\[codec implementation\]\]}}. 3. \[=Queue a task=\] to
   * perform these steps: 1. If |decoded outputs| is not empty, run the \[=Output AudioData=\] algorithm with |decoded
   * outputs|. 2. Remove |promise| from {{AudioDecoder/\[\[pending flush promises\]\]}}. 3. Resolve |promise|. 2. Return
   * \`"processed"\`.
   */

  // TODO(impl): Implement method logic
  return env.Undefined();
}

Napi::Value AudioDecoder::Reset(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Algorithm
  /*
   * Immediately resets all state including configuration, \[=control messages=\] in the \[=control message queue=\],
   * and all pending callbacks. When invoked, run the \[=Reset AudioDecoder=\] algorithm with an {{AbortError}}
   * {{DOMException}}.
   */

  // TODO(impl): Implement method logic
  return env.Undefined();
}

Napi::Value AudioDecoder::Close(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Algorithm
  /*
   * Immediately aborts all pending work and releases \[=system resources=\]. Close is final. When invoked, run the
   * \[=Close AudioDecoder=\] algorithm with an {{AbortError}} {{DOMException}}.
   */

  // TODO(impl): Implement method logic
  return env.Undefined();
}

Napi::Value AudioDecoder::IsConfigSupported(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Algorithm
  /*
   * Returns a promise indicating whether the provided |config| is supported by the User Agent. NOTE: The returned
   * {{AudioDecoderSupport}} {{AudioDecoderSupport/config}} will contain only the dictionary members that User Agent
   * recognized. Unrecognized dictionary members will be ignored. Authors can detect unrecognized dictionary members by
   * comparing {{AudioDecoderSupport/config}} to their provided |config|. When invoked, run these steps: 1. If |config|
   * is not a valid AudioDecoderConfig, return \[=a promise rejected with=\] {{TypeError}}. 2. Let |p| be a new
   * Promise. 3. Let |checkSupportQueue| be the result of starting a new parallel queue. 4. Enqueue the following steps
   * to |checkSupportQueue|: 1. Let |supported| be the result of running the Check Configuration Support algorithm with
   * |config|. 2. \[=Queue a task=\] to run the following steps: 1. Let |decoderSupport| be a newly constructed
   * {{AudioDecoderSupport}}, initialized as follows: 1. Set {{AudioDecoderSupport/config}} to the result of running the
   * Clone Configuration algorithm with |config|. 2. Set {{AudioDecoderSupport/supported}} to |supported|. 2. Resolve
   * |p| with |decoderSupport|. 5. Return |p|.
   */

  // TODO(impl): Implement method logic
  return env.Undefined();
}

}  // namespace webcodecs
