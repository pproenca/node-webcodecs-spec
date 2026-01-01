/**
 * VideoColorSpace - TypeScript wrapper for native VideoColorSpace
 * @see spec/context/VideoColorSpace.md
 */

import type { VideoColorPrimaries, VideoColorSpaceInit, VideoMatrixCoefficients, VideoTransferCharacteristics } from '../types/webcodecs';

// eslint-disable-next-line @typescript-eslint/no-var-requires
const bindings = require('bindings')('webcodecs');

export class VideoColorSpace {
  private readonly _native: unknown;

  constructor(init: VideoColorSpaceInit) {
    this._native = new bindings.VideoColorSpace(init);
  }

  get primaries(): VideoColorPrimaries | null {
    return (this._native as Record<string, unknown>).primaries as VideoColorPrimaries | null;
  }

  get transfer(): VideoTransferCharacteristics | null {
    return (this._native as Record<string, unknown>).transfer as VideoTransferCharacteristics | null;
  }

  get matrix(): VideoMatrixCoefficients | null {
    return (this._native as Record<string, unknown>).matrix as VideoMatrixCoefficients | null;
  }

  get fullRange(): boolean | null {
    return (this._native as Record<string, unknown>).fullRange as boolean | null;
  }


  toJSON(): VideoColorSpaceInit {
    return (this._native as Record<string, Function>).toJSON() as VideoColorSpaceInit;
  }

}
