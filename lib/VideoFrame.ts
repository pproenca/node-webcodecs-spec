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
  VideoFrameBufferInit,
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
  new (source: VideoFrame | AllowSharedBufferSource, init?: VideoFrameInit | VideoFrameBufferInit): NativeVideoFrame;
}

export class VideoFrame {
  /** @internal Native binding instance - exposed for native interop */
  readonly native: NativeVideoFrame;

  /**
   * Construct a VideoFrame from buffer data.
   * @param data - The raw pixel data as an ArrayBuffer or typed array
   * @param init - Initialization options specifying format, dimensions, etc.
   */
  constructor(data: AllowSharedBufferSource, init: VideoFrameBufferInit);

  /**
   * Construct a VideoFrame by cloning another VideoFrame.
   * @param source - The VideoFrame to clone
   * @param init - Optional initialization options to override values from source
   */
  constructor(source: VideoFrame, init?: VideoFrameInit);

  constructor(
    source: VideoFrame | AllowSharedBufferSource,
    init?: VideoFrameInit | VideoFrameBufferInit
  ) {
    const NativeClass = bindings.VideoFrame as NativeVideoFrameConstructor;
    // Pass native object if source is a VideoFrame, otherwise pass the buffer directly
    if (source instanceof VideoFrame) {
      this.native = new NativeClass(source.native as any, init);
    } else {
      this.native = new NativeClass(source as any, init);
    }
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
  allocationSize(options?: VideoFrameCopyToOptions): number {
    return this.native.allocationSize(options ?? {});
  }
  copyTo(
    destination: AllowSharedBufferSource,
    options?: VideoFrameCopyToOptions
  ): Promise<PlaneLayout[]> {
    return this.native.copyTo(destination, options ?? {});
  }
  clone(): VideoFrame {
    // Use the VideoFrame(VideoFrame, init) constructor to create a proper wrapper
    return new VideoFrame(this, {});
  }
  close(): void {
    this.native.close();
  }
}
