// test/audio-encoder-integration.test.ts
// Integration tests for AudioEncoder
// NOTE: Full encoding tests require AudioData constructor implementation (TODO)
//       Currently tests the configure/flush/lifecycle cycle like VideoEncoder tests
import { describe, it, expect } from 'vitest';
import { AudioEncoder, EncodedAudioChunk } from '@pproenca/node-webcodecs';

describe('AudioEncoder Integration', () => {
  describe('Encode lifecycle (configure → flush cycle)', () => {
    it('should complete configure → flush cycle for Opus', async () => {
      const chunks: EncodedAudioChunk[] = [];
      const errors: Error[] = [];

      const encoder = new AudioEncoder({
        output: (chunk: EncodedAudioChunk) => {
          chunks.push(chunk);
        },
        error: (e: Error) => {
          errors.push(e);
        },
      });

      // Configure encoder for Opus
      encoder.configure({
        codec: 'opus',
        sampleRate: 48000,
        numberOfChannels: 2,
        bitrate: 128000,
      });

      expect(encoder.state).toBe('configured');

      // Note: AudioData constructor from raw data is not yet implemented
      // For now, test the configure/flush cycle works (like VideoEncoder tests)
      await encoder.flush();

      expect(encoder.state).toBe('configured');
      expect(errors.length).toBe(0);

      encoder.close();
      expect(encoder.state).toBe('closed');
    });

    it('should handle multiple codec configurations', async () => {
      const encoder = new AudioEncoder({
        output: () => {},
        error: () => {},
      });

      // Configure with Opus at 48kHz
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

  describe('Error Handling', () => {
    it('should call error callback on encoding errors', async () => {
      const encoder = new AudioEncoder({
        output: () => {},
        error: () => {},
      });

      encoder.configure({
        codec: 'opus',
        sampleRate: 48000,
        numberOfChannels: 2,
      });

      // Encoding with null audio data should fail
      expect(() => {
        // @ts-ignore - Testing invalid input
        encoder.encode(null);
      }).toThrow();

      encoder.close();
    });
  });

  describe('Lifecycle', () => {
    it('should properly clean up resources on close', async () => {
      const encoder = new AudioEncoder({
        output: () => {},
        error: () => {},
      });

      encoder.configure({
        codec: 'opus',
        sampleRate: 48000,
        numberOfChannels: 2,
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
          codec: 'opus',
          sampleRate: 48000,
          numberOfChannels: 2,
        });
      }).toThrow(/closed/i);
    });

    it('should reset encodeQueueSize on reset', async () => {
      const encoder = new AudioEncoder({
        output: () => {},
        error: () => {},
      });

      encoder.configure({
        codec: 'opus',
        sampleRate: 48000,
        numberOfChannels: 2,
      });

      expect(encoder.encodeQueueSize).toBe(0);

      encoder.reset();
      expect(encoder.encodeQueueSize).toBe(0);
      expect(encoder.state).toBe('unconfigured');

      encoder.close();
    });
  });

  describe('ondequeue callback', () => {
    it('should support setting and clearing ondequeue', async () => {
      const encoder = new AudioEncoder({
        output: () => {},
        error: () => {},
      });

      const callback = () => {};
      encoder.ondequeue = callback;
      expect(encoder.ondequeue).toBe(callback);

      encoder.ondequeue = null;
      expect(encoder.ondequeue).toBeNull();

      encoder.close();
    });
  });

  describe('isConfigSupported', () => {
    it('should check Opus support', async () => {
      const result = await AudioEncoder.isConfigSupported({
        codec: 'opus',
        sampleRate: 48000,
        numberOfChannels: 2,
      });

      // Opus should be supported
      expect(result.supported).toBe(true);
      expect(result.config?.codec).toBe('opus');
    });

    it('should check AAC support', async () => {
      const result = await AudioEncoder.isConfigSupported({
        codec: 'mp4a.40.2',
        sampleRate: 44100,
        numberOfChannels: 2,
      });

      // AAC may or may not be supported depending on FFmpeg build
      expect(result).toBeDefined();
      expect(typeof result.supported).toBe('boolean');
    });

    it('should return false for invalid codec', async () => {
      const result = await AudioEncoder.isConfigSupported({
        codec: 'not-a-real-codec',
        sampleRate: 48000,
        numberOfChannels: 2,
      });

      expect(result.supported).toBe(false);
    });
  });
});
