/**
 * VideoDecoder - TypeScript wrapper for native VideoDecoder
 * @see spec/context/VideoDecoder.md
 */

import type { CodecState, EncodedVideoChunk, EventHandler, VideoDecoderConfig, VideoDecoderInit, VideoDecoderSupport } from '../types/webcodecs';

// eslint-disable-next-line @typescript-eslint/no-var-requires
const bindings = require('bindings')('webcodecs');

export class VideoDecoder {
  private readonly _native: unknown;

  constructor(init: VideoDecoderInit) {
    this._native = new bindings.VideoDecoder(init);
  }

  get state(): CodecState {
    return (this._native as Record<string, unknown>).state as CodecState;
  }

  get decodeQueueSize(): number {
    return (this._native as Record<string, unknown>).decodeQueueSize as number;
  }

  get ondequeue(): EventHandler {
    return (this._native as Record<string, unknown>).ondequeue as EventHandler;
  }

  set ondequeue(value: EventHandler) {
    (this._native as Record<string, unknown>).ondequeue = value;
  }


  configure(config: VideoDecoderConfig): void {
    return (this._native as Record<string, Function>).configure(config) as void;
  }

  decode(chunk: EncodedVideoChunk): void {
    return (this._native as Record<string, Function>).decode(chunk) as void;
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


  static isConfigSupported(config: VideoDecoderConfig): Promise<VideoDecoderSupport> {
    return bindings.VideoDecoder.isConfigSupported(config) as Promise<VideoDecoderSupport>;
  }
}
