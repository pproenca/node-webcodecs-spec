/**
 * EncodedAudioChunk - TypeScript wrapper for native EncodedAudioChunk
 * @see spec/context/EncodedAudioChunk.md
 */

import type { AllowSharedBufferSource, EncodedAudioChunkInit, EncodedAudioChunkType } from '../types/webcodecs';

// eslint-disable-next-line @typescript-eslint/no-var-requires
const bindings = require('bindings')('webcodecs');

export class EncodedAudioChunk {
  private readonly _native: unknown;

  constructor(init: EncodedAudioChunkInit) {
    this._native = new bindings.EncodedAudioChunk(init);
  }

  get type(): EncodedAudioChunkType {
    return (this._native as Record<string, unknown>).type as EncodedAudioChunkType;
  }

  get timestamp(): number {
    return (this._native as Record<string, unknown>).timestamp as number;
  }

  get duration(): number | null {
    return (this._native as Record<string, unknown>).duration as number | null;
  }

  get byteLength(): number {
    return (this._native as Record<string, unknown>).byteLength as number;
  }


  copyTo(destination: AllowSharedBufferSource): void {
    return (this._native as Record<string, Function>).copyTo(destination) as void;
  }

}
