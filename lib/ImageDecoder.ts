/**
 * ImageDecoder - TypeScript wrapper for native ImageDecoder
 * @see spec/context/ImageDecoder.md
 */

// eslint-disable-next-line @typescript-eslint/no-var-requires
const bindings = require('bindings')('webcodecs');

export class ImageDecoder {
  private readonly _native: any;

  constructor(init?: any) {
    this._native = new bindings.ImageDecoder(init);
  }


  get type(): any {
    return this._native.type;
  }

  get complete(): any {
    return this._native.complete;
  }

  get completed(): any {
    return this._native.completed;
  }

  get tracks(): any {
    return this._native.tracks;
  }


  decode(...args: any[]): any {
    return this._native.decode(...args);
  }

  reset(...args: any[]): any {
    return this._native.reset(...args);
  }

  close(): void {
    if (this._native?.close) {
      this._native.close();
    }
  }

  static isTypeSupported(...args: any[]): any {
    return bindings.ImageDecoder.isTypeSupported(...args);
  }
}
