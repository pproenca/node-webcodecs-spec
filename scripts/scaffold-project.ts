/**
 * Spec-First Code Generator for WebCodecs
 *
 * Clones w3c/webcodecs, parses Bikeshed HTML + WebIDL,
 * generates C++ headers/sources, TypeScript wrappers, and spec context docs.
 *
 * Usage:
 *   npm run scaffold           # Generate files, warn about binding.gyp
 *   npm run scaffold -- --write  # Also update binding.gyp sources
 */

import * as fs from 'node:fs/promises';
import * as path from 'node:path';
import { execSync } from 'node:child_process';
import { JSDOM } from 'jsdom';
import TurndownService from 'turndown';
import { parse as parseIDL, type InterfaceType, type OperationMemberType, type AttributeMemberType, type IDLRootType } from 'webidl2';

// --- CLI Arguments ---
const args = process.argv.slice(2);
const WRITE_BINDING_GYP = args.includes('--write');

// --- Configuration ---
const REPO_URL = 'https://github.com/w3c/webcodecs.git';
const ROOT_DIR = path.resolve(__dirname, '..');
const CACHE_DIR = path.join(ROOT_DIR, '.spec-cache');
const CONTEXT_DIR = path.join(ROOT_DIR, 'spec', 'context');
const SRC_DIR = path.join(ROOT_DIR, 'src', 'generated');
const LIB_DIR = path.join(ROOT_DIR, 'lib');
const BINDING_GYP_PATH = path.join(ROOT_DIR, 'binding.gyp');

// --- Type Definitions ---
interface MethodContext {
  name: string;
  signature: string;
  steps: string[];
  isStatic: boolean;
  args: { name: string; type: string }[];
  returnType: string;
}

interface AttributeContext {
  name: string;
  type: string;
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
  console.log(`[Scaffold] 🚀 Starting WebCodecs Scaffolding...`);

  // 1. Shallow Clone Source of Truth
  if (!await fileExists(CACHE_DIR)) {
    console.log(`[Scaffold] Cloning w3c/webcodecs (shallow)...`);
    execSync(`git clone --depth 1 ${REPO_URL} ${CACHE_DIR}`, { stdio: 'inherit' });
  } else {
    console.log(`[Scaffold] Using cached spec repo. Delete .spec-cache to refresh.`);
  }

  // 2. Prepare Directories
  await fs.mkdir(CONTEXT_DIR, { recursive: true });
  await fs.mkdir(SRC_DIR, { recursive: true });
  await fs.mkdir(LIB_DIR, { recursive: true });

  // 3. Parse Spec
  const specFile = path.join(CACHE_DIR, 'index.src.html');
  console.log(`[Scaffold] Parsing ${specFile}...`);
  const htmlContent = await fs.readFile(specFile, 'utf-8');

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
    await safeWrite(path.join(LIB_DIR, `${name}.ts`), generateTsSource(context));
  }

  // 5. Generate index files
  await fs.writeFile(
    path.join(SRC_DIR, 'index.h'),
    generatedClasses.map(c => `#include "${c}.h"`).join('\n') + '\n'
  );

  await fs.writeFile(
    path.join(LIB_DIR, 'index.ts'),
    generatedClasses.map(c => `export { ${c} } from './${c}';`).join('\n') + '\n'
  );

  // 6. Update or warn about binding.gyp
  const generatedSources = generatedClasses.map(c => `src/generated/${c}.cpp`);

  if (WRITE_BINDING_GYP) {
    await updateBindingGyp(generatedSources);
  } else {
    console.log('\n⚠️  Ensure the following files are in your binding.gyp "sources" list:');
    generatedClasses.forEach(c => console.log(`      "src/generated/${c}.cpp",`));
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

  // Get existing non-generated sources
  const existingSources = target.sources.filter(
    s => !s.startsWith('src/generated/')
  );

  // Merge: existing manual sources + generated sources
  target.sources = [...existingSources, ...generatedSources];

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

async function safeWrite(p: string, content: string): Promise<void> {
  if (await fileExists(p)) {
    console.warn(`[Scaffold] ⚠️  Skipping ${path.basename(p)} (File exists)`);
  } else {
    await fs.writeFile(p, content);
    console.log(`[Scaffold] ➕ Created ${path.basename(p)}`);
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
      const args = (op.arguments || []).map(arg => ({
        name: arg.name,
        type: getCppType(arg.idlType as any),
      }));

      methods.push({
        name: op.name,
        isStatic: op.special === 'static',
        signature: `${getCppType(op.idlType as any)} ${op.name}(${args.map(a => `${a.type} ${a.name}`).join(', ')})`,
        steps: algoMap.get(key) || ['See spec/context file.'],
        args,
        returnType: getCppType(op.idlType as any),
      });
    }

    if (member.type === 'constructor') {
      hasConstructor = true;
      const key = `${iface.name}.constructor`;
      const args = (member.arguments || []).map(arg => ({
        name: arg.name,
        type: getCppType(arg.idlType as any),
      }));

      methods.push({
        name: 'constructor',
        isStatic: false,
        signature: `constructor(${args.map(a => `${a.type} ${a.name}`).join(', ')})`,
        steps: algoMap.get(key) || ['Initialize internal slots.'],
        args,
        returnType: 'void',
      });
    }

    if (member.type === 'attribute') {
      const attr = member as AttributeMemberType;
      attributes.push({
        name: attr.name,
        type: getCppType(attr.idlType as any),
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
#include "../shared/Utils.h"

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

  return `/**
 * ${ctx.name} - TypeScript wrapper for native ${ctx.name}
 * @see spec/context/${ctx.name}.md
 */

// eslint-disable-next-line @typescript-eslint/no-var-requires
const bindings = require('bindings')('webcodecs');

export class ${ctx.name} {
  private readonly _native: any;

  constructor(init?: any) {
    this._native = new bindings.${ctx.name}(init);
  }

${ctx.attributes.map(a => `
  get ${a.name}(): any {
    return this._native.${a.name};
  }
${!a.readonly ? `
  set ${a.name}(value: any) {
    this._native.${a.name} = value;
  }
` : ''}`).join('')}
${methods.filter(m => !m.isStatic).map(m => `
  ${m.name}(...args: any[]): any {
    return this._native.${m.name}(...args);
  }
`).join('')}
${methods.filter(m => m.isStatic).map(m => `
  static ${m.name}(...args: any[]): any {
    return bindings.${ctx.name}.${m.name}(...args);
  }
`).join('')}
  /**
   * Explicit Resource Management (RAII)
   * Call this to release native resources immediately.
   */
  close(): void {
    if (this._native?.Release) {
      this._native.Release();
    }
  }
}
`;
}

function capitalize(s: string): string {
  return s.charAt(0).toUpperCase() + s.slice(1);
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
