/**
 * TypeScript AST parser using ts-morph for extracting class symbols.
 */
import { Project, SyntaxKind } from 'ts-morph';
import type { CodeLink } from '../types/task-schema.js';

export interface SymbolLocation {
  line: number;
  endLine?: number;
}

export interface TsClassParseResult {
  className: string;
  constructor?: SymbolLocation;
  methods: Map<string, SymbolLocation>;
  getters: Map<string, SymbolLocation>;
  setters: Map<string, SymbolLocation>;
  staticMethods: Map<string, SymbolLocation>;
}

export interface TsTestParseResult {
  describes: Map<string, SymbolLocation>;
  itBlocks: Array<{ name: string; location: SymbolLocation }>;
}

export async function parseTsFile(filePath: string): Promise<TsClassParseResult> {
  const project = new Project({ useInMemoryFileSystem: false });
  const sourceFile = project.addSourceFileAtPath(filePath);

  const classes = sourceFile.getClasses();
  if (classes.length === 0) {
    throw new Error(`No class found in ${filePath}`);
  }

  const cls = classes[0];
  const result: TsClassParseResult = {
    className: cls.getName() || 'Unknown',
    constructor: undefined,
    methods: new Map(),
    getters: new Map(),
    setters: new Map(),
    staticMethods: new Map(),
  };

  // Extract constructor
  const ctor = cls.getConstructors()[0];
  if (ctor) {
    result.constructor = {
      line: ctor.getStartLineNumber(),
      endLine: ctor.getEndLineNumber(),
    };
  }

  // Extract methods
  for (const method of cls.getMethods()) {
    const name = method.getName();
    const location: SymbolLocation = {
      line: method.getStartLineNumber(),
      endLine: method.getEndLineNumber(),
    };

    if (method.isStatic()) {
      result.staticMethods.set(name, location);
    } else {
      result.methods.set(name, location);
    }
  }

  // Extract getters
  for (const getter of cls.getGetAccessors()) {
    result.getters.set(getter.getName(), {
      line: getter.getStartLineNumber(),
      endLine: getter.getEndLineNumber(),
    });
  }

  // Extract setters
  for (const setter of cls.getSetAccessors()) {
    result.setters.set(setter.getName(), {
      line: setter.getStartLineNumber(),
      endLine: setter.getEndLineNumber(),
    });
  }

  return result;
}

export async function parseTsTestFile(filePath: string): Promise<TsTestParseResult> {
  const project = new Project({ useInMemoryFileSystem: false });
  const sourceFile = project.addSourceFileAtPath(filePath);

  const result: TsTestParseResult = {
    describes: new Map(),
    itBlocks: [],
  };

  // Find describe() and it() calls
  sourceFile.forEachDescendant((node) => {
    if (node.getKind() === SyntaxKind.CallExpression) {
      const callExpr = node.asKind(SyntaxKind.CallExpression);
      if (!callExpr) return;

      const expr = callExpr.getExpression();
      const exprText = expr.getText();

      if (exprText === 'describe' || exprText === 'it') {
        const args = callExpr.getArguments();
        if (args.length > 0) {
          const firstArg = args[0];
          // Get the string literal value
          const name = firstArg.getText().replace(/^['"]|['"]$/g, '');
          const location: SymbolLocation = {
            line: callExpr.getStartLineNumber(),
            endLine: callExpr.getEndLineNumber(),
          };

          if (exprText === 'describe') {
            result.describes.set(name, location);
          } else {
            result.itBlocks.push({ name, location });
          }
        }
      }
    }
  });

  return result;
}
