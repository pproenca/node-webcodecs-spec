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

const require = createRequire(import.meta.url);
const bindings = require('bindings')('webcodecs');

export class VideoFrame {
  private readonly native: unknown;

  constructor(init: VideoFrameInit) {
    this.native = new bindings.VideoFrame(init);
  }

  get format(): VideoPixelFormat | null {
    return (this.native as Record<string, unknown>).format as VideoPixelFormat | null;
  }

  get codedWidth(): number {
    return (this.native as Record<string, unknown>).codedWidth as number;
  }

  get codedHeight(): number {
    return (this.native as Record<string, unknown>).codedHeight as number;
  }

  get codedRect(): DOMRectReadOnly | null {
    return (this.native as Record<string, unknown>).codedRect as DOMRectReadOnly | null;
  }

  get visibleRect(): DOMRectReadOnly | null {
    return (this.native as Record<string, unknown>).visibleRect as DOMRectReadOnly | null;
  }

  get rotation(): number {
    return (this.native as Record<string, unknown>).rotation as number;
  }

  get flip(): boolean {
    return (this.native as Record<string, unknown>).flip as boolean;
  }

  get displayWidth(): number {
    return (this.native as Record<string, unknown>).displayWidth as number;
  }

  get displayHeight(): number {
    return (this.native as Record<string, unknown>).displayHeight as number;
  }

  get duration(): number | null {
    return (this.native as Record<string, unknown>).duration as number | null;
  }

  get timestamp(): number {
    return (this.native as Record<string, unknown>).timestamp as number;
  }

  get colorSpace(): VideoColorSpace {
    return (this.native as Record<string, unknown>).colorSpace as VideoColorSpace;
  }

  metadata(): VideoFrameMetadata {
    return (this.native as Record<string, Function>).metadata() as VideoFrameMetadata;
  }

  allocationSize(options: VideoFrameCopyToOptions): number {
    return (this.native as Record<string, Function>).allocationSize(options) as number;
  }

  copyTo(
    destination: AllowSharedBufferSource,
    options: VideoFrameCopyToOptions
  ): Promise<PlaneLayout[]> {
    return (this.native as Record<string, Function>).copyTo(destination, options) as Promise<
      PlaneLayout[]
    >;
  }

  clone(): VideoFrame {
    return (this.native as Record<string, Function>).clone() as VideoFrame;
  }

  close(): void {
    return (this.native as Record<string, Function>).close() as void;
  }
}
