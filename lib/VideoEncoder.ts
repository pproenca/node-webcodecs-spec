/**
 * VideoEncoder - TypeScript wrapper for native VideoEncoder
 * @see spec/context/VideoEncoder.md
 */

// eslint-disable-next-line @typescript-eslint/no-var-requires
const bindings = require('bindings')('webcodecs');

export class VideoEncoder {
  private readonly _native: any;

  constructor(init?: any) {
    this._native = new bindings.VideoEncoder(init);
  }


  get state(): any {
    return this._native.state;
  }

  get encodeQueueSize(): any {
    return this._native.encodeQueueSize;
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

  encode(...args: any[]): any {
    return this._native.encode(...args);
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
    return bindings.VideoEncoder.isConfigSupported(...args);
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
