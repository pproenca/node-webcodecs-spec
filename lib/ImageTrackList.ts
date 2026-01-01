/**
 * ImageTrackList - TypeScript wrapper for native ImageTrackList
 * @see spec/context/ImageTrackList.md
 */

import type { ImageTrack } from '../types/webcodecs';

// eslint-disable-next-line @typescript-eslint/no-var-requires
const bindings = require('bindings')('webcodecs');

export class ImageTrackList {
  private readonly _native: unknown;

  constructor(init: unknown) {
    this._native = new bindings.ImageTrackList(init);
  }

  get ready(): Promise<void> {
    return (this._native as Record<string, unknown>).ready as Promise<void>;
  }

  get length(): number {
    return (this._native as Record<string, unknown>).length as number;
  }

  get selectedIndex(): number {
    return (this._native as Record<string, unknown>).selectedIndex as number;
  }

  get selectedTrack(): ImageTrack | null {
    return (this._native as Record<string, unknown>).selectedTrack as ImageTrack | null;
  }


}
