/**
 * ImageTrackList - TypeScript wrapper for native ImageTrackList
 * @see spec/context/ImageTrackList.md
 */

import { createRequire } from 'node:module';
import type { ImageTrack } from '../types/webcodecs.js';

// Native binding loader - require() necessary for native addons in ESM
// See: https://nodejs.org/api/esm.html#interoperability-with-commonjs
const require = createRequire(import.meta.url);
const bindings = require('bindings')('webcodecs');

/** Native binding interface for ImageTrackList - matches C++ NAPI class shape */
interface NativeImageTrackList {
  readonly ready: Promise<void>;
  readonly length: number;
  readonly selectedIndex: number;
  readonly selectedTrack: ImageTrack | null;
}

/** Native constructor interface for ImageTrackList */
interface NativeImageTrackListConstructor {
  new (init: unknown): NativeImageTrackList;
}

export class ImageTrackList {
  private readonly native: NativeImageTrackList;

  constructor(init: unknown) {
    const NativeClass = bindings.ImageTrackList as NativeImageTrackListConstructor;
    this.native = new NativeClass(init);
  }

  get ready(): Promise<void> {
    return this.native.ready;
  }
  get length(): number {
    return this.native.length;
  }
  get selectedIndex(): number {
    return this.native.selectedIndex;
  }
  get selectedTrack(): ImageTrack | null {
    return this.native.selectedTrack;
  }
}
