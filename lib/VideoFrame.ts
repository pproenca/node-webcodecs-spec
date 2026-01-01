/**
 * VideoFrame - TypeScript wrapper for native VideoFrame
 * @see spec/context/VideoFrame.md
 */

import { createRequire } from 'node:module';
import type {
  AllowSharedBufferSource,
  DOMRectReadOnly,
  PlaneLayout,
  VideoColorSpace,
  VideoFrameCopyToOptions,
  VideoFrameInit,
  VideoFrameMetadata,
  VideoPixelFormat,
} from '../types/webcodecs.js';

// Native binding loader - require() necessary for native addons in ESM
// See: https://nodejs.org/api/esm.html#interoperability-with-commonjs
const require = createRequire(import.meta.url);
const bindings = require('bindings')('webcodecs');

/** Native binding interface for VideoFrame - matches C++ NAPI class shape */
interface NativeVideoFrame {
  readonly format: VideoPixelFormat | null;
  readonly codedWidth: number;
  readonly codedHeight: number;
  readonly codedRect: DOMRectReadOnly | null;
  readonly visibleRect: DOMRectReadOnly | null;
  readonly rotation: number;
  readonly flip: boolean;
  readonly displayWidth: number;
  readonly displayHeight: number;
  readonly duration: number | null;
  readonly timestamp: number;
  readonly colorSpace: VideoColorSpace;
  metadata(): VideoFrameMetadata;
  allocationSize(options: VideoFrameCopyToOptions): number;
  copyTo(
    destination: AllowSharedBufferSource,
    options: VideoFrameCopyToOptions
  ): Promise<PlaneLayout[]>;
  clone(): VideoFrame;
  close(): void;
}

/** Native constructor interface for VideoFrame */
interface NativeVideoFrameConstructor {
  new (init: VideoFrameInit): NativeVideoFrame;
}

export class VideoFrame {
  private readonly native: NativeVideoFrame;

  constructor(init: VideoFrameInit) {
    const NativeClass = bindings.VideoFrame as NativeVideoFrameConstructor;
    this.native = new NativeClass(init);
  }

  get format(): VideoPixelFormat | null {
    return this.native.format;
  }
  get codedWidth(): number {
    return this.native.codedWidth;
  }
  get codedHeight(): number {
    return this.native.codedHeight;
  }
  get codedRect(): DOMRectReadOnly | null {
    return this.native.codedRect;
  }
  get visibleRect(): DOMRectReadOnly | null {
    return this.native.visibleRect;
  }
  get rotation(): number {
    return this.native.rotation;
  }
  get flip(): boolean {
    return this.native.flip;
  }
  get displayWidth(): number {
    return this.native.displayWidth;
  }
  get displayHeight(): number {
    return this.native.displayHeight;
  }
  get duration(): number | null {
    return this.native.duration;
  }
  get timestamp(): number {
    return this.native.timestamp;
  }
  get colorSpace(): VideoColorSpace {
    return this.native.colorSpace;
  }

  metadata(): VideoFrameMetadata {
    return this.native.metadata();
  }
  allocationSize(options: VideoFrameCopyToOptions): number {
    return this.native.allocationSize(options);
  }
  copyTo(
    destination: AllowSharedBufferSource,
    options: VideoFrameCopyToOptions
  ): Promise<PlaneLayout[]> {
    return this.native.copyTo(destination, options);
  }
  clone(): VideoFrame {
    return this.native.clone();
  }
  close(): void {
    this.native.close();
  }
}
