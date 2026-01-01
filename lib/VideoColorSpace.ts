/**
 * VideoColorSpace - TypeScript wrapper for native VideoColorSpace
 * @see spec/context/VideoColorSpace.md
 */

// eslint-disable-next-line @typescript-eslint/no-var-requires
const bindings = require('bindings')('webcodecs');

export class VideoColorSpace {
  private readonly _native: any;

  constructor(init?: any) {
    this._native = new bindings.VideoColorSpace(init);
  }


  get primaries(): any {
    return this._native.primaries;
  }

  get transfer(): any {
    return this._native.transfer;
  }

  get matrix(): any {
    return this._native.matrix;
  }

  get fullRange(): any {
    return this._native.fullRange;
  }


  toJSON(...args: any[]): any {
    return this._native.toJSON(...args);
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
