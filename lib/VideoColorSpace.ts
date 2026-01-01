/**
 * VideoColorSpace - TypeScript wrapper for native VideoColorSpace
 * @see spec/context/VideoColorSpace.md
 */

import { createRequire } from 'node:module';
import type {
  VideoColorPrimaries,
  VideoColorSpaceInit,
  VideoMatrixCoefficients,
  VideoTransferCharacteristics,
} from '../types/webcodecs.js';

const require = createRequire(import.meta.url);
const bindings = require('bindings')('webcodecs');

export class VideoColorSpace {
  private readonly native: unknown;

  constructor(init: VideoColorSpaceInit) {
    this.native = new bindings.VideoColorSpace(init);
  }

  get primaries(): VideoColorPrimaries | null {
    return (this.native as Record<string, unknown>).primaries as VideoColorPrimaries | null;
  }

  get transfer(): VideoTransferCharacteristics | null {
    return (this.native as Record<string, unknown>).transfer as VideoTransferCharacteristics | null;
  }

  get matrix(): VideoMatrixCoefficients | null {
    return (this.native as Record<string, unknown>).matrix as VideoMatrixCoefficients | null;
  }

  get fullRange(): boolean | null {
    return (this.native as Record<string, unknown>).fullRange as boolean | null;
  }

  toJSON(): VideoColorSpaceInit {
    return (this.native as Record<string, Function>).toJSON() as VideoColorSpaceInit;
  }
}
