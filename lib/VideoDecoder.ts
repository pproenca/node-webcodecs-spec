/**
 * VideoDecoder - TypeScript wrapper for native VideoDecoder
 * @see spec/context/VideoDecoder.md
 */

// eslint-disable-next-line @typescript-eslint/no-var-requires
const bindings = require('bindings')('webcodecs');

export class VideoDecoder {
  private readonly _native: any;

  constructor(init?: any) {
    this._native = new bindings.VideoDecoder(init);
  }


  get state(): any {
    return this._native.state;
  }

  get decodeQueueSize(): any {
    return this._native.decodeQueueSize;
  }

  get ondequeue(): any {
    return this._native.ondequeue;
  }

  set ondequeue(value: any) {
    this._native.ondequeue = value;
  }


  configure(...args: any[]): any {
    return this._native.configure(...args);
  }

  decode(...args: any[]): any {
    return this._native.decode(...args);
  }

  flush(...args: any[]): any {
    return this._native.flush(...args);
  }

  reset(...args: any[]): any {
    return this._native.reset(...args);
  }

  close(...args: any[]): any {
    return this._native.close(...args);
  }


  static isConfigSupported(...args: any[]): any {
    return bindings.VideoDecoder.isConfigSupported(...args);
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
