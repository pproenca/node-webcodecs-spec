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

const require = createRequire(import.meta.url);
const bindings = require('bindings')('webcodecs');

export class EncodedAudioChunk {
  private readonly native: unknown;

  constructor(init: EncodedAudioChunkInit) {
    this.native = new bindings.EncodedAudioChunk(init);
  }

  get type(): EncodedAudioChunkType {
    return (this.native as Record<string, unknown>).type as EncodedAudioChunkType;
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
