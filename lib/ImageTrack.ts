/**
 * ImageTrack - TypeScript wrapper for native ImageTrack
 * @see spec/context/ImageTrack.md
 */

// eslint-disable-next-line @typescript-eslint/no-var-requires
const bindings = require('bindings')('webcodecs');

export class ImageTrack {
  private readonly _native: unknown;

  constructor(init: unknown) {
    this._native = new bindings.ImageTrack(init);
  }

  get animated(): boolean {
    return (this._native as Record<string, unknown>).animated as boolean;
  }

  get frameCount(): number {
    return (this._native as Record<string, unknown>).frameCount as number;
  }

  get repetitionCount(): number {
    return (this._native as Record<string, unknown>).repetitionCount as number;
  }

  get selected(): boolean {
    return (this._native as Record<string, unknown>).selected as boolean;
  }

  set selected(value: boolean) {
    (this._native as Record<string, unknown>).selected = value;
  }


}
