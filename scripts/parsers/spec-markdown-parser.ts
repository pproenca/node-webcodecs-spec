/**
 * Parses spec context markdown files to extract algorithm steps.
 */

export interface SpecMethod {
  signature: string;
  isStatic: boolean;
  algorithmSteps: string[];
}

export interface SpecAttribute {
  name: string;
  type: string;
  readonly: boolean;
}

export interface SpecParseResult {
  methods: Map<string, SpecMethod>;
  attributes: SpecAttribute[];
  description?: string;
}

export function parseSpecMarkdown(content: string): SpecParseResult {
  const methods = new Map<string, SpecMethod>();
  const attributes: SpecAttribute[] = [];

  // Extract attributes from "## Attributes" section
  const attrMatch = content.match(/## Attributes\n\n([\s\S]*?)(?=\n## |\n---|$)/);
  if (attrMatch) {
    const attrLines = attrMatch[1].split('\n');
    for (const line of attrLines) {
      // Match: - **name** (`type`) [ReadOnly]
      const match = line.match(/^- \*\*(\w+)\*\* \(`([^`]+)`\)( \[ReadOnly\])?/);
      if (match) {
        attributes.push({
          name: match[1],
          type: match[2],
          readonly: !!match[3],
        });
      }
    }
  }

  // Extract methods from ### headers
  const methodRegex = /### (\w+)\n\n(?:\*\*Static Method\*\*\n\n)?\*\*Signature:\*\* `([^`]+)`\n\n\*\*Algorithm:\*\*\n\n([\s\S]*?)(?=\n### |\n## |$)/g;
  let match;
  while ((match = methodRegex.exec(content)) !== null) {
    const name = match[1];
    const signature = match[2];
    const isStatic = content.slice(match.index, match.index + 200).includes('**Static Method**');
    const algorithmText = match[3];

    // Extract numbered steps
    const steps: string[] = [];
    const stepRegex = /^\d+\. (.+)$/gm;
    let stepMatch;
    while ((stepMatch = stepRegex.exec(algorithmText)) !== null) {
      steps.push(stepMatch[1]);
    }

    methods.set(name, {
      signature,
      isStatic,
      algorithmSteps: steps,
    });
  }

  return { methods, attributes };
}

export async function parseSpecFile(filePath: string): Promise<SpecParseResult> {
  const fs = await import('node:fs/promises');
  const content = await fs.readFile(filePath, 'utf-8');
  return parseSpecMarkdown(content);
}
