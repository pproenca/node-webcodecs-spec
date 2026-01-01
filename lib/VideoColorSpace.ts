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

// Native binding loader - require() necessary for native addons in ESM
// See: https://nodejs.org/api/esm.html#interoperability-with-commonjs
const require = createRequire(import.meta.url);
const bindings = require('bindings')('webcodecs');

/** Native binding interface for VideoColorSpace - matches C++ NAPI class shape */
interface NativeVideoColorSpace {
  readonly primaries: VideoColorPrimaries | null;
  readonly transfer: VideoTransferCharacteristics | null;
  readonly matrix: VideoMatrixCoefficients | null;
  readonly fullRange: boolean | null;
  toJSON(): VideoColorSpaceInit;
}

/** Native constructor interface for VideoColorSpace */
interface NativeVideoColorSpaceConstructor {
  new (init: VideoColorSpaceInit): NativeVideoColorSpace;
}

export class VideoColorSpace {
  private readonly native: NativeVideoColorSpace;

  constructor(init: VideoColorSpaceInit) {
    const NativeClass = bindings.VideoColorSpace as NativeVideoColorSpaceConstructor;
    this.native = new NativeClass(init);
  }

  get primaries(): VideoColorPrimaries | null {
    return this.native.primaries;
  }
  get transfer(): VideoTransferCharacteristics | null {
    return this.native.transfer;
  }
  get matrix(): VideoMatrixCoefficients | null {
    return this.native.matrix;
  }
  get fullRange(): boolean | null {
    return this.native.fullRange;
  }

  toJSON(): VideoColorSpaceInit {
    return this.native.toJSON();
  }
}
