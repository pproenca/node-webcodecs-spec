// test/video-decoder.test.ts
import { describe, it, expect, beforeEach } from 'vitest';
import { VideoDecoder } from '@pproenca/node-webcodecs';

describe('VideoDecoder', () => {
  describe('Constructor', () => {
    it('should create a VideoDecoder instance', () => {
      const decoder = new VideoDecoder({
        output: () => {},
        error: () => {},
      });
      expect(decoder).toBeDefined();
      expect(decoder.state).toBe('unconfigured');
    });

    it('should require output callback', () => {
      expect(() => {
        // @ts-ignore - Testing missing required field
        new VideoDecoder({ error: () => {} });
      }).toThrow();
    });

    it('should require error callback', () => {
      expect(() => {
        // @ts-ignore - Testing missing required field
        new VideoDecoder({ output: () => {} });
      }).toThrow();
    });
  });

  describe('State Machine', () => {
    let decoder: InstanceType<typeof VideoDecoder>;

    beforeEach(() => {
      decoder = new VideoDecoder({
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
  });

  describe('close()', () => {
    it('should transition to closed state', () => {
      const decoder = new VideoDecoder({
        output: () => {},
        error: () => {},
      });
      decoder.close();
      expect(decoder.state).toBe('closed');
    });

    it('should be idempotent', () => {
      const decoder = new VideoDecoder({
        output: () => {},
        error: () => {},
      });
      decoder.close();
      decoder.close(); // Should not throw
      expect(decoder.state).toBe('closed');
    });
  });
});
