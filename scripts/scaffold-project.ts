/**
 * Spec-First Code Generator for WebCodecs
 *
 * Fetches w3c/webcodecs spec directly, parses Bikeshed HTML + WebIDL,
 * generates C++ headers/sources, TypeScript wrappers, and spec context docs.
 *
 * Usage:
 *   npm run scaffold                       # Generate files, warn about binding.gyp
 *   npm run scaffold -- --write            # Also update binding.gyp sources
 *   npm run scaffold -- --force-ts         # Regenerate TypeScript wrappers with proper types
 *   npm run scaffold -- --write --force-ts # Both
 */

import * as fs from 'node:fs/promises';
import * as path from 'node:path';
import { JSDOM } from 'jsdom';
import TurndownService from 'turndown';
import { parse as parseIDL, type InterfaceType, type OperationMemberType, type AttributeMemberType, type IDLRootType } from 'webidl2';

// --- CLI Arguments ---
const args = process.argv.slice(2);
const WRITE_BINDING_GYP = args.includes('--write');
const FORCE_TS = args.includes('--force-ts');

// --- Configuration ---
const SPEC_URL = 'https://raw.githubusercontent.com/w3c/webcodecs/main/index.src.html';
const ROOT_DIR = path.resolve(__dirname, '..');
const CONTEXT_DIR = path.join(ROOT_DIR, 'spec', 'context');
// FLAT STRUCTURE: src/ and lib/ directly (no generated/ subfolder)
const SRC_DIR = path.join(ROOT_DIR, 'src');
const LIB_DIR = path.join(ROOT_DIR, 'lib');
const BINDING_GYP_PATH = path.join(ROOT_DIR, 'binding.gyp');
const TYPES_DIR = path.join(ROOT_DIR, 'types');

// --- Type Definitions ---
interface MethodContext {
  name: string;
  signature: string;
  steps: string[];
  isStatic: boolean;
  args: { name: string; type: string; tsType: string }[];
  returnType: string;
  tsReturnType: string;
}

interface AttributeContext {
  name: string;
  type: string;
  tsType: string;
  readonly: boolean;
}

interface InterfaceContext {
  name: string;
  desc: string;
  methods: MethodContext[];
  attributes: AttributeContext[];
  hasConstructor: boolean;
}

// --- Type Mapper (WebIDL -> C++) ---
const TYPE_MAP: Record<string, string> = {
  'void': 'void',
  'undefined': 'void',
  'boolean': 'bool',
  'byte': 'int8_t',
  'octet': 'uint8_t',
  'short': 'int16_t',
  'unsigned short': 'uint16_t',
  'long': 'int32_t',
  'unsigned long': 'uint32_t',
  'long long': 'int64_t',
  'unsigned long long': 'uint64_t',
  'float': 'float',
  'unrestricted float': 'float',
  'double': 'double',
  'unrestricted double': 'double',
  'DOMString': 'std::string',
  'USVString': 'std::string',
  'ByteString': 'std::string',
  'BufferSource': 'Napi::ArrayBuffer',
  'ArrayBuffer': 'Napi::ArrayBuffer',
  'ArrayBufferView': 'Napi::TypedArray',
  'Uint8Array': 'Napi::Uint8Array',
  'Promise': 'Napi::Promise',
  'sequence': 'std::vector',
  'object': 'Napi::Object',
  'any': 'Napi::Value',
};

function getCppType(idlType: string | { idlType: string | string[] }): string {
  if (typeof idlType === 'object') {
    if (Array.isArray(idlType.idlType)) {
      return 'Napi::Value'; // Complex union type
    }
    return getCppType(idlType.idlType);
  }
  return TYPE_MAP[idlType] || `${idlType}*`; // Assume pointer for interface types
}

// --- Main Execution ---
async function main() {
  console.log(`[Scaffold] 🚀 Fetching Source of Truth from ${SPEC_URL}...`);

  // 1. Direct Fetch (No Git Clone)
  const response = await fetch(SPEC_URL);
  if (!response.ok) {
    throw new Error(`Failed to fetch spec: ${response.status} ${response.statusText}`);
  }
  const htmlContent = await response.text();
  console.log(`[Scaffold] ✅ Fetched spec (${(htmlContent.length / 1024).toFixed(1)} KB)`);

  // 2. Prepare Directories
  await fs.mkdir(CONTEXT_DIR, { recursive: true });
  await fs.mkdir(SRC_DIR, { recursive: true });
  await fs.mkdir(LIB_DIR, { recursive: true });
  await fs.mkdir(TYPES_DIR, { recursive: true });

  // 3. Parse Spec
  const { idlText, algorithmMap, interfaceDescriptions } = parseBikeshed(htmlContent);

  // Write raw IDL for reference
  await fs.writeFile(path.join(CONTEXT_DIR, '_webcodecs.idl'), idlText);

  let idlAst: IDLRootType[];
  try {
    idlAst = parseIDL(idlText);
  } catch (err) {
    console.error('[Scaffold] ❌ Failed to parse IDL:', err);
    process.exit(1);
  }

  // 4. Generate Artifacts for EVERY Interface
  const generatedClasses: string[] = [];
  const skippedInterfaces = ['Window', 'Worker', 'DedicatedWorker', 'SharedWorker'];

  for (const item of idlAst) {
    if (item.type !== 'interface') continue;
    const iface = item as InterfaceType;
    const name = iface.name;

    // Skip global contexts
    if (skippedInterfaces.includes(name)) continue;

    console.log(`[Scaffold] Processing Interface: ${name}`);
    generatedClasses.push(name);

    const context = buildInterfaceContext(iface, algorithmMap, interfaceDescriptions);

    // A. Always Overwrite Spec Context (Truth for Validation Agent)
    await fs.writeFile(
      path.join(CONTEXT_DIR, `${name}.md`),
      generateMarkdownContext(context)
    );

    // B. Generate Code Skeletons (Only if missing)
    await safeWrite(path.join(SRC_DIR, `${name}.h`), generateCppHeader(context));
    await safeWrite(path.join(SRC_DIR, `${name}.cpp`), generateCppSource(context));
    await safeWrite(path.join(LIB_DIR, `${name}.ts`), generateTsSource(context), FORCE_TS);
  }

  // 5. Generate TypeScript index with type re-exports
  const indexContent = [
    '// Re-export all types from the generated type definitions',
    "// Using 'export type *' ensures this is compile-time only (no runtime import)",
    "export type * from '../types/webcodecs';",
    '',
    '// Export class implementations',
    ...generatedClasses.map(c => `export { ${c} } from './${c}';`),
    '',
  ].join('\n');
  await fs.writeFile(path.join(LIB_DIR, 'index.ts'), indexContent);

  // 5b. Generate TypeScript type definitions from full IDL AST
  const typesContent = generateTypeDefinitions(idlAst);
  await fs.writeFile(path.join(TYPES_DIR, 'webcodecs.d.ts'), typesContent);
  console.log('[Scaffold] ✅ Generated types/webcodecs.d.ts');

  // 6. Update or warn about binding.gyp
  const generatedSources = generatedClasses.map(c => `src/${c}.cpp`);

  if (WRITE_BINDING_GYP) {
    await updateBindingGyp(generatedSources);
  } else {
    console.log('\n⚠️  Ensure the following files are in your binding.gyp "sources" list:');
    generatedClasses.forEach(c => console.log(`      "src/${c}.cpp",`));
    console.log('\n💡 Run with --write to auto-update binding.gyp:');
    console.log('   npm run scaffold -- --write');
  }

  // 7. Summary
  console.log('\n[Scaffold] ✅ Generation complete!');
  console.log(`\n📁 Generated ${generatedClasses.length} interfaces:`);
  generatedClasses.forEach(c => console.log(`   - ${c}`));

  console.log('\n📝 Spec context written to: spec/context/');
  console.log('📝 Raw IDL written to: spec/context/_webcodecs.idl');
}

/**
 * Update binding.gyp to include generated source files
 */
async function updateBindingGyp(generatedSources: string[]): Promise<void> {
  if (!await fileExists(BINDING_GYP_PATH)) {
    console.warn('[Scaffold] ⚠️  binding.gyp not found, skipping update');
    return;
  }

  const content = await fs.readFile(BINDING_GYP_PATH, 'utf-8');
  let gyp: { targets: Array<{ target_name: string; sources: string[] }> };

  try {
    // binding.gyp is JSON (or JSON5-ish), parse it
    gyp = JSON.parse(content);
  } catch {
    console.error('[Scaffold] ❌ Failed to parse binding.gyp as JSON');
    return;
  }

  const target = gyp.targets?.find(t => t.target_name === 'webcodecs');
  if (!target) {
    console.error('[Scaffold] ❌ No "webcodecs" target found in binding.gyp');
    return;
  }

  // Get existing manual sources (addon.cpp, etc.)
  const manualSources = target.sources.filter(
    s => s === 'src/addon.cpp' || s.startsWith('src/shared/')
  );

  // Merge: manual sources + generated sources (deduped)
  const allSources = new Set([...manualSources, ...generatedSources]);
  target.sources = Array.from(allSources);

  // Write back with pretty formatting
  await fs.writeFile(BINDING_GYP_PATH, JSON.stringify(gyp, null, 2) + '\n');
  console.log('[Scaffold] ✅ Updated binding.gyp with generated sources');
}

// --- Helpers ---

async function fileExists(p: string): Promise<boolean> {
  try {
    await fs.access(p);
    return true;
  } catch {
    return false;
  }
}

async function safeWrite(p: string, content: string, force = false): Promise<void> {
  if (!force && await fileExists(p)) {
    console.warn(`[Scaffold] ⚠️  Skipping ${path.basename(p)} (File exists)`);
  } else {
    await fs.writeFile(p, content);
    console.log(`[Scaffold] ${force ? '🔄 Regenerated' : '➕ Created'} ${path.basename(p)}`);
  }
}

function buildInterfaceContext(
  iface: InterfaceType,
  algoMap: Map<string, string[]>,
  descMap: Map<string, string>
): InterfaceContext {
  const methods: MethodContext[] = [];
  const attributes: AttributeContext[] = [];
  let hasConstructor = false;

  for (const member of iface.members) {
    if (member.type === 'operation' && member.name) {
      const op = member as OperationMemberType;
      const key = `${iface.name}.${op.name}`;
      const args = (op.arguments || []).map((arg: { name: string; idlType: unknown }) => ({
        name: arg.name,
        type: getCppType(arg.idlType as any),
        tsType: getTsType(arg.idlType as any),
      }));

      methods.push({
        name: op.name!,
        isStatic: op.special === 'static',
        signature: `${getCppType(op.idlType as any)} ${op.name}(${args.map((a: { type: string; name: string }) => `${a.type} ${a.name}`).join(', ')})`,
        steps: algoMap.get(key) || ['See spec/context file.'],
        args,
        returnType: getCppType(op.idlType as any),
        tsReturnType: getTsType(op.idlType as any),
      });
    }

    if (member.type === 'constructor') {
      hasConstructor = true;
      const key = `${iface.name}.constructor`;
      const args = ((member as { arguments?: Array<{ name: string; idlType: unknown }> }).arguments || []).map((arg: { name: string; idlType: unknown }) => ({
        name: arg.name,
        type: getCppType(arg.idlType as any),
        tsType: getTsType(arg.idlType as any),
      }));

      methods.push({
        name: 'constructor',
        isStatic: false,
        signature: `constructor(${args.map((a: { type: string; name: string }) => `${a.type} ${a.name}`).join(', ')})`,
        steps: algoMap.get(key) || ['Initialize internal slots.'],
        args,
        returnType: 'void',
        tsReturnType: 'void',
      });
    }

    if (member.type === 'attribute') {
      const attr = member as AttributeMemberType;
      attributes.push({
        name: attr.name,
        type: getCppType(attr.idlType as any),
        tsType: getTsType(attr.idlType as any),
        readonly: attr.readonly,
      });
    }
  }

  return {
    name: iface.name,
    desc: descMap.get(iface.name) || 'No description found.',
    methods,
    attributes,
    hasConstructor,
  };
}

// --- Generators ---

function generateMarkdownContext(ctx: InterfaceContext): string {
  return `# ${ctx.name} Specification

> **Source:** W3C WebCodecs (Auto-generated by scaffold-project.ts)

## Description

${ctx.desc}

## Attributes

${ctx.attributes.length === 0 ? '_None_' : ctx.attributes.map(a =>
    `* **${a.name}** (\`${a.type}\`)${a.readonly ? ' [ReadOnly]' : ''}`
  ).join('\n')}

## Methods & Algorithms

${ctx.methods.map(m => `
### ${m.name}

**Static:** ${m.isStatic}
**Signature:** \`${m.signature}\`

**Algorithm Steps:**
${m.steps.map(s => `> ${s}`).join('\n')}
`).join('\n')}
`;
}

function generateCppHeader(ctx: InterfaceContext): string {
  const className = ctx.name;
  const methods = ctx.methods.filter(m => m.name !== 'constructor');

  return `#pragma once
#include <napi.h>
#include "shared/Utils.h"

namespace webcodecs {

/**
 * ${className} - W3C WebCodecs ${className} implementation
 * @see spec/context/${className}.md
 */
class ${className} : public Napi::ObjectWrap<${className}> {
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  ${className}(const Napi::CallbackInfo& info);
  ~${className}() override;

  // RAII Release
  void Release();

private:
  static Napi::FunctionReference constructor;

  // Internal Native Handle
  // TODO(impl): Define strict handle type (e.g., AVCodecContext*)
  void* handle_ = nullptr;

  // Attributes
${ctx.attributes.map(a => `  Napi::Value Get${capitalize(a.name)}(const Napi::CallbackInfo& info);`).join('\n')}
${ctx.attributes.filter(a => !a.readonly).map(a => `  void Set${capitalize(a.name)}(const Napi::CallbackInfo& info, const Napi::Value& value);`).join('\n')}

  // Methods
${methods.map(m => m.isStatic
    ? `  static Napi::Value ${capitalize(m.name)}(const Napi::CallbackInfo& info);`
    : `  Napi::Value ${capitalize(m.name)}(const Napi::CallbackInfo& info);`
  ).join('\n')}
};

}  // namespace webcodecs
`;
}

function generateCppSource(ctx: InterfaceContext): string {
  const className = ctx.name;
  const methods = ctx.methods.filter(m => m.name !== 'constructor');
  const constr = ctx.methods.find(m => m.name === 'constructor');

  return `#include "${className}.h"

namespace webcodecs {

Napi::FunctionReference ${className}::constructor;

Napi::Object ${className}::Init(Napi::Env env, Napi::Object exports) {
  Napi::Function func = DefineClass(env, "${className}", {
${ctx.attributes.map(a =>
    `    InstanceAccessor<&${className}::Get${capitalize(a.name)}${a.readonly ? '' : `, &${className}::Set${capitalize(a.name)}`}>("${a.name}"),`
  ).join('\n')}
${methods.filter(m => !m.isStatic).map(m =>
    `    InstanceMethod<&${className}::${capitalize(m.name)}>("${m.name}"),`
  ).join('\n')}
${methods.filter(m => m.isStatic).map(m =>
    `    StaticMethod<&${className}::${capitalize(m.name)}>("${m.name}"),`
  ).join('\n')}
  });

  constructor = Napi::Persistent(func);
  constructor.SuppressDestruct();
  exports.Set("${className}", func);
  return exports;
}

${className}::${className}(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<${className}>(info) {
  Napi::Env env = info.Env();

  // [SPEC] Constructor Algorithm
  /*
${constr ? constr.steps.map(s => `   * ${s}`).join('\n') : '   * Refer to spec context.'}
   */

  // TODO(impl): Implement Constructor & Resource Allocation
}

${className}::~${className}() {
  Release();
}

void ${className}::Release() {
  // TODO(impl): Free handle_ and native resources
  handle_ = nullptr;
}

// --- Attributes ---
${ctx.attributes.map(a => `
Napi::Value ${className}::Get${capitalize(a.name)}(const Napi::CallbackInfo& info) {
  // TODO(impl): Return ${a.name}
  return info.Env().Null();
}
${!a.readonly ? `
void ${className}::Set${capitalize(a.name)}(const Napi::CallbackInfo& info, const Napi::Value& value) {
  // TODO(impl): Set ${a.name}
}
` : ''}`).join('')}

// --- Methods ---
${methods.map(m => `
Napi::Value ${className}::${capitalize(m.name)}(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Algorithm
  /*
${m.steps.map(s => `   * ${s}`).join('\n')}
   */

  // TODO(impl): Implement method logic
  return env.Undefined();
}
`).join('')}
}  // namespace webcodecs
`;
}

function generateTsSource(ctx: InterfaceContext): string {
  const methods = ctx.methods.filter(m => m.name !== 'constructor');
  const imports = collectTypeImports(ctx);

  // Build import line if there are types to import
  const importLine = imports.length > 0
    ? `import type { ${imports.join(', ')} } from '../types/webcodecs';\n\n`
    : '';

  // Determine constructor parameter type
  const initType = ctx.hasConstructor ? `${ctx.name}Init` : 'unknown';

  return `/**
 * ${ctx.name} - TypeScript wrapper for native ${ctx.name}
 * @see spec/context/${ctx.name}.md
 */

${importLine}// eslint-disable-next-line @typescript-eslint/no-var-requires
const bindings = require('bindings')('webcodecs');

export class ${ctx.name} {
  private readonly _native: unknown;

  constructor(init: ${initType}) {
    this._native = new bindings.${ctx.name}(init);
  }
${ctx.attributes.map(a => `
  get ${a.name}(): ${a.tsType} {
    return (this._native as Record<string, unknown>).${a.name} as ${a.tsType};
  }
${!a.readonly ? `
  set ${a.name}(value: ${a.tsType}) {
    (this._native as Record<string, unknown>).${a.name} = value;
  }
` : ''}`).join('')}
${methods.filter(m => !m.isStatic).map(m => `
  ${m.name}(${m.args.map(a => `${a.name}: ${a.tsType}`).join(', ')}): ${m.tsReturnType} {
    return (this._native as Record<string, Function>).${m.name}(${m.args.map(a => a.name).join(', ')}) as ${m.tsReturnType};
  }
`).join('')}
${methods.filter(m => m.isStatic).map(m => `
  static ${m.name}(${m.args.map(a => `${a.name}: ${a.tsType}`).join(', ')}): ${m.tsReturnType} {
    return bindings.${ctx.name}.${m.name}(${m.args.map(a => a.name).join(', ')}) as ${m.tsReturnType};
  }
`).join('')}}
`;
}

function capitalize(s: string): string {
  return s.charAt(0).toUpperCase() + s.slice(1);
}

// --- TypeScript Wrapper Type Import Helpers ---

/**
 * Extract type names from a TypeScript type string.
 * Handles complex types like "Promise<VideoDecoderSupport>" and "EncodedVideoChunk | null".
 */
function extractTypeNames(typeStr: string, set: Set<string>): void {
  // Extract PascalCase type names (interfaces, classes, enums)
  const matches = typeStr.match(/[A-Z][a-zA-Z0-9]*/g);
  if (matches) {
    for (const match of matches) {
      set.add(match);
    }
  }
}

/**
 * Collect all type imports needed for a TypeScript wrapper class.
 * Returns an array of type names to import from webcodecs.d.ts.
 */
function collectTypeImports(ctx: InterfaceContext): string[] {
  const types = new Set<string>();

  // Init type for constructor (e.g., VideoDecoderInit)
  if (ctx.hasConstructor) {
    types.add(`${ctx.name}Init`);
  }

  // Attribute types
  for (const attr of ctx.attributes) {
    extractTypeNames(attr.tsType, types);
  }

  // Method types (params and return types)
  for (const method of ctx.methods) {
    if (method.name !== 'constructor') {
      extractTypeNames(method.tsReturnType, types);
      for (const arg of method.args) {
        extractTypeNames(arg.tsType, types);
      }
    }
  }

  // Filter out primitives, built-ins, and the class name itself
  const primitives = new Set([
    'void', 'boolean', 'number', 'string', 'any', 'unknown', 'null', 'undefined',
    'Promise', 'Array', 'ArrayBuffer', 'Uint8Array', 'SharedArrayBuffer',
    ctx.name, // Don't import the class we're generating
  ]);
  return Array.from(types).filter(t => !primitives.has(t)).sort();
}

// --- TypeScript Type Definition Generator ---

// WebIDL to TypeScript type mapping
const TS_TYPE_MAP: Record<string, string> = {
  'void': 'void',
  'undefined': 'void',
  'boolean': 'boolean',
  'byte': 'number',
  'octet': 'number',
  'short': 'number',
  'unsigned short': 'number',
  'long': 'number',
  'unsigned long': 'number',
  'long long': 'number',
  'unsigned long long': 'number',
  'float': 'number',
  'unrestricted float': 'number',
  'double': 'number',
  'unrestricted double': 'number',
  'DOMString': 'string',
  'USVString': 'string',
  'ByteString': 'string',
  'BufferSource': 'BufferSource',
  'AllowSharedBufferSource': 'AllowSharedBufferSource',
  'ArrayBuffer': 'ArrayBuffer',
  'ArrayBufferView': 'ArrayBufferView',
  'Uint8Array': 'Uint8Array',
  'ReadableStream': 'ReadableStream',
  'object': 'object',
  'any': 'any',
  'EventHandler': 'EventHandler',
  'EventTarget': 'EventTarget',
  'DOMException': 'DOMException',
  'DOMRectReadOnly': 'DOMRectReadOnly',
  'DOMRectInit': 'DOMRectInit',
  'CanvasImageSource': 'CanvasImageSource',
  // WebCodecs-specific types that should be preserved (not converted to 'any')
  'BitrateMode': 'BitrateMode',
};

function getTsType(idlType: any): string {
  if (!idlType) return 'any';

  // Handle nullable types
  // NOTE: webidl2 uses getters for properties like nullable, idlType, generic, union
  // Object spread doesn't copy getters, so we must access them explicitly
  if (idlType.nullable) {
    // Get the inner type directly instead of spreading
    const innerType = idlType.idlType;
    if (typeof innerType === 'string') {
      // Simple nullable type like "VideoColorPrimaries?"
      const baseType = TS_TYPE_MAP[innerType] || innerType;
      return `${baseType} | null`;
    }
    // Complex nullable type - recurse with the inner type
    const baseType = getTsType(innerType);
    return `${baseType} | null`;
  }

  // Handle union types
  if (idlType.union) {
    return idlType.idlType.map((t: any) => getTsType(t)).join(' | ');
  }

  // Handle Promise<T>
  if (idlType.generic === 'Promise') {
    const inner = idlType.idlType?.[0];
    return `Promise<${inner ? getTsType(inner) : 'void'}>`;
  }

  // Handle sequence<T>
  if (idlType.generic === 'sequence') {
    const inner = idlType.idlType?.[0];
    return `${getTsType(inner)}[]`;
  }

  // Handle FrozenArray<T>
  if (idlType.generic === 'FrozenArray') {
    const inner = idlType.idlType?.[0];
    return `readonly ${getTsType(inner)}[]`;
  }

  // Handle record<K, V>
  if (idlType.generic === 'record') {
    const [keyType, valType] = idlType.idlType || [];
    return `Record<${getTsType(keyType)}, ${getTsType(valType)}>`;
  }

  // Simple type name
  const typeName = typeof idlType === 'string' ? idlType : idlType.idlType;
  if (typeof typeName === 'string') {
    return TS_TYPE_MAP[typeName] || typeName;
  }

  // Nested type
  if (Array.isArray(typeName)) {
    return typeName.map((t: any) => getTsType(t)).join(' | ');
  }

  return getTsType(typeName);
}

function generateTypeDefinitions(ast: any[]): string {
  // Collect all type names for exports
  const exportNames = ast
    .filter((item: any) =>
      ['enum', 'typedef', 'callback', 'dictionary', 'interface'].includes(item.type) &&
      !['Window', 'Worker', 'DedicatedWorker', 'SharedWorker'].includes(item.name)
    )
    .map((item: any) => item.name);

  const lines: string[] = [
    '/**',
    ' * WebCodecs API Type Definitions for Node.js',
    ' * Auto-generated from W3C WebIDL specification',
    ' * ',
    ' * @packageDocumentation',
    ' * @see https://www.w3.org/TR/webcodecs/',
    ' */',
    '',
    '// --- Required DOM polyfill types for Node.js ---',
    '// These types are needed because Node.js doesn\'t have DOM globals',
    '',
    'export type BufferSource = ArrayBufferView | ArrayBuffer;',
    'export type AllowSharedBufferSource = ArrayBufferView | ArrayBuffer | SharedArrayBuffer;',
    'export type EventHandler = ((event: Event) => void) | null;',
    '',
    '// BitrateMode is defined in MediaStream Recording spec, used by AudioEncoderConfig',
    'export type BitrateMode = "constant" | "variable";',
    '',
    'export interface DOMRectInit {',
    '  height?: number;',
    '  width?: number;',
    '  x?: number;',
    '  y?: number;',
    '}',
    '',
    'export interface DOMRectReadOnly {',
    '  readonly bottom: number;',
    '  readonly height: number;',
    '  readonly left: number;',
    '  readonly right: number;',
    '  readonly top: number;',
    '  readonly width: number;',
    '  readonly x: number;',
    '  readonly y: number;',
    '}',
    '',
    '// Placeholder types for browser APIs not available in Node.js',
    'export type CanvasImageSource = unknown;',
    'export type ImageBitmap = unknown;',
    'export type OffscreenCanvas = unknown;',
    'export type PredefinedColorSpace = "display-p3" | "srgb";',
    'export type ColorSpaceConversion = "default" | "none";',
    '',
    '// --- WebCodecs Types (from W3C spec) ---',
    '',
  ];

  // Process each item in the AST
  for (const item of ast) {
    switch (item.type) {
      case 'enum':
        lines.push(generateEnumType(item));
        break;
      case 'typedef':
        lines.push(generateTypedef(item));
        break;
      case 'callback':
        lines.push(generateCallback(item));
        break;
      case 'dictionary':
        lines.push(generateDictionary(item));
        break;
      case 'interface':
        // Skip browser globals
        if (!['Window', 'Worker', 'DedicatedWorker', 'SharedWorker'].includes(item.name)) {
          lines.push(generateInterface(item));
        }
        break;
    }
  }

  // Add module declaration for ambient types (prevents conflicts with lib.dom.d.ts)
  lines.push('');
  lines.push('// --- Module augmentation for Node.js global scope ---');
  lines.push('declare global {');
  lines.push('  // WebCodecs types are available globally when this module is imported');
  exportNames.forEach(name => {
    lines.push(`  // type ${name} is available`);
  });
  lines.push('}');
  lines.push('');

  return lines.join('\n');
}

function generateEnumType(item: any): string {
  const values = item.values.map((v: any) => `"${v.value}"`).join(' | ');
  return `export type ${item.name} = ${values};\n`;
}

function generateTypedef(item: any): string {
  const tsType = getTsType(item.idlType);
  return `export type ${item.name} = ${tsType};\n`;
}

function generateCallback(item: any): string {
  const params = (item.arguments || [])
    .map((arg: any) => `${arg.name}${arg.optional ? '?' : ''}: ${getTsType(arg.idlType)}`)
    .join(', ');
  const returnType = getTsType(item.idlType);
  return `export type ${item.name} = (${params}) => ${returnType};\n`;
}

function generateDictionary(item: any): string {
  const lines: string[] = [];

  // Handle inheritance
  const ext = item.inheritance ? ` extends ${item.inheritance}` : '';
  lines.push(`export interface ${item.name}${ext} {`);

  for (const member of item.members || []) {
    if (member.type === 'field') {
      const optional = member.required ? '' : '?';
      const tsType = getTsType(member.idlType);
      lines.push(`  ${member.name}${optional}: ${tsType};`);
    }
  }

  lines.push('}');
  lines.push('');
  return lines.join('\n');
}

function generateInterface(item: any): string {
  const lines: string[] = [];

  // Handle inheritance
  const ext = item.inheritance ? ` extends ${item.inheritance}` : '';
  lines.push(`export interface ${item.name}${ext} {`);

  for (const member of item.members || []) {
    switch (member.type) {
      case 'attribute': {
        const readonly = member.readonly ? 'readonly ' : '';
        const tsType = getTsType(member.idlType);
        lines.push(`  ${readonly}${member.name}: ${tsType};`);
        break;
      }
      case 'operation': {
        if (member.name) {
          const params = (member.arguments || [])
            .map((arg: any) => `${arg.name}${arg.optional ? '?' : ''}: ${getTsType(arg.idlType)}`)
            .join(', ');
          const returnType = getTsType(member.idlType);
          // Static methods cannot be declared in interfaces in TypeScript
          // Comment them out like constructors, or omit the 'static' keyword
          // For type-checking purposes, static methods are available on the class, not instances
          if (member.special === 'static') {
            lines.push(`  // static ${member.name}(${params}): ${returnType};`);
          } else {
            lines.push(`  ${member.name}(${params}): ${returnType};`);
          }
        }
        break;
      }
      case 'constructor': {
        const params = (member.arguments || [])
          .map((arg: any) => `${arg.name}${arg.optional ? '?' : ''}: ${getTsType(arg.idlType)}`)
          .join(', ');
        // Note: constructor declarations use 'new' in interfaces
        lines.push(`  // constructor(${params})`);
        break;
      }
    }
  }

  lines.push('}');
  lines.push('');
  return lines.join('\n');
}

// --- Parser Implementation ---

function parseBikeshed(html: string): {
  idlText: string;
  algorithmMap: Map<string, string[]>;
  interfaceDescriptions: Map<string, string>;
} {
  const dom = new JSDOM(html);
  const doc = dom.window.document;
  const turndown = new TurndownService();

  // 1. Extract IDL Blocks (both <pre class='idl'> and <xmp class='idl'>)
  const preIdlElements = doc.querySelectorAll('pre.idl');
  const xmpIdlElements = doc.querySelectorAll('xmp.idl');
  const idlText = [
    ...Array.from(preIdlElements).map(el => el.textContent),
    ...Array.from(xmpIdlElements).map(el => el.textContent),
  ]
    .filter(Boolean)
    .join('\n\n');

  // 2. Extract Algorithms
  const algorithmMap = new Map<string, string[]>();
  const definitions = doc.querySelectorAll('dfn');

  definitions.forEach(dfn => {
    const forAttr = dfn.getAttribute('for');
    const methodAttr = dfn.hasAttribute('method');
    const constructorAttr = dfn.hasAttribute('constructor');

    if (forAttr && (methodAttr || constructorAttr)) {
      let memberName = '';
      if (constructorAttr) {
        memberName = 'constructor';
      } else {
        const text = dfn.textContent || '';
        memberName = text.split('(')[0].trim();
      }

      const key = `${forAttr}.${memberName}`;

      // Look for the next <dd> or algorithm steps
      const container = dfn.closest('dt');
      const sibling = container?.nextElementSibling;

      if (sibling && sibling.tagName === 'DD') {
        const md = turndown.turndown(sibling.innerHTML);
        algorithmMap.set(
          key,
          md.split('\n').map(s => s.trim()).filter(Boolean)
        );
      }
    }
  });

  // 3. Extract Descriptions
  const interfaceDescriptions = new Map<string, string>();
  doc.querySelectorAll('h2, h3, h4').forEach(h => {
    const match = h.textContent?.match(/(\w+)\s+Interface/);
    if (match) {
      let desc = '';
      let next = h.nextElementSibling;
      if (next && next.tagName === 'P') {
        desc = next.textContent || '';
      }
      interfaceDescriptions.set(match[1], desc);
    }
  });

  return { idlText, algorithmMap, interfaceDescriptions };
}

// --- Run ---
main().catch(err => {
  console.error('[Scaffold] ❌ Fatal error:', err);
  process.exit(1);
});
