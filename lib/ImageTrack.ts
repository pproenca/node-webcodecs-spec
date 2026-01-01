/**
 * ImageTrack - TypeScript wrapper for native ImageTrack
 * @see spec/context/ImageTrack.md
 */

import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);
const bindings = require('bindings')('webcodecs');

export class ImageTrack {
  private readonly native: unknown;

  constructor(init: unknown) {
    this.native = new bindings.ImageTrack(init);
  }

  get animated(): boolean {
    return (this.native as Record<string, unknown>).animated as boolean;
  }

  get frameCount(): number {
    return (this.native as Record<string, unknown>).frameCount as number;
  }

  get repetitionCount(): number {
    return (this.native as Record<string, unknown>).repetitionCount as number;
  }

  get selected(): boolean {
    return (this.native as Record<string, unknown>).selected as boolean;
  }

  set selected(value: boolean) {
    (this.native as Record<string, unknown>).selected = value;
  }
}
