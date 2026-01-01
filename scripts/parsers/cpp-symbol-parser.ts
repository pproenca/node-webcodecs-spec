/**
 * C++ symbol parser using regex patterns.
 * Simpler than tree-sitter for this use case - we just need line numbers.
 */
import * as fs from 'node:fs/promises';

export interface SymbolLocation {
  line: number;
  endLine?: number;
}

export interface CppHeaderParseResult {
  className: string;
  methods: Map<string, SymbolLocation>;
  staticMethods: Map<string, SymbolLocation>;
}

export interface CppImplParseResult {
  ctor?: SymbolLocation;
  methods: Map<string, SymbolLocation>;
}

export async function parseCppHeader(filePath: string): Promise<CppHeaderParseResult> {
  const content = await fs.readFile(filePath, 'utf-8');
  const lines = content.split('\n');

  const result: CppHeaderParseResult = {
    className: '',
    methods: new Map(),
    staticMethods: new Map(),
  };

  // Extract class name: "class VideoDecoder : public ..."
  const classMatch = content.match(/class\s+(\w+)\s*:/);
  if (classMatch) {
    result.className = classMatch[1];
  }

  // Find method declarations
  for (let i = 0; i < lines.length; i++) {
    const line = lines[i];
    const lineNum = i + 1;

    // Match: "static Napi::Value IsConfigSupported(..."
    const staticMatch = line.match(/^\s*static\s+\S+\s+(\w+)\s*\(/);
    if (staticMatch) {
      result.staticMethods.set(staticMatch[1], { line: lineNum });
      continue;
    }

    // Match: "Napi::Value GetState(..." or "void Configure(..."
    const methodMatch = line.match(/^\s*(?:Napi::Value|void|bool)\s+(\w+)\s*\(/);
    if (methodMatch) {
      result.methods.set(methodMatch[1], { line: lineNum });
    }
  }

  return result;
}

export async function parseCppImpl(filePath: string, className: string): Promise<CppImplParseResult> {
  const content = await fs.readFile(filePath, 'utf-8');
  const lines = content.split('\n');

  const result: CppImplParseResult = {
    methods: new Map(),
  };

  let braceCount = 0;
  let currentMethod: string | null = null;
  let methodStartLine = 0;

  for (let i = 0; i < lines.length; i++) {
    const line = lines[i];
    const lineNum = i + 1;

    // Match constructor: "VideoDecoder::VideoDecoder(..."
    const ctorPattern = new RegExp(`\\b${className}::${className}\\s*\\(`);
    const ctorMatch = line.match(ctorPattern);
    if (ctorMatch && !result.ctor) {
      result.ctor = { line: lineNum };
      currentMethod = 'ctor';
      methodStartLine = lineNum;
      braceCount = 0;
    }

    // Match method: "ReturnType ClassName::MethodName(..." where ReturnType could be "Napi::Value" or "void"
    const methodPattern = new RegExp(`\\b${className}::(\\w+)\\s*\\(`);
    const methodMatch = line.match(methodPattern);
    if (methodMatch && methodMatch[1] !== className) {
      const methodName = methodMatch[1];
      currentMethod = methodName;
      methodStartLine = lineNum;
      braceCount = 0;
      result.methods.set(methodName, { line: lineNum });
    }

    // Count braces to find method end
    if (currentMethod) {
      braceCount += (line.match(/\{/g) || []).length;
      braceCount -= (line.match(/\}/g) || []).length;

      if (braceCount === 0 && line.includes('}')) {
        const location = currentMethod === 'ctor'
          ? result.ctor
          : result.methods.get(currentMethod);
        if (location) {
          location.endLine = lineNum;
        }
        currentMethod = null;
      }
    }
  }

  return result;
}
