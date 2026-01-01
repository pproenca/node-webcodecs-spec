import { describe, it, expect, beforeAll } from 'vitest';
import * as fs from 'node:fs/promises';
import * as path from 'node:path';

describe('generate-tasks', () => {
  const outputDir = path.join(process.cwd(), 'docs/tasks');

  beforeAll(async () => {
    // Run the generator
    const { main } = await import('./generate-tasks.js');
    await main();
  });

  it('should generate VideoDecoder.json', async () => {
    const content = await fs.readFile(path.join(outputDir, 'VideoDecoder.json'), 'utf-8');
    const data = JSON.parse(content);

    expect(data.interface).toBe('VideoDecoder');
    expect(data.type).toBe('interface');
    expect(data.features.length).toBeGreaterThan(0);
  });

  it('should include code links in features', async () => {
    const content = await fs.readFile(path.join(outputDir, 'VideoDecoder.json'), 'utf-8');
    const data = JSON.parse(content);

    const configureFeature = data.features.find(
      (f: { id: string }) => f.id === 'VideoDecoder.configure'
    );
    expect(configureFeature).toBeDefined();
    expect(configureFeature.codeLinks.declaration?.file).toContain('VideoDecoder.h');
    expect(configureFeature.codeLinks.implementation?.line).toBeGreaterThan(0);
  });

  it('should generate types.json with dictionaries and enums', async () => {
    const content = await fs.readFile(path.join(outputDir, 'types.json'), 'utf-8');
    const data = JSON.parse(content);

    expect(data.dictionaries.length).toBeGreaterThan(0);
    expect(data.enums.length).toBeGreaterThan(0);
    expect(data.enums.some((e: { name: string }) => e.name === 'CodecState')).toBe(true);
  });

  it('should set all passes to false', async () => {
    const content = await fs.readFile(path.join(outputDir, 'VideoDecoder.json'), 'utf-8');
    const data = JSON.parse(content);

    for (const feature of data.features) {
      expect(feature.passes).toBe(false);
      for (const step of feature.steps) {
        expect(step.passes).toBe(false);
      }
    }
  });
});
