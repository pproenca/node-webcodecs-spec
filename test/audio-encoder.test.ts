// test/audio-encoder.test.ts
import { describe, it, expect, beforeEach } from 'vitest';
import { AudioEncoder } from '@pproenca/node-webcodecs';

describe('AudioEncoder', () => {
  describe('Constructor', () => {
    it('should create an AudioEncoder instance', () => {
      const encoder = new AudioEncoder({
        output: () => {},
        error: () => {},
      });
      expect(encoder).toBeDefined();
      expect(encoder.state).toBe('unconfigured');
    });

    it('should require output callback', () => {
      expect(() => {
        // @ts-ignore - Testing missing required field
        new AudioEncoder({ error: () => {} });
      }).toThrow();
    });

    it('should require error callback', () => {
      expect(() => {
        // @ts-ignore - Testing missing required field
        new AudioEncoder({ output: () => {} });
      }).toThrow();
    });

    it('should require init object', () => {
      expect(() => {
        // @ts-ignore - Testing missing required field
        new AudioEncoder();
      }).toThrow();
    });
  });

  describe('State Machine', () => {
    let encoder: InstanceType<typeof AudioEncoder>;

    beforeEach(() => {
      encoder = new AudioEncoder({
        output: () => {},
        error: () => {},
      });
    });

    it('should start in unconfigured state', () => {
      expect(encoder.state).toBe('unconfigured');
    });

    it('should have encodeQueueSize of 0 initially', () => {
      expect(encoder.encodeQueueSize).toBe(0);
    });

    it('should have null ondequeue initially', () => {
      expect(encoder.ondequeue).toBeNull();
    });
  });

  describe('close()', () => {
    it('should transition to closed state', () => {
      const encoder = new AudioEncoder({
        output: () => {},
        error: () => {},
      });
      encoder.close();
      expect(encoder.state).toBe('closed');
    });

    it('should be idempotent', () => {
      const encoder = new AudioEncoder({
        output: () => {},
        error: () => {},
      });
      encoder.close();
      encoder.close(); // Should not throw
      expect(encoder.state).toBe('closed');
    });
  });

  describe('configure()', () => {
    it('should throw on closed encoder', () => {
      const encoder = new AudioEncoder({
        output: () => {},
        error: () => {},
      });
      encoder.close();
      expect(() => {
        encoder.configure({
          codec: 'opus',
          sampleRate: 48000,
          numberOfChannels: 2,
        });
      }).toThrow(/closed/i);
    });

    it('should throw on missing codec', () => {
      const encoder = new AudioEncoder({
        output: () => {},
        error: () => {},
      });
      expect(() => {
        // @ts-ignore - Testing missing required field
        encoder.configure({ sampleRate: 48000, numberOfChannels: 2 });
      }).toThrow();
    });

    it('should throw on missing sampleRate', () => {
      const encoder = new AudioEncoder({
        output: () => {},
        error: () => {},
      });
      expect(() => {
        // @ts-ignore - Testing missing required field
        encoder.configure({ codec: 'opus', numberOfChannels: 2 });
      }).toThrow();
    });

    it('should throw on missing numberOfChannels', () => {
      const encoder = new AudioEncoder({
        output: () => {},
        error: () => {},
      });
      expect(() => {
        // @ts-ignore - Testing missing required field
        encoder.configure({ codec: 'opus', sampleRate: 48000 });
      }).toThrow();
    });

    it('should throw on invalid sampleRate', () => {
      const encoder = new AudioEncoder({
        output: () => {},
        error: () => {},
      });
      expect(() => {
        encoder.configure({
          codec: 'opus',
          sampleRate: 0,
          numberOfChannels: 2,
        });
      }).toThrow();
      expect(() => {
        encoder.configure({
          codec: 'opus',
          sampleRate: -1,
          numberOfChannels: 2,
        });
      }).toThrow();
    });

    it('should throw on invalid numberOfChannels', () => {
      const encoder = new AudioEncoder({
        output: () => {},
        error: () => {},
      });
      expect(() => {
        encoder.configure({
          codec: 'opus',
          sampleRate: 48000,
          numberOfChannels: 0,
        });
      }).toThrow();
      expect(() => {
        encoder.configure({
          codec: 'opus',
          sampleRate: 48000,
          numberOfChannels: -1,
        });
      }).toThrow();
    });

    it('should throw on unsupported codec', () => {
      const encoder = new AudioEncoder({
        output: () => {},
        error: () => {},
      });
      expect(() => {
        encoder.configure({
          codec: 'not-a-real-codec',
          sampleRate: 48000,
          numberOfChannels: 2,
        });
      }).toThrow(/unsupported|not supported/i);
    });

    it('should transition to configured state on valid config', () => {
      const encoder = new AudioEncoder({
        output: () => {},
        error: () => {},
      });
      encoder.configure({
        codec: 'opus',
        sampleRate: 48000,
        numberOfChannels: 2,
      });
      expect(encoder.state).toBe('configured');
    });

    it('should accept optional bitrate field', () => {
      const encoder = new AudioEncoder({
        output: () => {},
        error: () => {},
      });
      encoder.configure({
        codec: 'opus',
        sampleRate: 48000,
        numberOfChannels: 2,
        bitrate: 128000,
      });
      expect(encoder.state).toBe('configured');
    });

    it('should accept optional bitrateMode field', () => {
      const encoder = new AudioEncoder({
        output: () => {},
        error: () => {},
      });
      encoder.configure({
        codec: 'opus',
        sampleRate: 48000,
        numberOfChannels: 2,
        bitrateMode: 'variable',
      });
      expect(encoder.state).toBe('configured');
    });
  });

  describe('encode()', () => {
    it('should throw on unconfigured encoder', () => {
      const encoder = new AudioEncoder({
        output: () => {},
        error: () => {},
      });
      expect(() => {
        // @ts-ignore - Testing with invalid frame
        encoder.encode({});
      }).toThrow(/unconfigured/i);
    });

    it('should throw on closed encoder', () => {
      const encoder = new AudioEncoder({
        output: () => {},
        error: () => {},
      });
      encoder.close();
      expect(() => {
        // @ts-ignore - Testing with invalid frame
        encoder.encode({});
      }).toThrow(/closed/i);
    });
  });

  describe('flush()', () => {
    it('should return a rejected promise on unconfigured encoder', async () => {
      const encoder = new AudioEncoder({
        output: () => {},
        error: () => {},
      });
      await expect(encoder.flush()).rejects.toThrow(/unconfigured/i);
    });

    it('should return a rejected promise on closed encoder', async () => {
      const encoder = new AudioEncoder({
        output: () => {},
        error: () => {},
      });
      encoder.close();
      await expect(encoder.flush()).rejects.toThrow(/closed/i);
    });

    it('should return a promise on configured encoder', async () => {
      const encoder = new AudioEncoder({
        output: () => {},
        error: () => {},
      });
      encoder.configure({
        codec: 'opus',
        sampleRate: 48000,
        numberOfChannels: 2,
      });
      const promise = encoder.flush();
      expect(promise).toBeInstanceOf(Promise);
      // Flush should resolve since no encode calls were made
      await expect(promise).resolves.toBeUndefined();
    });
  });

  describe('reset()', () => {
    it('should throw on closed encoder', () => {
      const encoder = new AudioEncoder({
        output: () => {},
        error: () => {},
      });
      encoder.close();
      expect(() => {
        encoder.reset();
      }).toThrow(/closed/i);
    });

    it('should transition from configured to unconfigured', () => {
      const encoder = new AudioEncoder({
        output: () => {},
        error: () => {},
      });
      encoder.configure({
        codec: 'opus',
        sampleRate: 48000,
        numberOfChannels: 2,
      });
      expect(encoder.state).toBe('configured');
      encoder.reset();
      expect(encoder.state).toBe('unconfigured');
    });

    it('should reset encodeQueueSize to 0', () => {
      const encoder = new AudioEncoder({
        output: () => {},
        error: () => {},
      });
      encoder.configure({
        codec: 'opus',
        sampleRate: 48000,
        numberOfChannels: 2,
      });
      encoder.reset();
      expect(encoder.encodeQueueSize).toBe(0);
    });
  });

  describe('isConfigSupported()', () => {
    it('should return a promise with supported=true for Opus', async () => {
      const result = await AudioEncoder.isConfigSupported({
        codec: 'opus',
        sampleRate: 48000,
        numberOfChannels: 2,
      });
      expect(result).toBeDefined();
      expect(result.supported).toBe(true);
      expect(result.config).toBeDefined();
      expect(result.config.codec).toBe('opus');
      expect(result.config.sampleRate).toBe(48000);
      expect(result.config.numberOfChannels).toBe(2);
    });

    it('should return a promise with supported=false for invalid codec', async () => {
      const result = await AudioEncoder.isConfigSupported({
        codec: 'not-a-codec',
        sampleRate: 48000,
        numberOfChannels: 2,
      });
      expect(result).toBeDefined();
      expect(result.supported).toBe(false);
    });

    it('should reject on missing codec', async () => {
      await expect(
        // @ts-ignore - Testing missing required field
        AudioEncoder.isConfigSupported({ sampleRate: 48000, numberOfChannels: 2 })
      ).rejects.toThrow();
    });

    it('should reject on missing sampleRate', async () => {
      await expect(
        // @ts-ignore - Testing missing required field
        AudioEncoder.isConfigSupported({ codec: 'opus', numberOfChannels: 2 })
      ).rejects.toThrow();
    });

    it('should reject on missing numberOfChannels', async () => {
      await expect(
        // @ts-ignore - Testing missing required field
        AudioEncoder.isConfigSupported({ codec: 'opus', sampleRate: 48000 })
      ).rejects.toThrow();
    });

    it('should clone config with bitrate', async () => {
      const result = await AudioEncoder.isConfigSupported({
        codec: 'opus',
        sampleRate: 48000,
        numberOfChannels: 2,
        bitrate: 128000,
      });
      expect(result.config.bitrate).toBe(128000);
    });

    it('should clone config with bitrateMode', async () => {
      const result = await AudioEncoder.isConfigSupported({
        codec: 'opus',
        sampleRate: 48000,
        numberOfChannels: 2,
        bitrateMode: 'constant',
      });
      expect(result.config.bitrateMode).toBe('constant');
    });

    it('should check AAC support', async () => {
      const result = await AudioEncoder.isConfigSupported({
        codec: 'mp4a.40.2',
        sampleRate: 44100,
        numberOfChannels: 2,
      });
      expect(result).toBeDefined();
      expect(typeof result.supported).toBe('boolean');
    });

    it('should check FLAC support', async () => {
      const result = await AudioEncoder.isConfigSupported({
        codec: 'flac',
        sampleRate: 44100,
        numberOfChannels: 2,
      });
      expect(result).toBeDefined();
      expect(typeof result.supported).toBe('boolean');
    });

    it('should check MP3 support', async () => {
      const result = await AudioEncoder.isConfigSupported({
        codec: 'mp3',
        sampleRate: 44100,
        numberOfChannels: 2,
      });
      expect(result).toBeDefined();
      expect(typeof result.supported).toBe('boolean');
    });
  });

  describe('ondequeue', () => {
    it('should be settable and gettable', () => {
      const encoder = new AudioEncoder({
        output: () => {},
        error: () => {},
      });
      const callback = () => {};
      encoder.ondequeue = callback;
      expect(encoder.ondequeue).toBe(callback);
    });

    it('should be clearable with null', () => {
      const encoder = new AudioEncoder({
        output: () => {},
        error: () => {},
      });
      encoder.ondequeue = () => {};
      encoder.ondequeue = null;
      expect(encoder.ondequeue).toBeNull();
    });
  });

  describe('Integration: configure → flush → reset cycle', () => {
    it('should complete configure → flush → reset cycle', async () => {
      const encoder = new AudioEncoder({
        output: () => {},
        error: () => {},
      });

      // Initial state
      expect(encoder.state).toBe('unconfigured');

      // Configure
      encoder.configure({
        codec: 'opus',
        sampleRate: 48000,
        numberOfChannels: 2,
      });
      expect(encoder.state).toBe('configured');

      // Flush (should resolve immediately since no encode calls)
      await encoder.flush();
      expect(encoder.state).toBe('configured');

      // Reset
      encoder.reset();
      expect(encoder.state).toBe('unconfigured');

      // Can reconfigure after reset
      encoder.configure({
        codec: 'opus',
        sampleRate: 44100,
        numberOfChannels: 1,
      });
      expect(encoder.state).toBe('configured');

      // Close
      encoder.close();
      expect(encoder.state).toBe('closed');
    });

    it('should reject pending flush on reset', async () => {
      const encoder = new AudioEncoder({
        output: () => {},
        error: () => {},
      });

      encoder.configure({
        codec: 'opus',
        sampleRate: 48000,
        numberOfChannels: 2,
      });

      // Start flush
      const flushPromise = encoder.flush();

      // Immediately reset - this should reject the flush
      encoder.reset();

      // Flush should reject with AbortError
      await expect(flushPromise).rejects.toThrow(/abort/i);
    });
  });

  describe('Codec Reconfiguration', () => {
    it('should handle multiple codec configurations', async () => {
      const encoder = new AudioEncoder({
        output: () => {},
        error: () => {},
      });

      // Configure with Opus
      encoder.configure({
        codec: 'opus',
        sampleRate: 48000,
        numberOfChannels: 2,
      });
      expect(encoder.state).toBe('configured');

      // Reset and reconfigure with different sample rate
      encoder.reset();
      expect(encoder.state).toBe('unconfigured');

      encoder.configure({
        codec: 'opus',
        sampleRate: 24000,
        numberOfChannels: 1,
      });
      expect(encoder.state).toBe('configured');

      await encoder.flush();
      encoder.close();
    });
  });
});
