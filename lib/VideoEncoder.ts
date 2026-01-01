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
  VideoFrame,
} from '../types/webcodecs.js';

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
  encode(frame: VideoFrame, options: VideoEncoderEncodeOptions): void {
    this.native.encode(frame, options);
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
