/**
 * ImageTrackList - TypeScript wrapper for native ImageTrackList
 * @see spec/context/ImageTrackList.md
 */

// eslint-disable-next-line @typescript-eslint/no-var-requires
const bindings = require('bindings')('webcodecs');

export class ImageTrackList {
  private readonly _native: any;

  constructor(init?: any) {
    this._native = new bindings.ImageTrackList(init);
  }


  get ready(): any {
    return this._native.ready;
  }

  get length(): any {
    return this._native.length;
  }

  get selectedIndex(): any {
    return this._native.selectedIndex;
  }

  get selectedTrack(): any {
    return this._native.selectedTrack;
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
