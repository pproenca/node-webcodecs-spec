/**
 * EncodedVideoChunk - TypeScript wrapper for native EncodedVideoChunk
 * @see spec/context/EncodedVideoChunk.md
 */

import type { AllowSharedBufferSource, EncodedVideoChunkInit, EncodedVideoChunkType } from '../types/webcodecs';

// eslint-disable-next-line @typescript-eslint/no-var-requires
const bindings = require('bindings')('webcodecs');

export class EncodedVideoChunk {
  private readonly _native: unknown;

  constructor(init: EncodedVideoChunkInit) {
    this._native = new bindings.EncodedVideoChunk(init);
  }

  get type(): EncodedVideoChunkType {
    return (this._native as Record<string, unknown>).type as EncodedVideoChunkType;
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
