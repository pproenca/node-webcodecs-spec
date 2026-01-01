/**
 * @fileoverview Spec-First Code Generator for WebCodecs
 *
 * Fetches w3c/webcodecs spec directly, parses Bikeshed HTML + WebIDL,
 * generates C++ headers/sources, TypeScript wrappers, and spec context docs.
 *
 * @usage
 *   npm run scaffold                       # Generate files, warn about binding.gyp
 *   npm run scaffold -- --write            # Also update binding.gyp sources
 *   npm run scaffold -- --force-ts         # Regenerate TypeScript wrappers with proper types
 *   npm run scaffold -- --write --force-ts # Both
 */

import * as fs from 'node:fs/promises';
import * as path from 'node:path';
import { fileURLToPath } from 'node:url';
import { JSDOM } from 'jsdom';
import TurndownService from 'turndown';
import {
  parse as parseIDL,
  type InterfaceType,
  type OperationMemberType,
  type AttributeMemberType,
  type IDLRootType,
  type IDLTypeDescription,
  type Argument,
  type ConstructorMemberType,
  type DictionaryType,
  type FieldType,
  type EnumType,
  type TypedefType,
  type CallbackType,
  type IDLInterfaceMemberType,
} from 'webidl2';

// --- CLI Arguments ---
const args = process.argv.slice(2);
const WRITE_BINDING_GYP = args.includes('--write');
const FORCE_TS = args.includes('--force-ts');

// --- Configuration ---
const SPEC_URL = 'https://raw.githubusercontent.com/w3c/webcodecs/main/index.src.html';
const currentDir = path.dirname(fileURLToPath(import.meta.url));
const ROOT_DIR = path.resolve(currentDir, '..');
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
  void: 'void',
  undefined: 'void',
  boolean: 'bool',
  byte: 'int8_t',
  octet: 'uint8_t',
  short: 'int16_t',
  'unsigned short': 'uint16_t',
  long: 'int32_t',
  'unsigned long': 'uint32_t',
  'long long': 'int64_t',
  'unsigned long long': 'uint64_t',
  float: 'float',
  'unrestricted float': 'float',
  double: 'double',
  'unrestricted double': 'double',
  DOMString: 'std::string',
  USVString: 'std::string',
  ByteString: 'std::string',
  BufferSource: 'Napi::ArrayBuffer',
  ArrayBuffer: 'Napi::ArrayBuffer',
  ArrayBufferView: 'Napi::TypedArray',
  Uint8Array: 'Napi::Uint8Array',
  Promise: 'Napi::Promise',
  sequence: 'std::vector',
  object: 'Napi::Object',
  any: 'Napi::Value',
};

/**
 * Builds Bikeshed source using the CSSWG API.
 * @param source - Raw Bikeshed source HTML content
 * @returns Rendered HTML with resolved cross-references
 * @throws Error if API request fails
 */
async function buildWithBikeshed(source: string): Promise<string> {
  const formData = new FormData();
  formData.append('file', new Blob([source], { type: 'text/plain' }), 'spec.bs');
  formData.append('force', '1'); // Force build even with warnings
  formData.append('output', 'html');

  const response = await fetch('https://api.csswg.org/bikeshed/', {
    method: 'POST',
    body: formData,
  });

  if (!response.ok) {
    const errorText = await response.text();
    throw new Error(`Bikeshed API failed: ${response.status} - ${errorText}`);
  }

  return response.text();
}

/**
 * Cleans any remaining Bikeshed markup that wasn't fully rendered.
 * @param text - Text that may contain Bikeshed markup
 * @returns Cleaned text with markup converted to plain text
 */
function cleanBikeshedMarkup(text: string): string {
  return text
    // [=term=] -> term
    .replace(/\[=([^\]]+)=\]/g, '$1')
    // {{Type}} -> Type
    .replace(/\{\{([^}]+)\}\}/g, '$1')
    // |variable| -> variable
    .replace(/\|([^|]+)\|/g, '$1')
    // [[internal slot]] -> [internal slot]
    .replace(/\[\[([^\]]+)\]\]/g, '[$1]')
    // `"value"` -> "value"
    .replace(/`"([^"]+)"`/g, '"$1"')
    // Clean up escaped brackets
    .replace(/\\?\[=/g, '')
    .replace(/=\\?\]/g, '')
    .replace(/\\\[/g, '[')
    .replace(/\\\]/g, ']');
}

/**
 * Converts a WebIDL type to its C++ equivalent.
 * @param idlType - WebIDL type (string or IDLTypeDescription)
 * @returns C++ type string (e.g., 'std::string', 'Napi::Value')
 */
function getCppType(idlType: string | IDLTypeDescription): string {
  if (typeof idlType === 'string') {
    return TYPE_MAP[idlType] || `${idlType}*`; // Assume pointer for interface types
  }
  // IDLTypeDescription object
  if (idlType.union) {
    return 'Napi::Value'; // Complex union type
  }
  if (typeof idlType.idlType === 'string') {
    return getCppType(idlType.idlType);
  }
  if (Array.isArray(idlType.idlType)) {
    return 'Napi::Value'; // Complex generic type
  }
  return 'Napi::Value';
}

// --- Main Execution ---
async function main() {
  console.log(`[Scaffold] Fetching Source of Truth from ${SPEC_URL}...`);

  // 1. Direct Fetch (No Git Clone)
  const response = await fetch(SPEC_URL);
  if (!response.ok) {
    throw new Error(`Failed to fetch spec: ${response.status} ${response.statusText}`);
  }
  const rawBikeshedSource = await response.text();
  console.log(`[Scaffold] Fetched raw Bikeshed source (${(rawBikeshedSource.length / 1024).toFixed(1)} KB)`);

  // 2. Build with Bikeshed API
  console.log('[Scaffold] Building spec with Bikeshed API...');
  let htmlContent: string;
  try {
    htmlContent = await buildWithBikeshed(rawBikeshedSource);
    console.log(`[Scaffold] Built spec (${(htmlContent.length / 1024).toFixed(1)} KB)`);
  } catch (err) {
    console.warn('[Scaffold] Bikeshed API failed, falling back to raw source:', err);
    htmlContent = rawBikeshedSource;
  }

  // 3. Prepare Directories
  await fs.mkdir(CONTEXT_DIR, { recursive: true });
  await fs.mkdir(SRC_DIR, { recursive: true });
  await fs.mkdir(LIB_DIR, { recursive: true });
  await fs.mkdir(TYPES_DIR, { recursive: true });

  // 4. Parse Spec
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

  // 5. Generate Artifacts for EVERY Interface
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
    await fs.writeFile(path.join(CONTEXT_DIR, `${name}.md`), generateMarkdownContext(context));

    // B. Generate Code Skeletons (Only if missing)
    await safeWrite(path.join(SRC_DIR, `${name}.h`), generateCppHeader(context));
    await safeWrite(path.join(SRC_DIR, `${name}.cpp`), generateCppSource(context));
    await safeWrite(path.join(LIB_DIR, `${name}.ts`), generateTsSource(context), FORCE_TS);
  }

  // 6. Generate TypeScript index with type re-exports
  const indexContent = [
    '// Re-export all types from the generated type definitions',
    "// Using 'export type *' ensures this is compile-time only (no runtime import)",
    "export type * from '../types/webcodecs.js';",
    '',
    '// Export class implementations',
    ...generatedClasses.map((c) => `export { ${c} } from './${c}.js';`),
    '',
  ].join('\n');
  await fs.writeFile(path.join(LIB_DIR, 'index.ts'), indexContent);

  // 6b. Generate TypeScript type definitions from full IDL AST
  const typesContent = generateTypeDefinitions(idlAst);
  await fs.writeFile(path.join(TYPES_DIR, 'webcodecs.d.ts'), typesContent);
  console.log('[Scaffold] ✅ Generated types/webcodecs.d.ts');

  // 7. Update or warn about binding.gyp
  const generatedSources = generatedClasses.map((c) => `src/${c}.cpp`);

  if (WRITE_BINDING_GYP) {
    await updateBindingGyp(generatedSources);
  } else {
    console.log('\n⚠️  Ensure the following files are in your binding.gyp "sources" list:');
    generatedClasses.forEach((c) => console.log(`      "src/${c}.cpp",`));
    console.log('\n💡 Run with --write to auto-update binding.gyp:');
    console.log('   npm run scaffold -- --write');
  }

  // 8. Summary
  console.log('\n[Scaffold] ✅ Generation complete!');
  console.log(`\n📁 Generated ${generatedClasses.length} interfaces:`);
  generatedClasses.forEach((c) => console.log(`   - ${c}`));

  console.log('\n📝 Spec context written to: spec/context/');
  console.log('📝 Raw IDL written to: spec/context/_webcodecs.idl');
}

/**
 * Update binding.gyp to include generated source files
 */
async function updateBindingGyp(generatedSources: string[]): Promise<void> {
  if (!(await fileExists(BINDING_GYP_PATH))) {
    console.warn('[Scaffold] ⚠️  binding.gyp not found, skipping update');
    return;
  }

  const content = await fs.readFile(BINDING_GYP_PATH, 'utf-8');
  let gyp: { targets: { target_name: string; sources: string[] }[] };

  try {
    // binding.gyp is JSON (or JSON5-ish), parse it
    gyp = JSON.parse(content);
  } catch (err) {
    console.error('[Scaffold] ❌ Failed to parse binding.gyp as JSON:', err);
    return;
  }

  const target = gyp.targets?.find((t) => t.target_name === 'webcodecs');
  if (!target) {
    console.error('[Scaffold] ❌ No "webcodecs" target found in binding.gyp');
    return;
  }

  // Get existing manual sources (addon.cpp, etc.)
  const manualSources = target.sources.filter(
    (s) => s === 'src/addon.cpp' || s.startsWith('src/shared/')
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
    // fs.access throws if file doesn't exist - expected case for existence check
    return false;
  }
}

async function safeWrite(p: string, content: string, force = false): Promise<void> {
  if (!force && (await fileExists(p))) {
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
      const args = (op.arguments || []).map((arg: Argument) => ({
        name: arg.name,
        type: getCppType(arg.idlType),
        tsType: getTsType(arg.idlType),
      }));
      const returnIdlType = op.idlType;

      // Safe: op.name verified non-null by condition `member.name` at line 297
      const opName = op.name!;

      // Get algorithm steps with better fallback
      let steps = algoMap.get(key);
      if (!steps || steps.length === 0 || steps[0] === 'See spec/context file.') {
        // Try alternate key formats
        steps =
          algoMap.get(`${iface.name}.${opName}()`) ||
          algoMap.get(opName) ||
          [`Implementation follows W3C WebCodecs ${iface.name}.${opName}() specification.`];
      }

      methods.push({
        name: opName,
        isStatic: op.special === 'static',
        signature: returnIdlType
          ? `${getCppType(returnIdlType)} ${opName}(${args.map((a) => `${a.type} ${a.name}`).join(', ')})`
          : `void ${opName}(${args.map((a) => `${a.type} ${a.name}`).join(', ')})`,
        steps,
        args,
        returnType: returnIdlType ? getCppType(returnIdlType) : 'void',
        tsReturnType: returnIdlType ? getTsType(returnIdlType) : 'void',
      });
    }

    if (member.type === 'constructor') {
      hasConstructor = true;
      const ctor = member as ConstructorMemberType;
      const key = `${iface.name}.constructor`;
      const args = (ctor.arguments || []).map((arg: Argument) => ({
        name: arg.name,
        type: getCppType(arg.idlType),
        tsType: getTsType(arg.idlType),
      }));

      methods.push({
        name: 'constructor',
        isStatic: false,
        signature: `constructor(${args.map((a) => `${a.type} ${a.name}`).join(', ')})`,
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
        type: getCppType(attr.idlType),
        tsType: getTsType(attr.idlType),
        readonly: attr.readonly,
      });
    }
  }

  return {
    name: iface.name,
    desc: descMap.get(iface.name) || `${iface.name} interface from the W3C WebCodecs specification.`,
    methods,
    attributes,
    hasConstructor,
  };
}

// --- Generators ---

function generateMarkdownContext(ctx: InterfaceContext): string {
  // Build TOC
  const tocLines: string[] = ['## Table of Contents', ''];
  tocLines.push('- [Description](#description)');
  if (ctx.attributes.length > 0) {
    tocLines.push('- [Attributes](#attributes)');
  }
  if (ctx.methods.length > 0) {
    tocLines.push('- [Methods](#methods)');
    for (const m of ctx.methods) {
      const anchor = m.name.toLowerCase().replace(/[^a-z0-9]/g, '-');
      tocLines.push(`  - [${m.name}](#${anchor})`);
    }
  }
  tocLines.push('');

  return `# ${ctx.name} Specification

> **Source:** W3C WebCodecs (Auto-generated by scaffold-project.ts)

${tocLines.join('\n')}

## Description

${ctx.desc}

## Attributes

${
  ctx.attributes.length === 0
    ? '_None_'
    : ctx.attributes
        .map((a) => `- **${a.name}** (\`${a.type}\`)${a.readonly ? ' [ReadOnly]' : ''}`)
        .join('\n')
}

## Methods

${ctx.methods
  .map(
    (m) => `
### ${m.name}

${m.isStatic ? '**Static Method**\n\n' : ''}**Signature:** \`${m.signature}\`

**Algorithm:**

${m.steps.map((s, i) => {
  const cleanStep = cleanBikeshedMarkup(s);
  // Check if step already has a number prefix
  if (/^\d+\./.test(cleanStep)) {
    return cleanStep;
  }
  return `${i + 1}. ${cleanStep}`;
}).join('\n')}
`
  )
  .join('\n')}
`;
}

function generateCppHeader(ctx: InterfaceContext): string {
  const className = ctx.name;
  const methods = ctx.methods.filter((m) => m.name !== 'constructor');

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
${ctx.attributes.map((a) => `  Napi::Value Get${capitalize(a.name)}(const Napi::CallbackInfo& info);`).join('\n')}
${ctx.attributes
  .filter((a) => !a.readonly)
  .map(
    (a) =>
      `  void Set${capitalize(a.name)}(const Napi::CallbackInfo& info, const Napi::Value& value);`
  )
  .join('\n')}

  // Methods
${methods
  .map((m) =>
    m.isStatic
      ? `  static Napi::Value ${capitalize(m.name)}(const Napi::CallbackInfo& info);`
      : `  Napi::Value ${capitalize(m.name)}(const Napi::CallbackInfo& info);`
  )
  .join('\n')}
};

}  // namespace webcodecs
`;
}

function generateCppSource(ctx: InterfaceContext): string {
  const className = ctx.name;
  const methods = ctx.methods.filter((m) => m.name !== 'constructor');
  const constr = ctx.methods.find((m) => m.name === 'constructor');

  return `#include "${className}.h"

namespace webcodecs {

Napi::FunctionReference ${className}::constructor;

Napi::Object ${className}::Init(Napi::Env env, Napi::Object exports) {
  Napi::Function func = DefineClass(env, "${className}", {
${ctx.attributes
  .map(
    (a) =>
      `    InstanceAccessor<&${className}::Get${capitalize(a.name)}${a.readonly ? '' : `, &${className}::Set${capitalize(a.name)}`}>("${a.name}"),`
  )
  .join('\n')}
${methods
  .filter((m) => !m.isStatic)
  .map((m) => `    InstanceMethod<&${className}::${capitalize(m.name)}>("${m.name}"),`)
  .join('\n')}
${methods
  .filter((m) => m.isStatic)
  .map((m) => `    StaticMethod<&${className}::${capitalize(m.name)}>("${m.name}"),`)
  .join('\n')}
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
${constr ? constr.steps.map((s) => `   * ${s}`).join('\n') : '   * Refer to spec context.'}
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
${ctx.attributes
  .map(
    (a) => `
Napi::Value ${className}::Get${capitalize(a.name)}(const Napi::CallbackInfo& info) {
  // TODO(impl): Return ${a.name}
  return info.Env().Null();
}
${
  !a.readonly
    ? `
void ${className}::Set${capitalize(a.name)}(const Napi::CallbackInfo& info, const Napi::Value& value) {
  // TODO(impl): Set ${a.name}
}
`
    : ''
}`
  )
  .join('')}

// --- Methods ---
${methods
  .map(
    (m) => `
Napi::Value ${className}::${capitalize(m.name)}(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // [SPEC] Algorithm
  /*
${m.steps.map((s) => `   * ${s}`).join('\n')}
   */

  // TODO(impl): Implement method logic
  return env.Undefined();
}
`
  )
  .join('')}
}  // namespace webcodecs
`;
}

/**
 * Generates the native binding interface for type-safe access to C++ bindings.
 * @param ctx - Interface context with methods, attributes, and constructor info
 * @returns Native interface definition string
 */
function generateNativeInterface(ctx: InterfaceContext): string {
  const methods = ctx.methods.filter((m) => m.name !== 'constructor');
  const instanceMethods = methods.filter((m) => !m.isStatic);

  const lines: string[] = [];
  lines.push(`/** Native binding interface for ${ctx.name} - matches C++ NAPI class shape */`);
  lines.push(`interface Native${ctx.name} {`);

  // Attributes
  for (const attr of ctx.attributes) {
    const readonly = attr.readonly ? 'readonly ' : '';
    lines.push(`  ${readonly}${attr.name}: ${attr.tsType};`);
  }

  // Instance methods
  for (const method of instanceMethods) {
    const params = method.args.map((a) => `${a.name}: ${a.tsType}`).join(', ');
    lines.push(`  ${method.name}(${params}): ${method.tsReturnType};`);
  }

  lines.push('}');

  return lines.join('\n');
}

/**
 * Generates the native constructor interface for static methods.
 * @param ctx - Interface context with methods, attributes, and constructor info
 * @returns Native constructor interface definition string
 */
function generateNativeConstructorInterface(ctx: InterfaceContext): string {
  const methods = ctx.methods.filter((m) => m.name !== 'constructor');
  const staticMethods = methods.filter((m) => m.isStatic);
  const initType = ctx.hasConstructor ? `${ctx.name}Init` : 'unknown';

  const lines: string[] = [];
  lines.push(`/** Native constructor interface for ${ctx.name} */`);
  lines.push(`interface Native${ctx.name}Constructor {`);
  lines.push(`  new (init: ${initType}): Native${ctx.name};`);

  // Static methods
  for (const method of staticMethods) {
    const params = method.args.map((a) => `${a.name}: ${a.tsType}`).join(', ');
    lines.push(`  ${method.name}(${params}): ${method.tsReturnType};`);
  }

  lines.push('}');

  return lines.join('\n');
}

/**
 * Generates TypeScript wrapper class for a native binding.
 * Follows Google TypeScript Style Guide with proper interfaces instead of unsafe casts.
 * @param ctx - Interface context with methods, attributes, and constructor info
 * @returns TypeScript class source code
 */
function generateTsSource(ctx: InterfaceContext): string {
  const methods = ctx.methods.filter((m) => m.name !== 'constructor');
  const instanceMethods = methods.filter((m) => !m.isStatic);
  const staticMethods = methods.filter((m) => m.isStatic);
  const imports = collectTypeImports(ctx);

  // Build import line if there are types to import
  const importLine =
    imports.length > 0
      ? `import type { ${imports.join(', ')} } from '../types/webcodecs.js';\n\n`
      : '';

  // Determine constructor parameter type
  const initType = ctx.hasConstructor ? `${ctx.name}Init` : 'unknown';

  // Generate native interfaces
  const nativeInterface = generateNativeInterface(ctx);
  const nativeConstructorInterface = generateNativeConstructorInterface(ctx);

  // Generate getters
  const getters = ctx.attributes
    .map((a) => {
      const getter = `
  get ${a.name}(): ${a.tsType} {
    return this.native.${a.name};
  }`;
      const setter = !a.readonly
        ? `

  set ${a.name}(value: ${a.tsType}) {
    this.native.${a.name} = value;
  }`
        : '';
      return getter + setter;
    })
    .join('');

  // Generate instance methods
  const instanceMethodsCode = instanceMethods
    .map((m) => {
      const params = m.args.map((a) => `${a.name}: ${a.tsType}`).join(', ');
      const args = m.args.map((a) => a.name).join(', ');
      const isVoid = m.tsReturnType === 'void';
      if (isVoid) {
        return `
  ${m.name}(${params}): void {
    this.native.${m.name}(${args});
  }`;
      }
      return `
  ${m.name}(${params}): ${m.tsReturnType} {
    return this.native.${m.name}(${args});
  }`;
    })
    .join('');

  // Generate static methods
  const staticMethodsCode = staticMethods
    .map((m) => {
      const params = m.args.map((a) => `${a.name}: ${a.tsType}`).join(', ');
      const args = m.args.map((a) => a.name).join(', ');
      const isVoid = m.tsReturnType === 'void';
      if (isVoid) {
        return `
  static ${m.name}(${params}): void {
    const NativeClass = bindings.${ctx.name} as Native${ctx.name}Constructor;
    NativeClass.${m.name}(${args});
  }`;
      }
      return `
  static ${m.name}(${params}): ${m.tsReturnType} {
    const NativeClass = bindings.${ctx.name} as Native${ctx.name}Constructor;
    return NativeClass.${m.name}(${args});
  }`;
    })
    .join('');

  return `/**
 * ${ctx.name} - TypeScript wrapper for native ${ctx.name}
 * @see spec/context/${ctx.name}.md
 */

import { createRequire } from 'node:module';
${importLine}
// Native binding loader - require() necessary for native addons in ESM
// See: https://nodejs.org/api/esm.html#interoperability-with-commonjs
const require = createRequire(import.meta.url);
const bindings = require('bindings')('webcodecs');

${nativeInterface}

${nativeConstructorInterface}

export class ${ctx.name} {
  private readonly native: Native${ctx.name};

  constructor(init: ${initType}) {
    const NativeClass = bindings.${ctx.name} as Native${ctx.name}Constructor;
    this.native = new NativeClass(init);
  }
${getters}
${instanceMethodsCode}
${staticMethodsCode}
}
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
    'void',
    'boolean',
    'number',
    'string',
    'any',
    'unknown',
    'null',
    'undefined',
    'Promise',
    'Array',
    'ArrayBuffer',
    'Uint8Array',
    'SharedArrayBuffer',
    ctx.name, // Don't import the class we're generating
  ]);
  return Array.from(types)
    .filter((t) => !primitives.has(t))
    .sort();
}

// --- TypeScript Type Definition Generator ---

// WebIDL to TypeScript type mapping
const TS_TYPE_MAP: Record<string, string> = {
  void: 'void',
  undefined: 'void',
  boolean: 'boolean',
  byte: 'number',
  octet: 'number',
  short: 'number',
  'unsigned short': 'number',
  long: 'number',
  'unsigned long': 'number',
  'long long': 'number',
  'unsigned long long': 'number',
  float: 'number',
  'unrestricted float': 'number',
  double: 'number',
  'unrestricted double': 'number',
  DOMString: 'string',
  USVString: 'string',
  ByteString: 'string',
  BufferSource: 'BufferSource',
  AllowSharedBufferSource: 'AllowSharedBufferSource',
  ArrayBuffer: 'ArrayBuffer',
  ArrayBufferView: 'ArrayBufferView',
  Uint8Array: 'Uint8Array',
  ReadableStream: 'ReadableStream',
  object: 'object',
  any: 'any',
  EventHandler: 'EventHandler',
  EventTarget: 'EventTarget',
  DOMException: 'DOMException',
  DOMRectReadOnly: 'DOMRectReadOnly',
  DOMRectInit: 'DOMRectInit',
  CanvasImageSource: 'CanvasImageSource',
  // WebCodecs-specific types that should be preserved (not converted to 'any')
  BitrateMode: 'BitrateMode',
};

/**
 * Converts a WebIDL type to its TypeScript equivalent.
 * Handles nullable types, unions, generics (Promise, sequence, FrozenArray, record).
 * @param idlType - WebIDL type description or string
 * @returns TypeScript type string (e.g., 'string | null', 'Promise<void>')
 */
function getTsType(idlType: IDLTypeDescription | string | null): string {
  if (!idlType) return 'void';

  // Handle simple string type
  if (typeof idlType === 'string') {
    return TS_TYPE_MAP[idlType] || idlType;
  }

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
    // innerType can be IDLTypeDescription or IDLTypeDescription[]
    if (Array.isArray(innerType)) {
      const baseType = (innerType as IDLTypeDescription[]).map((t) => getTsType(t)).join(' | ');
      return `${baseType} | null`;
    }
    const baseType = getTsType(innerType as IDLTypeDescription);
    return `${baseType} | null`;
  }

  // Handle union types
  if (idlType.union) {
    return (idlType.idlType as IDLTypeDescription[]).map((t) => getTsType(t)).join(' | ');
  }

  // Handle Promise<T>
  if (idlType.generic === 'Promise') {
    const idlTypeArray = idlType.idlType as IDLTypeDescription[];
    const inner = idlTypeArray?.[0];
    return `Promise<${inner ? getTsType(inner) : 'void'}>`;
  }

  // Handle sequence<T>
  if (idlType.generic === 'sequence') {
    const idlTypeArray = idlType.idlType as IDLTypeDescription[];
    const inner = idlTypeArray?.[0];
    return `${getTsType(inner)}[]`;
  }

  // Handle FrozenArray<T>
  if (idlType.generic === 'FrozenArray') {
    const idlTypeArray = idlType.idlType as IDLTypeDescription[];
    const inner = idlTypeArray?.[0];
    return `readonly ${getTsType(inner)}[]`;
  }

  // Handle record<K, V>
  if (idlType.generic === 'record') {
    const idlTypeArray = idlType.idlType as IDLTypeDescription[];
    const [keyType, valType] = idlTypeArray || [];
    return `Record<${getTsType(keyType)}, ${getTsType(valType)}>`;
  }

  // Simple type name
  const typeName = idlType.idlType;
  if (typeof typeName === 'string') {
    return TS_TYPE_MAP[typeName] || typeName;
  }

  // Nested type
  if (Array.isArray(typeName)) {
    return (typeName as IDLTypeDescription[]).map((t) => getTsType(t)).join(' | ');
  }

  return getTsType(typeName as IDLTypeDescription);
}

/**
 * Generates TypeScript type definitions from WebIDL AST.
 * Creates interfaces, type aliases, enums, callbacks, and polyfill types.
 * @param ast - Parsed WebIDL AST from webidl2
 * @returns Complete TypeScript .d.ts file content
 */
function generateTypeDefinitions(ast: IDLRootType[]): string {
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
    "// These types are needed because Node.js doesn't have DOM globals",
    '',
    'export type BufferSource = ArrayBufferView | ArrayBuffer;',
    'export type AllowSharedBufferSource = ArrayBufferView | ArrayBuffer | SharedArrayBuffer;',
    'export type EventHandler = ((event: Event) => void) | null;',
    '',
    '// BitrateMode is defined in MediaStream Recording spec, used by AudioEncoderConfig',
    "export type BitrateMode = 'constant' | 'variable';",
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
    "export type PredefinedColorSpace = 'display-p3' | 'srgb';",
    "export type ColorSpaceConversion = 'default' | 'none';",
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

  // Note: No declare global block needed - types are exported from this module
  // and should be imported explicitly where used. Empty declare global blocks
  // with only comments have no effect and violate style guidelines.

  return lines.join('\n');
}

function generateEnumType(item: EnumType): string {
  const values = item.values.map((v) => `'${v.value}'`).join(' | ');
  return `export type ${item.name} = ${values};\n`;
}

function generateTypedef(item: TypedefType): string {
  const tsType = getTsType(item.idlType);
  return `export type ${item.name} = ${tsType};\n`;
}

function generateCallback(item: CallbackType): string {
  const params = (item.arguments || [])
    .map((arg: Argument) => `${arg.name}${arg.optional ? '?' : ''}: ${getTsType(arg.idlType)}`)
    .join(', ');
  const returnType = getTsType(item.idlType);
  return `export type ${item.name} = (${params}) => ${returnType};\n`;
}

function generateDictionary(item: DictionaryType): string {
  const members = item.members || [];

  // Empty dictionaries without inheritance should be type aliases
  // per Google TypeScript Style Guide: avoid empty interfaces
  if (members.length === 0 && !item.inheritance) {
    return [
      `/** ${item.name} is extensible per WebCodecs spec */`,
      `export type ${item.name} = Record<string, unknown>;`,
      '',
    ].join('\n');
  }

  const lines: string[] = [];

  // Handle inheritance
  const ext = item.inheritance ? ` extends ${item.inheritance}` : '';
  lines.push(`export interface ${item.name}${ext} {`);

  for (const member of members) {
    const field = member as FieldType;
    const optional = field.required ? '' : '?';
    const tsType = getTsType(field.idlType);
    lines.push(`  ${field.name}${optional}: ${tsType};`);
  }

  lines.push('}');
  lines.push('');
  return lines.join('\n');
}

function generateInterface(item: InterfaceType): string {
  const lines: string[] = [];

  // Handle inheritance
  const ext = item.inheritance ? ` extends ${item.inheritance}` : '';
  lines.push(`export interface ${item.name}${ext} {`);

  for (const member of item.members || []) {
    switch (member.type) {
      case 'attribute': {
        const attr = member as AttributeMemberType;
        const readonly = attr.readonly ? 'readonly ' : '';
        const tsType = getTsType(attr.idlType);
        lines.push(`  ${readonly}${attr.name}: ${tsType};`);
        break;
      }
      case 'operation': {
        const op = member as OperationMemberType;
        if (op.name) {
          const params = (op.arguments || [])
            .map(
              (arg: Argument) => `${arg.name}${arg.optional ? '?' : ''}: ${getTsType(arg.idlType)}`
            )
            .join(', ');
          const returnType = getTsType(op.idlType);
          // Static methods cannot be declared in interfaces in TypeScript
          // Comment them out like constructors, or omit the 'static' keyword
          // For type-checking purposes, static methods are available on the class, not instances
          if (op.special === 'static') {
            lines.push(`  // static ${op.name}(${params}): ${returnType};`);
          } else {
            lines.push(`  ${op.name}(${params}): ${returnType};`);
          }
        }
        break;
      }
      case 'constructor': {
        const ctor = member as ConstructorMemberType;
        const params = (ctor.arguments || [])
          .map(
            (arg: Argument) => `${arg.name}${arg.optional ? '?' : ''}: ${getTsType(arg.idlType)}`
          )
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
  const turndown = new TurndownService({
    headingStyle: 'atx',
    bulletListMarker: '-',
  });

  // Configure Turndown to preserve list structure
  turndown.addRule('orderedListItem', {
    filter: 'li',
    replacement: (content, node) => {
      const parent = node.parentNode as Element;
      if (parent?.tagName === 'OL') {
        const items = Array.from(parent.children);
        const index = items.indexOf(node as Element) + 1;
        return `${index}. ${content.trim()}\n`;
      }
      return `- ${content.trim()}\n`;
    },
  });

  // 1. Extract IDL Blocks (both <pre class='idl'> and <xmp class='idl'>)
  const preIdlElements = doc.querySelectorAll('pre.idl');
  const xmpIdlElements = doc.querySelectorAll('xmp.idl');
  const idlText = [
    ...Array.from(preIdlElements).map((el) => el.textContent),
    ...Array.from(xmpIdlElements).map((el) => el.textContent),
  ]
    .filter(Boolean)
    .join('\n\n');

  // 2. Extract Algorithms - look for method definitions
  const algorithmMap = new Map<string, string[]>();

  // In rendered Bikeshed, methods are in <dt>/<dd> pairs with data-dfn-for attribute
  const definitions = doc.querySelectorAll('dfn[data-dfn-for]');

  definitions.forEach((dfn) => {
    const forAttr = dfn.getAttribute('data-dfn-for');
    const dfnType = dfn.getAttribute('data-dfn-type');

    if (forAttr && (dfnType === 'method' || dfnType === 'constructor')) {
      const methodName =
        dfnType === 'constructor'
          ? 'constructor'
          : dfn.textContent?.split('(')[0].trim() || '';
      const key = `${forAttr}.${methodName}`;

      // Find the containing <dt> and get the next <dd>
      const dt = dfn.closest('dt');
      const dd = dt?.nextElementSibling;

      if (dd && dd.tagName === 'DD') {
        const md = cleanBikeshedMarkup(turndown.turndown(dd.innerHTML));
        const steps = md
          .split('\n')
          .map((s) => s.trim())
          .filter(Boolean);
        algorithmMap.set(key, steps);
      }
    }
  });

  // Fallback: Also try the source Bikeshed format with 'for' and 'method'/'constructor' attributes
  const fallbackDefinitions = doc.querySelectorAll('dfn[for]');

  fallbackDefinitions.forEach((dfn) => {
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

      // Skip if already found via rendered HTML format
      if (algorithmMap.has(key)) {
        return;
      }

      // Look for the next <dd> or algorithm steps
      const container = dfn.closest('dt');
      const sibling = container?.nextElementSibling;

      if (sibling && sibling.tagName === 'DD') {
        const md = cleanBikeshedMarkup(turndown.turndown(sibling.innerHTML));
        algorithmMap.set(
          key,
          md
            .split('\n')
            .map((s) => s.trim())
            .filter(Boolean)
        );
      }
    }
  });

  // 3. Extract Descriptions
  const interfaceDescriptions = new Map<string, string>();
  doc.querySelectorAll('h2, h3, h4').forEach((h) => {
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
main().catch((err) => {
  console.error('[Scaffold] ❌ Fatal error:', err);
  process.exit(1);
});
