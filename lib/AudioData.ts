/**
 * AudioData - TypeScript wrapper for native AudioData
 * @see spec/context/AudioData.md
 */

import { createRequire } from 'node:module';
import type {
  AllowSharedBufferSource,
  AudioDataCopyToOptions,
  AudioDataInit,
  AudioSampleFormat,
} from '../types/webcodecs.js';

// Native binding loader - require() necessary for native addons in ESM
// See: https://nodejs.org/api/esm.html#interoperability-with-commonjs
const require = createRequire(import.meta.url);
const bindings = require('bindings')('webcodecs');

/** Native binding interface for AudioData - matches C++ NAPI class shape */
interface NativeAudioData {
  readonly format: AudioSampleFormat | null;
  readonly sampleRate: number;
  readonly numberOfFrames: number;
  readonly numberOfChannels: number;
  readonly duration: number;
  readonly timestamp: number;
  allocationSize(options: AudioDataCopyToOptions): number;
  copyTo(destination: AllowSharedBufferSource, options: AudioDataCopyToOptions): void;
  clone(): AudioData;
  close(): void;
}

/** Native constructor interface for AudioData */
interface NativeAudioDataConstructor {
  new (init: AudioDataInit): NativeAudioData;
}

export class AudioData {
  private readonly native: NativeAudioData;

  constructor(init: AudioDataInit) {
    const NativeClass = bindings.AudioData as NativeAudioDataConstructor;
    this.native = new NativeClass(init);
  }

  get format(): AudioSampleFormat | null {
    return this.native.format;
  }
  get sampleRate(): number {
    return this.native.sampleRate;
  }
  get numberOfFrames(): number {
    return this.native.numberOfFrames;
  }
  get numberOfChannels(): number {
    return this.native.numberOfChannels;
  }
  get duration(): number {
    return this.native.duration;
  }
  get timestamp(): number {
    return this.native.timestamp;
  }

  allocationSize(options: AudioDataCopyToOptions): number {
    return this.native.allocationSize(options);
  }
  copyTo(destination: AllowSharedBufferSource, options: AudioDataCopyToOptions): void {
    this.native.copyTo(destination, options);
  }
  clone(): AudioData {
    return this.native.clone();
  }
  close(): void {
    this.native.close();
  }
}
