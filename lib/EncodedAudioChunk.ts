/**
 * EncodedAudioChunk - TypeScript wrapper for native EncodedAudioChunk
 * @see spec/context/EncodedAudioChunk.md
 */

import { createRequire } from 'node:module';
import type {
  AllowSharedBufferSource,
  EncodedAudioChunkInit,
  EncodedAudioChunkType,
} from '../types/webcodecs.js';

// Native binding loader - require() necessary for native addons in ESM
// See: https://nodejs.org/api/esm.html#interoperability-with-commonjs
const require = createRequire(import.meta.url);
const bindings = require('bindings')('webcodecs');

/** Native binding interface for EncodedAudioChunk - matches C++ NAPI class shape */
interface NativeEncodedAudioChunk {
  readonly type: EncodedAudioChunkType;
  readonly timestamp: number;
  readonly duration: number | null;
  readonly byteLength: number;
  copyTo(destination: AllowSharedBufferSource): void;
}

/** Native constructor interface for EncodedAudioChunk */
interface NativeEncodedAudioChunkConstructor {
  new (init: EncodedAudioChunkInit): NativeEncodedAudioChunk;
}

export class EncodedAudioChunk {
  private readonly native: NativeEncodedAudioChunk;

  constructor(init: EncodedAudioChunkInit) {
    const NativeClass = bindings.EncodedAudioChunk as NativeEncodedAudioChunkConstructor;
    this.native = new NativeClass(init);
  }

  get type(): EncodedAudioChunkType {
    return this.native.type;
  }
  get timestamp(): number {
    return this.native.timestamp;
  }
  get duration(): number | null {
    return this.native.duration;
  }
  get byteLength(): number {
    return this.native.byteLength;
  }

  copyTo(destination: AllowSharedBufferSource): void {
    this.native.copyTo(destination);
  }
}
