/**
 * ImageDecoder - TypeScript wrapper for native ImageDecoder
 * @see spec/context/ImageDecoder.md
 */

import type { ImageDecodeOptions, ImageDecodeResult, ImageDecoderInit, ImageTrackList } from '../types/webcodecs';

// eslint-disable-next-line @typescript-eslint/no-var-requires
const bindings = require('bindings')('webcodecs');

export class ImageDecoder {
  private readonly _native: unknown;

  constructor(init: ImageDecoderInit) {
    this._native = new bindings.ImageDecoder(init);
  }

  get type(): string {
    return (this._native as Record<string, unknown>).type as string;
  }

  get complete(): boolean {
    return (this._native as Record<string, unknown>).complete as boolean;
  }

  get completed(): Promise<void> {
    return (this._native as Record<string, unknown>).completed as Promise<void>;
  }

  get tracks(): ImageTrackList {
    return (this._native as Record<string, unknown>).tracks as ImageTrackList;
  }


  decode(options: ImageDecodeOptions): Promise<ImageDecodeResult> {
    return (this._native as Record<string, Function>).decode(options) as Promise<ImageDecodeResult>;
  }

  reset(): void {
    return (this._native as Record<string, Function>).reset() as void;
  }

  close(): void {
    return (this._native as Record<string, Function>).close() as void;
  }


  static isTypeSupported(type: string): Promise<boolean> {
    return bindings.ImageDecoder.isTypeSupported(type) as Promise<boolean>;
  }
}
