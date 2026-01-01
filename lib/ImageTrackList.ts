/**
 * ImageTrackList - TypeScript wrapper for native ImageTrackList
 * @see spec/context/ImageTrackList.md
 */

import { createRequire } from 'node:module';
import type { ImageTrack } from '../types/webcodecs.js';

const require = createRequire(import.meta.url);
const bindings = require('bindings')('webcodecs');

export class ImageTrackList {
  private readonly native: unknown;

  constructor(init: unknown) {
    this.native = new bindings.ImageTrackList(init);
  }

  get ready(): Promise<void> {
    return (this.native as Record<string, unknown>).ready as Promise<void>;
  }

  get length(): number {
    return (this.native as Record<string, unknown>).length as number;
  }

  get selectedIndex(): number {
    return (this.native as Record<string, unknown>).selectedIndex as number;
  }

  get selectedTrack(): ImageTrack | null {
    return (this.native as Record<string, unknown>).selectedTrack as ImageTrack | null;
  }
}
