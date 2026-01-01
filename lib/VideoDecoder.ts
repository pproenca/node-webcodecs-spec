/**
 * VideoDecoder - TypeScript wrapper for native VideoDecoder
 * @see spec/context/VideoDecoder.md
 */

import { createRequire } from 'node:module';
import type {
  CodecState,
  EncodedVideoChunk,
  EventHandler,
  VideoDecoderConfig,
  VideoDecoderInit,
  VideoDecoderSupport,
} from '../types/webcodecs.js';

// Native binding loader - require() necessary for native addons in ESM
// See: https://nodejs.org/api/esm.html#interoperability-with-commonjs
const require = createRequire(import.meta.url);
const bindings = require('bindings')('webcodecs');

/** Native binding interface for VideoDecoder - matches C++ NAPI class shape */
interface NativeVideoDecoder {
  readonly state: CodecState;
  readonly decodeQueueSize: number;
  ondequeue: EventHandler;
  configure(config: VideoDecoderConfig): void;
  decode(chunk: EncodedVideoChunk): void;
  flush(): Promise<void>;
  reset(): void;
  close(): void;
}

/** Native constructor interface for VideoDecoder */
interface NativeVideoDecoderConstructor {
  new (init: VideoDecoderInit): NativeVideoDecoder;
  isConfigSupported(config: VideoDecoderConfig): Promise<VideoDecoderSupport>;
}

export class VideoDecoder {
  private readonly native: NativeVideoDecoder;

  constructor(init: VideoDecoderInit) {
    const NativeClass = bindings.VideoDecoder as NativeVideoDecoderConstructor;
    this.native = new NativeClass(init);
  }

  get state(): CodecState {
    return this.native.state;
  }
  get decodeQueueSize(): number {
    return this.native.decodeQueueSize;
  }
  get ondequeue(): EventHandler {
    return this.native.ondequeue;
  }

  set ondequeue(value: EventHandler) {
    this.native.ondequeue = value;
  }

  configure(config: VideoDecoderConfig): void {
    this.native.configure(config);
  }
  decode(chunk: EncodedVideoChunk): void {
    this.native.decode(chunk);
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

  static isConfigSupported(config: VideoDecoderConfig): Promise<VideoDecoderSupport> {
    const NativeClass = bindings.VideoDecoder as NativeVideoDecoderConstructor;
    return NativeClass.isConfigSupported(config);
  }
}
