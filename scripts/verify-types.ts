/**
 * Verification script to ensure generated TypeScript types match the WebCodecs IDL
 */

import * as fs from 'node:fs';
import * as path from 'node:path';
// @ts-expect-error - webidl2 has no @types package
import { parse as parseIDL } from 'webidl2';

const idlPath = path.join(__dirname, '..', 'spec', 'context', '_webcodecs.idl');
const tsPath = path.join(__dirname, '..', 'types', 'webcodecs.d.ts');

const idlContent = fs.readFileSync(idlPath, 'utf-8');
const tsContent = fs.readFileSync(tsPath, 'utf-8');

const ast = parseIDL(idlContent);

let errors: string[] = [];
let warnings: string[] = [];
let checked = 0;

// Helper to check if a type is defined in TS
function typeIsDefined(typeName: string): boolean {
  // Check for type/interface/export declarations
  const patterns = [
    new RegExp(`export type ${typeName}\\s*=`),
    new RegExp(`export interface ${typeName}\\s*[{<]`),
    new RegExp(`export interface ${typeName} extends`),
  ];
  return patterns.some(p => p.test(tsContent));
}

// Helper to check if a member exists on an interface/type
function memberExists(interfaceName: string, memberName: string, memberType: 'attribute' | 'method'): boolean {
  // Find the interface block - handle nested braces by matching balanced braces
  // Use word boundary or specific patterns to avoid partial matches (e.g., ImageTrack vs ImageTrackList)
  const startPattern = new RegExp(`export interface ${interfaceName}(?:\\s+extends\\s+\\w+)?\\s*\\{`);
  const startMatch = tsContent.match(startPattern);
  if (!startMatch || startMatch.index === undefined) return false;

  // Find the matching closing brace
  let braceCount = 1;
  let pos = startMatch.index + startMatch[0].length;
  const bodyStart = pos;

  while (braceCount > 0 && pos < tsContent.length) {
    if (tsContent[pos] === '{') braceCount++;
    else if (tsContent[pos] === '}') braceCount--;
    pos++;
  }

  const body = tsContent.substring(bodyStart, pos - 1);

  if (memberType === 'attribute') {
    // Check for "memberName:" or "readonly memberName:"
    const attrPattern = new RegExp(`(readonly\\s+)?${memberName}\\s*[?]?\\s*:`);
    return attrPattern.test(body);
  } else {
    // Check for "memberName(" including "static memberName("
    const methodPattern = new RegExp(`(static\\s+)?${memberName}\\s*\\(`);
    return methodPattern.test(body);
  }
}

console.log('=== WebCodecs Type Verification ===\n');

// Check all interfaces
console.log('Checking interfaces...');
for (const item of ast) {
  if (item.type === 'interface') {
    const name = item.name;
    if (['Window', 'Worker', 'DedicatedWorker', 'SharedWorker'].includes(name)) continue;

    checked++;
    if (!typeIsDefined(name)) {
      errors.push(`Interface ${name} is not defined in TypeScript`);
    } else {
      // Check members
      for (const member of item.members || []) {
        if (member.type === 'attribute') {
          checked++;
          if (!memberExists(name, member.name, 'attribute')) {
            errors.push(`${name}.${member.name} attribute is missing`);
          }
        } else if (member.type === 'operation' && member.name) {
          checked++;
          if (!memberExists(name, member.name, 'method')) {
            errors.push(`${name}.${member.name}() method is missing`);
          }
        }
      }
    }
  }
}

// Check all dictionaries
console.log('Checking dictionaries...');
for (const item of ast) {
  if (item.type === 'dictionary') {
    checked++;
    if (!typeIsDefined(item.name)) {
      errors.push(`Dictionary ${item.name} is not defined in TypeScript`);
    }
  }
}

// Check all enums
console.log('Checking enums...');
for (const item of ast) {
  if (item.type === 'enum') {
    checked++;
    if (!typeIsDefined(item.name)) {
      errors.push(`Enum ${item.name} is not defined in TypeScript`);
    } else {
      // Verify enum values
      const enumRegex = new RegExp(`export type ${item.name}\\s*=\\s*([^;]+);`);
      const match = tsContent.match(enumRegex);
      if (match) {
        const tsValues = match[1];
        for (const val of item.values) {
          if (!tsValues.includes(`"${val.value}"`)) {
            errors.push(`Enum ${item.name} is missing value "${val.value}"`);
          }
        }
      }
    }
  }
}

// Check all typedefs
console.log('Checking typedefs...');
for (const item of ast) {
  if (item.type === 'typedef') {
    checked++;
    if (!typeIsDefined(item.name)) {
      errors.push(`Typedef ${item.name} is not defined in TypeScript`);
    }
  }
}

// Check all callbacks
console.log('Checking callbacks...');
for (const item of ast) {
  if (item.type === 'callback') {
    checked++;
    if (!typeIsDefined(item.name)) {
      errors.push(`Callback ${item.name} is not defined in TypeScript`);
    }
  }
}

// Check polyfill types
console.log('Checking polyfill types...');
const requiredPolyfills = ['BufferSource', 'AllowSharedBufferSource', 'BitrateMode', 'EventHandler', 'DOMRectInit', 'DOMRectReadOnly'];
for (const polyfill of requiredPolyfills) {
  checked++;
  if (!typeIsDefined(polyfill)) {
    errors.push(`Required polyfill type ${polyfill} is missing`);
  }
}

// Check specific type correctness
console.log('Checking type correctness...');

// Check AllowSharedBufferSource includes SharedArrayBuffer
checked++;
if (!tsContent.includes('AllowSharedBufferSource = ArrayBufferView | ArrayBuffer | SharedArrayBuffer')) {
  errors.push('AllowSharedBufferSource does not include SharedArrayBuffer');
}

// Check BitrateMode values
checked++;
if (!tsContent.includes('BitrateMode = "constant" | "variable"')) {
  errors.push('BitrateMode does not have correct values');
}

// Check nullable types are correct (not 'any')
const nullableChecks = [
  { interface: 'VideoColorSpace', member: 'primaries', expected: 'VideoColorPrimaries | null' },
  { interface: 'VideoColorSpace', member: 'transfer', expected: 'VideoTransferCharacteristics | null' },
  { interface: 'VideoColorSpace', member: 'matrix', expected: 'VideoMatrixCoefficients | null' },
  { interface: 'VideoColorSpace', member: 'fullRange', expected: 'boolean | null' },
  { interface: 'AudioData', member: 'format', expected: 'AudioSampleFormat | null' },
  { interface: 'VideoFrame', member: 'format', expected: 'VideoPixelFormat | null' },
  { interface: 'VideoFrame', member: 'codedRect', expected: 'DOMRectReadOnly | null' },
  { interface: 'VideoFrame', member: 'visibleRect', expected: 'DOMRectReadOnly | null' },
  { interface: 'VideoFrame', member: 'duration', expected: 'number | null' },
  { interface: 'EncodedAudioChunk', member: 'duration', expected: 'number | null' },
  { interface: 'EncodedVideoChunk', member: 'duration', expected: 'number | null' },
  { interface: 'ImageTrackList', member: 'selectedTrack', expected: 'ImageTrack | null' },
];

for (const check of nullableChecks) {
  checked++;
  const regex = new RegExp(`${check.member}:\\s*${check.expected.replace(/[|]/g, '\\|').replace(/\s+/g, '\\s*')}`);
  if (!regex.test(tsContent)) {
    errors.push(`${check.interface}.${check.member} should be ${check.expected}`);
  }
}

// Print results
console.log('\n=== Verification Results ===\n');
console.log(`Checked: ${checked} items`);
console.log(`Errors: ${errors.length}`);
console.log(`Warnings: ${warnings.length}`);

if (errors.length > 0) {
  console.log('\n❌ ERRORS:');
  errors.forEach(e => console.log(`  - ${e}`));
}

if (warnings.length > 0) {
  console.log('\n⚠️  WARNINGS:');
  warnings.forEach(w => console.log(`  - ${w}`));
}

if (errors.length === 0 && warnings.length === 0) {
  console.log('\n✅ All checks passed! TypeScript types are compliant with WebCodecs IDL.');
}

process.exit(errors.length > 0 ? 1 : 0);
