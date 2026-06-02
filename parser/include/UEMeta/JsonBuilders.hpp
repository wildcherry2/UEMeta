#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <glaze/glaze.hpp>

#include "UEMeta/JsonHelpers.hpp"

namespace UEMeta {
    /**
     * @brief JSON representation of one C++ template parameter declaration.
     */
    struct JsonTemplateParameter {
        /**
         * @brief Template parameter category, such as `typename`, `class`, `nonType`, or `classTemplate`.
         *
         * @code{.cpp}
         * template <typename T, class U, int N, template <class> class Alloc>
         *           // kind for T == "typename"; kind for N == "nonType"
         * @endcode
         */
        std::string kind;

        /**
         * @brief Identifier introduced by the template parameter.
         *
         * @code{.cpp}
         * template <typename T>
         *                    // name == "T"
         * @endcode
         */
        std::string name;

        /**
         * @brief Formatted Doxygen documentation attached to the template parameter declaration.
         */
        std::string documentation;

        /**
         * @brief Declared type of a non-type template parameter, including the parameter name.
         *
         * @code{.cpp}
         * template <int Count>
         *           // type == "int Count"
         * @endcode
         */
        std::string type;

        /**
         * @brief True when the parameter is a template parameter pack.
         *
         * @code{.cpp}
         * template <typename... Args>
         *                    // isParameterPack == true
         * @endcode
         */
        bool is_parameter_pack{};

        /**
         * @brief Inner template parameters for a template-template parameter.
         *
         * @code{.cpp}
         * template <template <typename Element> class Container>
         *                    // parameters contains Element
         * @endcode
         */
        std::vector<JsonTemplateParameter> parameters;
    };

    /**
     * @brief ABI vtable slot metadata for a virtual method.
     */
    struct JsonVTableIndex {
        /**
         * @brief Method slot index in the relevant virtual table.
         *
         * @code{.cpp}
         * struct Interface { virtual void Tick(); };
         *                    // index identifies Tick's virtual slot
         * @endcode
         */
        std::uint64_t index{};

        /**
         * @brief ABI vtable pointer offset used for this slot, or zero when the ABI has no separate offset.
         *
         * @code{.cpp}
         * struct Left { virtual void A(); };
         * struct Right { virtual void B(); };
         * struct Derived : Left, Right { void B() override; };
         *                              // offset identifies Right's vfptr position
         * @endcode
         */
        std::int64_t offset{};
    };

    /**
     * @brief JSON representation of one function parameter declaration.
     */
    struct JsonParameter {
        /**
         * @brief Parameter identifier.
         *
         * @code{.cpp}
         * void Free(int count);
         *               // name == "count"
         * @endcode
         */
        std::string name;

        /**
         * @brief Parameter type without the parameter identifier.
         *
         * @code{.cpp}
         * void Free(ns::Alpha* value);
         *           // type == "ns::Alpha *"
         * @endcode
         */
        std::string type;

        /**
         * @brief Pretty-printed parameter declaration, including type and name.
         *
         * @code{.cpp}
         * void Free(ns::Alpha* value);
         *           // declaration == "ns::Alpha *value"
         * @endcode
         */
        std::string declaration;

        /**
         * @brief Formatted Doxygen documentation attached to the parameter declaration.
         */
        std::string documentation;
    };

    /**
     * @brief JSON representation of a free function, method, constructor, destructor, or conversion function.
     */
    struct JsonFunction {
        /**
         * @brief Function declaration category emitted as `functionKind`.
         *
         * @code{.cpp}
         * void Free();
         * struct Alpha { Alpha(); void Method(); ~Alpha(); explicit operator bool() const; };
         * // functionKind values include "function", "constructor", "method", "destructor", and "conversion"
         * @endcode
         */
        std::string function_kind;

        /**
         * @brief Unqualified function or method name.
         *
         * @code{.cpp}
         * namespace ns { struct Alpha { void Method(); }; }
         *                              // name == "Method"
         * @endcode
         */
        std::string name;

        /**
         * @brief Fully qualified function name with a leading global scope qualifier.
         *
         * @code{.cpp}
         * namespace ns { struct Alpha { void Method(); }; }
         *                              // qualifiedName == "::ns::Alpha::Method"
         * @endcode
         */
        std::string qualified_name;

        /**
         * @brief Source file that contains the function declaration location.
         *
         * @code{.cpp}
         * // In Source.cpp:
         * void Free();
         * // file is the normalized path to Source.cpp
         * @endcode
         */
        std::string file;

        /**
         * @brief Lexical namespace and record scope containing the function.
         *
         * @code{.cpp}
         * namespace ns { struct Alpha { void Method(); }; }
         *                              // scope == {"ns", "Alpha"}
         * @endcode
         */
        std::vector<std::string> scope;

        /**
         * @brief Formatted Doxygen documentation attached to the function declaration.
         */
        std::string documentation;

        /**
         * @brief Return type for ordinary functions and methods; empty for constructors and destructors.
         *
         * @code{.cpp}
         * int Count();
         * // returnType == "int"
         * @endcode
         */
        std::string return_type;

        /**
         * @brief C++ access specifier for a class member function.
         *
         * @code{.cpp}
         * struct Alpha { private: void Hidden(); };
         *                         // access == "private"
         * @endcode
         */
        std::string access;

        /**
         * @brief Storage class written on a free function declaration.
         *
         * @code{.cpp}
         * static void Internal();
         * // storageClass == "static"
         * @endcode
         */
        std::string storage_class;

        /**
         * @brief Reference qualifier on a non-static member function.
         *
         * @code{.cpp}
         * struct Alpha { void Touch() &; };
         *                             // refQualifier == "&"
         * @endcode
         */
        std::string ref_qualifier;

        /**
         * @brief Exception specification attached to the function type.
         *
         * @code{.cpp}
         * void Save() noexcept;
         *             // exceptionSpec == "noexcept"
         * @endcode
         */
        std::string exception_spec;

        /**
         * @brief True when a non-static member function is declared `const`.
         *
         * @code{.cpp}
         * struct Alpha { int Size() const; };
         *                           // isConst == true
         * @endcode
         */
        bool is_const{};

        /**
         * @brief True when a non-static member function is declared `volatile`.
         *
         * @code{.cpp}
         * struct Alpha { int Size() volatile; };
         *                           // isVolatile == true
         * @endcode
         */
        bool is_volatile{};

        /**
         * @brief True when a member function is declared `static`.
         *
         * @code{.cpp}
         * struct Alpha { static void Helper(); };
         *                // isStatic == true
         * @endcode
         */
        bool is_static{};

        /**
         * @brief True when a member function is virtual, including overrides.
         *
         * @code{.cpp}
         * struct Base { virtual void Tick(); };
         *               // isVirtual == true
         * @endcode
         */
        bool is_virtual{};

        /**
         * @brief True when a virtual member function is pure.
         *
         * @code{.cpp}
         * struct Base { virtual void Tick() = 0; };
         *                                    // isPure == true
         * @endcode
         */
        bool is_pure{};

        /**
         * @brief True when the function is declared `constexpr`.
         *
         * @code{.cpp}
         * constexpr int Value();
         * // isConstexpr == true
         * @endcode
         */
        bool is_constexpr{};

        /**
         * @brief True when the function is declared `consteval`.
         *
         * @code{.cpp}
         * consteval int Id();
         * // isConsteval == true
         * @endcode
         */
        bool is_consteval{};

        /**
         * @brief True when the function is declared or treated as inline.
         *
         * @code{.cpp}
         * inline void Draw();
         * // isInline == true
         * @endcode
         */
        bool is_inline{};

        /**
         * @brief True when the function declaration is explicitly deleted.
         *
         * @code{.cpp}
         * struct Alpha { Alpha(const Alpha&) = delete; };
         *                                     // isDeleted == true
         * @endcode
         */
        bool is_deleted{};

        /**
         * @brief True when the function declaration is explicitly defaulted.
         *
         * @code{.cpp}
         * struct Alpha { Alpha() = default; };
         *                         // isDefaulted == true
         * @endcode
         */
        bool is_defaulted{};

        /**
         * @brief True when a constructor or conversion function is declared `explicit`.
         *
         * @code{.cpp}
         * struct Alpha { explicit Alpha(int); explicit operator bool() const; };
         *                // isExplicit == true
         * @endcode
         */
        bool is_explicit{};

        /**
         * @brief ABI virtual table slot information for a virtual member function, when available.
         *
         * @code{.cpp}
         * struct Base { virtual void Tick(); };
         *               // vtableIndex describes Tick's virtual slot
         * @endcode
         */
        std::optional<JsonVTableIndex> vtable_index;

        /**
         * @brief Template parameters declared on a function template.
         *
         * @code{.cpp}
         * template <typename T>
         * T Identity(T value);
         * // templateParameters contains T
         * @endcode
         */
        std::vector<JsonTemplateParameter> template_parameters;

        /**
         * @brief True when the function is an explicit function template specialization or instantiation.
         *
         * @code{.cpp}
         * template <typename T> T Identity(T value);
         * template <> int Identity<int>(int value);
         * // isTemplateSpecialization == true for the specialization
         * @endcode
         */
        bool is_template_specialization{};

        /**
         * @brief Clang template specialization category for a function template specialization.
         *
         * @code{.cpp}
         * template <> int Identity<int>(int value);
         * // templateSpecializationKind == "explicitSpecialization"
         * @endcode
         */
        std::string template_specialization_kind;

        /**
         * @brief Fully qualified primary function template name for a function template specialization.
         *
         * @code{.cpp}
         * namespace ns { template <typename T> T Identity(T value); }
         * template <> int ns::Identity<int>(int value);
         * // primaryTemplateQualifiedName == "::ns::Identity"
         * @endcode
         */
        std::string primary_template_qualified_name;

        /**
         * @brief Template arguments used by a function template specialization.
         *
         * @code{.cpp}
         * template <> int Identity<int>(int value);
         * // templateArguments contains "int"
         * @endcode
         */
        std::vector<std::string> template_arguments;

        /**
         * @brief Ordered function parameter list.
         *
         * @code{.cpp}
         * void Free(ns::Alpha* value, int count);
         *           // parameters contains value and count
         * @endcode
         */
        std::vector<JsonParameter> parameters;
    };

    /**
     * @brief JSON representation of a global variable or static data member.
     */
    struct JsonVariable {
        /**
         * @brief Unqualified variable identifier.
         *
         * @code{.cpp}
         * inline int globalVar;
         *            // name == "globalVar"
         * @endcode
         */
        std::string name;

        /**
         * @brief Fully qualified variable name with a leading global scope qualifier.
         *
         * @code{.cpp}
         * namespace ns { inline int value; }
         *                     // qualifiedName == "::ns::value"
         * @endcode
         */
        std::string qualified_name;

        /**
         * @brief Source file that contains the variable declaration location.
         *
         * @code{.cpp}
         * // In Globals.cpp:
         * inline int globalVar;
         * // file is the normalized path to Globals.cpp
         * @endcode
         */
        std::string file;

        /**
         * @brief Lexical namespace and record scope containing the variable.
         *
         * @code{.cpp}
         * namespace ns { struct Alpha { static int value; }; }
         *                              // scope == {"ns", "Alpha"}
         * @endcode
         */
        std::vector<std::string> scope;

        /**
         * @brief Formatted Doxygen documentation attached to the variable declaration.
         */
        std::string documentation;

        /**
         * @brief Variable type without the variable identifier.
         *
         * @code{.cpp}
         * inline const int globalVar = 1;
         *        // type == "const int"
         * @endcode
         */
        std::string type;

        /**
         * @brief Pretty-printed variable declaration, including type and name.
         *
         * @code{.cpp}
         * inline const int globalVar = 1;
         *        // declaration == "const int globalVar"
         * @endcode
         */
        std::string declaration;

        /**
         * @brief C++ access specifier for a static data member.
         *
         * @code{.cpp}
         * class Alpha { private: static int hidden; };
         *                        // access == "private"
         * @endcode
         */
        std::string access;

        /**
         * @brief Storage class written on the variable declaration.
         *
         * @code{.cpp}
         * static int fileLocal;
         * // storageClass == "static"
         * @endcode
         */
        std::string storage_class;

        /**
         * @brief Template parameters declared on a variable template.
         *
         * @code{.cpp}
         * template <typename T>
         * inline T value{};
         * // templateParameters contains T
         * @endcode
         */
        std::vector<JsonTemplateParameter> template_parameters;

        /**
         * @brief True when the variable is an explicit variable template specialization or instantiation.
         *
         * @code{.cpp}
         * template <typename T> inline T value{};
         * template <> inline int value<int> = 1;
         * // isTemplateSpecialization == true for the specialization
         * @endcode
         */
        bool is_template_specialization{};

        /**
         * @brief Clang template specialization category for a variable template specialization.
         */
        std::string template_specialization_kind;

        /**
         * @brief Fully qualified primary variable template name for a variable template specialization.
         */
        std::string primary_template_qualified_name;

        /**
         * @brief Template arguments used by a variable template specialization.
         */
        std::vector<std::string> template_arguments;

        /**
         * @brief True when the variable is declared `constexpr`.
         *
         * @code{.cpp}
         * constexpr int MaxCount = 4;
         * // isConstexpr == true
         * @endcode
         */
        bool is_constexpr{};

        /**
         * @brief True when the variable is declared `inline`.
         *
         * @code{.cpp}
         * inline int globalVar;
         * // isInline == true
         * @endcode
         */
        bool is_inline{};

        /**
         * @brief True when the variable is a static data member of a record.
         *
         * @code{.cpp}
         * struct Alpha { static double staticValue; };
         *                // isStaticDataMember == true
         * @endcode
         */
        bool is_static_data_member{};

        /**
         * @brief True when the variable has thread-local storage duration.
         *
         * @code{.cpp}
         * thread_local int tlsValue;
         * // isThreadLocal == true
         * @endcode
         */
        bool is_thread_local{};
    };

    /**
     * @brief JSON representation of an enum constant.
     */
    struct JsonEnumerator {
        /**
         * @brief Enumerator identifier.
         *
         * @code{.cpp}
         * enum class Mode { One = 1 };
         *                   // name == "One"
         * @endcode
         */
        std::string name;

        /**
         * @brief Evaluated integral enumerator value as a decimal string.
         *
         * @code{.cpp}
         * enum class Mode { One = 1 };
         *                         // value == "1"
         * @endcode
         */
        std::string value;

        /**
         * @brief Source file that contains the enumerator declaration location.
         *
         * @code{.cpp}
         * // In Types.cpp:
         * enum class Mode { One = 1 };
         * // file is the normalized path to Types.cpp
         * @endcode
         */
        std::string file;

        /**
         * @brief Lexical namespace and record scope containing the enumerator.
         *
         * @code{.cpp}
         * namespace ns { struct Alpha { enum class Mode { One = 1 }; }; }
         *                                             // scope == {"ns", "Alpha"}
         * @endcode
         */
        std::vector<std::string> scope;

        /**
         * @brief Formatted Doxygen documentation attached to the enumerator declaration.
         */
        std::string documentation;
    };

    /**
     * @brief JSON representation of a non-static data member field.
     */
    struct JsonField {
        /**
         * @brief Field identifier, empty for anonymous fields.
         *
         * @code{.cpp}
         * struct Alpha { int field; };
         *                    // name == "field"
         * @endcode
         */
        std::string name;

        /**
         * @brief Source file that contains the field declaration location.
         *
         * @code{.cpp}
         * // In Types.cpp:
         * struct Alpha { int field; };
         * // file is the normalized path to Types.cpp
         * @endcode
         */
        std::string file;

        /**
         * @brief Lexical namespace and record scope containing the field.
         *
         * @code{.cpp}
         * namespace ns { struct Alpha { int field; }; }
         *                              // scope == {"ns", "Alpha"}
         * @endcode
         */
        std::vector<std::string> scope;

        /**
         * @brief Formatted Doxygen documentation attached to the field declaration.
         */
        std::string documentation;

        /**
         * @brief Field type without the field identifier.
         *
         * @code{.cpp}
         * struct Alpha { const int field; };
         *                // type == "const int"
         * @endcode
         */
        std::string type;

        /**
         * @brief Pretty-printed field declaration, including type and name.
         *
         * @code{.cpp}
         * struct Alpha { const int field; };
         *                // declaration == "const int field"
         * @endcode
         */
        std::string declaration;

        /**
         * @brief C++ access specifier for the field.
         *
         * @code{.cpp}
         * class Alpha { public: int field; };
         *                       // access == "public"
         * @endcode
         */
        std::string access;

        /**
         * @brief True when the field is declared `mutable`.
         *
         * @code{.cpp}
         * struct Alpha { mutable int cache; };
         *                // isMutable == true
         * @endcode
         */
        bool is_mutable{};

        /**
         * @brief True when the field is a bit-field.
         *
         * @code{.cpp}
         * struct Flags { unsigned mask : 3; };
         *                              // isBitfield == true
         * @endcode
         */
        bool is_bitfield{};

        /**
         * @brief Constant width of a bit-field in bits, when available.
         *
         * @code{.cpp}
         * struct Flags { unsigned mask : 3; };
         *                               // bitWidth == 3
         * @endcode
         */
        std::optional<std::uint64_t> bit_width;

        /**
         * @brief ABI field offset from the containing record start, in bits, when layout is available.
         *
         * @code{.cpp}
         * struct Alpha { double pad; int field; };
         *                            // offsetBits describes field's layout offset
         * @endcode
         */
        std::optional<std::uint64_t> offset_bits;
    };

    /**
     * @brief JSON representation of one direct C++ base specifier.
     */
    struct JsonBase {
        /**
         * @brief Written base type.
         *
         * @code{.cpp}
         * struct Derived : public Base {};
         *                         // type == "Base"
         * @endcode
         */
        std::string type;

        /**
         * @brief Fully qualified base record name with a leading global scope qualifier.
         *
         * @code{.cpp}
         * namespace ns { struct Base {}; struct Derived : Base {}; }
         *                                           // qualifiedName == "::ns::Base"
         * @endcode
         */
        std::string qualified_name;

        /**
         * @brief Access specifier on the base clause.
         *
         * @code{.cpp}
         * struct Derived : protected Base {};
         *                  // access == "protected"
         * @endcode
         */
        std::string access;

        /**
         * @brief True when the base specifier uses virtual inheritance.
         *
         * @code{.cpp}
         * struct Derived : virtual Base {};
         *                  // isVirtual == true
         * @endcode
         */
        bool is_virtual{};

        /**
         * @brief ABI base subobject offset in bytes, when record layout is available.
         *
         * @code{.cpp}
         * struct Left {};
         * struct Right {};
         * struct Derived : Left, Right {};
         *                        // offset describes Right's base subobject location
         * @endcode
         */
        std::optional<std::int64_t> offset;
    };

    /**
     * @brief JSON representation of one file's ordered include directives.
     */
    struct JsonIncludeOrder {
        /**
         * @brief Source file that contains the include directives.
         */
        std::string file;

        /**
         * @brief Included files in directive order for the source file.
         */
        std::vector<std::string> inclusions;
    };

    struct JsonClassDeclaration;
    struct JsonStructDeclaration;
    struct JsonUnionDeclaration;
    struct JsonEnumDeclaration;
    struct JsonForwardDeclaration;
    struct JsonAliasDeclaration;

    enum class JsonDeclarationBucket {
        Class,
        Struct,
        Union,
        Enum,
        ForwardDeclaration,
        Alias,
        FreeFunction,
        Global
    };

    struct JsonDeclarationSlot {
        JsonDeclarationBucket bucket{};
        std::size_t index{};
    };

    /**
     * @brief Fields shared by every emitted top-level or nested declaration.
     */
    struct JsonDeclarationCommon {
        std::string name;
        std::string qualified_name;
        std::string file;
        std::string hash;
        std::vector<std::string> scope;
        std::string documentation;
        bool is_anonymous{};
    };

    /**
     * @brief Nested type declarations grouped by their concrete declaration shape.
     */
    struct JsonNestedDeclarations {
        std::vector<JsonDeclarationSlot> declaration_order;
        std::vector<JsonClassDeclaration> classes;
        std::vector<JsonStructDeclaration> structs;
        std::vector<JsonUnionDeclaration> unions;
        std::vector<JsonEnumDeclaration> enums;
        std::vector<JsonForwardDeclaration> forward_declarations;
        std::vector<JsonAliasDeclaration> aliases;
    };

    /**
     * @brief Complete class declaration payload.
     */
    struct JsonClassDeclaration {
        JsonDeclarationCommon common;
        std::vector<JsonTemplateParameter> template_parameters;
        bool is_template_specialization{};
        std::string template_specialization_kind;
        std::string primary_template_qualified_name;
        std::vector<std::string> template_arguments;
        bool is_complete_definition{};
        std::optional<std::uint64_t> size_bytes;
        std::optional<std::uint64_t> align_bytes;
        std::vector<JsonBase> bases;
        std::vector<JsonField> fields;
        std::vector<JsonVariable> static_variables;
        std::vector<JsonFunction> methods;
        JsonNestedDeclarations nested;
    };

    /**
     * @brief Complete struct declaration payload.
     */
    struct JsonStructDeclaration {
        JsonDeclarationCommon common;
        std::vector<JsonTemplateParameter> template_parameters;
        bool is_template_specialization{};
        std::string template_specialization_kind;
        std::string primary_template_qualified_name;
        std::vector<std::string> template_arguments;
        bool is_complete_definition{};
        std::optional<std::uint64_t> size_bytes;
        std::optional<std::uint64_t> align_bytes;
        std::vector<JsonBase> bases;
        std::vector<JsonField> fields;
        std::vector<JsonVariable> static_variables;
        std::vector<JsonFunction> methods;
        JsonNestedDeclarations nested;
    };

    /**
     * @brief Complete union declaration payload.
     */
    struct JsonUnionDeclaration {
        JsonDeclarationCommon common;
        std::vector<JsonTemplateParameter> template_parameters;
        bool is_template_specialization{};
        std::string template_specialization_kind;
        std::string primary_template_qualified_name;
        std::vector<std::string> template_arguments;
        bool is_complete_definition{};
        std::optional<std::uint64_t> size_bytes;
        std::optional<std::uint64_t> align_bytes;
        std::vector<JsonField> fields;
        JsonNestedDeclarations nested;
    };

    /**
     * @brief Complete enum declaration payload.
     */
    struct JsonEnumDeclaration {
        JsonDeclarationCommon common;
        std::string underlying_type;
        bool is_scoped{};
        std::string scoped_kind;
        std::vector<JsonEnumerator> enumerators;
    };

    /**
     * @brief Record or enum forward declaration payload.
     */
    struct JsonForwardDeclaration {
        JsonDeclarationCommon common;
        std::vector<JsonTemplateParameter> template_parameters;
        bool is_template_specialization{};
        std::string template_specialization_kind;
        std::string primary_template_qualified_name;
        std::vector<std::string> template_arguments;
        std::string forward_declaration_kind;
        std::string underlying_type;
        bool is_scoped{};
        std::string scoped_kind;
    };

    /**
     * @brief Type alias declaration payload.
     */
    struct JsonAliasDeclaration {
        JsonDeclarationCommon common;
        std::vector<JsonTemplateParameter> template_parameters;
        std::string aliased_type;
    };

    /**
     * @brief Top-level free function declaration payload.
     */
    struct JsonFreeFunctionDeclaration {
        JsonDeclarationCommon common;
        std::string function_kind;
        std::string return_type;
        std::string access;
        std::string storage_class;
        std::string ref_qualifier;
        std::string exception_spec;
        bool is_const{};
        bool is_volatile{};
        bool is_static{};
        bool is_virtual{};
        bool is_pure{};
        bool is_constexpr{};
        bool is_consteval{};
        bool is_inline{};
        bool is_deleted{};
        bool is_defaulted{};
        bool is_explicit{};
        std::optional<JsonVTableIndex> vtable_index;
        std::vector<JsonTemplateParameter> template_parameters;
        bool is_template_specialization{};
        std::string template_specialization_kind;
        std::string primary_template_qualified_name;
        std::vector<std::string> template_arguments;
        std::vector<JsonParameter> parameters;
    };

    /**
     * @brief Top-level global variable declaration payload.
     */
    struct JsonGlobalVariableDeclaration {
        JsonDeclarationCommon common;
        std::vector<JsonTemplateParameter> template_parameters;
        bool is_template_specialization{};
        std::string template_specialization_kind;
        std::string primary_template_qualified_name;
        std::vector<std::string> template_arguments;
        std::string type;
        std::string declaration;
        std::string access;
        std::string storage_class;
        bool is_constexpr{};
        bool is_inline{};
        bool is_static_data_member{};
        bool is_thread_local{};
    };

    /**
     * @brief Top-level declarations grouped by concrete declaration shape.
     */
    struct JsonTopLevelDeclarations {
        std::vector<JsonDeclarationSlot> declaration_order;
        std::vector<JsonClassDeclaration> classes;
        std::vector<JsonStructDeclaration> structs;
        std::vector<JsonUnionDeclaration> unions;
        std::vector<JsonEnumDeclaration> enums;
        std::vector<JsonForwardDeclaration> forward_declarations;
        std::vector<JsonAliasDeclaration> aliases;
        std::vector<JsonFreeFunctionDeclaration> free_functions;
        std::vector<JsonGlobalVariableDeclaration> globals;
    };

    /**
     * @brief Root JSON object emitted for one source file.
     */
    struct JsonFileOutput : JsonTopLevelDeclarations {
        /**
         * @brief Source file path represented by this output file.
         */
        std::string path;

        /**
         * @brief File content hash for the source file represented by this output.
         */
        std::string hash;

        /**
         * @brief Direct includes recorded for this source file.
         */
        std::vector<std::string> includes;

    };

}

/**
 * @brief Glaze metadata that maps JsonTemplateParameter fields to JSON object keys.
 */
template <>
struct glz::meta<UEMeta::JsonTemplateParameter> {
    using T = UEMeta::JsonTemplateParameter;

    static constexpr auto value = object(
        "kind", &T::kind,
        "name", &T::name,
        "documentation", &T::documentation,
        "type", &T::type,
        "isParameterPack", &T::is_parameter_pack,
        "parameters", &T::parameters
    );

    /**
     * @brief Omits empty optional JSON fields while writing template parameter metadata.
     */
    template <typename Value>
    static constexpr bool skip_if(Value&& value, const std::string_view key, const meta_context&) {
        using V = std::decay_t<Value>;
        if constexpr (std::same_as<V, std::string>) {
            return (key == "name" || key == "documentation" || key == "type") && value.empty();
        } else if constexpr (std::same_as<V, bool>) {
            return !value;
        } else if constexpr (std::same_as<V, std::vector<UEMeta::JsonTemplateParameter>>) {
            return key == "parameters" && value.empty();
        }
        return false;
    }
};

/**
 * @brief Glaze metadata that maps JsonVTableIndex fields to JSON object keys.
 */
template <>
struct glz::meta<UEMeta::JsonVTableIndex> {
    using T = UEMeta::JsonVTableIndex;

    static constexpr auto value = object(
        "index", &T::index,
        "offset", &T::offset
    );
};

/**
 * @brief Glaze metadata that maps JsonParameter fields to JSON object keys.
 */
template <>
struct glz::meta<UEMeta::JsonParameter> {
    using T = UEMeta::JsonParameter;

    static constexpr auto value = object(
        "name", &T::name,
        "type", &T::type,
        "declaration", &T::declaration,
        "documentation", &T::documentation
    );

    /**
     * @brief Omits empty optional JSON fields while writing function parameter metadata.
     */
    template <typename Value>
    static constexpr bool skip_if(Value&& value, const std::string_view key, const meta_context&) {
        using V = std::decay_t<Value>;
        if constexpr (std::same_as<V, std::string>) {
            return (key == "name" || key == "documentation") && value.empty();
        }
        return false;
    }
};

/**
 * @brief Glaze metadata that maps JsonFunction fields to JSON object keys.
 */
template <>
struct glz::meta<UEMeta::JsonFunction> {
    using T = UEMeta::JsonFunction;

    static constexpr auto value = object(
        "functionKind", &T::function_kind,
        "name", &T::name,
        "qualifiedName", &T::qualified_name,
        "file", glz::custom<nullptr, UEMeta::JsonDetail::FileScrubber<UEMeta::JsonFunction>>,
        "scope", &T::scope,
        "documentation", &T::documentation,
        "returnType", &T::return_type,
        "access", &T::access,
        "storageClass", &T::storage_class,
        "isConst", &T::is_const,
        "isVolatile", &T::is_volatile,
        "isStatic", &T::is_static,
        "isVirtual", &T::is_virtual,
        "isPure", &T::is_pure,
        "isConstexpr", &T::is_constexpr,
        "isConsteval", &T::is_consteval,
        "isInline", &T::is_inline,
        "isDeleted", &T::is_deleted,
        "isDefaulted", &T::is_defaulted,
        "isExplicit", &T::is_explicit,
        "refQualifier", &T::ref_qualifier,
        "exceptionSpec", &T::exception_spec,
        "templateParameters", &T::template_parameters,
        "isTemplateSpecialization", &T::is_template_specialization,
        "templateSpecializationKind", &T::template_specialization_kind,
        "primaryTemplateQualifiedName", &T::primary_template_qualified_name,
        "templateArguments", &T::template_arguments,
        "parameters", &T::parameters,
        "vtableIndex", &T::vtable_index
    );

    /**
     * @brief Omits empty optional JSON fields while writing function metadata.
     */
    template <typename Value>
    static constexpr bool skip_if(Value&& value, const std::string_view key, const meta_context&) {
        using V = std::decay_t<Value>;
        if constexpr (std::same_as<V, std::string>) {
            return (key == "qualifiedName" || key == "returnType" || key == "access" ||
                    key == "storageClass" || key == "documentation" ||
                    key == "refQualifier" || key == "exceptionSpec" ||
                    key == "templateSpecializationKind" || key == "primaryTemplateQualifiedName") &&
                   value.empty();
        } else if constexpr (std::same_as<V, bool>) {
            return !value;
        } else if constexpr (std::same_as<V, std::vector<UEMeta::JsonTemplateParameter>>) {
            return key == "templateParameters" && value.empty();
        } else if constexpr (std::same_as<V, std::vector<std::string>>) {
            return key == "templateArguments" && value.empty();
        }
        return false;
    }
};

/**
 * @brief Glaze metadata that maps JsonVariable fields to JSON object keys.
 */
template <>
struct glz::meta<UEMeta::JsonVariable> {
    using T = UEMeta::JsonVariable;

    static constexpr auto value = object(
        "name", &T::name,
        "qualifiedName", &T::qualified_name,
        "file", glz::custom<nullptr, UEMeta::JsonDetail::FileScrubber<UEMeta::JsonVariable>>,
        "scope", &T::scope,
        "documentation", &T::documentation,
        "type", &T::type,
        "declaration", &T::declaration,
        "access", &T::access,
        "storageClass", &T::storage_class,
        "templateParameters", &T::template_parameters,
        "isTemplateSpecialization", &T::is_template_specialization,
        "templateSpecializationKind", &T::template_specialization_kind,
        "primaryTemplateQualifiedName", &T::primary_template_qualified_name,
        "templateArguments", &T::template_arguments,
        "isConstexpr", &T::is_constexpr,
        "isInline", &T::is_inline,
        "isStaticDataMember", &T::is_static_data_member,
        "isThreadLocal", &T::is_thread_local
    );

    /**
     * @brief Omits empty optional JSON fields while writing variable metadata.
     */
    template <typename Value>
    static constexpr bool skip_if(Value&& value, const std::string_view key, const meta_context&) {
        using V = std::decay_t<Value>;
        if constexpr (std::same_as<V, std::string>) {
            return (key == "qualifiedName" || key == "documentation" ||
                    key == "access" || key == "storageClass" ||
                    key == "templateSpecializationKind" || key == "primaryTemplateQualifiedName") && value.empty();
        } else if constexpr (std::same_as<V, bool>) {
            return !value;
        } else if constexpr (std::same_as<V, std::vector<UEMeta::JsonTemplateParameter>>) {
            return key == "templateParameters" && value.empty();
        } else if constexpr (std::same_as<V, std::vector<std::string>>) {
            return key == "templateArguments" && value.empty();
        }
        return false;
    }
};

/**
 * @brief Glaze metadata that maps JsonEnumerator fields to JSON object keys.
 */
template <>
struct glz::meta<UEMeta::JsonEnumerator> {
    using T = UEMeta::JsonEnumerator;

    static constexpr auto value = object(
        "name", &T::name,
        "value", &T::value,
        "file", glz::custom<nullptr, UEMeta::JsonDetail::FileScrubber<UEMeta::JsonEnumerator>>,
        "scope", &T::scope,
        "documentation", &T::documentation
    );

    /**
     * @brief Omits empty optional JSON fields while writing enumerator metadata.
     */
    template <typename Value>
    static constexpr bool skip_if(Value&& value, const std::string_view key, const meta_context&) {
        using V = std::decay_t<Value>;
        if constexpr (std::same_as<V, std::string>) {
            return key == "documentation" && value.empty();
        }
        return false;
    }
};

/**
 * @brief Glaze metadata that maps JsonField fields to JSON object keys.
 */
template <>
struct glz::meta<UEMeta::JsonField> {
    using T = UEMeta::JsonField;

    static constexpr auto value = object(
        "name", &T::name,
        "file", glz::custom<nullptr, UEMeta::JsonDetail::FileScrubber<UEMeta::JsonField>>,
        "scope", &T::scope,
        "documentation", &T::documentation,
        "type", &T::type,
        "declaration", &T::declaration,
        "access", &T::access,
        "isMutable", &T::is_mutable,
        "isBitfield", &T::is_bitfield,
        "bitWidth", &T::bit_width,
        "offsetBits", &T::offset_bits
    );

    /**
     * @brief Omits empty optional JSON fields while writing field metadata.
     */
    template <typename Value>
    static constexpr bool skip_if(Value&& value, const std::string_view key, const meta_context&) {
        using V = std::decay_t<Value>;
        if constexpr (std::same_as<V, std::string>) {
            return (key == "name" || key == "documentation" || key == "access") && value.empty();
        } else if constexpr (std::same_as<V, bool>) {
            return !value;
        }
        return false;
    }
};

/**
 * @brief Glaze metadata that maps JsonBase fields to JSON object keys.
 */
template <>
struct glz::meta<UEMeta::JsonBase> {
    using T = UEMeta::JsonBase;

    static constexpr auto value = object(
        "type", &T::type,
        "qualifiedName", &T::qualified_name,
        "access", &T::access,
        "isVirtual", &T::is_virtual,
        "offset", &T::offset
    );

    /**
     * @brief Omits empty optional JSON fields while writing base-class metadata.
     */
    template <typename Value>
    static constexpr bool skip_if(Value&& value, const std::string_view key, const meta_context&) {
        using V = std::decay_t<Value>;
        if constexpr (std::same_as<V, std::string>) {
            return (key == "qualifiedName" || key == "access") && value.empty();
        } else if constexpr (std::same_as<V, bool>) {
            return !value;
        }
        return false;
    }
};

template <>
struct glz::meta<UEMeta::JsonClassDeclaration> {
    static constexpr auto custom_write = true;
};

template <>
struct glz::meta<UEMeta::JsonStructDeclaration> {
    static constexpr auto custom_write = true;
};

template <>
struct glz::meta<UEMeta::JsonUnionDeclaration> {
    static constexpr auto custom_write = true;
};

template <>
struct glz::meta<UEMeta::JsonEnumDeclaration> {
    static constexpr auto custom_write = true;
};

template <>
struct glz::meta<UEMeta::JsonForwardDeclaration> {
    static constexpr auto custom_write = true;
};

template <>
struct glz::meta<UEMeta::JsonAliasDeclaration> {
    static constexpr auto custom_write = true;
};

template <>
struct glz::meta<UEMeta::JsonFreeFunctionDeclaration> {
    static constexpr auto custom_write = true;
};

template <>
struct glz::meta<UEMeta::JsonGlobalVariableDeclaration> {
    static constexpr auto custom_write = true;
};

template <>
struct glz::meta<UEMeta::JsonNestedDeclarations> {
    static constexpr auto custom_write = true;
};

namespace UEMeta::JsonDetail {
    template <typename Declarations>
    struct OrderedDeclarationsView {
        const Declarations* declarations{};
    };

    template <typename Declarations>
    static bool HasOrderedDeclaration(const Declarations& declarations, const JsonDeclarationSlot& slot) noexcept {
        switch (slot.bucket) {
            case JsonDeclarationBucket::Class:
                return slot.index < declarations.classes.size();
            case JsonDeclarationBucket::Struct:
                return slot.index < declarations.structs.size();
            case JsonDeclarationBucket::Union:
                return slot.index < declarations.unions.size();
            case JsonDeclarationBucket::Enum:
                return slot.index < declarations.enums.size();
            case JsonDeclarationBucket::ForwardDeclaration:
                return slot.index < declarations.forward_declarations.size();
            case JsonDeclarationBucket::Alias:
                return slot.index < declarations.aliases.size();
            case JsonDeclarationBucket::FreeFunction:
                if constexpr (requires { declarations.free_functions; }) {
                    return slot.index < declarations.free_functions.size();
                }
                return false;
            case JsonDeclarationBucket::Global:
                if constexpr (requires { declarations.globals; }) {
                    return slot.index < declarations.globals.size();
                }
                return false;
        }

        return false;
    }

    template <auto Opts, typename Declarations>
    static void WriteOrderedDeclaration(const Declarations& declarations,
                                        const JsonDeclarationSlot& slot,
                                        glz::is_context auto&& ctx,
                                        auto&& b,
                                        auto&& ix) noexcept {
        switch (slot.bucket) {
            case JsonDeclarationBucket::Class:
                glz::serialize<glz::JSON>::op<Opts>(declarations.classes[slot.index], ctx, b, ix);
                break;
            case JsonDeclarationBucket::Struct:
                glz::serialize<glz::JSON>::op<Opts>(declarations.structs[slot.index], ctx, b, ix);
                break;
            case JsonDeclarationBucket::Union:
                glz::serialize<glz::JSON>::op<Opts>(declarations.unions[slot.index], ctx, b, ix);
                break;
            case JsonDeclarationBucket::Enum:
                glz::serialize<glz::JSON>::op<Opts>(declarations.enums[slot.index], ctx, b, ix);
                break;
            case JsonDeclarationBucket::ForwardDeclaration:
                glz::serialize<glz::JSON>::op<Opts>(declarations.forward_declarations[slot.index], ctx, b, ix);
                break;
            case JsonDeclarationBucket::Alias:
                glz::serialize<glz::JSON>::op<Opts>(declarations.aliases[slot.index], ctx, b, ix);
                break;
            case JsonDeclarationBucket::FreeFunction:
                if constexpr (requires { declarations.free_functions; }) {
                    glz::serialize<glz::JSON>::op<Opts>(declarations.free_functions[slot.index], ctx, b, ix);
                }
                break;
            case JsonDeclarationBucket::Global:
                if constexpr (requires { declarations.globals; }) {
                    glz::serialize<glz::JSON>::op<Opts>(declarations.globals[slot.index], ctx, b, ix);
                }
                break;
        }
    }

    template <auto Opts, typename Declarations>
    static void WriteOrderedDeclarations(const Declarations& declarations,
                                         glz::is_context auto&& ctx,
                                         auto&& b,
                                         auto&& ix) noexcept {
        glz::dump('[', b, ix);
        bool wrote = false;
        for (const auto& slot : declarations.declaration_order) {
            if (!HasOrderedDeclaration(declarations, slot)) {
                continue;
            }
            if (wrote) {
                glz::dump(',', b, ix);
            }
            WriteOrderedDeclaration<Opts>(declarations, slot, ctx, b, ix);
            wrote = true;
        }
        glz::dump(']', b, ix);
    }

    template <auto Opts, typename Declaration, typename Payload>
    static void WriteDeclarationObject(const std::string_view kind,
                                       const Declaration& declaration,
                                       const Payload& payload,
                                       glz::is_context auto&& ctx,
                                       auto&& b,
                                       auto&& ix) noexcept {
        auto name = NonEmptyString(declaration.common.name);
        auto qualified_name = NonEmptyString(declaration.common.qualified_name);
        auto scope = SpanOf(declaration.common.scope);
        auto docs = NonEmptyString(declaration.common.documentation);
        auto is_anonymous = TrueOnly(declaration.common.is_anonymous);
        auto hash = NonEmptyString(declaration.common.hash);
        // glz::obj stores string-like values as string_view, so keep the scrubbed string alive through serialization.
        auto file = FileScrubber(declaration.common);
        auto common = glz::obj{
            "kind", kind,
            "name", name,
            "qualifiedName", qualified_name,
            "file", file,
            "hash", hash,
            "scope", scope,
            "documentation", docs,
            "isAnonymous", is_anonymous
        };
        auto object = glz::merge{common, payload};
        glz::serialize<glz::JSON>::op<Opts>(object, ctx, b, ix);
    }

    template <auto Opts, typename Declaration>
    static void WriteClassLikeDeclaration(const std::string_view kind,
                                          const Declaration& declaration,
                                          glz::is_context auto&& ctx,
                                          auto&& b,
                                          auto&& ix) noexcept {
        auto template_parameters = NonEmptySpanOf(declaration.template_parameters);
        auto is_template_specialization = TrueOnly(declaration.is_template_specialization);
        auto template_specialization_kind = NonEmptyString(declaration.template_specialization_kind);
        auto primary_template_qualified_name = NonEmptyString(declaration.primary_template_qualified_name);
        auto template_arguments = NonEmptySpanOf(declaration.template_arguments);
        auto is_complete_definition = TrueOnly(declaration.is_complete_definition);
        auto bases = SpanOf(declaration.bases);
        auto fields = SpanOf(declaration.fields);
        auto static_variables = SpanOf(declaration.static_variables);
        auto methods = SpanOf(declaration.methods);
        auto payload = glz::obj{
            "templateParameters", template_parameters,
            "isTemplateSpecialization", is_template_specialization,
            "templateSpecializationKind", template_specialization_kind,
            "primaryTemplateQualifiedName", primary_template_qualified_name,
            "templateArguments", template_arguments,
            "isCompleteDefinition", is_complete_definition,
            "sizeBytes", declaration.size_bytes,
            "alignBytes", declaration.align_bytes,
            "bases", bases,
            "fields", fields,
            "staticVariables", static_variables,
            "methods", methods,
            "nested", declaration.nested
        };
        WriteDeclarationObject<Opts>(kind, declaration, payload, ctx, b, ix);
    }

    template <auto Opts>
    static void WriteUnionDeclaration(const JsonUnionDeclaration& declaration,
                                      glz::is_context auto&& ctx,
                                      auto&& b,
                                      auto&& ix) noexcept {
        auto template_parameters = NonEmptySpanOf(declaration.template_parameters);
        auto is_template_specialization = TrueOnly(declaration.is_template_specialization);
        auto template_specialization_kind = NonEmptyString(declaration.template_specialization_kind);
        auto primary_template_qualified_name = NonEmptyString(declaration.primary_template_qualified_name);
        auto template_arguments = NonEmptySpanOf(declaration.template_arguments);
        auto is_complete_definition = TrueOnly(declaration.is_complete_definition);
        auto fields = SpanOf(declaration.fields);
        auto payload = glz::obj{
            "templateParameters", template_parameters,
            "isTemplateSpecialization", is_template_specialization,
            "templateSpecializationKind", template_specialization_kind,
            "primaryTemplateQualifiedName", primary_template_qualified_name,
            "templateArguments", template_arguments,
            "isCompleteDefinition", is_complete_definition,
            "sizeBytes", declaration.size_bytes,
            "alignBytes", declaration.align_bytes,
            "fields", fields,
            "nested", declaration.nested
        };
        WriteDeclarationObject<Opts>("union", declaration, payload, ctx, b, ix);
    }

    template <auto Opts, typename Declaration>
    static void WriteFunctionDeclaration(const std::string_view kind,
                                         const Declaration& declaration,
                                         glz::is_context auto&& ctx,
                                         auto&& b,
                                         auto&& ix) noexcept {
        auto return_type = NonEmptyString(declaration.return_type);
        auto access = NonEmptyString(declaration.access);
        auto storage_class = NonEmptyString(declaration.storage_class);
        auto is_const = TrueOnly(declaration.is_const);
        auto is_volatile = TrueOnly(declaration.is_volatile);
        auto is_static = TrueOnly(declaration.is_static);
        auto is_virtual = TrueOnly(declaration.is_virtual);
        auto is_pure = TrueOnly(declaration.is_pure);
        auto is_constexpr = TrueOnly(declaration.is_constexpr);
        auto is_consteval = TrueOnly(declaration.is_consteval);
        auto is_inline = TrueOnly(declaration.is_inline);
        auto is_deleted = TrueOnly(declaration.is_deleted);
        auto is_defaulted = TrueOnly(declaration.is_defaulted);
        auto is_explicit = TrueOnly(declaration.is_explicit);
        auto ref_qualifier = NonEmptyString(declaration.ref_qualifier);
        auto exception_spec = NonEmptyString(declaration.exception_spec);
        auto template_parameters = NonEmptySpanOf(declaration.template_parameters);
        auto is_template_specialization = TrueOnly(declaration.is_template_specialization);
        auto template_specialization_kind = NonEmptyString(declaration.template_specialization_kind);
        auto primary_template_qualified_name = NonEmptyString(declaration.primary_template_qualified_name);
        auto template_arguments = NonEmptySpanOf(declaration.template_arguments);
        auto parameters = SpanOf(declaration.parameters);
        auto payload = glz::obj{
            "functionKind", declaration.function_kind,
            "returnType", return_type,
            "access", access,
            "storageClass", storage_class,
            "isConst", is_const,
            "isVolatile", is_volatile,
            "isStatic", is_static,
            "isVirtual", is_virtual,
            "isPure", is_pure,
            "isConstexpr", is_constexpr,
            "isConsteval", is_consteval,
            "isInline", is_inline,
            "isDeleted", is_deleted,
            "isDefaulted", is_defaulted,
            "isExplicit", is_explicit,
            "refQualifier", ref_qualifier,
            "exceptionSpec", exception_spec,
            "templateParameters", template_parameters,
            "isTemplateSpecialization", is_template_specialization,
            "templateSpecializationKind", template_specialization_kind,
            "primaryTemplateQualifiedName", primary_template_qualified_name,
            "templateArguments", template_arguments,
            "parameters", parameters,
            "vtableIndex", declaration.vtable_index
        };
        WriteDeclarationObject<Opts>(kind, declaration, payload, ctx, b, ix);
    }

    template <auto Opts, typename Declaration>
    static void WriteVariableDeclaration(const std::string_view kind,
                                         const Declaration& declaration,
                                         glz::is_context auto&& ctx,
                                         auto&& b,
                                         auto&& ix) noexcept {
        auto access = NonEmptyString(declaration.access);
        auto storage_class = NonEmptyString(declaration.storage_class);
        auto template_parameters = NonEmptySpanOf(declaration.template_parameters);
        auto is_template_specialization = TrueOnly(declaration.is_template_specialization);
        auto template_specialization_kind = NonEmptyString(declaration.template_specialization_kind);
        auto primary_template_qualified_name = NonEmptyString(declaration.primary_template_qualified_name);
        auto template_arguments = NonEmptySpanOf(declaration.template_arguments);
        auto is_constexpr = TrueOnly(declaration.is_constexpr);
        auto is_inline = TrueOnly(declaration.is_inline);
        auto is_static_data_member = TrueOnly(declaration.is_static_data_member);
        auto is_thread_local = TrueOnly(declaration.is_thread_local);
        auto payload = glz::obj{
            "type", declaration.type,
            "declaration", declaration.declaration,
            "access", access,
            "storageClass", storage_class,
            "templateParameters", template_parameters,
            "isTemplateSpecialization", is_template_specialization,
            "templateSpecializationKind", template_specialization_kind,
            "primaryTemplateQualifiedName", primary_template_qualified_name,
            "templateArguments", template_arguments,
            "isConstexpr", is_constexpr,
            "isInline", is_inline,
            "isStaticDataMember", is_static_data_member,
            "isThreadLocal", is_thread_local
        };
        WriteDeclarationObject<Opts>(kind, declaration, payload, ctx, b, ix);
    }
}

template <>
struct glz::to<glz::JSON, UEMeta::JsonClassDeclaration> {
    template <auto Opts>
    static void op(const UEMeta::JsonClassDeclaration& declaration, is_context auto&& ctx, auto&& b, auto&& ix) noexcept {
        UEMeta::JsonDetail::WriteClassLikeDeclaration<Opts>("class", declaration, ctx, b, ix);
    }
};

template <>
struct glz::to<glz::JSON, UEMeta::JsonStructDeclaration> {
    template <auto Opts>
    static void op(const UEMeta::JsonStructDeclaration& declaration, is_context auto&& ctx, auto&& b, auto&& ix) noexcept {
        UEMeta::JsonDetail::WriteClassLikeDeclaration<Opts>("struct", declaration, ctx, b, ix);
    }
};

template <>
struct glz::to<glz::JSON, UEMeta::JsonUnionDeclaration> {
    template <auto Opts>
    static void op(const UEMeta::JsonUnionDeclaration& declaration, is_context auto&& ctx, auto&& b, auto&& ix) noexcept {
        UEMeta::JsonDetail::WriteUnionDeclaration<Opts>(declaration, ctx, b, ix);
    }
};

template <>
struct glz::to<glz::JSON, UEMeta::JsonEnumDeclaration> {
    template <auto Opts>
    static void op(const UEMeta::JsonEnumDeclaration& declaration, is_context auto&& ctx, auto&& b, auto&& ix) noexcept {
        using namespace UEMeta::JsonDetail;

        auto underlying_type = NonEmptyString(declaration.underlying_type);
        auto is_scoped = TrueOnly(declaration.is_scoped);
        auto scoped_kind = NonEmptyString(declaration.scoped_kind);
        auto enumerators = SpanOf(declaration.enumerators);
        auto payload = glz::obj{
            "underlyingType", underlying_type,
            "isScoped", is_scoped,
            "scopedKind", scoped_kind,
            "enumerators", enumerators
        };
        WriteDeclarationObject<Opts>("enum", declaration, payload, ctx, b, ix);
    }
};

template <>
struct glz::to<glz::JSON, UEMeta::JsonForwardDeclaration> {
    template <auto Opts>
    static void op(const UEMeta::JsonForwardDeclaration& declaration, is_context auto&& ctx, auto&& b, auto&& ix) noexcept {
        using namespace UEMeta::JsonDetail;

        auto template_parameters = NonEmptySpanOf(declaration.template_parameters);
        auto is_template_specialization = TrueOnly(declaration.is_template_specialization);
        auto template_specialization_kind = NonEmptyString(declaration.template_specialization_kind);
        auto primary_template_qualified_name = NonEmptyString(declaration.primary_template_qualified_name);
        auto template_arguments = NonEmptySpanOf(declaration.template_arguments);
        auto forward_declaration_kind = NonEmptyString(declaration.forward_declaration_kind);
        auto underlying_type = NonEmptyString(declaration.underlying_type);
        auto is_scoped = TrueOnly(declaration.is_scoped);
        auto scoped_kind = NonEmptyString(declaration.scoped_kind);
        auto payload = glz::obj{
            "templateParameters", template_parameters,
            "isTemplateSpecialization", is_template_specialization,
            "templateSpecializationKind", template_specialization_kind,
            "primaryTemplateQualifiedName", primary_template_qualified_name,
            "templateArguments", template_arguments,
            "forwardDeclarationKind", forward_declaration_kind,
            "underlyingType", underlying_type,
            "isScoped", is_scoped,
            "scopedKind", scoped_kind
        };
        WriteDeclarationObject<Opts>("forwardDeclaration", declaration, payload, ctx, b, ix);
    }
};

template <>
struct glz::to<glz::JSON, UEMeta::JsonAliasDeclaration> {
    template <auto Opts>
    static void op(const UEMeta::JsonAliasDeclaration& declaration, is_context auto&& ctx, auto&& b, auto&& ix) noexcept {
        using namespace UEMeta::JsonDetail;

        auto template_parameters = NonEmptySpanOf(declaration.template_parameters);
        auto payload = glz::obj{
            "templateParameters", template_parameters,
            "aliasedType", declaration.aliased_type
        };
        WriteDeclarationObject<Opts>("alias", declaration, payload, ctx, b, ix);
    }
};

template <>
struct glz::to<glz::JSON, UEMeta::JsonFreeFunctionDeclaration> {
    template <auto Opts>
    static void op(const UEMeta::JsonFreeFunctionDeclaration& declaration, is_context auto&& ctx, auto&& b, auto&& ix) noexcept {
        UEMeta::JsonDetail::WriteFunctionDeclaration<Opts>("function", declaration, ctx, b, ix);
    }
};

template <>
struct glz::to<glz::JSON, UEMeta::JsonGlobalVariableDeclaration> {
    template <auto Opts>
    static void op(const UEMeta::JsonGlobalVariableDeclaration& declaration, is_context auto&& ctx, auto&& b, auto&& ix) noexcept {
        using namespace UEMeta::JsonDetail;

        auto template_parameters = NonEmptySpanOf(declaration.template_parameters);
        auto is_template_specialization = TrueOnly(declaration.is_template_specialization);
        auto template_specialization_kind = NonEmptyString(declaration.template_specialization_kind);
        auto primary_template_qualified_name = NonEmptyString(declaration.primary_template_qualified_name);
        auto template_arguments = NonEmptySpanOf(declaration.template_arguments);
        auto access = NonEmptyString(declaration.access);
        auto storage_class = NonEmptyString(declaration.storage_class);
        auto is_constexpr = TrueOnly(declaration.is_constexpr);
        auto is_inline = TrueOnly(declaration.is_inline);
        auto is_static_data_member = TrueOnly(declaration.is_static_data_member);
        auto is_thread_local = TrueOnly(declaration.is_thread_local);
        auto payload = glz::obj{
            "templateParameters", template_parameters,
            "isTemplateSpecialization", is_template_specialization,
            "templateSpecializationKind", template_specialization_kind,
            "primaryTemplateQualifiedName", primary_template_qualified_name,
            "templateArguments", template_arguments,
            "type", declaration.type,
            "declaration", declaration.declaration,
            "access", access,
            "storageClass", storage_class,
            "isConstexpr", is_constexpr,
            "isInline", is_inline,
            "isStaticDataMember", is_static_data_member,
            "isThreadLocal", is_thread_local
        };
        WriteDeclarationObject<Opts>("variable", declaration, payload, ctx, b, ix);
    }
};

template <>
struct glz::meta<UEMeta::JsonDetail::OrderedDeclarationsView<UEMeta::JsonTopLevelDeclarations>> {
    static constexpr auto custom_write = true;
};

template <>
struct glz::to<glz::JSON, UEMeta::JsonDetail::OrderedDeclarationsView<UEMeta::JsonTopLevelDeclarations>> {
    template <auto Opts>
    static void op(const UEMeta::JsonDetail::OrderedDeclarationsView<UEMeta::JsonTopLevelDeclarations>& view,
                   is_context auto&& ctx,
                   auto&& b,
                   auto&& ix) noexcept {
        if (!view.declarations) {
            glz::dump("[]", b, ix);
            return;
        }

        UEMeta::JsonDetail::WriteOrderedDeclarations<Opts>(*view.declarations, ctx, b, ix);
    }
};

template <>
struct glz::to<glz::JSON, UEMeta::JsonNestedDeclarations> {
    template <auto Opts>
    static void op(const UEMeta::JsonNestedDeclarations& declarations, is_context auto&& ctx, auto&& b, auto&& ix) noexcept {
        UEMeta::JsonDetail::WriteOrderedDeclarations<Opts>(declarations, ctx, b, ix);
    }
};

template <>
struct glz::meta<UEMeta::JsonFileOutput> {
    static constexpr auto custom_write = true;
};

template <>
struct glz::to<glz::JSON, UEMeta::JsonFileOutput> {
    template <auto Opts>
    static void op(const UEMeta::JsonFileOutput& output, is_context auto&& ctx, auto&& b, auto&& ix) noexcept {
        auto declarations = UEMeta::JsonDetail::OrderedDeclarationsView<UEMeta::JsonTopLevelDeclarations>{&output};
        auto object = glz::obj{
            "path", output.path,
            "hash", output.hash,
            "includes", output.includes,
            "declarations", declarations
        };
        serialize<JSON>::op<Opts>(object, ctx, b, ix);
    }
};
