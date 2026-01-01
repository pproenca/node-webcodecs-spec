/**
 * ImageTrack - TypeScript wrapper for native ImageTrack
 * @see spec/context/ImageTrack.md
 */

import { createRequire } from 'node:module';

// Native binding loader - require() necessary for native addons in ESM
// See: https://nodejs.org/api/esm.html#interoperability-with-commonjs
const require = createRequire(import.meta.url);
const bindings = require('bindings')('webcodecs');

/** Native binding interface for ImageTrack - matches C++ NAPI class shape */
interface NativeImageTrack {
  readonly animated: boolean;
  readonly frameCount: number;
  readonly repetitionCount: number;
  selected: boolean;
}

/** Native constructor interface for ImageTrack */
interface NativeImageTrackConstructor {
  new (init: unknown): NativeImageTrack;
}

export class ImageTrack {
  private readonly native: NativeImageTrack;

  constructor(init: unknown) {
    const NativeClass = bindings.ImageTrack as NativeImageTrackConstructor;
    this.native = new NativeClass(init);
  }

  get animated(): boolean {
    return this.native.animated;
  }
  get frameCount(): number {
    return this.native.frameCount;
  }
  get repetitionCount(): number {
    return this.native.repetitionCount;
  }
  get selected(): boolean {
    return this.native.selected;
  }

  set selected(value: boolean) {
    this.native.selected = value;
  }
}
