/**
 * VideoEncoder - TypeScript wrapper for native VideoEncoder
 * @see spec/context/VideoEncoder.md
 */

import type { CodecState, EventHandler, VideoEncoderConfig, VideoEncoderEncodeOptions, VideoEncoderInit, VideoEncoderSupport, VideoFrame } from '../types/webcodecs';

// eslint-disable-next-line @typescript-eslint/no-var-requires
const bindings = require('bindings')('webcodecs');

export class VideoEncoder {
  private readonly _native: unknown;

  constructor(init: VideoEncoderInit) {
    this._native = new bindings.VideoEncoder(init);
  }

  get state(): CodecState {
    return (this._native as Record<string, unknown>).state as CodecState;
  }

  get encodeQueueSize(): number {
    return (this._native as Record<string, unknown>).encodeQueueSize as number;
  }

  get ondequeue(): EventHandler {
    return (this._native as Record<string, unknown>).ondequeue as EventHandler;
  }

  set ondequeue(value: EventHandler) {
    (this._native as Record<string, unknown>).ondequeue = value;
  }


  configure(config: VideoEncoderConfig): void {
    return (this._native as Record<string, Function>).configure(config) as void;
  }

  encode(frame: VideoFrame, options: VideoEncoderEncodeOptions): void {
    return (this._native as Record<string, Function>).encode(frame, options) as void;
  }

  flush(): Promise<void> {
    return (this._native as Record<string, Function>).flush() as Promise<void>;
  }

  reset(): void {
    return (this._native as Record<string, Function>).reset() as void;
  }

  close(): void {
    return (this._native as Record<string, Function>).close() as void;
  }


  static isConfigSupported(config: VideoEncoderConfig): Promise<VideoEncoderSupport> {
    return bindings.VideoEncoder.isConfigSupported(config) as Promise<VideoEncoderSupport>;
  }
}
