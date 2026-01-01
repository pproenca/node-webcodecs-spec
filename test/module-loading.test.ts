// test/module-loading.test.ts
import { describe, it, expect } from 'vitest';

describe('Module Loading', () => {
  it('should load the native module without errors', async () => {
    const module = await import('@pproenca/node-webcodecs');
    expect(module).toBeDefined();
  });

  it('should export VideoDecoder class', async () => {
    const { VideoDecoder } = await import('@pproenca/node-webcodecs');
    expect(VideoDecoder).toBeDefined();
    expect(typeof VideoDecoder).toBe('function');
  });

  it('should export VideoEncoder class', async () => {
    const { VideoEncoder } = await import('@pproenca/node-webcodecs');
    expect(VideoEncoder).toBeDefined();
    expect(typeof VideoEncoder).toBe('function');
  });

  it('should export AudioDecoder class', async () => {
    const { AudioDecoder } = await import('@pproenca/node-webcodecs');
    expect(AudioDecoder).toBeDefined();
    expect(typeof AudioDecoder).toBe('function');
  });

  it('should export AudioEncoder class', async () => {
    const { AudioEncoder } = await import('@pproenca/node-webcodecs');
    expect(AudioEncoder).toBeDefined();
    expect(typeof AudioEncoder).toBe('function');
  });

  it('should export VideoFrame class', async () => {
    const { VideoFrame } = await import('@pproenca/node-webcodecs');
    expect(VideoFrame).toBeDefined();
    expect(typeof VideoFrame).toBe('function');
  });

  it('should export AudioData class', async () => {
    const { AudioData } = await import('@pproenca/node-webcodecs');
    expect(AudioData).toBeDefined();
    expect(typeof AudioData).toBe('function');
  });

  it('should export EncodedVideoChunk class', async () => {
    const { EncodedVideoChunk } = await import('@pproenca/node-webcodecs');
    expect(EncodedVideoChunk).toBeDefined();
    expect(typeof EncodedVideoChunk).toBe('function');
  });

  it('should export EncodedAudioChunk class', async () => {
    const { EncodedAudioChunk } = await import('@pproenca/node-webcodecs');
    expect(EncodedAudioChunk).toBeDefined();
    expect(typeof EncodedAudioChunk).toBe('function');
  });

  it('should export ImageDecoder class', async () => {
    const { ImageDecoder } = await import('@pproenca/node-webcodecs');
    expect(ImageDecoder).toBeDefined();
    expect(typeof ImageDecoder).toBe('function');
  });
});
