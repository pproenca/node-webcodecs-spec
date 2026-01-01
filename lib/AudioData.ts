/**
 * AudioData - TypeScript wrapper for native AudioData
 * @see spec/context/AudioData.md
 */

import { createRequire } from 'node:module';
import type {
  AllowSharedBufferSource,
  AudioDataCopyToOptions,
  AudioDataInit,
  AudioSampleFormat,
} from '../types/webcodecs.js';

const require = createRequire(import.meta.url);
const bindings = require('bindings')('webcodecs');

export class AudioData {
  private readonly native: unknown;

  constructor(init: AudioDataInit) {
    this.native = new bindings.AudioData(init);
  }

  get format(): AudioSampleFormat | null {
    return (this.native as Record<string, unknown>).format as AudioSampleFormat | null;
  }

  get sampleRate(): number {
    return (this.native as Record<string, unknown>).sampleRate as number;
  }

  get numberOfFrames(): number {
    return (this.native as Record<string, unknown>).numberOfFrames as number;
  }

  get numberOfChannels(): number {
    return (this.native as Record<string, unknown>).numberOfChannels as number;
  }

  get duration(): number {
    return (this.native as Record<string, unknown>).duration as number;
  }

  get timestamp(): number {
    return (this.native as Record<string, unknown>).timestamp as number;
  }

  allocationSize(options: AudioDataCopyToOptions): number {
    return (this.native as Record<string, Function>).allocationSize(options) as number;
  }

  copyTo(destination: AllowSharedBufferSource, options: AudioDataCopyToOptions): void {
    return (this.native as Record<string, Function>).copyTo(destination, options) as void;
  }

  clone(): AudioData {
    return (this.native as Record<string, Function>).clone() as AudioData;
  }

  close(): void {
    return (this.native as Record<string, Function>).close() as void;
  }
}
