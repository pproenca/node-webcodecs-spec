// test/audio-data.test.ts
import { describe, it, expect } from 'vitest';
import { AudioData } from '@pproenca/node-webcodecs';

describe('AudioData', () => {
  // Helper to create test audio data
  function createTestAudioData(options: {
    format?: string;
    sampleRate?: number;
    numberOfFrames?: number;
    numberOfChannels?: number;
    timestamp?: number;
  } = {}) {
    const format = options.format || 'f32';
    const sampleRate = options.sampleRate || 48000;
    const numberOfFrames = options.numberOfFrames || 1024;
    const numberOfChannels = options.numberOfChannels || 2;
    const timestamp = options.timestamp || 0;

    // Calculate bytes per sample
    const bytesPerSample = format.startsWith('f32') ? 4 : format.startsWith('s32') ? 4 : format.startsWith('s16') ? 2 : 1;
    const isPlanar = format.includes('-planar');

    // Create test data with a simple pattern
    const totalSamples = numberOfFrames * numberOfChannels;
    const dataSize = totalSamples * bytesPerSample;
    const data = new ArrayBuffer(dataSize);
    const view = new DataView(data);

    // Fill with test pattern
    if (format === 'f32' || format === 'f32-planar') {
      const floatView = new Float32Array(data);
      for (let i = 0; i < totalSamples; i++) {
        floatView[i] = Math.sin((i / totalSamples) * Math.PI * 2) * 0.5;
      }
    } else if (format === 's16' || format === 's16-planar') {
      const intView = new Int16Array(data);
      for (let i = 0; i < totalSamples; i++) {
        intView[i] = Math.floor(Math.sin((i / totalSamples) * Math.PI * 2) * 16000);
      }
    }

    return new AudioData({
      format: format as any,
      sampleRate,
      numberOfFrames,
      numberOfChannels,
      timestamp,
      data,
    });
  }

  describe('Constructor', () => {
    it('should create AudioData with f32 format', () => {
      const audioData = createTestAudioData({ format: 'f32' });
      expect(audioData).toBeDefined();
      expect(audioData.format).toBe('f32');
      expect(audioData.sampleRate).toBe(48000);
      expect(audioData.numberOfFrames).toBe(1024);
      expect(audioData.numberOfChannels).toBe(2);
      audioData.close();
    });

    it('should create AudioData with s16 format', () => {
      const audioData = createTestAudioData({ format: 's16' });
      expect(audioData).toBeDefined();
      expect(audioData.format).toBe('s16');
      audioData.close();
    });

    it('should create AudioData with planar format', () => {
      const audioData = createTestAudioData({ format: 'f32-planar' });
      expect(audioData).toBeDefined();
      expect(audioData.format).toBe('f32-planar');
      audioData.close();
    });

    it('should calculate duration correctly', () => {
      const audioData = createTestAudioData({
        sampleRate: 48000,
        numberOfFrames: 48000, // 1 second
      });
      // Duration should be 1_000_000 microseconds (1 second)
      expect(audioData.duration).toBeCloseTo(1000000, -2);
      audioData.close();
    });
  });

  describe('allocationSize()', () => {
    it('should return correct size for interleaved format', () => {
      const audioData = createTestAudioData({
        format: 'f32',
        numberOfFrames: 100,
        numberOfChannels: 2,
      });
      // f32 = 4 bytes, 100 frames, 2 channels = 800 bytes
      const size = audioData.allocationSize({ planeIndex: 0 });
      expect(size).toBe(800);
      audioData.close();
    });

    it('should return correct size for planar format', () => {
      const audioData = createTestAudioData({
        format: 'f32-planar',
        numberOfFrames: 100,
        numberOfChannels: 2,
      });
      // Planar: f32 = 4 bytes, 100 frames, 1 plane = 400 bytes per plane
      const size = audioData.allocationSize({ planeIndex: 0 });
      expect(size).toBe(400);
      audioData.close();
    });

    it('should throw RangeError for invalid planeIndex on interleaved', () => {
      const audioData = createTestAudioData({ format: 'f32' });
      expect(() => audioData.allocationSize({ planeIndex: 1 })).toThrow(RangeError);
      audioData.close();
    });

    it('should throw RangeError for planeIndex >= numberOfChannels on planar', () => {
      const audioData = createTestAudioData({
        format: 'f32-planar',
        numberOfChannels: 2,
      });
      expect(() => audioData.allocationSize({ planeIndex: 2 })).toThrow(RangeError);
      audioData.close();
    });

    it('should handle frameOffset', () => {
      const audioData = createTestAudioData({
        format: 'f32',
        numberOfFrames: 100,
        numberOfChannels: 2,
      });
      // With frameOffset=50, only 50 frames left
      const size = audioData.allocationSize({ planeIndex: 0, frameOffset: 50 });
      expect(size).toBe(400); // 50 frames * 2 channels * 4 bytes
      audioData.close();
    });

    it('should handle frameCount', () => {
      const audioData = createTestAudioData({
        format: 'f32',
        numberOfFrames: 100,
        numberOfChannels: 2,
      });
      // Request only 25 frames
      const size = audioData.allocationSize({ planeIndex: 0, frameCount: 25 });
      expect(size).toBe(200); // 25 frames * 2 channels * 4 bytes
      audioData.close();
    });

    it('should throw RangeError for frameOffset >= numberOfFrames', () => {
      const audioData = createTestAudioData({ numberOfFrames: 100 });
      expect(() => audioData.allocationSize({ planeIndex: 0, frameOffset: 100 })).toThrow(RangeError);
      audioData.close();
    });

    it('should throw RangeError for frameCount > available frames', () => {
      const audioData = createTestAudioData({ numberOfFrames: 100 });
      expect(() => audioData.allocationSize({ planeIndex: 0, frameOffset: 50, frameCount: 60 })).toThrow(RangeError);
      audioData.close();
    });

    it('should calculate size with format conversion', () => {
      const audioData = createTestAudioData({
        format: 'f32',
        numberOfFrames: 100,
        numberOfChannels: 2,
      });
      // Convert to s16 (2 bytes per sample instead of 4)
      const size = audioData.allocationSize({ planeIndex: 0, format: 's16' });
      expect(size).toBe(400); // 100 frames * 2 channels * 2 bytes
      audioData.close();
    });

    it('should throw InvalidStateError when closed', () => {
      const audioData = createTestAudioData();
      audioData.close();
      expect(() => audioData.allocationSize({ planeIndex: 0 })).toThrow(/closed|InvalidStateError/i);
    });
  });

  describe('copyTo()', () => {
    it('should copy interleaved data to buffer', () => {
      const audioData = createTestAudioData({
        format: 'f32',
        numberOfFrames: 100,
        numberOfChannels: 2,
      });
      const dest = new ArrayBuffer(800);
      audioData.copyTo(dest, { planeIndex: 0 });

      // Verify data was copied
      const floatView = new Float32Array(dest);
      expect(floatView.length).toBe(200);
      audioData.close();
    });

    it('should copy planar data to buffer', () => {
      const audioData = createTestAudioData({
        format: 'f32-planar',
        numberOfFrames: 100,
        numberOfChannels: 2,
      });
      const dest = new ArrayBuffer(400);
      audioData.copyTo(dest, { planeIndex: 0 });

      // Verify data was copied
      const floatView = new Float32Array(dest);
      expect(floatView.length).toBe(100);
      audioData.close();
    });

    it('should copy different planes for planar format', () => {
      const audioData = createTestAudioData({
        format: 'f32-planar',
        numberOfFrames: 100,
        numberOfChannels: 2,
      });
      const dest0 = new ArrayBuffer(400);
      const dest1 = new ArrayBuffer(400);

      audioData.copyTo(dest0, { planeIndex: 0 });
      audioData.copyTo(dest1, { planeIndex: 1 });

      // Both should have data
      const view0 = new Float32Array(dest0);
      const view1 = new Float32Array(dest1);
      expect(view0.length).toBe(100);
      expect(view1.length).toBe(100);
      audioData.close();
    });

    it('should handle frameOffset in copyTo', () => {
      const audioData = createTestAudioData({
        format: 'f32',
        numberOfFrames: 100,
        numberOfChannels: 2,
      });
      const dest = new ArrayBuffer(400); // Only need 50 frames
      audioData.copyTo(dest, { planeIndex: 0, frameOffset: 50 });

      const floatView = new Float32Array(dest);
      expect(floatView.length).toBe(100); // 50 frames * 2 channels
      audioData.close();
    });

    it('should handle frameCount in copyTo', () => {
      const audioData = createTestAudioData({
        format: 'f32',
        numberOfFrames: 100,
        numberOfChannels: 2,
      });
      const dest = new ArrayBuffer(200); // Only 25 frames
      audioData.copyTo(dest, { planeIndex: 0, frameCount: 25 });

      const floatView = new Float32Array(dest);
      expect(floatView.length).toBe(50); // 25 frames * 2 channels
      audioData.close();
    });

    it('should throw RangeError if destination is too small', () => {
      const audioData = createTestAudioData({
        format: 'f32',
        numberOfFrames: 100,
        numberOfChannels: 2,
      });
      const dest = new ArrayBuffer(100); // Too small
      expect(() => audioData.copyTo(dest, { planeIndex: 0 })).toThrow(RangeError);
      audioData.close();
    });

    it('should throw InvalidStateError when closed', () => {
      const audioData = createTestAudioData();
      audioData.close();
      const dest = new ArrayBuffer(1000);
      expect(() => audioData.copyTo(dest, { planeIndex: 0 })).toThrow(/closed|InvalidStateError/i);
    });
  });

  describe('Format Conversion', () => {
    it('should convert f32 to s16', () => {
      const audioData = createTestAudioData({
        format: 'f32',
        numberOfFrames: 100,
        numberOfChannels: 2,
      });

      // Request s16 output
      const size = audioData.allocationSize({ planeIndex: 0, format: 's16' });
      expect(size).toBe(400); // 100 frames * 2 channels * 2 bytes

      const dest = new ArrayBuffer(size);
      audioData.copyTo(dest, { planeIndex: 0, format: 's16' });

      // Verify s16 data
      const intView = new Int16Array(dest);
      expect(intView.length).toBe(200);
      audioData.close();
    });

    it('should convert s16 to f32', () => {
      const audioData = createTestAudioData({
        format: 's16',
        numberOfFrames: 100,
        numberOfChannels: 2,
      });

      // Request f32 output
      const size = audioData.allocationSize({ planeIndex: 0, format: 'f32' });
      expect(size).toBe(800); // 100 frames * 2 channels * 4 bytes

      const dest = new ArrayBuffer(size);
      audioData.copyTo(dest, { planeIndex: 0, format: 'f32' });

      // Verify f32 data
      const floatView = new Float32Array(dest);
      expect(floatView.length).toBe(200);
      audioData.close();
    });

    it('should convert to f32-planar (spec requirement)', () => {
      const audioData = createTestAudioData({
        format: 's16',
        numberOfFrames: 100,
        numberOfChannels: 2,
      });

      // Per spec: conversion to f32-planar MUST always be supported
      const size = audioData.allocationSize({ planeIndex: 0, format: 'f32-planar' });
      expect(size).toBe(400); // 100 frames * 1 plane * 4 bytes

      const dest = new ArrayBuffer(size);
      audioData.copyTo(dest, { planeIndex: 0, format: 'f32-planar' });

      // Verify f32 data
      const floatView = new Float32Array(dest);
      expect(floatView.length).toBe(100);
      audioData.close();
    });

    it('should convert interleaved to planar', () => {
      const audioData = createTestAudioData({
        format: 'f32',
        numberOfFrames: 100,
        numberOfChannels: 2,
      });

      // Request planar output
      const size = audioData.allocationSize({ planeIndex: 0, format: 'f32-planar' });
      expect(size).toBe(400); // 100 frames * 1 plane * 4 bytes

      const dest0 = new ArrayBuffer(size);
      const dest1 = new ArrayBuffer(size);

      audioData.copyTo(dest0, { planeIndex: 0, format: 'f32-planar' });
      audioData.copyTo(dest1, { planeIndex: 1, format: 'f32-planar' });

      // Both planes should have data
      const view0 = new Float32Array(dest0);
      const view1 = new Float32Array(dest1);
      expect(view0.length).toBe(100);
      expect(view1.length).toBe(100);
      audioData.close();
    });

    it('should convert planar to interleaved', () => {
      const audioData = createTestAudioData({
        format: 'f32-planar',
        numberOfFrames: 100,
        numberOfChannels: 2,
      });

      // Request interleaved output
      const size = audioData.allocationSize({ planeIndex: 0, format: 'f32' });
      expect(size).toBe(800); // 100 frames * 2 channels * 4 bytes

      const dest = new ArrayBuffer(size);
      audioData.copyTo(dest, { planeIndex: 0, format: 'f32' });

      const floatView = new Float32Array(dest);
      expect(floatView.length).toBe(200);
      audioData.close();
    });
  });

  describe('clone()', () => {
    it('should create an independent copy', () => {
      const audioData = createTestAudioData();
      const clone = audioData.clone();

      expect(clone).toBeDefined();
      expect(clone.format).toBe(audioData.format);
      expect(clone.sampleRate).toBe(audioData.sampleRate);
      expect(clone.numberOfFrames).toBe(audioData.numberOfFrames);
      expect(clone.numberOfChannels).toBe(audioData.numberOfChannels);

      // Close original should not affect clone
      audioData.close();
      expect(clone.format).not.toBeNull();

      clone.close();
    });
  });

  describe('close()', () => {
    it('should be idempotent', () => {
      const audioData = createTestAudioData();
      audioData.close();
      audioData.close(); // Should not throw
    });

    it('should nullify format after close', () => {
      const audioData = createTestAudioData();
      audioData.close();
      expect(audioData.format).toBeNull();
    });
  });
});
