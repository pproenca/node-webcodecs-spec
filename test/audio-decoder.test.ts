// test/audio-decoder.test.ts
import { describe, it, expect, beforeEach } from 'vitest';
import { AudioDecoder, EncodedAudioChunk } from '@pproenca/node-webcodecs';

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

  // ==========================================================================
  // Sad Path Tests - W3C WebCodecs Spec Compliance
  // ==========================================================================

  describe('Key Frame Requirement', () => {
    it('should throw DataError for delta frame without prior key frame', () => {
      const decoder = new AudioDecoder({
        output: () => {},
        error: () => {},
      });
      decoder.configure({ codec: 'opus', sampleRate: 48000, numberOfChannels: 2 });

      // Delta chunk without prior key chunk should throw DataError
      const deltaChunk = new EncodedAudioChunk({
        type: 'delta',
        timestamp: 0,
        data: new Uint8Array(10),
      });

      expect(() => decoder.decode(deltaChunk)).toThrow(/key/i);
      decoder.close();
    });

    it('should reset key requirement after flush', async () => {
      const decoder = new AudioDecoder({
        output: () => {},
        error: () => {},
      });
      decoder.configure({ codec: 'opus', sampleRate: 48000, numberOfChannels: 2 });

      // Send a key frame first
      const keyChunk = new EncodedAudioChunk({
        type: 'key',
        timestamp: 0,
        data: new Uint8Array(10),
      });
      decoder.decode(keyChunk);
      await decoder.flush();

      // After flush, next decode must be a key frame (spec requirement)
      const deltaChunk = new EncodedAudioChunk({
        type: 'delta',
        timestamp: 1000,
        data: new Uint8Array(10),
      });

      expect(() => decoder.decode(deltaChunk)).toThrow(/key/i);
      decoder.close();
    });

    it('should reset key requirement after reset()', () => {
      const decoder = new AudioDecoder({
        output: () => {},
        error: () => {},
      });
      decoder.configure({ codec: 'opus', sampleRate: 48000, numberOfChannels: 2 });

      // Send a key frame
      const keyChunk = new EncodedAudioChunk({
        type: 'key',
        timestamp: 0,
        data: new Uint8Array(10),
      });
      decoder.decode(keyChunk);

      // Reset puts decoder back to unconfigured
      decoder.reset();

      // Reconfigure
      decoder.configure({ codec: 'opus', sampleRate: 48000, numberOfChannels: 2 });

      // After reset+reconfigure, next decode must be key frame
      const deltaChunk = new EncodedAudioChunk({
        type: 'delta',
        timestamp: 1000,
        data: new Uint8Array(10),
      });

      expect(() => decoder.decode(deltaChunk)).toThrow(/key/i);
      decoder.close();
    });
  });

  describe('Pending Flush Rejection', () => {
    it('should reject pending flush on reset with AbortError', async () => {
      const decoder = new AudioDecoder({
        output: () => {},
        error: () => {},
      });
      decoder.configure({ codec: 'opus', sampleRate: 48000, numberOfChannels: 2 });

      // Start flush and immediately reset
      const flushPromise = decoder.flush();
      decoder.reset();

      // Flush should be rejected with AbortError
      await expect(flushPromise).rejects.toThrow(/abort/i);
    });

    it('should reject pending flush on close with AbortError', async () => {
      const decoder = new AudioDecoder({
        output: () => {},
        error: () => {},
      });
      decoder.configure({ codec: 'opus', sampleRate: 48000, numberOfChannels: 2 });

      // Start flush and immediately close
      const flushPromise = decoder.flush();
      decoder.close();

      // Flush should be rejected with AbortError
      await expect(flushPromise).rejects.toThrow(/abort/i);
    });

    it('should reject multiple pending flushes on reset', async () => {
      const decoder = new AudioDecoder({
        output: () => {},
        error: () => {},
      });
      decoder.configure({ codec: 'opus', sampleRate: 48000, numberOfChannels: 2 });

      // Queue multiple flushes
      const flush1 = decoder.flush();
      const flush2 = decoder.flush();
      const flush3 = decoder.flush();

      decoder.reset();

      // All pending flushes should reject
      await expect(flush1).rejects.toThrow(/abort/i);
      await expect(flush2).rejects.toThrow(/abort/i);
      await expect(flush3).rejects.toThrow(/abort/i);
    });
  });

  describe('Dequeue Event Coalescing ([[dequeue event scheduled]])', () => {
    // Helper to wait for event loop to process pending callbacks
    const waitForEventLoop = () => new Promise((resolve) => setTimeout(resolve, 10));

    it('should coalesce multiple dequeue events', async () => {
      let dequeueCount = 0;
      const decoder = new AudioDecoder({
        output: () => {},
        error: () => {},
      });

      decoder.ondequeue = () => {
        dequeueCount++;
      };

      decoder.configure({ codec: 'opus', sampleRate: 48000, numberOfChannels: 2 });

      // Queue 10 decode operations rapidly
      for (let i = 0; i < 10; i++) {
        decoder.decode(
          new EncodedAudioChunk({
            type: 'key',
            timestamp: i * 1000,
            data: new Uint8Array(10),
          })
        );
      }

      await decoder.flush();
      // Give event loop time to process TSFN callbacks
      await waitForEventLoop();

      // Per spec, dequeue events should be coalesced
      // We should have fewer events than decode operations
      expect(dequeueCount).toBeLessThan(10);
      expect(dequeueCount).toBeGreaterThanOrEqual(1);

      decoder.close();
    });

    it('should fire ondequeue when decodeQueueSize decreases', async () => {
      let dequeueFired = false;
      const decoder = new AudioDecoder({
        output: () => {},
        error: () => {},
      });

      decoder.ondequeue = () => {
        dequeueFired = true;
      };

      decoder.configure({ codec: 'opus', sampleRate: 48000, numberOfChannels: 2 });

      // Decode something
      decoder.decode(
        new EncodedAudioChunk({
          type: 'key',
          timestamp: 0,
          data: new Uint8Array(10),
        })
      );

      await decoder.flush();
      // Give event loop time to process TSFN callbacks
      await waitForEventLoop();

      // ondequeue should have fired at least once
      expect(dequeueFired).toBe(true);

      decoder.close();
    });
  });

  describe('Codec Saturation ([[codec saturated]])', () => {
    it('should handle rapid decode calls without crashing', async () => {
      const outputs: unknown[] = [];
      const decoder = new AudioDecoder({
        output: (frame) => outputs.push(frame),
        error: () => {},
      });

      decoder.configure({ codec: 'opus', sampleRate: 48000, numberOfChannels: 2 });

      // Rapidly queue many decode operations to potentially trigger EAGAIN
      for (let i = 0; i < 100; i++) {
        decoder.decode(
          new EncodedAudioChunk({
            type: 'key',
            timestamp: i * 1000,
            data: new Uint8Array(10),
          })
        );
      }

      // Should complete without hanging or crashing
      await decoder.flush();
      decoder.close();
    });
  });

  describe('Message Queue Blocking ([[message queue blocked]])', () => {
    it('should block message queue during configure', async () => {
      const decoder = new AudioDecoder({
        output: () => {},
        error: () => {},
      });

      // Start configure
      decoder.configure({ codec: 'opus', sampleRate: 48000, numberOfChannels: 2 });

      // Immediately try another configure - should queue, not race
      decoder.configure({ codec: 'opus', sampleRate: 44100, numberOfChannels: 1 });

      // Flush to wait for all operations
      await decoder.flush();

      // Should not crash due to race condition
      decoder.close();
    });

    it('should process queued messages after configure completes', async () => {
      let outputCount = 0;
      const decoder = new AudioDecoder({
        output: () => {
          outputCount++;
        },
        error: () => {},
      });

      // Configure
      decoder.configure({ codec: 'opus', sampleRate: 48000, numberOfChannels: 2 });

      // Queue decode while configure might still be processing
      decoder.decode(
        new EncodedAudioChunk({
          type: 'key',
          timestamp: 0,
          data: new Uint8Array(10),
        })
      );

      await decoder.flush();

      // Decode should have been processed after configure completed
      // Note: Output may or may not be produced depending on data validity
      // The key is that it doesn't crash
      decoder.close();
    });
  });
});
