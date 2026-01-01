/**
 * VideoEncoder - TypeScript wrapper for native VideoEncoder
 * @see spec/context/VideoEncoder.md
 */

import { createRequire } from 'node:module';
import type {
  CodecState,
  EventHandler,
  VideoEncoderConfig,
  VideoEncoderEncodeOptions,
  VideoEncoderInit,
  VideoEncoderSupport,
  VideoFrame,
} from '../types/webcodecs.js';

const require = createRequire(import.meta.url);
const bindings = require('bindings')('webcodecs');

export class VideoEncoder {
  private readonly native: unknown;

  constructor(init: VideoEncoderInit) {
    this.native = new bindings.VideoEncoder(init);
  }

  get state(): CodecState {
    return (this.native as Record<string, unknown>).state as CodecState;
  }

  get encodeQueueSize(): number {
    return (this.native as Record<string, unknown>).encodeQueueSize as number;
  }

  get ondequeue(): EventHandler {
    return (this.native as Record<string, unknown>).ondequeue as EventHandler;
  }

  set ondequeue(value: EventHandler) {
    (this.native as Record<string, unknown>).ondequeue = value;
  }

  configure(config: VideoEncoderConfig): void {
    return (this.native as Record<string, Function>).configure(config) as void;
  }

  encode(frame: VideoFrame, options: VideoEncoderEncodeOptions): void {
    return (this.native as Record<string, Function>).encode(frame, options) as void;
  }

  flush(): Promise<void> {
    return (this.native as Record<string, Function>).flush() as Promise<void>;
  }

  reset(): void {
    return (this.native as Record<string, Function>).reset() as void;
  }

  close(): void {
    return (this.native as Record<string, Function>).close() as void;
  }

  static isConfigSupported(config: VideoEncoderConfig): Promise<VideoEncoderSupport> {
    return bindings.VideoEncoder.isConfigSupported(config) as Promise<VideoEncoderSupport>;
  }
}
