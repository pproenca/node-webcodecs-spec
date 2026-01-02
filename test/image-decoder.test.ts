// test/image-decoder.test.ts
import { describe, it, expect } from 'vitest';
import { ImageDecoder } from '@pproenca/node-webcodecs';

describe('ImageDecoder', () => {
  describe('isTypeSupported()', () => {
    it('should return a Promise', () => {
      const result = ImageDecoder.isTypeSupported('image/jpeg');
      expect(result).toBeInstanceOf(Promise);
    });

    it('should support image/jpeg', async () => {
      const result = await ImageDecoder.isTypeSupported('image/jpeg');
      expect(result).toBe(true);
    });

    it('should support image/png', async () => {
      const result = await ImageDecoder.isTypeSupported('image/png');
      expect(result).toBe(true);
    });

    it('should support image/webp', async () => {
      const result = await ImageDecoder.isTypeSupported('image/webp');
      expect(result).toBe(true);
    });

    it('should support image/gif', async () => {
      const result = await ImageDecoder.isTypeSupported('image/gif');
      expect(result).toBe(true);
    });

    it('should support image/avif', async () => {
      const result = await ImageDecoder.isTypeSupported('image/avif');
      expect(result).toBe(true);
    });

    it('should support image/bmp', async () => {
      const result = await ImageDecoder.isTypeSupported('image/bmp');
      expect(result).toBe(true);
    });

    it('should not support unknown type', async () => {
      const result = await ImageDecoder.isTypeSupported('image/unknown');
      expect(result).toBe(false);
    });
  });

  describe('Constructor validation', () => {
    it('should require init object', () => {
      expect(() => {
        // @ts-ignore - Testing missing required field
        new ImageDecoder();
      }).toThrow();
    });

    it('should require data in init', () => {
      expect(() => {
        // @ts-ignore - Testing missing required field
        new ImageDecoder({ type: 'image/jpeg' });
      }).toThrow(/data/i);
    });

    it('should require type in init', () => {
      const data = new Uint8Array([0xff, 0xd8, 0xff, 0xe0]);
      expect(() => {
        // @ts-ignore - Testing missing required field
        new ImageDecoder({ data });
      }).toThrow(/type/i);
    });

    it('should throw for unsupported type', () => {
      const data = new Uint8Array([0x00, 0x00, 0x00, 0x00]);
      expect(() => {
        new ImageDecoder({ data, type: 'image/unsupported' });
      }).toThrow(/unsupported|not supported/i);
    });
  });

  describe('Constructor with valid data', () => {
    // Minimal valid JPEG header (SOI + APP0 markers)
    const minimalJpegData = new Uint8Array([
      0xff, 0xd8, // SOI
      0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, // APP0
      0xff, 0xd9  // EOI
    ]);

    it('should create instance with Uint8Array data', () => {
      // Note: This may fail at parsing stage due to incomplete JPEG data,
      // but construction itself should not segfault
      const decoder = new ImageDecoder({ data: minimalJpegData, type: 'image/jpeg' });
      expect(decoder).toBeDefined();
      expect(decoder.type).toBe('image/jpeg');
      expect(decoder.complete).toBe(false);
      decoder.close();
    });

    it('should create instance with ArrayBuffer data', () => {
      const buffer = minimalJpegData.buffer.slice(
        minimalJpegData.byteOffset,
        minimalJpegData.byteOffset + minimalJpegData.byteLength
      );
      const decoder = new ImageDecoder({ data: buffer, type: 'image/jpeg' });
      expect(decoder).toBeDefined();
      expect(decoder.type).toBe('image/jpeg');
      decoder.close();
    });

    it('should have tracks property', () => {
      const decoder = new ImageDecoder({ data: minimalJpegData, type: 'image/jpeg' });
      expect(decoder.tracks).toBeDefined();
      decoder.close();
    });

    it('should have completed property as Promise', () => {
      const decoder = new ImageDecoder({ data: minimalJpegData, type: 'image/jpeg' });
      expect(decoder.completed).toBeInstanceOf(Promise);
      decoder.close();
    });
  });

  describe('close()', () => {
    it('should be idempotent', () => {
      const data = new Uint8Array([0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0xff, 0xd9]);
      const decoder = new ImageDecoder({ data, type: 'image/jpeg' });
      decoder.close();
      // Should not throw when called again
      expect(() => decoder.close()).not.toThrow();
    });
  });

  describe('decode() after close', () => {
    it('should reject with InvalidStateError', async () => {
      const data = new Uint8Array([0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0xff, 0xd9]);
      const decoder = new ImageDecoder({ data, type: 'image/jpeg' });
      decoder.close();

      await expect(decoder.decode()).rejects.toThrow(/InvalidStateError|closed/i);
    });
  });

  describe('reset()', () => {
    it('should throw InvalidStateError when closed', () => {
      const data = new Uint8Array([0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0xff, 0xd9]);
      const decoder = new ImageDecoder({ data, type: 'image/jpeg' });
      decoder.close();

      expect(() => decoder.reset()).toThrow(/InvalidStateError|closed/i);
    });
  });

  // Note: Full constructor tests with actual image decoding are in integration tests
  // These unit tests just validate the API surface and error handling
});
