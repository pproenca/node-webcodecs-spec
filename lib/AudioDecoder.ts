/**
 * AudioDecoder - TypeScript wrapper for native AudioDecoder
 * @see spec/context/AudioDecoder.md
 */

// eslint-disable-next-line @typescript-eslint/no-var-requires
const bindings = require('bindings')('webcodecs');

export class AudioDecoder {
  private readonly _native: any;

  constructor(init?: any) {
    this._native = new bindings.AudioDecoder(init);
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

  close(): void {
    if (this._native?.close) {
      this._native.close();
    }
  }

  static isConfigSupported(...args: any[]): any {
    return bindings.AudioDecoder.isConfigSupported(...args);
  }
}
