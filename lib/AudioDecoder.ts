/**
 * AudioDecoder - TypeScript wrapper for native AudioDecoder
 * @see spec/context/AudioDecoder.md
 */

import { createRequire } from 'node:module';
import type {
  AudioDecoderConfig,
  AudioDecoderInit,
  AudioDecoderSupport,
  CodecState,
  EncodedAudioChunk,
  EventHandler,
} from '../types/webcodecs.js';

// Native binding loader - require() necessary for native addons in ESM
// See: https://nodejs.org/api/esm.html#interoperability-with-commonjs
const require = createRequire(import.meta.url);
const bindings = require('bindings')('webcodecs');

/** Native binding interface for AudioDecoder - matches C++ NAPI class shape */
interface NativeAudioDecoder {
  readonly state: CodecState;
  readonly decodeQueueSize: number;
  ondequeue: EventHandler;
  configure(config: AudioDecoderConfig): void;
  decode(chunk: EncodedAudioChunk): void;
  flush(): Promise<void>;
  reset(): void;
  close(): void;
}

/** Native constructor interface for AudioDecoder */
interface NativeAudioDecoderConstructor {
  new (init: AudioDecoderInit): NativeAudioDecoder;
  isConfigSupported(config: AudioDecoderConfig): Promise<AudioDecoderSupport>;
}

export class AudioDecoder {
  private readonly native: NativeAudioDecoder;

  constructor(init: AudioDecoderInit) {
    const NativeClass = bindings.AudioDecoder as NativeAudioDecoderConstructor;
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

  configure(config: AudioDecoderConfig): void {
    this.native.configure(config);
  }
  decode(chunk: EncodedAudioChunk): void {
    // Unwrap TypeScript EncodedAudioChunk to pass native object to C++
    // The native code expects either a native EncodedAudioChunk or plain object
    const nativeChunk = (chunk as { native?: unknown }).native ?? chunk;
    this.native.decode(nativeChunk);
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

  static isConfigSupported(config: AudioDecoderConfig): Promise<AudioDecoderSupport> {
    const NativeClass = bindings.AudioDecoder as NativeAudioDecoderConstructor;
    return NativeClass.isConfigSupported(config);
  }
}
