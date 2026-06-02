# Parser JSON Shapes

Generated metadata is written as one JSON file per source file group. Optional
properties below are omitted by the serializers when their source value is empty,
false, or unset; required arrays may still serialize as `[]`.

```ts
type FilePath = string;
type Md5Hex = string;
type DecimalIntegerString = string;

type AccessSpecifier = "public" | "protected" | "private";
type StorageClass = "extern" | "static" | "private_extern" | "auto" | "register";
type RefQualifier = "&" | "&&";
type ScopedKind = "class" | "struct";
type RecordKind = "class" | "struct" | "union";
type ForwardDeclarationKind = RecordKind | "enum";
type DeclarationKind = RecordKind | "enum" | "forwardDeclaration" | "alias" | "function" | "variable";
type FunctionKind = "function" | "constructor" | "method" | "destructor" | "conversion";
type TemplateParameterKind = "typename" | "class" | "nonType" | "typenameTemplate" | "classTemplate" | string;
type ExceptionSpec =
  | "throw()"
  | "throw(...)"
  | "__declspec(nothrow)"
  | "noexcept"
  | "noexcept(dependent)"
  | "noexcept(false)"
  | "noexcept(true)"
  | "unevaluated"
  | "uninstantiated"
  | "unparsed";

interface ParserMetadataJson {
  path: FilePath;
  hash: Md5Hex;
  includes: FilePath[];
  declarations: Declaration[];
}

interface DeclarationCommon {
  kind: DeclarationKind;
  name?: string;
  qualifiedName?: string;
  file: FilePath;
  hash?: Md5Hex;
  scope: string[];
  documentation?: string;
  isAnonymous?: true;
  templateParameters?: TemplateParameter[];
}

type Declaration =
  | RecordDeclaration
  | EnumDeclaration
  | ForwardDeclaration
  | AliasDeclaration
  | FunctionDeclaration
  | VariableDeclaration;

type NestedDeclaration =
  | RecordDeclaration
  | EnumDeclaration
  | ForwardDeclaration
  | AliasDeclaration;

interface RecordDeclaration extends DeclarationCommon {
  kind: RecordKind;
  isCompleteDefinition?: true;
  sizeBytes?: number;
  alignBytes?: number;
  bases: BaseSpecifier[];
  fields: Field[];
  staticVariables: VariableMetadata[];
  methods: FunctionMetadata[];
  nested: NestedDeclaration[];
}

interface EnumDeclaration extends DeclarationCommon {
  kind: "enum";
  underlyingType?: string;
  isScoped?: true;
  scopedKind?: ScopedKind;
  enumerators: Enumerator[];
}

interface ForwardDeclaration extends DeclarationCommon {
  kind: "forwardDeclaration";
  forwardDeclarationKind: ForwardDeclarationKind;
  underlyingType?: string;
  isScoped?: true;
  scopedKind?: ScopedKind;
}

interface AliasDeclaration extends DeclarationCommon {
  kind: "alias";
  aliasedType: string;
}

// Top-level function and variable payloads are flattened into the declaration.
interface FunctionDeclaration extends DeclarationCommon, FunctionDetails {
  kind: "function";
  functionKind: "function";
}

interface VariableDeclaration extends DeclarationCommon, VariableDetails {
  kind: "variable";
}

interface TemplateParameter {
  kind: TemplateParameterKind;
  name?: string;
  documentation?: string;
  type?: string;
  isParameterPack?: true;
  parameters?: TemplateParameter[];
}

interface Parameter {
  name?: string;
  type: string;
  declaration: string;
  documentation?: string;
}

interface VTableIndex {
  index: number;
  offset: number;
}

interface FunctionDetails {
  functionKind: FunctionKind;
  returnType?: string;
  access?: AccessSpecifier;
  storageClass?: StorageClass;
  isConst?: true;
  isVolatile?: true;
  isStatic?: true;
  isVirtual?: true;
  isPure?: true;
  isConstexpr?: true;
  isConsteval?: true;
  isInline?: true;
  isDeleted?: true;
  isDefaulted?: true;
  isExplicit?: true;
  refQualifier?: RefQualifier;
  exceptionSpec?: ExceptionSpec;
  templateParameters?: TemplateParameter[];
  parameters: Parameter[];
  vtableIndex?: VTableIndex;
}

interface FunctionMetadata extends FunctionDetails {
  name: string;
  qualifiedName?: string;
  file: FilePath;
  scope: string[];
  documentation?: string;
}

interface VariableDetails {
  type: string;
  declaration: string;
  access?: AccessSpecifier;
  storageClass?: StorageClass;
  isConstexpr?: true;
  isInline?: true;
  isStaticDataMember?: true;
  isThreadLocal?: true;
}

interface VariableMetadata extends VariableDetails {
  name: string;
  qualifiedName?: string;
  file: FilePath;
  scope: string[];
  documentation?: string;
}

interface Enumerator {
  name: string;
  value: DecimalIntegerString;
  file: FilePath;
  scope: string[];
  documentation?: string;
}

interface Field {
  name?: string;
  file: FilePath;
  scope: string[];
  documentation?: string;
  type: string;
  declaration: string;
  access?: AccessSpecifier;
  isMutable?: true;
  isBitfield?: true;
  bitWidth?: number;
  offsetBits?: number;
}

interface BaseSpecifier {
  type: string;
  qualifiedName?: string;
  access?: AccessSpecifier;
  isVirtual?: true;
  offset?: number;
}

// The parser reads compile_commands.json and builds a one-entry filtered
// compile command buffer for Clang.
type CompileCommandsJson = CompileCommandEntry[];
type FilteredCompileCommandsJson = [FilteredCompileCommandEntry];

interface CompileCommandEntry {
  file: FilePath;
  command?: string;
  directory?: FilePath;
  output?: string;
  [unknownKey: string]: unknown;
}

interface FilteredCompileCommandEntry {
  file: FilePath;
  command: string;
  directory: FilePath;
  output: string;
}
```
