/**
 * EncodedAudioChunk - TypeScript wrapper for native EncodedAudioChunk
 * @see spec/context/EncodedAudioChunk.md
 */

// eslint-disable-next-line @typescript-eslint/no-var-requires
const bindings = require('bindings')('webcodecs');

export class EncodedAudioChunk {
  private readonly _native: any;

  constructor(init?: any) {
    this._native = new bindings.EncodedAudioChunk(init);
  }


  get type(): any {
    return this._native.type;
  }

  get timestamp(): any {
    return this._native.timestamp;
  }

  get duration(): any {
    return this._native.duration;
  }

  get byteLength(): any {
    return this._native.byteLength;
  }


  copyTo(...args: any[]): any {
    return this._native.copyTo(...args);
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
