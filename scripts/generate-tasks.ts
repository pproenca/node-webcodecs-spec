/**
 * Task Generator - Parses WebIDL and generates JSON task files with code links.
 *
 * Usage: npx tsx scripts/generate-tasks.ts
 *
 * Reads:
 *   - spec/context/_webcodecs.idl
 *   - spec/context/<Interface>.md
 *   - src/<Interface>.h, src/<Interface>.cpp
 *   - lib/<Interface>.ts
 *   - test/<interface>.test.ts
 *
 * Outputs:
 *   - docs/tasks/<Interface>.json
 *   - docs/tasks/types.json
 */

import * as fs from 'node:fs/promises';
import * as path from 'node:path';
import { fileURLToPath } from 'node:url';
import * as webidl2 from 'webidl2';

import type { TaskFile, TypesFile, Feature, DictionaryDef, EnumDef } from './types/task-schema.js';
import { parseSpecFile } from './parsers/spec-markdown-parser.js';
import { parseTsFile } from './parsers/ts-ast-parser.js';
import { parseCppHeader, parseCppImpl } from './parsers/cpp-symbol-parser.js';
import { matchIdlToCode, MissingSymbolError } from './matchers/symbol-matcher.js';

const currentDir = path.dirname(fileURLToPath(import.meta.url));
const ROOT_DIR = path.join(currentDir, '..');
const IDL_PATH = path.join(ROOT_DIR, 'spec/context/_webcodecs.idl');
const SPEC_DIR = path.join(ROOT_DIR, 'spec/context');
const SRC_DIR = path.join(ROOT_DIR, 'src');
const LIB_DIR = path.join(ROOT_DIR, 'lib');
const OUTPUT_DIR = path.join(ROOT_DIR, 'docs/tasks');

function capitalize(str: string): string {
  return str.charAt(0).toUpperCase() + str.slice(1);
}

function parseIdlType(idlType: webidl2.IDLTypeDescription | null): string {
  if (!idlType) return 'void';
  if (typeof idlType.idlType === 'string') {
    return idlType.idlType;
  }
  if (Array.isArray(idlType.idlType)) {
    return idlType.idlType.map((t) => parseIdlType(t as webidl2.IDLTypeDescription)).join(' | ');
  }
  return 'unknown';
}

function getExtendedAttributes(node: webidl2.InterfaceType): string[] {
  return node.extAttrs?.map(attr => {
    if (attr.rhs) {
      const rhsValue = typeof attr.rhs.value === 'string' ? attr.rhs.value : JSON.stringify(attr.rhs.value);
      return `${attr.name}=${rhsValue}`;
    }
    return attr.name;
  }) || [];
}

function getIdlLineRange(content: string, nodeName: string): string {
  const lines = content.split('\n');
  for (let i = 0; i < lines.length; i++) {
    if (lines[i].includes(`interface ${nodeName}`)) {
      // Find end of interface
      let braceCount = 0;
      let started = false;
      for (let j = i; j < lines.length; j++) {
        if (lines[j].includes('{')) { braceCount++; started = true; }
        if (lines[j].includes('}')) braceCount--;
        if (started && braceCount === 0) {
          return `spec/context/_webcodecs.idl#L${i + 1}-L${j + 1}`;
        }
      }
    }
  }
  return 'spec/context/_webcodecs.idl';
}

async function fileExists(filePath: string): Promise<boolean> {
  try {
    await fs.access(filePath);
    return true;
  } catch {
    return false;
  }
}

async function generateInterfaceTask(
  node: webidl2.InterfaceType,
  idlContent: string
): Promise<TaskFile> {
  const interfaceName = node.name;
  const files = {
    cppHeader: `src/${interfaceName}.h`,
    cppImpl: `src/${interfaceName}.cpp`,
    tsWrapper: `lib/${interfaceName}.ts`,
    tests: `test/${interfaceName.toLowerCase()}.test.ts`,
  };

  // Check if files exist
  const cppHeaderPath = path.join(SRC_DIR, `${interfaceName}.h`);
  const cppImplPath = path.join(SRC_DIR, `${interfaceName}.cpp`);
  const tsWrapperPath = path.join(LIB_DIR, `${interfaceName}.ts`);
  const specPath = path.join(SPEC_DIR, `${interfaceName}.md`);

  // Parse source files
  let cppHeader, cppImpl, tsClass;
  const errors: MissingSymbolError[] = [];

  try {
    if (await fileExists(cppHeaderPath)) {
      cppHeader = await parseCppHeader(cppHeaderPath);
    }
    if (await fileExists(cppImplPath)) {
      cppImpl = await parseCppImpl(cppImplPath, interfaceName);
    }
    if (await fileExists(tsWrapperPath)) {
      tsClass = await parseTsFile(tsWrapperPath);
    }
  } catch (e) {
    console.warn(`Warning: Could not parse files for ${interfaceName}:`, e);
  }

  // Parse spec markdown for algorithms
  let specData;
  if (await fileExists(specPath)) {
    specData = await parseSpecFile(specPath);
  }

  const features: Feature[] = [];

  for (const member of node.members) {
    let feature: Feature | null = null;

    if (member.type === 'attribute') {
      const attrName = member.name;

      feature = {
        id: `${interfaceName}.${attrName}`,
        category: 'attribute',
        name: attrName,
        description: `${member.readonly ? 'Readonly' : 'Read/write'} attribute`,
        returnType: parseIdlType(member.idlType),
        readonly: member.readonly,
        codeLinks: {},
        algorithmRef: `spec/context/${interfaceName}.md#attributes`,
        algorithmSteps: [],
        steps: [
          {
            id: `${interfaceName}.${attrName}.cpp-getter`,
            description: `C++: Implement Get${capitalize(attrName)}() returning ${parseIdlType(member.idlType)}`,
            passes: false,
          },
          ...(member.readonly ? [] : [{
            id: `${interfaceName}.${attrName}.cpp-setter`,
            description: `C++: Implement Set${capitalize(attrName)}()`,
            passes: false,
          }]),
          {
            id: `${interfaceName}.${attrName}.ts-binding`,
            description: `TS: Wire getter${member.readonly ? '' : ' and setter'} to native`,
            passes: false,
          },
          {
            id: `${interfaceName}.${attrName}.test`,
            description: 'Test: Verify initial value and behavior',
            passes: false,
          },
        ],
        passes: false,
      };

      // Try to match code links
      if (cppHeader && cppImpl && tsClass) {
        try {
          const match = matchIdlToCode(
            interfaceName,
            { type: 'attribute', name: attrName, readonly: member.readonly },
            cppHeader, cppImpl, tsClass, files
          );
          feature.codeLinks = match.codeLinks;
        } catch (e) {
          if (e instanceof MissingSymbolError) {
            errors.push(e);
          }
        }
      }
    } else if (member.type === 'operation' && member.name) {
      const methodName = member.name;
      const isStatic = member.special === 'static';
      const specMethod = specData?.methods.get(methodName);
      const params = member.arguments.map(arg => `${arg.name}: ${parseIdlType(arg.idlType)}`);

      feature = {
        id: `${interfaceName}.${methodName}`,
        category: isStatic ? 'static-method' : 'method',
        name: `${methodName}(${params.join(', ')})`,
        description: specMethod?.algorithmSteps[0] || `${isStatic ? 'Static method' : 'Method'} ${methodName}`,
        returnType: parseIdlType(member.idlType),
        codeLinks: {},
        algorithmRef: `spec/context/${interfaceName}.md#${methodName.toLowerCase()}`,
        algorithmSteps: specMethod?.algorithmSteps || [],
        steps: [
          {
            id: `${interfaceName}.${methodName}.cpp-impl`,
            description: 'C++: Implement method logic per W3C spec algorithm',
            passes: false,
          },
          ...(parseIdlType(member.idlType).includes('Promise') ? [{
            id: `${interfaceName}.${methodName}.cpp-async`,
            description: 'C++: Use AsyncWorker for non-blocking execution',
            passes: false,
          }] : []),
          {
            id: `${interfaceName}.${methodName}.cpp-errors`,
            description: 'C++: Handle error cases with proper DOMException types',
            passes: false,
          },
          {
            id: `${interfaceName}.${methodName}.ts-binding`,
            description: 'TS: Wire method to native implementation',
            passes: false,
          },
          {
            id: `${interfaceName}.${methodName}.test-happy`,
            description: 'Test: Write test case for happy path',
            passes: false,
          },
          {
            id: `${interfaceName}.${methodName}.test-errors`,
            description: 'Test: Write test case for error conditions',
            passes: false,
          },
        ],
        passes: false,
      };

      // Try to match code links
      if (cppHeader && cppImpl && tsClass) {
        try {
          const match = matchIdlToCode(
            interfaceName,
            { type: 'operation', name: methodName, special: member.special },
            cppHeader, cppImpl, tsClass, files
          );
          feature.codeLinks = match.codeLinks;
        } catch (e) {
          if (e instanceof MissingSymbolError) {
            errors.push(e);
          }
        }
      }
    } else if (member.type === 'constructor') {
      const params = member.arguments.map(arg => `${arg.name}: ${parseIdlType(arg.idlType)}`);

      feature = {
        id: `${interfaceName}.constructor`,
        category: 'constructor',
        name: `constructor(${params.join(', ')})`,
        description: `Create ${interfaceName} instance`,
        codeLinks: {},
        algorithmRef: `spec/context/${interfaceName}.md#constructor`,
        algorithmSteps: specData?.methods.get('constructor')?.algorithmSteps || ['Initialize internal slots'],
        steps: [
          {
            id: `${interfaceName}.constructor.cpp-impl`,
            description: 'C++: Implement constructor with parameter validation',
            passes: false,
          },
          {
            id: `${interfaceName}.constructor.cpp-init`,
            description: 'C++: Initialize internal state slots',
            passes: false,
          },
          {
            id: `${interfaceName}.constructor.ts-binding`,
            description: 'TS: Wire TypeScript wrapper to native constructor',
            passes: false,
          },
          {
            id: `${interfaceName}.constructor.test`,
            description: 'Test: Verify constructor creates valid instance',
            passes: false,
          },
        ],
        passes: false,
      };

      // Try to match code links
      if (cppHeader && cppImpl && tsClass) {
        try {
          const match = matchIdlToCode(
            interfaceName,
            { type: 'constructor', name: 'constructor' },
            cppHeader, cppImpl, tsClass, files
          );
          feature.codeLinks = match.codeLinks;
        } catch (e) {
          if (e instanceof MissingSymbolError) {
            errors.push(e);
          }
        }
      }
    }

    if (feature) {
      features.push(feature);
    }
  }

  // Report errors but don't fail (for now - change to throw for strict mode)
  if (errors.length > 0) {
    console.warn(`\nWarning: Missing symbols for ${interfaceName}:`);
    for (const err of errors) {
      console.warn(`  - ${err.memberName} (${err.memberType}): expected ${err.expectedSymbol} in ${err.expectedIn}`);
    }
  }

  const inheritance = node.inheritance?.name;

  return {
    interface: interfaceName,
    type: 'interface',
    source: {
      idl: getIdlLineRange(idlContent, interfaceName),
      spec: await fileExists(specPath) ? `spec/context/${interfaceName}.md` : undefined,
    },
    inheritance,
    extendedAttributes: getExtendedAttributes(node),
    files,
    features,
  };
}

function generateTypesFile(ast: webidl2.IDLRootType[]): TypesFile {
  const dictionaries: DictionaryDef[] = [];
  const enums: EnumDef[] = [];

  for (const node of ast) {
    if (node.type === 'dictionary') {
      dictionaries.push({
        name: node.name,
        source: { idl: `spec/context/_webcodecs.idl` },
        fields: node.members.map(m => ({
          name: m.name,
          type: parseIdlType(m.idlType),
          required: m.required,
          defaultValue: m.default?.value?.toString(),
        })),
      });
    } else if (node.type === 'enum') {
      enums.push({
        name: node.name,
        source: { idl: `spec/context/_webcodecs.idl` },
        values: node.values.map(v => v.value),
      });
    }
  }

  return { dictionaries, enums };
}

export async function main(): Promise<void> {
  // Ensure output directory exists
  await fs.mkdir(OUTPUT_DIR, { recursive: true });

  // Read and parse IDL
  const idlContent = await fs.readFile(IDL_PATH, 'utf-8');
  const ast = webidl2.parse(idlContent);

  let interfaceCount = 0;
  let featureCount = 0;

  // Generate interface task files
  for (const node of ast) {
    if (node.type === 'interface') {
      const taskFile = await generateInterfaceTask(node, idlContent);
      const outputPath = path.join(OUTPUT_DIR, `${node.name}.json`);
      await fs.writeFile(outputPath, JSON.stringify(taskFile, null, 2));
      console.log(`Generated: ${outputPath} (${taskFile.features.length} features)`);
      interfaceCount++;
      featureCount += taskFile.features.length;
    }
  }

  // Generate types.json
  const typesFile = generateTypesFile(ast);
  const typesPath = path.join(OUTPUT_DIR, 'types.json');
  await fs.writeFile(typesPath, JSON.stringify(typesFile, null, 2));
  console.log(`Generated: ${typesPath} (${typesFile.dictionaries.length} dictionaries, ${typesFile.enums.length} enums)`);

  console.log(`\nDone! Generated ${interfaceCount} interface files with ${featureCount} total features.`);
}

// Run if executed directly
if (import.meta.url === `file://${process.argv[1]}`) {
  main().catch((err) => {
    console.error(err);
    process.exit(1);
  });
}
