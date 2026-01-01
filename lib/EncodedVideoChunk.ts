/**
 * EncodedVideoChunk - TypeScript wrapper for native EncodedVideoChunk
 * @see spec/context/EncodedVideoChunk.md
 */

import { createRequire } from 'node:module';
import type {
  AllowSharedBufferSource,
  EncodedVideoChunkInit,
  EncodedVideoChunkType,
} from '../types/webcodecs.js';

// Native binding loader - require() necessary for native addons in ESM
// See: https://nodejs.org/api/esm.html#interoperability-with-commonjs
const require = createRequire(import.meta.url);
const bindings = require('bindings')('webcodecs');

/** Native binding interface for EncodedVideoChunk - matches C++ NAPI class shape */
interface NativeEncodedVideoChunk {
  readonly type: EncodedVideoChunkType;
  readonly timestamp: number;
  readonly duration: number | null;
  readonly byteLength: number;
  copyTo(destination: AllowSharedBufferSource): void;
}

/** Native constructor interface for EncodedVideoChunk */
interface NativeEncodedVideoChunkConstructor {
  new (init: EncodedVideoChunkInit): NativeEncodedVideoChunk;
}

export class EncodedVideoChunk {
  private readonly native: NativeEncodedVideoChunk;

  constructor(init: EncodedVideoChunkInit) {
    const NativeClass = bindings.EncodedVideoChunk as NativeEncodedVideoChunkConstructor;
    this.native = new NativeClass(init);
  }

  get type(): EncodedVideoChunkType {
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
