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

const require = createRequire(import.meta.url);
const bindings = require('bindings')('webcodecs');

export class EncodedVideoChunk {
  private readonly native: unknown;

  constructor(init: EncodedVideoChunkInit) {
    this.native = new bindings.EncodedVideoChunk(init);
  }

  get type(): EncodedVideoChunkType {
    return (this.native as Record<string, unknown>).type as EncodedVideoChunkType;
  }

  get timestamp(): number {
    return (this.native as Record<string, unknown>).timestamp as number;
  }

  get duration(): number | null {
    return (this.native as Record<string, unknown>).duration as number | null;
  }

  get byteLength(): number {
    return (this.native as Record<string, unknown>).byteLength as number;
  }

  copyTo(destination: AllowSharedBufferSource): void {
    return (this.native as Record<string, Function>).copyTo(destination) as void;
  }
}
