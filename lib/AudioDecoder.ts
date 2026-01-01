/**
 * AudioDecoder - TypeScript wrapper for native AudioDecoder
 * @see spec/context/AudioDecoder.md
 */

import { createRequire } from 'node:module';
import type {
  AudioDecoderConfig,
  AudioDecoderInit,
  AudioDecoderSupport,
  CodecState,
  EncodedAudioChunk,
  EventHandler,
} from '../types/webcodecs.js';

const require = createRequire(import.meta.url);
const bindings = require('bindings')('webcodecs');

export class AudioDecoder {
  private readonly native: unknown;

  constructor(init: AudioDecoderInit) {
    this.native = new bindings.AudioDecoder(init);
  }

  get state(): CodecState {
    return (this.native as Record<string, unknown>).state as CodecState;
  }

  get decodeQueueSize(): number {
    return (this.native as Record<string, unknown>).decodeQueueSize as number;
  }

  get ondequeue(): EventHandler {
    return (this.native as Record<string, unknown>).ondequeue as EventHandler;
  }

  set ondequeue(value: EventHandler) {
    (this.native as Record<string, unknown>).ondequeue = value;
  }

  configure(config: AudioDecoderConfig): void {
    return (this.native as Record<string, Function>).configure(config) as void;
  }

  decode(chunk: EncodedAudioChunk): void {
    return (this.native as Record<string, Function>).decode(chunk) as void;
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

  static isConfigSupported(config: AudioDecoderConfig): Promise<AudioDecoderSupport> {
    return bindings.AudioDecoder.isConfigSupported(config) as Promise<AudioDecoderSupport>;
  }
}
