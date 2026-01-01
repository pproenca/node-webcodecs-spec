/**
 * VideoFrame - TypeScript wrapper for native VideoFrame
 * @see spec/context/VideoFrame.md
 */

// eslint-disable-next-line @typescript-eslint/no-var-requires
const bindings = require('bindings')('webcodecs');

export class VideoFrame {
  private readonly _native: any;

  constructor(init?: any) {
    this._native = new bindings.VideoFrame(init);
  }


  get format(): any {
    return this._native.format;
  }

  get codedWidth(): any {
    return this._native.codedWidth;
  }

  get codedHeight(): any {
    return this._native.codedHeight;
  }

  get codedRect(): any {
    return this._native.codedRect;
  }

  get visibleRect(): any {
    return this._native.visibleRect;
  }

  get rotation(): any {
    return this._native.rotation;
  }

  get flip(): any {
    return this._native.flip;
  }

  get displayWidth(): any {
    return this._native.displayWidth;
  }

  get displayHeight(): any {
    return this._native.displayHeight;
  }

  get duration(): any {
    return this._native.duration;
  }

  get timestamp(): any {
    return this._native.timestamp;
  }

  get colorSpace(): any {
    return this._native.colorSpace;
  }


  metadata(...args: any[]): any {
    return this._native.metadata(...args);
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

  close(...args: any[]): any {
    return this._native.close(...args);
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
