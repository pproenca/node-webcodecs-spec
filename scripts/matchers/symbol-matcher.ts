/**
 * Maps IDL members to code locations across C++ and TypeScript files.
 */
import type { CodeLink } from '../types/task-schema.js';
import type { TsClassParseResult } from '../parsers/ts-ast-parser.js';
import type { CppHeaderParseResult, CppImplParseResult } from '../parsers/cpp-symbol-parser.js';

export class MissingSymbolError extends Error {
  constructor(
    public interfaceName: string,
    public memberName: string,
    public memberType: string,
    public expectedIn: string,
    public expectedSymbol: string
  ) {
    super(`Missing symbol: ${interfaceName}.${memberName} (${memberType}) - expected ${expectedSymbol} in ${expectedIn}`);
    this.name = 'MissingSymbolError';
  }
}

export interface IdlMember {
  type: 'attribute' | 'operation' | 'constructor';
  name: string;
  readonly?: boolean;
  special?: string; // 'static'
}

export interface CodeLinks {
  declaration?: CodeLink;
  implementation?: CodeLink;
  tsBinding?: CodeLink;
  test?: CodeLink;
}

export interface MatchResult {
  codeLinks: CodeLinks;
}

function capitalize(str: string): string {
  return str.charAt(0).toUpperCase() + str.slice(1);
}

export function matchIdlToCode(
  interfaceName: string,
  idlMember: IdlMember,
  cppHeader: CppHeaderParseResult,
  cppImpl: CppImplParseResult,
  tsClass: TsClassParseResult,
  files?: { cppHeader: string; cppImpl: string; tsWrapper: string }
): MatchResult {
  const codeLinks: CodeLinks = {};
  const filePrefix = files || {
    cppHeader: `src/${interfaceName}.h`,
    cppImpl: `src/${interfaceName}.cpp`,
    tsWrapper: `lib/${interfaceName}.ts`,
  };

  if (idlMember.type === 'attribute') {
    // Attributes map to GetXxx() in C++ and get xxx() in TS
    const cppGetterName = `Get${capitalize(idlMember.name)}`;

    // Check C++ header
    const headerLoc = cppHeader.methods.get(cppGetterName);
    if (!headerLoc) {
      throw new MissingSymbolError(
        interfaceName, idlMember.name, 'attribute',
        filePrefix.cppHeader, cppGetterName
      );
    }
    codeLinks.declaration = { file: filePrefix.cppHeader, line: headerLoc.line };

    // Check C++ impl
    const implLoc = cppImpl.methods.get(cppGetterName);
    if (!implLoc) {
      throw new MissingSymbolError(
        interfaceName, idlMember.name, 'attribute',
        filePrefix.cppImpl, `${interfaceName}::${cppGetterName}`
      );
    }
    codeLinks.implementation = {
      file: filePrefix.cppImpl,
      line: implLoc.line,
      endLine: implLoc.endLine,
    };

    // Check TS getter
    const tsLoc = tsClass.getters.get(idlMember.name);
    if (!tsLoc) {
      throw new MissingSymbolError(
        interfaceName, idlMember.name, 'attribute',
        filePrefix.tsWrapper, `get ${idlMember.name}()`
      );
    }
    codeLinks.tsBinding = {
      file: filePrefix.tsWrapper,
      line: tsLoc.line,
      endLine: tsLoc.endLine,
    };
  } else if (idlMember.type === 'operation') {
    const cppMethodName = capitalize(idlMember.name);
    const isStatic = idlMember.special === 'static';

    // Check C++ header
    const methodsMap = isStatic ? cppHeader.staticMethods : cppHeader.methods;
    const headerLoc = methodsMap.get(cppMethodName);
    if (!headerLoc) {
      throw new MissingSymbolError(
        interfaceName, idlMember.name, isStatic ? 'static-method' : 'method',
        filePrefix.cppHeader, cppMethodName
      );
    }
    codeLinks.declaration = { file: filePrefix.cppHeader, line: headerLoc.line };

    // Check C++ impl
    const implLoc = cppImpl.methods.get(cppMethodName);
    if (!implLoc) {
      throw new MissingSymbolError(
        interfaceName, idlMember.name, isStatic ? 'static-method' : 'method',
        filePrefix.cppImpl, `${interfaceName}::${cppMethodName}`
      );
    }
    codeLinks.implementation = {
      file: filePrefix.cppImpl,
      line: implLoc.line,
      endLine: implLoc.endLine,
    };

    // Check TS
    const tsMap = isStatic ? tsClass.staticMethods : tsClass.methods;
    const tsLoc = tsMap.get(idlMember.name);
    if (!tsLoc) {
      throw new MissingSymbolError(
        interfaceName, idlMember.name, isStatic ? 'static-method' : 'method',
        filePrefix.tsWrapper, `${isStatic ? 'static ' : ''}${idlMember.name}()`
      );
    }
    codeLinks.tsBinding = {
      file: filePrefix.tsWrapper,
      line: tsLoc.line,
      endLine: tsLoc.endLine,
    };
  } else if (idlMember.type === 'constructor') {
    // Constructor
    codeLinks.declaration = {
      file: filePrefix.cppHeader,
      line: cppHeader.methods.get(interfaceName)?.line || 1,
    };

    if (cppImpl.ctor) {
      codeLinks.implementation = {
        file: filePrefix.cppImpl,
        line: cppImpl.ctor.line,
        endLine: cppImpl.ctor.endLine,
      };
    }

    if (tsClass.constructor) {
      codeLinks.tsBinding = {
        file: filePrefix.tsWrapper,
        line: tsClass.constructor.line,
        endLine: tsClass.constructor.endLine,
      };
    }
  }

  return { codeLinks };
}
