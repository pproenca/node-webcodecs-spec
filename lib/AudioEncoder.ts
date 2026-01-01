/**
 * AudioEncoder - TypeScript wrapper for native AudioEncoder
 * @see spec/context/AudioEncoder.md
 */

import type { AudioData, AudioEncoderConfig, AudioEncoderInit, AudioEncoderSupport, CodecState, EventHandler } from '../types/webcodecs';

// eslint-disable-next-line @typescript-eslint/no-var-requires
const bindings = require('bindings')('webcodecs');

export class AudioEncoder {
  private readonly _native: unknown;

  constructor(init: AudioEncoderInit) {
    this._native = new bindings.AudioEncoder(init);
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


  configure(config: AudioEncoderConfig): void {
    return (this._native as Record<string, Function>).configure(config) as void;
  }

  encode(data: AudioData): void {
    return (this._native as Record<string, Function>).encode(data) as void;
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


  static isConfigSupported(config: AudioEncoderConfig): Promise<AudioEncoderSupport> {
    return bindings.AudioEncoder.isConfigSupported(config) as Promise<AudioEncoderSupport>;
  }
}
