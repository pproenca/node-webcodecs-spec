import { describe, it, expect } from 'vitest';
import { parseSpecMarkdown } from './spec-markdown-parser.js';

describe('parseSpecMarkdown', () => {
  it('should extract method algorithm steps', () => {
    const markdown = `
### configure

**Signature:** \`void configure(VideoDecoderConfig* config)\`

**Algorithm:**

1. If config is not valid, throw TypeError.
2. If state is "closed", throw InvalidStateError.
3. Set state to "configured".
`;
    const result = parseSpecMarkdown(markdown);

    expect(result.methods.get('configure')).toBeDefined();
    expect(result.methods.get('configure')?.signature).toBe('void configure(VideoDecoderConfig* config)');
    expect(result.methods.get('configure')?.algorithmSteps).toHaveLength(3);
    expect(result.methods.get('configure')?.algorithmSteps[0]).toContain('TypeError');
  });

  it('should identify static methods', () => {
    const markdown = `
### isConfigSupported

**Static Method**

**Signature:** \`Napi::Value isConfigSupported(VideoDecoderConfig* config)\`

**Algorithm:**

1. Returns a promise.
`;
    const result = parseSpecMarkdown(markdown);

    expect(result.methods.get('isConfigSupported')?.isStatic).toBe(true);
  });

  it('should extract attributes', () => {
    const markdown = `
## Attributes

- **state** (\`CodecState*\`) [ReadOnly]
- **ondequeue** (\`EventHandler*\`)
`;
    const result = parseSpecMarkdown(markdown);

    expect(result.attributes).toHaveLength(2);
    expect(result.attributes[0].name).toBe('state');
    expect(result.attributes[0].readonly).toBe(true);
    expect(result.attributes[1].name).toBe('ondequeue');
    expect(result.attributes[1].readonly).toBe(false);
  });
});
