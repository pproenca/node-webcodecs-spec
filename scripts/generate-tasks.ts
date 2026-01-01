/**
 * Task Generator - Parses WebIDL and generates Markdown task files.
 *
 * Usage: npx tsx scripts/generate-tasks.ts
 *
 * Reads: spec/context/_webcodecs.idl
 * Outputs: docs/tasks/<interface-name>.md
 */

import * as fs from 'fs';
import * as path from 'path';
import * as webidl2 from 'webidl2';

const IDL_PATH = path.join(__dirname, '../spec/context/_webcodecs.idl');
const OUTPUT_DIR = path.join(__dirname, '../docs/tasks');

interface TaskItem {
  type: 'attribute' | 'method' | 'constructor' | 'static-method';
  name: string;
  returnType: string;
  params?: string[];
  readonly?: boolean;
}

function parseIdlType(idlType: webidl2.IDLTypeDescription | null): string {
  if (!idlType) return 'void';
  if (typeof idlType.idlType === 'string') {
    return idlType.idlType;
  }
  if (Array.isArray(idlType.idlType)) {
    return idlType.idlType.map(t => parseIdlType(t as webidl2.IDLTypeDescription)).join(' | ');
  }
  return 'unknown';
}

function extractTasks(member: webidl2.IDLInterfaceMemberType): TaskItem | null {
  switch (member.type) {
    case 'attribute':
      return {
        type: 'attribute',
        name: member.name,
        returnType: parseIdlType(member.idlType),
        readonly: member.readonly,
      };
    case 'operation':
      if (!member.name) return null;
      return {
        type: member.special === 'static' ? 'static-method' : 'method',
        name: member.name,
        returnType: parseIdlType(member.idlType),
        params: member.arguments.map(arg => `${arg.name}: ${parseIdlType(arg.idlType)}`),
      };
    case 'constructor':
      return {
        type: 'constructor',
        name: 'constructor',
        returnType: 'instance',
        params: member.arguments.map(arg => `${arg.name}: ${parseIdlType(arg.idlType)}`),
      };
    default:
      return null;
  }
}

function capitalize(str: string): string {
  return str.charAt(0).toUpperCase() + str.slice(1);
}

function generateTaskMarkdown(interfaceName: string, tasks: TaskItem[]): string {
  const lines: string[] = [
    `# ${interfaceName} Implementation Tasks`,
    '',
    `> Auto-generated from \`spec/context/_webcodecs.idl\``,
    '',
    `## Overview`,
    '',
    `Implement the \`${interfaceName}\` class following the W3C WebCodecs specification.`,
    '',
    `**Files:**`,
    `- C++ Header: \`src/${interfaceName}.h\``,
    `- C++ Implementation: \`src/${interfaceName}.cpp\``,
    `- TypeScript Wrapper: \`lib/${interfaceName}.ts\``,
    `- Tests: \`test/${interfaceName.toLowerCase()}.test.ts\``,
    '',
    `---`,
    '',
    `## Implementation Checklist`,
    '',
  ];

  // Constructor
  const constructor = tasks.find(t => t.type === 'constructor');
  if (constructor) {
    lines.push(`### Constructor`);
    lines.push('');
    lines.push(`- [ ] Implement C++ constructor with parameter validation`);
    lines.push(`- [ ] Initialize internal state slots`);
    lines.push(`- [ ] Wire TypeScript wrapper to native constructor`);
    if (constructor.params?.length) {
      lines.push(`- [ ] Validate required parameters: \`${constructor.params.join(', ')}\``);
    }
    lines.push('');
  }

  // Attributes
  const attributes = tasks.filter(t => t.type === 'attribute');
  if (attributes.length > 0) {
    lines.push(`### Attributes`);
    lines.push('');
    for (const attr of attributes) {
      const prefix = attr.readonly ? '(readonly)' : '(read/write)';
      lines.push(`#### \`${attr.name}\` ${prefix}`);
      lines.push('');
      lines.push(`- [ ] C++: Implement \`Get${capitalize(attr.name)}()\` returning \`${attr.returnType}\``);
      if (!attr.readonly) {
        lines.push(`- [ ] C++: Implement \`Set${capitalize(attr.name)}()\``);
      }
      lines.push(`- [ ] TS: Wire getter${attr.readonly ? '' : ' and setter'} to native`);
      lines.push(`- [ ] Test: Verify initial value and behavior`);
      lines.push('');
    }
  }

  // Methods
  const methods = tasks.filter(t => t.type === 'method');
  if (methods.length > 0) {
    lines.push(`### Methods`);
    lines.push('');
    for (const method of methods) {
      const params = method.params?.join(', ') || '';
      lines.push(`#### \`${method.name}(${params})\``);
      lines.push('');
      lines.push(`- [ ] C++: Implement method logic per W3C spec algorithm`);
      if (method.returnType.includes('Promise')) {
        lines.push(`- [ ] C++: Use AsyncWorker for non-blocking execution`);
      }
      lines.push(`- [ ] C++: Handle error cases with proper DOMException types`);
      lines.push(`- [ ] TS: Wire method to native implementation`);
      lines.push(`- [ ] Test: Write test case for happy path`);
      lines.push(`- [ ] Test: Write test case for error conditions`);
      lines.push('');
    }
  }

  // Static Methods
  const staticMethods = tasks.filter(t => t.type === 'static-method');
  if (staticMethods.length > 0) {
    lines.push(`### Static Methods`);
    lines.push('');
    for (const method of staticMethods) {
      const params = method.params?.join(', ') || '';
      lines.push(`#### \`${interfaceName}.${method.name}(${params})\``);
      lines.push('');
      lines.push(`- [ ] C++: Implement as StaticMethod on class`);
      lines.push(`- [ ] C++: Return Promise with appropriate result type`);
      lines.push(`- [ ] TS: Expose as static method on class`);
      lines.push(`- [ ] Test: Verify supported configurations`);
      lines.push('');
    }
  }

  // Memory Management (if applicable)
  if (['VideoFrame', 'AudioData', 'EncodedVideoChunk', 'EncodedAudioChunk'].includes(interfaceName)) {
    lines.push(`### Memory Management`);
    lines.push('');
    lines.push(`- [ ] Implement \`close()\` to free native resources immediately`);
    lines.push(`- [ ] Add C++ destructor as fallback for GC`);
    lines.push(`- [ ] Test: Verify no memory leaks under stress`);
    lines.push(`- [ ] Test: Verify \`close()\` is idempotent`);
    lines.push('');
  }

  lines.push(`---`);
  lines.push('');
  lines.push(`## Verification`);
  lines.push('');
  lines.push('```bash');
  lines.push(`npm run build && npm test -- --grep "${interfaceName}"`);
  lines.push('```');
  lines.push('');

  return lines.join('\n');
}

async function main() {
  // Ensure output directory exists
  if (!fs.existsSync(OUTPUT_DIR)) {
    fs.mkdirSync(OUTPUT_DIR, { recursive: true });
  }

  // Read and parse IDL
  const idlContent = fs.readFileSync(IDL_PATH, 'utf-8');
  const ast = webidl2.parse(idlContent);

  let interfaceCount = 0;
  let taskCount = 0;

  for (const node of ast) {
    if (node.type === 'interface') {
      const interfaceName = node.name;
      const tasks: TaskItem[] = [];

      for (const member of node.members) {
        const task = extractTasks(member);
        if (task) {
          tasks.push(task);
          taskCount++;
        }
      }

      if (tasks.length > 0) {
        const markdown = generateTaskMarkdown(interfaceName, tasks);
        const outputPath = path.join(OUTPUT_DIR, `${interfaceName}.md`);
        fs.writeFileSync(outputPath, markdown);
        console.log(`Generated: ${outputPath} (${tasks.length} tasks)`);
        interfaceCount++;
      }
    }
  }

  console.log(`\nDone! Generated ${interfaceCount} task files with ${taskCount} total tasks.`);
}

main().catch(console.error);
