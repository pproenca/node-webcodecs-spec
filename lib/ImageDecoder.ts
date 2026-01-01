/**
 * ImageDecoder - TypeScript wrapper for native ImageDecoder
 * @see spec/context/ImageDecoder.md
 */

import { createRequire } from 'node:module';
import type {
  ImageDecodeOptions,
  ImageDecodeResult,
  ImageDecoderInit,
  ImageTrackList,
} from '../types/webcodecs.js';

// Native binding loader - require() necessary for native addons in ESM
// See: https://nodejs.org/api/esm.html#interoperability-with-commonjs
const require = createRequire(import.meta.url);
const bindings = require('bindings')('webcodecs');

/** Native binding interface for ImageDecoder - matches C++ NAPI class shape */
interface NativeImageDecoder {
  readonly type: string;
  readonly complete: boolean;
  readonly completed: Promise<void>;
  readonly tracks: ImageTrackList;
  decode(options: ImageDecodeOptions): Promise<ImageDecodeResult>;
  reset(): void;
  close(): void;
}

/** Native constructor interface for ImageDecoder */
interface NativeImageDecoderConstructor {
  new (init: ImageDecoderInit): NativeImageDecoder;
  isTypeSupported(type: string): Promise<boolean>;
}

export class ImageDecoder {
  private readonly native: NativeImageDecoder;

  constructor(init: ImageDecoderInit) {
    const NativeClass = bindings.ImageDecoder as NativeImageDecoderConstructor;
    this.native = new NativeClass(init);
  }

  get type(): string {
    return this.native.type;
  }
  get complete(): boolean {
    return this.native.complete;
  }
  get completed(): Promise<void> {
    return this.native.completed;
  }
  get tracks(): ImageTrackList {
    return this.native.tracks;
  }

  decode(options: ImageDecodeOptions): Promise<ImageDecodeResult> {
    return this.native.decode(options);
  }
  reset(): void {
    this.native.reset();
  }
  close(): void {
    this.native.close();
  }

  static isTypeSupported(type: string): Promise<boolean> {
    const NativeClass = bindings.ImageDecoder as NativeImageDecoderConstructor;
    return NativeClass.isTypeSupported(type);
  }
}
