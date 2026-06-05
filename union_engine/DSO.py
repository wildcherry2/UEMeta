from __future__ import annotations

from typing import Annotated, Literal

from pydantic import BaseModel, ConfigDict, Field as PydanticField


FilePath = str
Md5Hex = str
DecimalIntegerString = str

AccessSpecifier = Literal["public", "protected", "private"]
StorageClass = Literal["extern", "static", "private_extern", "auto", "register"]
RefQualifier = Literal["&", "&&"]
ScopedKind = Literal["class", "struct"]
RecordKind = Literal["class", "struct", "union"]
ForwardDeclarationKind = RecordKind | Literal["enum"]
DeclarationKind = RecordKind | Literal[
    "enum",
    "forwardDeclaration",
    "alias",
    "function",
    "variable",
]
FunctionKind = Literal[
    "function",
    "constructor",
    "method",
    "destructor",
    "conversion",
]
TemplateParameterKind = Literal[
    "typename",
    "class",
    "nonType",
    "typenameTemplate",
    "classTemplate",
] | str
TemplateSpecializationKind = Literal[
    "partialSpecialization",
    "explicitSpecialization",
    "explicitInstantiationDeclaration",
    "explicitInstantiationDefinition",
]
ExceptionSpec = Literal[
    "throw()",
    "throw(...)",
    "__declspec(nothrow)",
    "noexcept",
    "noexcept(dependent)",
    "noexcept(false)",
    "noexcept(true)",
    "unevaluated",
    "uninstantiated",
    "unparsed",
]

class DeclarationCommon(BaseModel): # rename to JDeclarationCommon, then make MDeclarationCommon with overrode types
    kind: DeclarationKind
    name: str | None = None
    qualifiedName: str | None = None
    file: FilePath
    occurrenceIndex: int | None = None
    scope: list[str]
    isAnonymous: Literal[True] | None = None
    hash: Md5Hex | None = None
    documentation: str | None = None

DeclarationCommon_VF = frozenset(['file', 'occurrenceIndex', 'hash', 'documentation'])

class TemplateParameter(BaseModel):
    kind: TemplateParameterKind
    name: str | None = None
    documentation: str | None = None
    type: str | None = None
    isParameterPack: Literal[True] | None = None
    parameters: list[TemplateParameter] | None = None

TemplateParameter_VF = frozenset(['documentation', 'name', 'type', 'isParameterPack', 'kind'])


class Parameter(BaseModel):
    name: str | None = None
    type: str
    declaration: str
    documentation: str | None = None

Parameter_VF = frozenset(['name', 'declaration', 'documentation'])


class VTableIndex(BaseModel):
    index: int
    offset: int


class FunctionDetails(BaseModel):
    functionKind: FunctionKind
    returnType: str | None = None
    type: str
    access: AccessSpecifier | None = None
    storageClass: StorageClass | None = None
    isConst: Literal[True] | None = None
    isVolatile: Literal[True] | None = None
    isStatic: Literal[True] | None = None
    isVirtual: Literal[True] | None = None
    isPure: Literal[True] | None = None
    isConstexpr: Literal[True] | None = None
    isConsteval: Literal[True] | None = None
    isInline: Literal[True] | None = None
    isDeleted: Literal[True] | None = None
    isDefaulted: Literal[True] | None = None
    isExplicit: Literal[True] | None = None
    refQualifier: RefQualifier | None = None
    exceptionSpec: ExceptionSpec | None = None
    templateParameters: list[TemplateParameter] | None = None
    isTemplateSpecialization: Literal[True] | None = None
    templateSpecializationKind: TemplateSpecializationKind | None = None
    primaryTemplateQualifiedName: str | None = None
    templateArguments: list[str] | None = None
    parameters: list[Parameter]
    vtableIndex: VTableIndex | None = None

FunctionDetails_VF = frozenset(['access', 'returnType', 'storageClass', 'isStatic',
                                'isVirtual', 'isPure', 'isConstexpr', 'isConsteval',
                                'isInline', 'isDeleted', 'isDefaulted', 'isExplicit',
                                'exceptionSpec', 'isTemplateSpecialization',
                                'templateSpecializationKind', 'primaryTemplateQualifiedName',
                                'templateArguments', 'parameters', 'vtableIndex', 'type'])


class FunctionMetadata(FunctionDetails):
    name: str
    qualifiedName: str | None = None
    file: FilePath
    scope: list[str]
    documentation: str | None = None

FunctionMetadata_VF = frozenset(['file', 'documentation']).union(FunctionDetails_VF)


class VariableDetails(BaseModel):
    templateParameters: list[TemplateParameter] | None = None
    isTemplateSpecialization: Literal[True] | None = None
    templateSpecializationKind: TemplateSpecializationKind | None = None
    primaryTemplateQualifiedName: str | None = None
    templateArguments: list[str] | None = None
    type: str
    declaration: str
    access: AccessSpecifier | None = None
    storageClass: StorageClass | None = None
    isConstexpr: Literal[True] | None = None
    isInline: Literal[True] | None = None
    isStaticDataMember: Literal[True] | None = None
    isThreadLocal: Literal[True] | None = None

VariableDetails_VF = frozenset(['isTemplateSpecialization', 'templateSpecializationKind',
                                'primaryTemplateQualifiedName', 'templateArguments', 'type', 'declaration', 'access',
                                'storageClass', 'isConstexpr', 'isInline', 'isStaticDataMember', 'isThreadLocal'])


class VariableMetadata(VariableDetails):
    name: str
    qualifiedName: str | None = None
    file: FilePath
    scope: list[str]
    documentation: str | None = None

VariableMetadata_VF = frozenset(['file', 'documentation']).union(VariableDetails_VF)


class Enumerator(BaseModel):
    name: str
    value: DecimalIntegerString
    file: FilePath
    scope: list[str]
    documentation: str | None = None

Enumerator_VF = frozenset(['file', 'documentation', 'value'])


class Field(BaseModel):
    name: str | None = None
    file: FilePath
    scope: list[str]
    documentation: str | None = None
    type: str
    declaration: str
    access: AccessSpecifier | None = None
    isMutable: Literal[True] | None = None
    isBitfield: Literal[True] | None = None
    bitWidth: int | None = None
    offsetBits: int | None = None

Field_VF = frozenset(['file', 'documentation', 'type', 'declaration', 'access', 'isMutable', 'isBitfield', 'bitWidth', 'offsetBits'])


class BaseSpecifier(BaseModel):
    type: str
    qualifiedName: str | None = None
    access: AccessSpecifier | None = None
    isVirtual: Literal[True] | None = None
    offset: int | None = None

BaseSpecifier_VF = frozenset(['type', 'access', 'isVirtual', 'offset'])

class RecordLayoutDetails(BaseModel):
    templateParameters: list[TemplateParameter] | None = None
    isTemplateSpecialization: Literal[True] | None = None
    templateSpecializationKind: TemplateSpecializationKind | None = None
    primaryTemplateQualifiedName: str | None = None
    templateArguments: list[str] | None = None
    isCompleteDefinition: Literal[True] | None = None
    sizeBytes: int | None = None
    alignBytes: int | None = None
    fields: list[Field]
    nested: list[NestedDeclaration]

RecordLayoutDetails_VF = frozenset(['isTemplateSpecialization', 'templateSpecializationKind',
                                    'primaryTemplateQualifiedName', 'templateArguments', 'sizeBytes', 'alignBytes'])


class ClassDeclaration(DeclarationCommon, RecordLayoutDetails):
    kind: Literal["class"]
    bases: list[BaseSpecifier]
    staticVariables: list[VariableMetadata]
    methods: list[FunctionMetadata]

ClassDeclaration_VF = DeclarationCommon_VF.union(RecordLayoutDetails_VF)


class StructDeclaration(DeclarationCommon, RecordLayoutDetails):
    kind: Literal["struct"]
    bases: list[BaseSpecifier]
    staticVariables: list[VariableMetadata]
    methods: list[FunctionMetadata]

StructDeclaration_VF = DeclarationCommon_VF.union(RecordLayoutDetails_VF)


class UnionDeclaration(DeclarationCommon, RecordLayoutDetails):
    kind: Literal["union"]

UnionDeclaration_VF = DeclarationCommon_VF.union(RecordLayoutDetails_VF)


class EnumDeclaration(DeclarationCommon):
    kind: Literal["enum"]
    underlyingType: str | None = None
    isScoped: Literal[True] | None = None
    scopedKind: ScopedKind | None = None
    enumerators: list[Enumerator]

EnumDeclaration_VF = frozenset(['scopedKind', 'isScoped', 'underlyingType']).union(DeclarationCommon_VF)


class ForwardDeclaration(DeclarationCommon):
    kind: Literal["forwardDeclaration"]
    forwardDeclarationKind: ForwardDeclarationKind
    templateParameters: list[TemplateParameter] | None = None
    isTemplateSpecialization: Literal[True] | None = None
    templateSpecializationKind: TemplateSpecializationKind | None = None
    primaryTemplateQualifiedName: str | None = None
    templateArguments: list[str] | None = None
    underlyingType: str | None = None
    isScoped: Literal[True] | None = None
    scopedKind: ScopedKind | None = None

ForwardDeclaration_VF = frozenset(['isTemplateSpecialization', 'templateSpecializationKind',
                                   'primaryTemplateQualifiedName', 'templateArguments', 'underlyingType',
                                   'isScoped', 'scopedKind']).union(DeclarationCommon_VF)


class AliasDeclaration(DeclarationCommon):
    kind: Literal["alias"]
    templateParameters: list[TemplateParameter] | None = None
    aliasedType: str

AliasDeclaration_VF = frozenset(['aliasedType']).union(DeclarationCommon_VF)

class FreeFunctionDeclaration(DeclarationCommon, FunctionDetails):
    kind: Literal["function"]
    functionKind: Literal["function"]

FreeFunctionDeclaration_VF = DeclarationCommon_VF.union(FunctionDetails_VF)

class GlobalDeclaration(DeclarationCommon, VariableDetails):
    kind: Literal["variable"]

GlobalDeclaration_VF = DeclarationCommon_VF.union(VariableDetails_VF)

NestedDeclaration = Annotated[
    ClassDeclaration
    | StructDeclaration
    | UnionDeclaration
    | EnumDeclaration
    | ForwardDeclaration
    | AliasDeclaration,
    PydanticField(discriminator="kind"),
]
Declaration = Annotated[
    NestedDeclaration | FreeFunctionDeclaration | GlobalDeclaration,
    PydanticField(discriminator="kind"),
]

class ParserFileMetadataJson(BaseModel):
    path: FilePath
    hash: Md5Hex
    includes: list[FilePath]
