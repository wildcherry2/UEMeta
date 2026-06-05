# Parser JSON Shapes

Generated metadata is written according to the required `--split-strategy`
option. `--split-strategy file` writes one JSON file per source file group.
`--split-strategy decl` writes one top-level declaration object per file, using
the declaration kind as the file extension, plus one `.file` metadata object per
source file.

The comments in the TypeScript declarations below mirror the documentation
attached to the DSO structs. Optional properties are omitted by the serializers
when their source value is empty, false, or unset. Required arrays may still
serialize as `[]`.

Optional `true` flags are presence-only: they are emitted as `true` when the
condition applies and omitted otherwise, rather than emitted as `false`. Optional
template specialization fields are emitted only for specializations or explicit
instantiations. Optional layout fields are emitted only when Clang exposes the
corresponding ABI layout.

`documentation` fields contain formatted Doxygen documentation extracted from
the C++ declaration. For templates, the parser also checks the described template
declaration when the instantiated declaration itself does not carry docs.

```ts
/** Normalized source or output path. Example: "Source/Types.cpp". */
type FilePath = string;
/** Lowercase MD5 hex digest. Example: "d41d8cd98f00b204e9800998ecf8427e". */
type Md5Hex = string;
/** Integer serialized as decimal text. Example: "1". */
type DecimalIntegerString = string;

/** C++ access specifier text. Example: "public". */
type AccessSpecifier = "public" | "protected" | "private";
/** C++ storage class text. Example: "static". */
type StorageClass = "extern" | "static" | "private_extern" | "auto" | "register";
/** Member-function reference qualifier. Example: "&". */
type RefQualifier = "&" | "&&";
/** Scoped enum keyword. Example: "class". */
type ScopedKind = "class" | "struct";
/** C++ record declaration keyword. Example: "struct". */
type RecordKind = "class" | "struct" | "union";
/** Forward-declared C++ declaration category. Example: "class". */
type ForwardDeclarationKind = RecordKind | "enum";
/** Top-level or nested declaration category. Example: "class". */
type DeclarationKind = RecordKind | "enum" | "forwardDeclaration" | "alias" | "function" | "variable";
/** Function declaration category. Example: "method". */
type FunctionKind = "function" | "constructor" | "method" | "destructor" | "conversion";
/** Template parameter category. Example: "nonType". */
type TemplateParameterKind = "typename" | "class" | "nonType" | "typenameTemplate" | "classTemplate" | string;
/** Clang template specialization category. Example: "explicitSpecialization". */
type TemplateSpecializationKind =
  | "partialSpecialization"
  | "explicitSpecialization"
  | "explicitInstantiationDeclaration"
  | "explicitInstantiationDefinition";
/** C++ exception specification text. Example: "noexcept". */
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

/** Root JSON object emitted for one source file when using --split-strategy file. */
interface ParserMetadataJson {
  /** Source file path represented by this output file. Example: "Source/Types.cpp". */
  path: FilePath;
  /** File content hash for the source file represented by this output. Example: "d41d8cd98f00b204e9800998ecf8427e". */
  hash: Md5Hex;
  /** Direct includes recorded for this source file. Example: ["Source/Types.h"]. */
  includes: FilePath[];
  /** Top-level declarations emitted from this source file, in source order. */
  declarations: Declaration[];
}

/** File metadata emitted alongside declaration-split output. */
interface ParserFileMetadataJson {
  /** Source file path represented by this metadata file. Example: "Source/Types.cpp". */
  path: FilePath;
  /** File content hash for the represented source file. Example: "d41d8cd98f00b204e9800998ecf8427e". */
  hash: Md5Hex;
  /** Direct includes recorded for this source file. Example: ["Source/Types.h"]. */
  includes: FilePath[];
}

/** Fields shared by every emitted top-level or nested declaration. */
interface DeclarationCommon {
  /** Declaration category emitted as the `kind` discriminator. Example: "class". */
  kind: DeclarationKind;
  /** Unqualified declaration identifier, omitted for unnamed declarations. Example: "Alpha". */
  name?: string;
  /** Fully qualified declaration name with a leading global scope qualifier. Example: "::ns::Alpha". */
  qualifiedName?: string;
  /** Source file that contains the declaration location. Example: "Source/Types.h". */
  file: FilePath;
  /** Content hash for the declaration source range, when available. Example: "9e107d9d372bb6826bd81d3542a419d6". */
  hash?: Md5Hex;
  /** Zero-based order of this top-level declaration within its source file; omitted for nested declarations. */
  occurrenceIndex?: number;
  /** Lexical namespace and record scope containing the declaration. Example: ["ns", "Alpha"]. */
  scope: string[];
  /** Formatted Doxygen documentation attached to the declaration. Example: "Owns player inventory state.". */
  documentation?: string;
  /** True when the declaration has no source-level identifier. */
  isAnonymous?: true;
}

type Declaration =
  | ClassDeclaration
  | StructDeclaration
  | UnionDeclaration
  | EnumDeclaration
  | ForwardDeclaration
  | AliasDeclaration
  | FreeFunctionDeclaration
  | GlobalDeclaration;

type NestedDeclaration =
  | ClassDeclaration
  | StructDeclaration
  | UnionDeclaration
  | EnumDeclaration
  | ForwardDeclaration
  | AliasDeclaration;

/** Shared record, struct, class, and union metadata. */
interface RecordLayoutDetails {
  /** Template parameters declared on the record template. */
  templateParameters?: TemplateParameter[];
  /** True when the record is an explicit template specialization or instantiation. */
  isTemplateSpecialization?: true;
  /** Clang template specialization category for the record specialization; omitted when not specialized. Example: "explicitSpecialization". */
  templateSpecializationKind?: TemplateSpecializationKind;
  /** Fully qualified primary template name for a record specialization; omitted when not specialized. Example: "::ns::Box". */
  primaryTemplateQualifiedName?: string;
  /** Template arguments used by a record specialization; omitted when not specialized or unavailable. Example: ["int"]. */
  templateArguments?: string[];
  /** True when Clang has a complete record definition. */
  isCompleteDefinition?: true;
  /** ABI record size in bytes, when layout is available. */
  sizeBytes?: number;
  /** ABI record alignment in bytes, when layout is available. */
  alignBytes?: number;
  /** Non-static data members declared by the record. */
  fields: Field[];
  /** Nested type and alias declarations in source order. */
  nested: NestedDeclaration[];
}

/** Complete class declaration payload. */
interface ClassDeclaration extends DeclarationCommon, RecordLayoutDetails {
  kind: "class";
  /** Direct C++ base specifiers. */
  bases: BaseSpecifier[];
  /** Static data members declared by the class. */
  staticVariables: VariableMetadata[];
  /** Member functions, constructors, destructors, and conversions declared by the class. */
  methods: FunctionMetadata[];
}

/** Complete struct declaration payload. */
interface StructDeclaration extends DeclarationCommon, RecordLayoutDetails {
  kind: "struct";
  /** Direct C++ base specifiers. */
  bases: BaseSpecifier[];
  /** Static data members declared by the struct. */
  staticVariables: VariableMetadata[];
  /** Member functions, constructors, destructors, and conversions declared by the struct. */
  methods: FunctionMetadata[];
}

/** Complete union declaration payload. */
interface UnionDeclaration extends DeclarationCommon, RecordLayoutDetails {
  kind: "union";
}

/** Complete enum declaration payload. */
interface EnumDeclaration extends DeclarationCommon {
  kind: "enum";
  /** Enum underlying integer type, when Clang exposes one. Example: "uint8_t". */
  underlyingType?: string;
  /** True when the enum is declared as a scoped enum. */
  isScoped?: true;
  /** Scoped enum keyword, either `class` or `struct`. Example: "class". */
  scopedKind?: ScopedKind;
  /** Enumerators declared by the enum. */
  enumerators: Enumerator[];
}

/** Record or enum forward declaration payload. */
interface ForwardDeclaration extends DeclarationCommon {
  kind: "forwardDeclaration";
  /** C++ declaration category being forward-declared. Example: "class". */
  forwardDeclarationKind: ForwardDeclarationKind;
  /** Template parameters declared on a forward-declared record template; omitted for non-templates. */
  templateParameters?: TemplateParameter[];
  /** True when the forward declaration is an explicit template specialization or instantiation. */
  isTemplateSpecialization?: true;
  /** Clang template specialization category for the forward declaration; omitted when not specialized. Example: "explicitSpecialization". */
  templateSpecializationKind?: TemplateSpecializationKind;
  /** Fully qualified primary template name for a forward-declaration specialization; omitted when not specialized. Example: "::ns::Box". */
  primaryTemplateQualifiedName?: string;
  /** Template arguments used by a forward-declaration specialization; omitted when not specialized or unavailable. Example: ["int"]. */
  templateArguments?: string[];
  /** Enum underlying integer type, when this is an enum forward declaration. Example: "uint8_t". */
  underlyingType?: string;
  /** True when this is a scoped enum forward declaration. */
  isScoped?: true;
  /** Scoped enum keyword, either `class` or `struct`. Example: "class". */
  scopedKind?: ScopedKind;
}

/** Type alias declaration payload. */
interface AliasDeclaration extends DeclarationCommon {
  kind: "alias";
  /** Template parameters declared on an alias template; omitted for non-template aliases. */
  templateParameters?: TemplateParameter[];
  /** Type named by the alias declaration. Example: "std::vector<int>". */
  aliasedType: string;
}

/** Top-level function payloads are flattened into the declaration object. */
interface FreeFunctionDeclaration extends DeclarationCommon, FunctionDetails {
  kind: "function";
  functionKind: "function";
}

/** Top-level variable payloads are flattened into the declaration object. */
interface GlobalDeclaration extends DeclarationCommon, VariableDetails {
  kind: "variable";
}

/** JSON representation of one C++ template parameter declaration. */
interface TemplateParameter {
  /** Template parameter category, such as `typename`, `class`, `nonType`, or `classTemplate`. Example: "typename". */
  kind: TemplateParameterKind;
  /** Identifier introduced by the template parameter; for `template <int Count>`, this is "Count". Example: "T". */
  name?: string;
  /** Formatted Doxygen documentation attached to the template parameter declaration. Example: "Element type.". */
  documentation?: string;
  /** Declared type of a non-type template parameter, including the parameter name; omitted for type parameters. Example: "int Count". */
  type?: string;
  /** True when the parameter is a template parameter pack. */
  isParameterPack?: true;
  /** Inner template parameters for a template-template parameter; omitted for ordinary type and non-type parameters. */
  parameters?: TemplateParameter[];
}

/** JSON representation of one function parameter declaration. */
interface Parameter {
  /** Parameter identifier; omitted for unnamed parameters. Example: "count". */
  name?: string;
  /** Parameter type without the parameter identifier. Example: "ns::Alpha *". */
  type: string;
  /** Pretty-printed parameter declaration, including type and name. Example: "ns::Alpha *value". */
  declaration: string;
  /** Formatted Doxygen documentation attached to the parameter declaration. Example: "Number of elements to read.". */
  documentation?: string;
}

/** ABI vtable slot metadata for a virtual method. */
interface VTableIndex {
  /** Method slot index in the relevant virtual table. */
  index: number;
  /** ABI vtable pointer offset used for this slot, or zero when the ABI has no separate offset. */
  offset: number;
}

/** JSON representation of a free function, method, constructor, destructor, or conversion function. */
interface FunctionDetails {
  /** Function declaration category emitted as `functionKind`. Example: "method". */
  functionKind: FunctionKind;
  /** Function type/signature without the function identifier. Example: "int (float) const". */
  type: string;
  /** Return type for ordinary functions and methods; omitted for constructors and destructors. Example: "int". */
  returnType?: string;
  /** C++ access specifier for a class member function. Example: "private". */
  access?: AccessSpecifier;
  /** Storage class written on a free function declaration. Example: "static". */
  storageClass?: StorageClass;
  /** True when a non-static member function is declared `const`. */
  isConst?: true;
  /** True when a non-static member function is declared `volatile`. */
  isVolatile?: true;
  /** True when a member function is declared `static`. */
  isStatic?: true;
  /** True when a member function is virtual, including overrides. */
  isVirtual?: true;
  /** True when a virtual member function is pure. */
  isPure?: true;
  /** True when the function is declared `constexpr`. */
  isConstexpr?: true;
  /** True when the function is declared `consteval`. */
  isConsteval?: true;
  /** True when the function is declared or treated as inline. */
  isInline?: true;
  /** True when the function declaration is explicitly deleted. */
  isDeleted?: true;
  /** True when the function declaration is explicitly defaulted. */
  isDefaulted?: true;
  /** True when a constructor or conversion function is declared `explicit`. */
  isExplicit?: true;
  /** Reference qualifier on a non-static member function. Example: "&". */
  refQualifier?: RefQualifier;
  /** Exception specification attached to the function type. Example: "noexcept". */
  exceptionSpec?: ExceptionSpec;
  /** Template parameters declared on a function template; omitted for non-template functions. */
  templateParameters?: TemplateParameter[];
  /** True when the function is an explicit function template specialization or instantiation. */
  isTemplateSpecialization?: true;
  /** Clang template specialization category for a function template specialization; omitted when not specialized. Example: "explicitSpecialization". */
  templateSpecializationKind?: TemplateSpecializationKind;
  /** Fully qualified primary function template name for a function template specialization; omitted when not specialized. Example: "::ns::Identity". */
  primaryTemplateQualifiedName?: string;
  /** Template arguments used by a function template specialization; omitted when not specialized or unavailable. Example: ["int"]. */
  templateArguments?: string[];
  /** Ordered function parameter list. */
  parameters: Parameter[];
  /** ABI virtual table slot information for a virtual member function, when available. */
  vtableIndex?: VTableIndex;
}

/** Member-function declaration metadata. */
interface FunctionMetadata extends FunctionDetails {
  /** Unqualified function or method name. Example: "Method". */
  name: string;
  /** Fully qualified function name with a leading global scope qualifier. Example: "::ns::Alpha::Method". */
  qualifiedName?: string;
  /** Source file that contains the function declaration location. Example: "Source/Types.cpp". */
  file: FilePath;
  /** Lexical namespace and record scope containing the function. Example: ["ns", "Alpha"]. */
  scope: string[];
  /** Formatted Doxygen documentation attached to the function declaration. Example: "Updates cached bounds.". */
  documentation?: string;
}

/** JSON representation of a global variable or static data member. */
interface VariableDetails {
  /** Template parameters declared on a variable template; omitted for non-template variables. */
  templateParameters?: TemplateParameter[];
  /** True when the variable is an explicit variable template specialization or instantiation. */
  isTemplateSpecialization?: true;
  /** Clang template specialization category for a variable template specialization; omitted when not specialized. Example: "explicitSpecialization". */
  templateSpecializationKind?: TemplateSpecializationKind;
  /** Fully qualified primary variable template name for a variable template specialization; omitted when not specialized. Example: "::ns::Value". */
  primaryTemplateQualifiedName?: string;
  /** Template arguments used by a variable template specialization; omitted when not specialized or unavailable. Example: ["int"]. */
  templateArguments?: string[];
  /** Variable type without the variable identifier. Example: "const int". */
  type: string;
  /** Pretty-printed variable declaration, including type and name. Example: "const int globalVar". */
  declaration: string;
  /** C++ access specifier for a static data member. Example: "private". */
  access?: AccessSpecifier;
  /** Storage class written on the variable declaration. Example: "static". */
  storageClass?: StorageClass;
  /** True when the variable is declared `constexpr`. */
  isConstexpr?: true;
  /** True when the variable is declared `inline`. */
  isInline?: true;
  /** True when the variable is a static data member of a record. */
  isStaticDataMember?: true;
  /** True when the variable has thread-local storage duration. */
  isThreadLocal?: true;
}

/** Static data member metadata. */
interface VariableMetadata extends VariableDetails {
  /** Unqualified variable identifier. Example: "globalVar". */
  name: string;
  /** Fully qualified variable name with a leading global scope qualifier. Example: "::ns::value". */
  qualifiedName?: string;
  /** Source file that contains the variable declaration location. Example: "Source/Globals.cpp". */
  file: FilePath;
  /** Lexical namespace and record scope containing the variable. Example: ["ns", "Alpha"]. */
  scope: string[];
  /** Formatted Doxygen documentation attached to the variable declaration. Example: "Maximum supported count.". */
  documentation?: string;
}

/** JSON representation of an enum constant. */
interface Enumerator {
  /** Enumerator identifier. Example: "One". */
  name: string;
  /** Evaluated integral enumerator value as a decimal string. Example: "1". */
  value: DecimalIntegerString;
  /** Source file that contains the enumerator declaration location. Example: "Source/Types.h". */
  file: FilePath;
  /** Lexical namespace and record scope containing the enumerator. Example: ["ns", "Alpha"]. */
  scope: string[];
  /** Formatted Doxygen documentation attached to the enumerator declaration. Example: "Default mode.". */
  documentation?: string;
}

/** JSON representation of a non-static data member field. */
interface Field {
  /** Field identifier, omitted for anonymous fields. Example: "field". */
  name?: string;
  /** Source file that contains the field declaration location. Example: "Source/Types.h". */
  file: FilePath;
  /** Lexical namespace and record scope containing the field. Example: ["ns", "Alpha"]. */
  scope: string[];
  /** Formatted Doxygen documentation attached to the field declaration. Example: "Cached size.". */
  documentation?: string;
  /** Field type without the field identifier. Example: "const int". */
  type: string;
  /** Pretty-printed field declaration, including type and name. Example: "const int field". */
  declaration: string;
  /** C++ access specifier for the field. Example: "public". */
  access?: AccessSpecifier;
  /** True when the field is declared `mutable`. */
  isMutable?: true;
  /** True when the field is a bit-field. */
  isBitfield?: true;
  /** Constant width of a bit-field in bits, when available. */
  bitWidth?: number;
  /** ABI field offset from the containing record start, in bits, when layout is available. */
  offsetBits?: number;
}

/** JSON representation of one direct C++ base specifier. */
interface BaseSpecifier {
  /** Written base type. Example: "Base". */
  type: string;
  /** Fully qualified base record name with a leading global scope qualifier. Example: "::ns::Base". */
  qualifiedName?: string;
  /** Access specifier on the base clause. Example: "protected". */
  access?: AccessSpecifier;
  /** True when the base specifier uses virtual inheritance. */
  isVirtual?: true;
  /** ABI base subobject offset in bytes, when record layout is available. */
  offset?: number;
}

// The parser reads compile_commands.json and builds a one-entry filtered
// compile command buffer for Clang.
type CompileCommandsJson = CompileCommandEntry[];
type FilteredCompileCommandsJson = [FilteredCompileCommandEntry];

interface CompileCommandEntry {
  /** Translation-unit source file path from compile_commands.json. Example: "Source/Module.cpp". */
  file: FilePath;
  /** Original compiler command line, if the database uses command form. Example: "clang-cl /c Source/Module.cpp". */
  command?: string;
  /** Working directory used to run the compiler command. Example: "C:/Project". */
  directory?: FilePath;
  /** Compiler output path from the original compile command. Example: "Intermediate/Module.obj". */
  output?: string;
  /** Extra compile database fields are accepted by the input shape. Example key: "arguments". */
  [unknownKey: string]: unknown;
}

interface FilteredCompileCommandEntry {
  /** Translation-unit source file path selected for this parser run. Example: "Source/Module.cpp". */
  file: FilePath;
  /** Rewritten compiler command line passed to Clang tooling. Example: "clang-cl /c Source/Module.cpp". */
  command: string;
  /** Working directory used to run the rewritten compiler command. Example: "C:/Project". */
  directory: FilePath;
  /** Compiler output path copied from the selected compile command. Example: "Intermediate/Module.obj". */
  output: string;
}
```
