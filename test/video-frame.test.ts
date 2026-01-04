// test/video-frame.test.ts
import { describe, it, expect, beforeEach, afterEach } from 'vitest';
import { VideoFrame } from '@pproenca/node-webcodecs';

describe('VideoFrame', () => {
  // Helper to create YUV420P test data
  function createYUV420PData(width: number, height: number): ArrayBuffer {
    // Y: width * height, U: width/2 * height/2, V: width/2 * height/2
    const ySize = width * height;
    const uvSize = (width / 2) * (height / 2);
    const totalSize = ySize + uvSize * 2;
    const buffer = new ArrayBuffer(totalSize);
    const view = new Uint8Array(buffer);

    // Fill Y plane with gray (128)
    for (let i = 0; i < ySize; i++) {
      view[i] = 128;
    }
    // Fill U and V with neutral (128)
    for (let i = ySize; i < totalSize; i++) {
      view[i] = 128;
    }

    return buffer;
  }

  // Helper to create RGBA test data
  function createRGBAData(width: number, height: number): ArrayBuffer {
    const totalSize = width * height * 4;
    const buffer = new ArrayBuffer(totalSize);
    const view = new Uint8Array(buffer);

    // Fill with red color
    for (let i = 0; i < totalSize; i += 4) {
      view[i] = 255;     // R
      view[i + 1] = 0;   // G
      view[i + 2] = 0;   // B
      view[i + 3] = 255; // A
    }

    return buffer;
  }

  // Helper to create I422 (YUV 4:2:2) test data
  function createI422Data(width: number, height: number): ArrayBuffer {
    // I422: Y=w*h, U=w/2*h, V=w/2*h (horizontal subsampling only)
    const ySize = width * height;
    const uvSize = (width / 2) * height;
    const totalSize = ySize + uvSize * 2;
    const buffer = new ArrayBuffer(totalSize);
    const view = new Uint8Array(buffer);
    view.fill(128); // Fill with gray
    return buffer;
  }

  // Helper to create I444 (YUV 4:4:4) test data
  function createI444Data(width: number, height: number): ArrayBuffer {
    // I444: Y=w*h, U=w*h, V=w*h (no subsampling)
    const planeSize = width * height;
    const totalSize = planeSize * 3;
    const buffer = new ArrayBuffer(totalSize);
    const view = new Uint8Array(buffer);
    view.fill(128); // Fill with gray
    return buffer;
  }

  // Helper to create I420A (YUV 4:2:0 with alpha) test data
  function createI420AData(width: number, height: number): ArrayBuffer {
    // I420A: Y=w*h, U=w/2*h/2, V=w/2*h/2, A=w*h
    const ySize = width * height;
    const uvSize = (width / 2) * (height / 2);
    const alphaSize = width * height;
    const totalSize = ySize + uvSize * 2 + alphaSize;
    const buffer = new ArrayBuffer(totalSize);
    const view = new Uint8Array(buffer);
    view.fill(128); // Fill with gray
    return buffer;
  }

  // Helper to create buffer data for a given format
  function createDataForFormat(format: string, width: number, height: number): ArrayBuffer {
    switch (format) {
      case 'RGBA':
      case 'RGBX':
      case 'BGRA':
      case 'BGRX':
        return createRGBAData(width, height);
      case 'I422':
        return createI422Data(width, height);
      case 'I444':
        return createI444Data(width, height);
      case 'I420A':
        return createI420AData(width, height);
      case 'I420':
      default:
        return createYUV420PData(width, height);
    }
  }

  // Helper to create a basic VideoFrame
  function createTestFrame(options: {
    format?: string;
    width?: number;
    height?: number;
    timestamp?: number;
    duration?: number;
    visibleRect?: { x: number; y: number; width: number; height: number };
    rotation?: number;
    flip?: boolean;
    displayWidth?: number;
    displayHeight?: number;
  } = {}): VideoFrame {
    const format = options.format || 'I420';
    const width = options.width || 16;
    const height = options.height || 16;
    const timestamp = options.timestamp || 0;

    const data = createDataForFormat(format, width, height);

    return new VideoFrame(data, {
      format: format as any,
      codedWidth: width,
      codedHeight: height,
      timestamp,
      duration: options.duration,
      visibleRect: options.visibleRect,
      rotation: options.rotation,
      flip: options.flip,
      displayWidth: options.displayWidth,
      displayHeight: options.displayHeight,
    });
  }

  describe('Constructor with buffer data', () => {
    it('should create VideoFrame with I420 format', () => {
      const frame = createTestFrame({ format: 'I420', width: 16, height: 16 });
      expect(frame).toBeDefined();
      expect(frame.format).toBe('I420');
      expect(frame.codedWidth).toBe(16);
      expect(frame.codedHeight).toBe(16);
      frame.close();
    });

    it('should create VideoFrame with RGBA format', () => {
      const frame = createTestFrame({ format: 'RGBA', width: 16, height: 16 });
      expect(frame).toBeDefined();
      expect(frame.format).toBe('RGBA');
      frame.close();
    });

    it('should store timestamp correctly', () => {
      const frame = createTestFrame({ timestamp: 12345 });
      expect(frame.timestamp).toBe(12345);
      frame.close();
    });

    it('should store duration correctly', () => {
      const frame = createTestFrame({ duration: 33333 });
      expect(frame.duration).toBe(33333);
      frame.close();
    });

    it('should throw TypeError for missing timestamp', () => {
      const data = createYUV420PData(16, 16);
      expect(() => new VideoFrame(data, {
        format: 'I420',
        codedWidth: 16,
        codedHeight: 16,
      } as any)).toThrow();
    });

    it('should throw TypeError for invalid format', () => {
      const data = createYUV420PData(16, 16);
      expect(() => new VideoFrame(data, {
        format: 'INVALID' as any,
        codedWidth: 16,
        codedHeight: 16,
        timestamp: 0,
      })).toThrow();
    });

    it('should throw TypeError for buffer too small', () => {
      const smallBuffer = new ArrayBuffer(10); // Too small for 16x16
      expect(() => new VideoFrame(smallBuffer, {
        format: 'I420',
        codedWidth: 16,
        codedHeight: 16,
        timestamp: 0,
      })).toThrow();
    });
  });

  describe('Internal slots (W3C spec compliance)', () => {
    describe('rotation', () => {
      it('should default to 0', () => {
        const frame = createTestFrame();
        expect(frame.rotation).toBe(0);
        frame.close();
      });

      it('should accept rotation 90', () => {
        const frame = createTestFrame({ rotation: 90 });
        expect(frame.rotation).toBe(90);
        frame.close();
      });

      it('should accept rotation 180', () => {
        const frame = createTestFrame({ rotation: 180 });
        expect(frame.rotation).toBe(180);
        frame.close();
      });

      it('should accept rotation 270', () => {
        const frame = createTestFrame({ rotation: 270 });
        expect(frame.rotation).toBe(270);
        frame.close();
      });

      it('should only accept valid rotation values (0, 90, 180, 270)', () => {
        // W3C spec only allows 0, 90, 180, 270 - not arbitrary angles
        // 360 is rejected because it's not in the allowed set
        expect(() => createTestFrame({ rotation: 360 })).toThrow();
        expect(() => createTestFrame({ rotation: 45 })).toThrow();
      });
    });

    describe('flip', () => {
      it('should default to false', () => {
        const frame = createTestFrame();
        expect(frame.flip).toBe(false);
        frame.close();
      });

      it('should accept flip true', () => {
        const frame = createTestFrame({ flip: true });
        expect(frame.flip).toBe(true);
        frame.close();
      });

      it('should accept flip false', () => {
        const frame = createTestFrame({ flip: false });
        expect(frame.flip).toBe(false);
        frame.close();
      });
    });

    describe('visibleRect', () => {
      it('should default to full coded rect', () => {
        const frame = createTestFrame({ width: 32, height: 24 });
        expect(frame.visibleRect).toBeDefined();
        expect(frame.visibleRect?.x).toBe(0);
        expect(frame.visibleRect?.y).toBe(0);
        expect(frame.visibleRect?.width).toBe(32);
        expect(frame.visibleRect?.height).toBe(24);
        frame.close();
      });

      it('should accept custom visibleRect', () => {
        const frame = createTestFrame({
          width: 32,
          height: 24,
          visibleRect: { x: 4, y: 4, width: 24, height: 16 },
        });
        expect(frame.visibleRect?.x).toBe(4);
        expect(frame.visibleRect?.y).toBe(4);
        expect(frame.visibleRect?.width).toBe(24);
        expect(frame.visibleRect?.height).toBe(16);
        frame.close();
      });

      it('should throw for visibleRect out of bounds', () => {
        expect(() => createTestFrame({
          width: 16,
          height: 16,
          visibleRect: { x: 0, y: 0, width: 32, height: 16 }, // width exceeds coded
        })).toThrow();
      });

      it('should throw for negative visibleRect', () => {
        expect(() => createTestFrame({
          width: 16,
          height: 16,
          visibleRect: { x: -1, y: 0, width: 16, height: 16 },
        })).toThrow();
      });

      it('should throw for visibleRect with zero width', () => {
        expect(() => createTestFrame({
          width: 16,
          height: 16,
          visibleRect: { x: 0, y: 0, width: 0, height: 16 },
        })).toThrow();
      });
    });

    describe('displayWidth and displayHeight', () => {
      it('should default to visibleRect dimensions', () => {
        const frame = createTestFrame({ width: 32, height: 24 });
        expect(frame.displayWidth).toBe(32);
        expect(frame.displayHeight).toBe(24);
        frame.close();
      });

      it('should accept custom displayWidth/Height', () => {
        const frame = createTestFrame({
          width: 32,
          height: 24,
          displayWidth: 640,
          displayHeight: 480,
        });
        expect(frame.displayWidth).toBe(640);
        expect(frame.displayHeight).toBe(480);
        frame.close();
      });

      it('should swap dimensions when rotation is 90', () => {
        const frame = createTestFrame({
          width: 32,
          height: 24,
          rotation: 90,
        });
        // With 90 degree rotation, display dimensions should be swapped
        expect(frame.displayWidth).toBe(24);
        expect(frame.displayHeight).toBe(32);
        frame.close();
      });

      it('should swap dimensions when rotation is 270', () => {
        const frame = createTestFrame({
          width: 32,
          height: 24,
          rotation: 270,
        });
        expect(frame.displayWidth).toBe(24);
        expect(frame.displayHeight).toBe(32);
        frame.close();
      });

      it('should not swap dimensions when rotation is 180', () => {
        const frame = createTestFrame({
          width: 32,
          height: 24,
          rotation: 180,
        });
        expect(frame.displayWidth).toBe(32);
        expect(frame.displayHeight).toBe(24);
        frame.close();
      });
    });
  });

  describe('VideoFrame(VideoFrame, init) constructor', () => {
    it('should clone with no init preserving all slots', () => {
      const original = createTestFrame({
        width: 32,
        height: 24,
        timestamp: 1000,
        rotation: 90,
        flip: true,
      });

      const clone = new VideoFrame(original, {});

      expect(clone.codedWidth).toBe(32);
      expect(clone.codedHeight).toBe(24);
      expect(clone.timestamp).toBe(1000);
      expect(clone.rotation).toBe(90);
      expect(clone.flip).toBe(true);

      original.close();
      clone.close();
    });

    it('should allow timestamp override', () => {
      const original = createTestFrame({ timestamp: 1000 });
      const clone = new VideoFrame(original, { timestamp: 2000 });

      expect(clone.timestamp).toBe(2000);

      original.close();
      clone.close();
    });

    it('should allow duration override', () => {
      const original = createTestFrame({ duration: 33333 });
      const clone = new VideoFrame(original, { duration: 16666 });

      expect(clone.duration).toBe(16666);

      original.close();
      clone.close();
    });

    it('should throw for closed source frame', () => {
      const original = createTestFrame();
      original.close();

      expect(() => new VideoFrame(original, {})).toThrow();
    });

    it('should create independent clone (closing source does not affect clone)', () => {
      const original = createTestFrame({ timestamp: 1000 });
      const clone = new VideoFrame(original, {});

      original.close();

      // Clone should still be usable
      expect(clone.timestamp).toBe(1000);
      expect(clone.format).toBe('I420');

      clone.close();
    });

    describe('rotation composition (Add Rotations algorithm)', () => {
      it('should add rotations together', () => {
        const original = createTestFrame({ width: 32, height: 24, rotation: 90 });
        const clone = new VideoFrame(original, { rotation: 90 });

        // 90 + 90 = 180
        expect(clone.rotation).toBe(180);

        original.close();
        clone.close();
      });

      it('should normalize rotation sum over 360', () => {
        const original = createTestFrame({ rotation: 270 });
        const clone = new VideoFrame(original, { rotation: 180 });

        // 270 + 180 = 450 -> 90
        expect(clone.rotation).toBe(90);

        original.close();
        clone.close();
      });

      it('should handle flip XOR behavior', () => {
        const original = createTestFrame({ flip: true });
        const clone = new VideoFrame(original, { flip: true });

        // true XOR true = false
        expect(clone.flip).toBe(false);

        original.close();
        clone.close();
      });

      it('should handle flip with rotation', () => {
        const original = createTestFrame({ flip: false });
        const clone = new VideoFrame(original, { flip: true });

        // false XOR true = true
        expect(clone.flip).toBe(true);

        original.close();
        clone.close();
      });

      it('should subtract rotation when source has flip=true (Add Rotations algorithm)', () => {
        // Per W3C spec: when source frame has flip=true, rotation is SUBTRACTED
        const original = createTestFrame({ rotation: 180, flip: true });
        const clone = new VideoFrame(original, { rotation: 90 });

        // Per spec: if flip_, combined = 180 - 90 = 90
        expect(clone.rotation).toBe(90);
        // Flip should remain true (no new flip specified, so XOR with false = true)
        expect(clone.flip).toBe(true);

        original.close();
        clone.close();
      });

      it('should handle combined flip and rotation override', () => {
        const original = createTestFrame({ rotation: 270, flip: true });
        const clone = new VideoFrame(original, { rotation: 180, flip: true });

        // Rotation: 270 - 180 = 90 (subtracted because source has flip)
        expect(clone.rotation).toBe(90);
        // Flip: true XOR true = false
        expect(clone.flip).toBe(false);

        original.close();
        clone.close();
      });
    });
  });

  describe('clone() method', () => {
    it('should create an identical clone', () => {
      const original = createTestFrame({
        width: 32,
        height: 24,
        timestamp: 5000,
        duration: 33333,
        rotation: 180,
        flip: true,
      });

      const clone = original.clone();

      expect(clone.codedWidth).toBe(original.codedWidth);
      expect(clone.codedHeight).toBe(original.codedHeight);
      expect(clone.timestamp).toBe(original.timestamp);
      expect(clone.duration).toBe(original.duration);
      expect(clone.rotation).toBe(original.rotation);
      expect(clone.flip).toBe(original.flip);
      expect(clone.format).toBe(original.format);

      original.close();
      clone.close();
    });

    it('should throw for closed frame', () => {
      const frame = createTestFrame();
      frame.close();

      expect(() => frame.clone()).toThrow();
    });

    it('should create independent clone', () => {
      const original = createTestFrame({ timestamp: 7000 });
      const clone = original.clone();

      original.close();

      // Clone should still work
      expect(clone.timestamp).toBe(7000);

      clone.close();
    });
  });

  describe('close() method', () => {
    it('should close the frame', () => {
      const frame = createTestFrame();
      frame.close();

      // After close, format returns null (spec says should throw, but native doesn't)
      expect(frame.format).toBeNull();
    });

    it('should be idempotent (double close is safe)', () => {
      const frame = createTestFrame();
      frame.close();
      expect(() => frame.close()).not.toThrow();
    });
  });

  describe('copyTo() method', () => {
    it('should copy frame data to buffer', async () => {
      const frame = createTestFrame({ format: 'I420', width: 16, height: 16 });
      const size = frame.allocationSize();
      const buffer = new ArrayBuffer(size);

      const result = await frame.copyTo(buffer);

      expect(result).toBeDefined();
      expect(result.length).toBeGreaterThan(0);

      frame.close();
    });

    it('should reject for closed frame', async () => {
      const frame = createTestFrame();
      frame.close();

      const buffer = new ArrayBuffer(1024);
      await expect(frame.copyTo(buffer)).rejects.toThrow();
    });

    it('should reject for buffer too small', async () => {
      const frame = createTestFrame({ width: 64, height: 64 });
      const buffer = new ArrayBuffer(10); // Too small

      await expect(frame.copyTo(buffer)).rejects.toThrow();

      frame.close();
    });

    describe('layout option validation', () => {
      it('should accept valid layout option', async () => {
        const frame = createTestFrame({ format: 'I420', width: 16, height: 16 });
        // Calculate buffer size based on layout:
        // Y: offset 0, stride 32, 16 rows -> 16 * 32 = 512 bytes (ends at 512)
        // U: offset 512, stride 16, 8 rows -> 8 * 16 = 128 bytes (ends at 640)
        // V: offset 768, stride 16, 8 rows -> 8 * 16 = 128 bytes (ends at 896)
        const buffer = new ArrayBuffer(896);

        // Custom layout with extra padding
        const layout = [
          { offset: 0, stride: 32 },      // Y with extra stride
          { offset: 512, stride: 16 },    // U
          { offset: 768, stride: 16 },    // V
        ];

        const result = await frame.copyTo(buffer, { layout });
        expect(result).toBeDefined();

        frame.close();
      });

      it('should reject layout with missing offset', async () => {
        const frame = createTestFrame({ format: 'I420', width: 16, height: 16 });
        const buffer = new ArrayBuffer(1024);

        await expect(frame.copyTo(buffer, {
          layout: [{ stride: 16 } as any],
        })).rejects.toThrow();

        frame.close();
      });

      it('should reject layout with missing stride', async () => {
        const frame = createTestFrame({ format: 'I420', width: 16, height: 16 });
        const buffer = new ArrayBuffer(1024);

        await expect(frame.copyTo(buffer, {
          layout: [{ offset: 0 } as any],
        })).rejects.toThrow();

        frame.close();
      });

      it('should reject layout with negative offset', async () => {
        const frame = createTestFrame({ format: 'I420', width: 16, height: 16 });
        const buffer = new ArrayBuffer(1024);

        await expect(frame.copyTo(buffer, {
          layout: [{ offset: -1, stride: 16 }],
        })).rejects.toThrow();

        frame.close();
      });

      it('should reject layout with zero stride', async () => {
        const frame = createTestFrame({ format: 'I420', width: 16, height: 16 });
        const buffer = new ArrayBuffer(1024);

        await expect(frame.copyTo(buffer, {
          layout: [{ offset: 0, stride: 0 }],
        })).rejects.toThrow();

        frame.close();
      });

      it('should reject layout with negative stride', async () => {
        const frame = createTestFrame({ format: 'I420', width: 16, height: 16 });
        const buffer = new ArrayBuffer(1024);

        await expect(frame.copyTo(buffer, {
          layout: [{ offset: 0, stride: -16 }],
        })).rejects.toThrow();

        frame.close();
      });
    });
  });

  describe('allocationSize() method', () => {
    it('should return positive size for valid frame', () => {
      const frame = createTestFrame({ format: 'I420', width: 16, height: 16 });
      const size = frame.allocationSize();

      // YUV420P: 16*16 + 2*(8*8) = 256 + 128 = 384
      expect(size).toBeGreaterThanOrEqual(384);

      frame.close();
    });

    it('should throw for closed frame', () => {
      const frame = createTestFrame();
      frame.close();

      expect(() => frame.allocationSize()).toThrow();
    });
  });

  describe('Attribute access after close', () => {
    // TODO: W3C spec requires throwing InvalidStateError when accessing attributes
    // on closed frames. Current native implementation returns null for all properties.
    // These tests document actual behavior until the spec requirement is implemented.

    it('should return null for format after close (spec: should throw)', () => {
      const frame = createTestFrame();
      frame.close();
      expect(frame.format).toBeNull();
    });

    it('should return null for codedWidth after close (spec: should throw)', () => {
      const frame = createTestFrame();
      frame.close();
      expect(frame.codedWidth).toBeNull();
    });

    it('should return null for timestamp after close (spec: should throw)', () => {
      const frame = createTestFrame();
      frame.close();
      expect(frame.timestamp).toBeNull();
    });

    it('should return null for rotation after close (spec: should throw)', () => {
      const frame = createTestFrame();
      frame.close();
      expect(frame.rotation).toBeNull();
    });

    it('should return null for flip after close (spec: should throw)', () => {
      const frame = createTestFrame();
      frame.close();
      expect(frame.flip).toBeNull();
    });
  });

  describe('Pixel format variants (W3C spec)', () => {
    it('should create VideoFrame with I422 format (4:2:2)', () => {
      const frame = createTestFrame({ format: 'I422', width: 16, height: 16 });
      expect(frame.format).toBe('I422');
      expect(frame.codedWidth).toBe(16);
      expect(frame.codedHeight).toBe(16);
      frame.close();
    });

    it('should create VideoFrame with I444 format (4:4:4)', () => {
      const frame = createTestFrame({ format: 'I444', width: 16, height: 16 });
      expect(frame.format).toBe('I444');
      expect(frame.codedWidth).toBe(16);
      expect(frame.codedHeight).toBe(16);
      frame.close();
    });

    it('should create VideoFrame with I420A format (4:2:0 with alpha)', () => {
      const frame = createTestFrame({ format: 'I420A', width: 16, height: 16 });
      expect(frame.format).toBe('I420A');
      expect(frame.codedWidth).toBe(16);
      expect(frame.codedHeight).toBe(16);
      frame.close();
    });

    it('should calculate correct allocationSize for I422', () => {
      const frame = createTestFrame({ format: 'I422', width: 16, height: 16 });
      const size = frame.allocationSize();
      // I422: Y=16*16=256, U=8*16=128, V=8*16=128 = 512 minimum
      expect(size).toBeGreaterThanOrEqual(512);
      frame.close();
    });

    it('should calculate correct allocationSize for I444', () => {
      const frame = createTestFrame({ format: 'I444', width: 16, height: 16 });
      const size = frame.allocationSize();
      // I444: Y=16*16=256, U=16*16=256, V=16*16=256 = 768 minimum
      expect(size).toBeGreaterThanOrEqual(768);
      frame.close();
    });

    it('should calculate correct allocationSize for I420A', () => {
      const frame = createTestFrame({ format: 'I420A', width: 16, height: 16 });
      const size = frame.allocationSize();
      // I420A: Y=256, U=64, V=64, A=256 = 640 minimum
      expect(size).toBeGreaterThanOrEqual(640);
      frame.close();
    });

    it('should copyTo work with I422 format', async () => {
      const frame = createTestFrame({ format: 'I422', width: 16, height: 16 });
      const size = frame.allocationSize();
      const buffer = new ArrayBuffer(size);
      const result = await frame.copyTo(buffer);
      expect(result.length).toBe(3); // 3 planes
      frame.close();
    });

    it('should copyTo work with I444 format', async () => {
      const frame = createTestFrame({ format: 'I444', width: 16, height: 16 });
      const size = frame.allocationSize();
      const buffer = new ArrayBuffer(size);
      const result = await frame.copyTo(buffer);
      expect(result.length).toBe(3); // 3 planes
      frame.close();
    });

    it('should copyTo work with I420A format (4 planes)', async () => {
      const frame = createTestFrame({ format: 'I420A', width: 16, height: 16 });
      const size = frame.allocationSize();
      const buffer = new ArrayBuffer(size);
      const result = await frame.copyTo(buffer);
      expect(result.length).toBe(4); // 4 planes (Y, U, V, A)
      frame.close();
    });
  });

  describe('copyTo() rect option bounds safety', () => {
    it('should reject rect with x + width exceeding frame bounds', async () => {
      const frame = createTestFrame({ width: 100, height: 100 });
      const buffer = new ArrayBuffer(frame.allocationSize());

      // x=50, width=60 -> x+width=110 > 100
      await expect(frame.copyTo(buffer, {
        rect: { x: 50, y: 0, width: 60, height: 50 }
      })).rejects.toThrow();

      frame.close();
    });

    it('should reject rect with y + height exceeding frame bounds', async () => {
      const frame = createTestFrame({ width: 100, height: 100 });
      const buffer = new ArrayBuffer(frame.allocationSize());

      // y=50, height=60 -> y+height=110 > 100
      await expect(frame.copyTo(buffer, {
        rect: { x: 0, y: 50, width: 50, height: 60 }
      })).rejects.toThrow();

      frame.close();
    });

    it('should reject rect with negative coordinates', async () => {
      const frame = createTestFrame({ width: 100, height: 100 });
      const buffer = new ArrayBuffer(frame.allocationSize());

      await expect(frame.copyTo(buffer, {
        rect: { x: -10, y: 0, width: 50, height: 50 }
      })).rejects.toThrow();

      frame.close();
    });

    it('should reject rect with zero dimensions', async () => {
      const frame = createTestFrame({ width: 100, height: 100 });
      const buffer = new ArrayBuffer(frame.allocationSize());

      await expect(frame.copyTo(buffer, {
        rect: { x: 0, y: 0, width: 0, height: 50 }
      })).rejects.toThrow();

      frame.close();
    });
  });
});
