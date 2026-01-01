/**
 * AudioEncoder - TypeScript wrapper for native AudioEncoder
 * @see spec/context/AudioEncoder.md
 */

// eslint-disable-next-line @typescript-eslint/no-var-requires
const bindings = require('bindings')('webcodecs');

export class AudioEncoder {
  private readonly _native: any;

  constructor(init?: any) {
    this._native = new bindings.AudioEncoder(init);
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

  close(): void {
    if (this._native?.close) {
      this._native.close();
    }
  }

  static isConfigSupported(...args: any[]): any {
    return bindings.AudioEncoder.isConfigSupported(...args);
  }
}
