/**
 * AudioEncoder - TypeScript wrapper for native AudioEncoder
 * @see spec/context/AudioEncoder.md
 */

import { createRequire } from 'node:module';
import type {
  AudioData,
  AudioEncoderConfig,
  AudioEncoderInit,
  AudioEncoderSupport,
  CodecState,
  EventHandler,
} from '../types/webcodecs.js';

// Native binding loader - require() necessary for native addons in ESM
// See: https://nodejs.org/api/esm.html#interoperability-with-commonjs
const require = createRequire(import.meta.url);
const bindings = require('bindings')('webcodecs');

/** Native binding interface for AudioEncoder - matches C++ NAPI class shape */
interface NativeAudioEncoder {
  readonly state: CodecState;
  readonly encodeQueueSize: number;
  ondequeue: EventHandler;
  configure(config: AudioEncoderConfig): void;
  encode(data: AudioData): void;
  flush(): Promise<void>;
  reset(): void;
  close(): void;
}

/** Native constructor interface for AudioEncoder */
interface NativeAudioEncoderConstructor {
  new (init: AudioEncoderInit): NativeAudioEncoder;
  isConfigSupported(config: AudioEncoderConfig): Promise<AudioEncoderSupport>;
}

export class AudioEncoder {
  private readonly native: NativeAudioEncoder;

  constructor(init: AudioEncoderInit) {
    const NativeClass = bindings.AudioEncoder as NativeAudioEncoderConstructor;
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

  configure(config: AudioEncoderConfig): void {
    this.native.configure(config);
  }
  encode(data: AudioData): void {
    this.native.encode(data);
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

  static isConfigSupported(config: AudioEncoderConfig): Promise<AudioEncoderSupport> {
    const NativeClass = bindings.AudioEncoder as NativeAudioEncoderConstructor;
    return NativeClass.isConfigSupported(config);
  }
}
