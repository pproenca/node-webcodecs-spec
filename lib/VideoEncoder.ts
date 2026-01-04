/**
 * VideoEncoder - TypeScript wrapper for native VideoEncoder
 * @see spec/context/VideoEncoder.md
 */

import { createRequire } from 'node:module';
import type {
  CodecState,
  EventHandler,
  VideoEncoderConfig,
  VideoEncoderEncodeOptions,
  VideoEncoderInit,
  VideoEncoderSupport,
  VideoFrame as VideoFrameType,
} from '../types/webcodecs.js';
import { VideoFrame } from './VideoFrame.js';

// Native binding loader - require() necessary for native addons in ESM
// See: https://nodejs.org/api/esm.html#interoperability-with-commonjs
const require = createRequire(import.meta.url);
const bindings = require('bindings')('webcodecs');

/** Native binding interface for VideoEncoder - matches C++ NAPI class shape */
interface NativeVideoEncoder {
  readonly state: CodecState;
  readonly encodeQueueSize: number;
  ondequeue: EventHandler;
  configure(config: VideoEncoderConfig): void;
  encode(frame: VideoFrame, options: VideoEncoderEncodeOptions): void;
  flush(): Promise<void>;
  reset(): void;
  close(): void;
}

/** Native constructor interface for VideoEncoder */
interface NativeVideoEncoderConstructor {
  new (init: VideoEncoderInit): NativeVideoEncoder;
  isConfigSupported(config: VideoEncoderConfig): Promise<VideoEncoderSupport>;
}

export class VideoEncoder {
  private readonly native: NativeVideoEncoder;

  constructor(init: VideoEncoderInit) {
    const NativeClass = bindings.VideoEncoder as NativeVideoEncoderConstructor;
    this.native = new NativeClass(init);
  }

  get state(): CodecState {
    return this.native.state;
  }
  get encodeQueueSize(): number {
    return this.native.encodeQueueSize;
  }
  get ondequeue(): EventHandler {
    return this.native.ondequeue;
  }

  set ondequeue(value: EventHandler) {
    this.native.ondequeue = value;
  }

  configure(config: VideoEncoderConfig): void {
    this.native.configure(config);
  }
  /**
   * Enqueues a control message to encode the given frame.
   *
   * Per W3C WebCodecs spec section 6.5, the frame is cloned before encoding
   * to allow the caller to close the original frame immediately after this call.
   * The clone operation is performed in the native C++ layer (video_encoder.cpp:505-510)
   * using FFmpeg's av_frame_clone(), ensuring the frame data remains valid throughout
   * the async encoding process.
   *
   * @param frame - The VideoFrame to encode. May be closed after this call returns.
   * @param options - Encoding options, including keyFrame hint.
   * @throws {InvalidStateError} If encoder state is not "configured".
   * @throws {TypeError} If frame is detached.
   * @throws {DataError} If frame orientation doesn't match active orientation.
   *
   * @see https://www.w3.org/TR/webcodecs/#dom-videoencoder-encode
   */
  encode(frame: VideoFrame, options: VideoEncoderEncodeOptions): void {
    // Extract native object from wrapper if present (for TypeScript wrapper classes)
    const nativeFrame = (frame as { native?: unknown }).native ?? frame;
    this.native.encode(nativeFrame, options);
  }
  flush(): Promise<void> {
    return this.native.flush();
  }
  reset(): void {
    this.native.reset();
  }
  close(): void {
    this.native.close();
  }

  static isConfigSupported(config: VideoEncoderConfig): Promise<VideoEncoderSupport> {
    const NativeClass = bindings.VideoEncoder as NativeVideoEncoderConstructor;
    return NativeClass.isConfigSupported(config);
  }
}
