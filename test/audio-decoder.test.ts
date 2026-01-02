// test/audio-decoder.test.ts
import { describe, it, expect, beforeEach } from 'vitest';
import { AudioDecoder } from '@pproenca/node-webcodecs';

describe('AudioDecoder', () => {
  describe('Constructor', () => {
    it('should create an AudioDecoder instance', () => {
      const decoder = new AudioDecoder({
        output: () => {},
        error: () => {},
      });
      expect(decoder).toBeDefined();
      expect(decoder.state).toBe('unconfigured');
    });

    it('should require output callback', () => {
      expect(() => {
        // @ts-ignore - Testing missing required field
        new AudioDecoder({ error: () => {} });
      }).toThrow();
    });

    it('should require error callback', () => {
      expect(() => {
        // @ts-ignore - Testing missing required field
        new AudioDecoder({ output: () => {} });
      }).toThrow();
    });

    it('should require init object', () => {
      expect(() => {
        // @ts-ignore - Testing missing required field
        new AudioDecoder();
      }).toThrow();
    });
  });

  describe('State Machine', () => {
    let decoder: InstanceType<typeof AudioDecoder>;

    beforeEach(() => {
      decoder = new AudioDecoder({
        output: () => {},
        error: () => {},
      });
    });

    it('should start in unconfigured state', () => {
      expect(decoder.state).toBe('unconfigured');
    });

    it('should have decodeQueueSize of 0 initially', () => {
      expect(decoder.decodeQueueSize).toBe(0);
    });

    it('should have null ondequeue initially', () => {
      expect(decoder.ondequeue).toBeNull();
    });
  });

  describe('close()', () => {
    it('should transition to closed state', () => {
      const decoder = new AudioDecoder({
        output: () => {},
        error: () => {},
      });
      decoder.close();
      expect(decoder.state).toBe('closed');
    });

    it('should be idempotent', () => {
      const decoder = new AudioDecoder({
        output: () => {},
        error: () => {},
      });
      decoder.close();
      decoder.close(); // Should not throw
      expect(decoder.state).toBe('closed');
    });
  });

  describe('configure()', () => {
    it('should throw on closed decoder', () => {
      const decoder = new AudioDecoder({
        output: () => {},
        error: () => {},
      });
      decoder.close();
      expect(() => {
        decoder.configure({ codec: 'opus' });
      }).toThrow(/closed/i);
    });

    it('should throw on missing codec', () => {
      const decoder = new AudioDecoder({
        output: () => {},
        error: () => {},
      });
      expect(() => {
        // @ts-ignore - Testing missing required field
        decoder.configure({});
      }).toThrow();
    });

    it('should throw on unsupported codec', () => {
      const decoder = new AudioDecoder({
        output: () => {},
        error: () => {},
      });
      expect(() => {
        decoder.configure({ codec: 'not-a-real-codec' });
      }).toThrow(/unsupported|not supported/i);
    });
  });

  describe('decode()', () => {
    it('should throw on unconfigured decoder', () => {
      const decoder = new AudioDecoder({
        output: () => {},
        error: () => {},
      });
      expect(() => {
        decoder.decode({ type: 'key', timestamp: 0, data: new Uint8Array(10) });
      }).toThrow(/unconfigured/i);
    });

    it('should throw on closed decoder', () => {
      const decoder = new AudioDecoder({
        output: () => {},
        error: () => {},
      });
      decoder.close();
      expect(() => {
        decoder.decode({ type: 'key', timestamp: 0, data: new Uint8Array(10) });
      }).toThrow(/closed/i);
    });
  });

  describe('flush()', () => {
    it('should return a rejected promise on unconfigured decoder', async () => {
      const decoder = new AudioDecoder({
        output: () => {},
        error: () => {},
      });
      await expect(decoder.flush()).rejects.toThrow(/unconfigured/i);
    });

    it('should return a rejected promise on closed decoder', async () => {
      const decoder = new AudioDecoder({
        output: () => {},
        error: () => {},
      });
      decoder.close();
      await expect(decoder.flush()).rejects.toThrow(/closed/i);
    });
  });

  describe('reset()', () => {
    it('should throw on closed decoder', () => {
      const decoder = new AudioDecoder({
        output: () => {},
        error: () => {},
      });
      decoder.close();
      expect(() => {
        decoder.reset();
      }).toThrow(/closed/i);
    });
  });

  describe('isConfigSupported()', () => {
    it('should return a promise with supported=true for valid codec', async () => {
      const result = await AudioDecoder.isConfigSupported({ codec: 'opus' });
      expect(result).toBeDefined();
      expect(result.supported).toBe(true);
      expect(result.config).toBeDefined();
      expect(result.config.codec).toBe('opus');
    });

    it('should return a promise with supported=false for invalid codec', async () => {
      const result = await AudioDecoder.isConfigSupported({ codec: 'not-a-codec' });
      expect(result).toBeDefined();
      expect(result.supported).toBe(false);
    });

    it('should reject on missing codec', async () => {
      await expect(
        // @ts-ignore - Testing missing required field
        AudioDecoder.isConfigSupported({})
      ).rejects.toThrow();
    });

    it('should clone config with sampleRate', async () => {
      const result = await AudioDecoder.isConfigSupported({
        codec: 'opus',
        sampleRate: 48000,
      });
      expect(result.config.sampleRate).toBe(48000);
    });

    it('should clone config with numberOfChannels', async () => {
      const result = await AudioDecoder.isConfigSupported({
        codec: 'opus',
        numberOfChannels: 2,
      });
      expect(result.config.numberOfChannels).toBe(2);
    });
  });

  describe('ondequeue', () => {
    it('should be settable and gettable', () => {
      const decoder = new AudioDecoder({
        output: () => {},
        error: () => {},
      });
      const callback = () => {};
      decoder.ondequeue = callback;
      expect(decoder.ondequeue).toBe(callback);
    });

    it('should be clearable with null', () => {
      const decoder = new AudioDecoder({
        output: () => {},
        error: () => {},
      });
      decoder.ondequeue = () => {};
      decoder.ondequeue = null;
      expect(decoder.ondequeue).toBeNull();
    });
  });
});
