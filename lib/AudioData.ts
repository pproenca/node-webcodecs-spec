/**
 * AudioData - TypeScript wrapper for native AudioData
 * @see spec/context/AudioData.md
 */

// eslint-disable-next-line @typescript-eslint/no-var-requires
const bindings = require('bindings')('webcodecs');

export class AudioData {
  private readonly _native: any;

  constructor(init?: any) {
    this._native = new bindings.AudioData(init);
  }


  get format(): any {
    return this._native.format;
  }

  get sampleRate(): any {
    return this._native.sampleRate;
  }

  get numberOfFrames(): any {
    return this._native.numberOfFrames;
  }

  get numberOfChannels(): any {
    return this._native.numberOfChannels;
  }

  get duration(): any {
    return this._native.duration;
  }

  get timestamp(): any {
    return this._native.timestamp;
  }


  allocationSize(...args: any[]): any {
    return this._native.allocationSize(...args);
  }

  copyTo(...args: any[]): any {
    return this._native.copyTo(...args);
  }

  clone(...args: any[]): any {
    return this._native.clone(...args);
  }

  close(): void {
    if (this._native?.close) {
      this._native.close();
    }
  }
}
