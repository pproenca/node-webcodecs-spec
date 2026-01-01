import { describe, it, expect } from 'vitest';
import { matchIdlToCode, MissingSymbolError } from './symbol-matcher.js';
import type { TsClassParseResult } from '../parsers/ts-ast-parser.js';
import type { CppHeaderParseResult, CppImplParseResult } from '../parsers/cpp-symbol-parser.js';

describe('matchIdlToCode', () => {
  it('should match IDL attribute to C++ getter', () => {
    const idlMember = { type: 'attribute' as const, name: 'state', readonly: true };
    const cppHeader: CppHeaderParseResult = {
      className: 'VideoDecoder',
      methods: new Map([['GetState', { line: 50 }]]),
      staticMethods: new Map(),
    };
    const cppImpl: CppImplParseResult = {
      methods: new Map([['GetState', { line: 92, endLine: 94 }]]),
    };
    const tsClass: TsClassParseResult = {
      className: 'VideoDecoder',
      methods: new Map(),
      getters: new Map([['state', { line: 47, endLine: 49 }]]),
      setters: new Map(),
      staticMethods: new Map(),
    };

    const result = matchIdlToCode('VideoDecoder', idlMember, cppHeader, cppImpl, tsClass);

    expect(result.codeLinks.declaration?.line).toBe(50);
    expect(result.codeLinks.implementation?.line).toBe(92);
    expect(result.codeLinks.tsBinding?.line).toBe(47);
  });

  it('should throw MissingSymbolError when C++ getter not found', () => {
    const idlMember = { type: 'attribute' as const, name: 'missingAttr', readonly: true };
    const cppHeader: CppHeaderParseResult = {
      className: 'VideoDecoder',
      methods: new Map(),
      staticMethods: new Map(),
    };
    const cppImpl: CppImplParseResult = { methods: new Map() };
    const tsClass: TsClassParseResult = {
      className: 'VideoDecoder',
      methods: new Map(),
      getters: new Map(),
      setters: new Map(),
      staticMethods: new Map(),
    };

    expect(() => matchIdlToCode('VideoDecoder', idlMember, cppHeader, cppImpl, tsClass)).toThrow(
      MissingSymbolError
    );
  });

  it('should match IDL operation to C++ method', () => {
    const idlMember = { type: 'operation' as const, name: 'configure' };
    const cppHeader: CppHeaderParseResult = {
      className: 'VideoDecoder',
      methods: new Map([['Configure', { line: 56 }]]),
      staticMethods: new Map(),
    };
    const cppImpl: CppImplParseResult = {
      methods: new Map([['Configure', { line: 113, endLine: 123 }]]),
    };
    const tsClass: TsClassParseResult = {
      className: 'VideoDecoder',
      methods: new Map([['configure', { line: 61, endLine: 63 }]]),
      getters: new Map(),
      setters: new Map(),
      staticMethods: new Map(),
    };

    const result = matchIdlToCode('VideoDecoder', idlMember, cppHeader, cppImpl, tsClass);

    expect(result.codeLinks.declaration?.line).toBe(56);
    expect(result.codeLinks.implementation?.line).toBe(113);
    expect(result.codeLinks.tsBinding?.line).toBe(61);
  });

  it('should match IDL static method', () => {
    const idlMember = { type: 'operation' as const, name: 'isConfigSupported', special: 'static' };
    const cppHeader: CppHeaderParseResult = {
      className: 'VideoDecoder',
      methods: new Map(),
      staticMethods: new Map([['IsConfigSupported', { line: 61 }]]),
    };
    const cppImpl: CppImplParseResult = {
      methods: new Map([['IsConfigSupported', { line: 173, endLine: 185 }]]),
    };
    const tsClass: TsClassParseResult = {
      className: 'VideoDecoder',
      methods: new Map(),
      getters: new Map(),
      setters: new Map(),
      staticMethods: new Map([['isConfigSupported', { line: 77, endLine: 80 }]]),
    };

    const result = matchIdlToCode('VideoDecoder', idlMember, cppHeader, cppImpl, tsClass);

    expect(result.codeLinks.declaration?.line).toBe(61);
    expect(result.codeLinks.implementation?.line).toBe(173);
    expect(result.codeLinks.tsBinding?.line).toBe(77);
  });
});
