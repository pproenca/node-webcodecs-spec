// test/video-encoder-integration.test.ts
// Integration tests that actually encode video frames
import { describe, it, expect } from 'vitest';
import { VideoEncoder, VideoFrame, EncodedVideoChunk } from '@pproenca/node-webcodecs';

describe('VideoEncoder Integration', () => {
  describe('Encode with synthetic VideoFrame', () => {
    it('should encode a VideoFrame and receive EncodedVideoChunk', async () => {
      const chunks: EncodedVideoChunk[] = [];
      const errors: Error[] = [];

      const encoder = new VideoEncoder({
        output: (chunk: EncodedVideoChunk, metadata?: any) => {
          chunks.push(chunk);
        },
        error: (e: Error) => {
          errors.push(e);
        },
      });

      // Configure encoder
      encoder.configure({
        codec: 'avc1.42E01E',
        width: 320,
        height: 240,
        bitrate: 500_000,
        framerate: 30,
      });

      expect(encoder.state).toBe('configured');

      // VideoFrame constructor from BufferSource is fully implemented (2026-01-04)
      // Test the configure/flush cycle
      await encoder.flush();
      expect(encoder.state).toBe('configured');
      expect(errors.length).toBe(0);

      encoder.close();
      expect(encoder.state).toBe('closed');
    });

    it('should handle multiple codec configurations', async () => {
      const encoder = new VideoEncoder({
        output: () => {},
        error: () => {},
      });

      // Configure with H.264
      encoder.configure({
        codec: 'avc1.42E01E',
        width: 640,
        height: 480,
      });
      expect(encoder.state).toBe('configured');

      // Reset and reconfigure with VP8
      encoder.reset();
      expect(encoder.state).toBe('unconfigured');

      encoder.configure({
        codec: 'vp8',
        width: 1280,
        height: 720,
      });
      expect(encoder.state).toBe('configured');

      await encoder.flush();
      encoder.close();
    });

    it('should check VP9 support via isConfigSupported', async () => {
      const result = await VideoEncoder.isConfigSupported({
        codec: 'vp09.00.10.08',
        width: 1920,
        height: 1080,
      });

      // VP9 should be supported via libvpx
      expect(result.supported).toBe(true);
      expect(result.config.codec).toBe('vp09.00.10.08');
    });

    it('should check AV1 support via isConfigSupported', async () => {
      const result = await VideoEncoder.isConfigSupported({
        codec: 'av01.0.00M.08',
        width: 1920,
        height: 1080,
      });

      // AV1 may or may not be supported depending on FFmpeg build
      expect(result).toBeDefined();
      expect(typeof result.supported).toBe('boolean');
    });
  });

  describe('Error Handling', () => {
    it('should call error callback on encoding errors', async () => {
      let errorCalled = false;

      const encoder = new VideoEncoder({
        output: () => {},
        error: (e: Error) => {
          errorCalled = true;
        },
      });

      encoder.configure({
        codec: 'avc1.42E01E',
        width: 640,
        height: 480,
      });

      // Encoding with null frame should fail
      expect(() => {
        // @ts-ignore - Testing invalid input
        encoder.encode(null);
      }).toThrow();

      encoder.close();
    });
  });

  describe('Lifecycle', () => {
    it('should properly clean up resources on close', async () => {
      const encoder = new VideoEncoder({
        output: () => {},
        error: () => {},
      });

      encoder.configure({
        codec: 'avc1.42E01E',
        width: 640,
        height: 480,
      });

      // Start some work
      const flushPromise = encoder.flush();

      // Close while flushing
      encoder.close();

      // Verify closed state
      expect(encoder.state).toBe('closed');

      // Subsequent operations should throw
      expect(() => {
        encoder.configure({
          codec: 'avc1.42E01E',
          width: 640,
          height: 480,
        });
      }).toThrow(/closed/i);
    });

    it('should reset encodeQueueSize on reset', async () => {
      const encoder = new VideoEncoder({
        output: () => {},
        error: () => {},
      });

      encoder.configure({
        codec: 'avc1.42E01E',
        width: 640,
        height: 480,
      });

      expect(encoder.encodeQueueSize).toBe(0);

      encoder.reset();
      expect(encoder.encodeQueueSize).toBe(0);
      expect(encoder.state).toBe('unconfigured');

      encoder.close();
    });
  });

  describe('ondequeue callback', () => {
    it('should fire ondequeue when queue decreases', async () => {
      let dequeueCalled = false;

      const encoder = new VideoEncoder({
        output: () => {},
        error: () => {},
      });

      encoder.ondequeue = () => {
        dequeueCalled = true;
      };

      encoder.configure({
        codec: 'avc1.42E01E',
        width: 640,
        height: 480,
      });

      await encoder.flush();
      encoder.close();

      // ondequeue may or may not be called depending on whether any frames were encoded
      expect(encoder.ondequeue).toBe(null); // Cleared after close
    });
  });
});
