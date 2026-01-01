/**
 * AudioData - TypeScript wrapper for native AudioData
 * @see spec/context/AudioData.md
 */

import type { AllowSharedBufferSource, AudioDataCopyToOptions, AudioDataInit, AudioSampleFormat } from '../types/webcodecs';

// eslint-disable-next-line @typescript-eslint/no-var-requires
const bindings = require('bindings')('webcodecs');

export class AudioData {
  private readonly _native: unknown;

  constructor(init: AudioDataInit) {
    this._native = new bindings.AudioData(init);
  }

  get format(): AudioSampleFormat | null {
    return (this._native as Record<string, unknown>).format as AudioSampleFormat | null;
  }

  get sampleRate(): number {
    return (this._native as Record<string, unknown>).sampleRate as number;
  }

  get numberOfFrames(): number {
    return (this._native as Record<string, unknown>).numberOfFrames as number;
  }

  get numberOfChannels(): number {
    return (this._native as Record<string, unknown>).numberOfChannels as number;
  }

  get duration(): number {
    return (this._native as Record<string, unknown>).duration as number;
  }

  get timestamp(): number {
    return (this._native as Record<string, unknown>).timestamp as number;
  }


  allocationSize(options: AudioDataCopyToOptions): number {
    return (this._native as Record<string, Function>).allocationSize(options) as number;
  }

  copyTo(destination: AllowSharedBufferSource, options: AudioDataCopyToOptions): void {
    return (this._native as Record<string, Function>).copyTo(destination, options) as void;
  }

  clone(): AudioData {
    return (this._native as Record<string, Function>).clone() as AudioData;
  }

  close(): void {
    return (this._native as Record<string, Function>).close() as void;
  }

}
