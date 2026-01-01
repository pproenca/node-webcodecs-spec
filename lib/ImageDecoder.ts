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

const require = createRequire(import.meta.url);
const bindings = require('bindings')('webcodecs');

export class ImageDecoder {
  private readonly native: unknown;

  constructor(init: ImageDecoderInit) {
    this.native = new bindings.ImageDecoder(init);
  }

  get type(): string {
    return (this.native as Record<string, unknown>).type as string;
  }

  get complete(): boolean {
    return (this.native as Record<string, unknown>).complete as boolean;
  }

  get completed(): Promise<void> {
    return (this.native as Record<string, unknown>).completed as Promise<void>;
  }

  get tracks(): ImageTrackList {
    return (this.native as Record<string, unknown>).tracks as ImageTrackList;
  }

  decode(options: ImageDecodeOptions): Promise<ImageDecodeResult> {
    return (this.native as Record<string, Function>).decode(options) as Promise<ImageDecodeResult>;
  }

  reset(): void {
    return (this.native as Record<string, Function>).reset() as void;
  }

  close(): void {
    return (this.native as Record<string, Function>).close() as void;
  }

  static isTypeSupported(type: string): Promise<boolean> {
    return bindings.ImageDecoder.isTypeSupported(type) as Promise<boolean>;
  }
}
