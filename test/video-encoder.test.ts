// test/video-encoder.test.ts
import { describe, it, expect, beforeEach } from 'vitest';
import { VideoEncoder } from '@pproenca/node-webcodecs';

describe('VideoEncoder', () => {
  describe('Constructor', () => {
    it('should create a VideoEncoder instance', () => {
      const encoder = new VideoEncoder({
        output: () => {},
        error: () => {},
      });
      expect(encoder).toBeDefined();
      expect(encoder.state).toBe('unconfigured');
    });

    it('should require output callback', () => {
      expect(() => {
        // @ts-ignore - Testing missing required field
        new VideoEncoder({ error: () => {} });
      }).toThrow();
    });

    it('should require error callback', () => {
      expect(() => {
        // @ts-ignore - Testing missing required field
        new VideoEncoder({ output: () => {} });
      }).toThrow();
    });

    it('should require init object', () => {
      expect(() => {
        // @ts-ignore - Testing missing required field
        new VideoEncoder();
      }).toThrow();
    });
  });

  describe('State Machine', () => {
    let encoder: InstanceType<typeof VideoEncoder>;

    beforeEach(() => {
      encoder = new VideoEncoder({
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
      const encoder = new VideoEncoder({
        output: () => {},
        error: () => {},
      });
      encoder.close();
      expect(encoder.state).toBe('closed');
    });

    it('should be idempotent', () => {
      const encoder = new VideoEncoder({
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
      const encoder = new VideoEncoder({
        output: () => {},
        error: () => {},
      });
      encoder.close();
      expect(() => {
        encoder.configure({ codec: 'avc1.42E01E', width: 640, height: 480 });
      }).toThrow(/closed/i);
    });

    it('should throw on missing codec', () => {
      const encoder = new VideoEncoder({
        output: () => {},
        error: () => {},
      });
      expect(() => {
        // @ts-ignore - Testing missing required field
        encoder.configure({ width: 640, height: 480 });
      }).toThrow();
    });

    it('should throw on missing width', () => {
      const encoder = new VideoEncoder({
        output: () => {},
        error: () => {},
      });
      expect(() => {
        // @ts-ignore - Testing missing required field
        encoder.configure({ codec: 'avc1.42E01E', height: 480 });
      }).toThrow();
    });

    it('should throw on missing height', () => {
      const encoder = new VideoEncoder({
        output: () => {},
        error: () => {},
      });
      expect(() => {
        // @ts-ignore - Testing missing required field
        encoder.configure({ codec: 'avc1.42E01E', width: 640 });
      }).toThrow();
    });

    it('should throw on invalid dimensions', () => {
      const encoder = new VideoEncoder({
        output: () => {},
        error: () => {},
      });
      expect(() => {
        encoder.configure({ codec: 'avc1.42E01E', width: 0, height: 480 });
      }).toThrow();
      expect(() => {
        encoder.configure({ codec: 'avc1.42E01E', width: 640, height: 0 });
      }).toThrow();
      expect(() => {
        encoder.configure({ codec: 'avc1.42E01E', width: -1, height: 480 });
      }).toThrow();
    });

    it('should throw on unsupported codec', () => {
      const encoder = new VideoEncoder({
        output: () => {},
        error: () => {},
      });
      expect(() => {
        encoder.configure({ codec: 'not-a-real-codec', width: 640, height: 480 });
      }).toThrow(/unsupported|not supported/i);
    });

    it('should transition to configured state on valid config', () => {
      const encoder = new VideoEncoder({
        output: () => {},
        error: () => {},
      });
      encoder.configure({ codec: 'avc1.42E01E', width: 640, height: 480 });
      expect(encoder.state).toBe('configured');
    });

    it('should accept optional config fields', () => {
      const encoder = new VideoEncoder({
        output: () => {},
        error: () => {},
      });
      encoder.configure({
        codec: 'avc1.42E01E',
        width: 640,
        height: 480,
        displayWidth: 640,
        displayHeight: 480,
        bitrate: 1_000_000,
        framerate: 30,
        latencyMode: 'quality',
      });
      expect(encoder.state).toBe('configured');
    });
  });

  describe('encode()', () => {
    it('should throw on unconfigured encoder', () => {
      const encoder = new VideoEncoder({
        output: () => {},
        error: () => {},
      });
      expect(() => {
        // @ts-ignore - Testing with invalid frame
        encoder.encode({});
      }).toThrow(/unconfigured/i);
    });

    it('should throw on closed encoder', () => {
      const encoder = new VideoEncoder({
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
      const encoder = new VideoEncoder({
        output: () => {},
        error: () => {},
      });
      await expect(encoder.flush()).rejects.toThrow(/unconfigured/i);
    });

    it('should return a rejected promise on closed encoder', async () => {
      const encoder = new VideoEncoder({
        output: () => {},
        error: () => {},
      });
      encoder.close();
      await expect(encoder.flush()).rejects.toThrow(/closed/i);
    });

    it('should return a promise on configured encoder', async () => {
      const encoder = new VideoEncoder({
        output: () => {},
        error: () => {},
      });
      encoder.configure({ codec: 'avc1.42E01E', width: 640, height: 480 });
      const promise = encoder.flush();
      expect(promise).toBeInstanceOf(Promise);
      // Flush should resolve since no encode calls were made
      await expect(promise).resolves.toBeUndefined();
    });
  });

  describe('reset()', () => {
    it('should throw on closed encoder', () => {
      const encoder = new VideoEncoder({
        output: () => {},
        error: () => {},
      });
      encoder.close();
      expect(() => {
        encoder.reset();
      }).toThrow(/closed/i);
    });

    it('should transition from configured to unconfigured', () => {
      const encoder = new VideoEncoder({
        output: () => {},
        error: () => {},
      });
      encoder.configure({ codec: 'avc1.42E01E', width: 640, height: 480 });
      expect(encoder.state).toBe('configured');
      encoder.reset();
      expect(encoder.state).toBe('unconfigured');
    });

    it('should reset encodeQueueSize to 0', () => {
      const encoder = new VideoEncoder({
        output: () => {},
        error: () => {},
      });
      encoder.configure({ codec: 'avc1.42E01E', width: 640, height: 480 });
      encoder.reset();
      expect(encoder.encodeQueueSize).toBe(0);
    });
  });

  describe('isConfigSupported()', () => {
    it('should return a promise with supported=true for valid codec', async () => {
      const result = await VideoEncoder.isConfigSupported({
        codec: 'avc1.42E01E',
        width: 640,
        height: 480,
      });
      expect(result).toBeDefined();
      expect(result.supported).toBe(true);
      expect(result.config).toBeDefined();
      expect(result.config.codec).toBe('avc1.42E01E');
      expect(result.config.width).toBe(640);
      expect(result.config.height).toBe(480);
    });

    it('should return a promise with supported=false for invalid codec', async () => {
      const result = await VideoEncoder.isConfigSupported({
        codec: 'not-a-codec',
        width: 640,
        height: 480,
      });
      expect(result).toBeDefined();
      expect(result.supported).toBe(false);
    });

    it('should reject on missing codec', async () => {
      await expect(
        // @ts-ignore - Testing missing required field
        VideoEncoder.isConfigSupported({ width: 640, height: 480 })
      ).rejects.toThrow();
    });

    it('should reject on missing dimensions', async () => {
      await expect(
        // @ts-ignore - Testing missing required field
        VideoEncoder.isConfigSupported({ codec: 'avc1.42E01E' })
      ).rejects.toThrow();
    });

    it('should clone config with bitrate', async () => {
      const result = await VideoEncoder.isConfigSupported({
        codec: 'avc1.42E01E',
        width: 640,
        height: 480,
        bitrate: 1_000_000,
      });
      expect(result.config.bitrate).toBe(1_000_000);
    });

    it('should clone config with framerate', async () => {
      const result = await VideoEncoder.isConfigSupported({
        codec: 'avc1.42E01E',
        width: 640,
        height: 480,
        framerate: 30,
      });
      expect(result.config.framerate).toBe(30);
    });
  });

  describe('ondequeue', () => {
    it('should be settable and gettable', () => {
      const encoder = new VideoEncoder({
        output: () => {},
        error: () => {},
      });
      const callback = () => {};
      encoder.ondequeue = callback;
      expect(encoder.ondequeue).toBe(callback);
    });

    it('should be clearable with null', () => {
      const encoder = new VideoEncoder({
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
      const encoder = new VideoEncoder({
        output: () => {},
        error: () => {},
      });

      // Initial state
      expect(encoder.state).toBe('unconfigured');

      // Configure
      encoder.configure({ codec: 'avc1.42E01E', width: 640, height: 480 });
      expect(encoder.state).toBe('configured');

      // Flush (should resolve immediately since no encode calls)
      await encoder.flush();
      expect(encoder.state).toBe('configured');

      // Reset
      encoder.reset();
      expect(encoder.state).toBe('unconfigured');

      // Can reconfigure after reset
      encoder.configure({ codec: 'vp8', width: 1920, height: 1080 });
      expect(encoder.state).toBe('configured');

      // Close
      encoder.close();
      expect(encoder.state).toBe('closed');
    });

    it('should reject pending flush on reset', async () => {
      const encoder = new VideoEncoder({
        output: () => {},
        error: () => {},
      });

      encoder.configure({ codec: 'avc1.42E01E', width: 640, height: 480 });

      // Start flush
      const flushPromise = encoder.flush();

      // Immediately reset - this should reject the flush
      encoder.reset();

      // Flush should reject with AbortError
      await expect(flushPromise).rejects.toThrow(/abort/i);
    });
  });
});
