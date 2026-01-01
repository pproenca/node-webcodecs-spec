// scripts/types/task-schema.ts
/**
 * JSON schema types for generated task files.
 * @see docs/plans/2026-01-01-generate-tasks-rewrite-design.md
 */

export interface CodeLink {
  file: string;
  line: number;
  endLine?: number;
}

export interface Step {
  id: string;
  description: string;
  codeRef?: CodeLink;
  passes: boolean;
}

export interface Feature {
  id: string;
  category: 'constructor' | 'attribute' | 'method' | 'static-method' | 'getter' | 'iterable';
  name: string;
  description: string;
  returnType?: string;
  readonly?: boolean;
  codeLinks: {
    declaration?: CodeLink;
    implementation?: CodeLink;
    tsBinding?: CodeLink;
    test?: CodeLink;
  };
  algorithmRef?: string;
  algorithmSteps: string[];
  steps: Step[];
  passes: boolean;
}

export interface TaskFile {
  interface: string;
  type: 'interface';
  source: {
    idl: string;
    spec?: string;
  };
  inheritance?: string;
  extendedAttributes: string[];
  files: {
    cppHeader: string;
    cppImpl: string;
    tsWrapper: string;
    tests: string;
  };
  features: Feature[];
}

export interface DictionaryField {
  name: string;
  type: string;
  required: boolean;
  defaultValue?: string;
}

export interface DictionaryDef {
  name: string;
  source: { idl: string };
  fields: DictionaryField[];
}

export interface EnumDef {
  name: string;
  source: { idl: string };
  values: string[];
}

export interface TypesFile {
  dictionaries: DictionaryDef[];
  enums: EnumDef[];
}
