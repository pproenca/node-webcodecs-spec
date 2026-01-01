import { describe, it, expect } from 'vitest';
import { parseTsFile, parseTsTestFile } from './ts-ast-parser.js';
import * as path from 'node:path';

describe('parseTsFile', () => {
  it('should extract class methods with line numbers', async () => {
    const filePath = path.join(process.cwd(), 'lib/VideoDecoder.ts');
    const result = await parseTsFile(filePath);

    expect(result.className).toBe('VideoDecoder');
    expect(result.methods.get('configure')).toBeDefined();
    expect(result.methods.get('configure')?.line).toBeGreaterThan(0);
  });

  it('should extract getters', async () => {
    const filePath = path.join(process.cwd(), 'lib/VideoDecoder.ts');
    const result = await parseTsFile(filePath);

    expect(result.getters.get('state')).toBeDefined();
    expect(result.getters.get('state')?.line).toBeGreaterThan(0);
  });

  it('should extract static methods', async () => {
    const filePath = path.join(process.cwd(), 'lib/VideoDecoder.ts');
    const result = await parseTsFile(filePath);

    expect(result.staticMethods.get('isConfigSupported')).toBeDefined();
  });

  it('should extract constructor', async () => {
    const filePath = path.join(process.cwd(), 'lib/VideoDecoder.ts');
    const result = await parseTsFile(filePath);

    expect(result.constructor).toBeDefined();
    expect(result.constructor?.line).toBeGreaterThan(0);
  });
});

describe('parseTsTestFile', () => {
  it('should extract describe/it blocks with line numbers', async () => {
    const filePath = path.join(process.cwd(), 'test/video-decoder.test.ts');
    const result = await parseTsTestFile(filePath);

    expect(result.describes.get('VideoDecoder')).toBeDefined();
    expect(result.itBlocks.length).toBeGreaterThan(0);
  });
});
