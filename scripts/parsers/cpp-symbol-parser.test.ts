import { describe, it, expect } from 'vitest';
import { parseCppHeader, parseCppImpl } from './cpp-symbol-parser.js';
import * as path from 'node:path';

describe('parseCppHeader', () => {
  it('should extract class name', async () => {
    const filePath = path.join(process.cwd(), 'src/VideoDecoder.h');
    const result = await parseCppHeader(filePath);

    expect(result.className).toBe('VideoDecoder');
  });

  it('should extract method declarations', async () => {
    const filePath = path.join(process.cwd(), 'src/VideoDecoder.h');
    const result = await parseCppHeader(filePath);

    expect(result.methods.get('Configure')).toBeDefined();
    expect(result.methods.get('GetState')).toBeDefined();
  });

  it('should identify static methods', async () => {
    const filePath = path.join(process.cwd(), 'src/VideoDecoder.h');
    const result = await parseCppHeader(filePath);

    expect(result.staticMethods.get('IsConfigSupported')).toBeDefined();
  });
});

describe('parseCppImpl', () => {
  it('should extract method implementations with line ranges', async () => {
    const filePath = path.join(process.cwd(), 'src/VideoDecoder.cpp');
    const result = await parseCppImpl(filePath, 'VideoDecoder');

    expect(result.methods.get('GetState')).toBeDefined();
    expect(result.methods.get('GetState')?.line).toBeGreaterThan(0);
  });

  it('should extract constructor implementation', async () => {
    const filePath = path.join(process.cwd(), 'src/VideoDecoder.cpp');
    const result = await parseCppImpl(filePath, 'VideoDecoder');

    expect(result.ctor).toBeDefined();
    expect(result.ctor?.line).toBeGreaterThan(0);
  });
});
