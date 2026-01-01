/**
 * ImageTrack - TypeScript wrapper for native ImageTrack
 * @see spec/context/ImageTrack.md
 */

// eslint-disable-next-line @typescript-eslint/no-var-requires
const bindings = require('bindings')('webcodecs');

export class ImageTrack {
  private readonly _native: any;

  constructor(init?: any) {
    this._native = new bindings.ImageTrack(init);
  }


  get animated(): any {
    return this._native.animated;
  }

  get frameCount(): any {
    return this._native.frameCount;
  }

  get repetitionCount(): any {
    return this._native.repetitionCount;
  }

  get selected(): any {
    return this._native.selected;
  }

  set selected(value: any) {
    this._native.selected = value;
  }



  /**
   * Explicit Resource Management (RAII)
   * Call this to release native resources immediately.
   */
  close(): void {
    if (this._native?.Release) {
      this._native.Release();
    }
  }
}
