#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <simdjson.h>

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
        std::string kind;

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
     * @brief Top-level JSON declaration object and nested declaration payload.
     */
    struct JsonDeclaration {
        /**
         * @brief Declaration category, such as `class`, `struct`, `union`, `enum`, `alias`, `function`, or `variable`.
         *
         * @code{.cpp}
         * class Alpha {};
         * enum class Mode {};
         * using Alias = Alpha;
         * void Free();
         * inline int globalVar;
         * // kind identifies the declaration grammar above
         * @endcode
         */
        std::string kind;

        /**
         * @brief Unqualified declaration name; omitted from JSON for function and variable payload declarations.
         *
         * @code{.cpp}
         * namespace ns { class Alpha {}; }
         *                      // name == "Alpha"
         * @endcode
         */
        std::string name;

        /**
         * @brief Fully qualified declaration name with a leading global scope qualifier.
         *
         * @code{.cpp}
         * namespace ns { class Alpha {}; }
         *                      // qualifiedName == "::ns::Alpha"
         * @endcode
         */
        std::string qualified_name;

        /**
         * @brief Source file that contains the declaration location.
         *
         * @code{.cpp}
         * // In uemeta_scope_fixture.cpp:
         * namespace ns { class Alpha {}; }
         * // file is the normalized path to uemeta_scope_fixture.cpp
         * @endcode
         */
        std::string file;

        /**
         * @brief Lexical namespace and record scope containing the declaration.
         *
         * @code{.cpp}
         * namespace ns { struct Alpha { struct Nested {}; }; }
         *                              // Nested scope == {"ns", "Alpha"}
         * @endcode
         */
        std::vector<std::string> scope;

        /**
         * @brief True when the declaration has no C++ identifier.
         *
         * @code{.cpp}
         * struct Alpha { union { int i; float f; }; };
         *                // anonymous union has isAnonymous == true
         * @endcode
         */
        bool is_anonymous{};

        /**
         * @brief Template parameters declared by a class, alias, function, or variable template.
         *
         * @code{.cpp}
         * template <typename T>
         * struct Box {};
         * // templateParameters contains T
         * @endcode
         */
        std::vector<JsonTemplateParameter> template_parameters;

        /**
         * @brief Enum underlying integer type.
         *
         * @code{.cpp}
         * enum class Mode : unsigned int { A };
         *                   // underlyingType == "unsigned int"
         * @endcode
         */
        std::string underlying_type;

        /**
         * @brief True when the enum uses scoped enum syntax.
         *
         * @code{.cpp}
         * enum class Mode { A };
         * // isScoped == true
         * @endcode
         */
        bool is_scoped{};

        /**
         * @brief Scoped enum keyword, either `class` or `struct`.
         *
         * @code{.cpp}
         * enum struct Flags { A };
         *      // scopedKind == "struct"
         * @endcode
         */
        std::string scoped_kind;

        /**
         * @brief Enumerators declared inside an enum body.
         *
         * @code{.cpp}
         * enum class Mode { One = 1, Two = 2 };
         *                   // enumerators contains One and Two
         * @endcode
         */
        std::vector<JsonEnumerator> enumerators;

        /**
         * @brief True when a record declaration includes a definition body.
         *
         * @code{.cpp}
         * struct Forward;
         * struct Complete { int value; };
         * // Complete has isCompleteDefinition == true
         * @endcode
         */
        bool is_complete_definition{};

        /**
         * @brief ABI size of a complete record definition in bytes, when layout is available.
         *
         * @code{.cpp}
         * struct Alpha { double d; int i; };
         * // sizeBytes describes Alpha's object size
         * @endcode
         */
        std::optional<std::uint64_t> size_bytes;

        /**
         * @brief ABI alignment of a complete record definition in bytes, when layout is available.
         *
         * @code{.cpp}
         * struct Alpha { double d; int i; };
         * // alignBytes describes Alpha's required alignment
         * @endcode
         */
        std::optional<std::uint64_t> align_bytes;

        /**
         * @brief Direct base specifiers on a class or struct declaration.
         *
         * @code{.cpp}
         * struct Derived : public Base {};
         *                  // bases contains Base
         * @endcode
         */
        std::vector<JsonBase> bases;

        /**
         * @brief Non-static data members declared in a record body.
         *
         * @code{.cpp}
         * struct Alpha { int field; };
         *                // fields contains field
         * @endcode
         */
        std::vector<JsonField> fields;

        /**
         * @brief Static data members declared in a record body.
         *
         * @code{.cpp}
         * struct Alpha { inline static double staticValue = 0.0; };
         *                // staticVariables contains staticValue
         * @endcode
         */
        std::vector<JsonVariable> static_variables;

        /**
         * @brief Member functions declared in a record body.
         *
         * @code{.cpp}
         * struct Alpha { virtual void Method(); };
         *                // methods contains Method
         * @endcode
         */
        std::vector<JsonFunction> methods;

        /**
         * @brief Nested records, enums, and aliases declared in a record body.
         *
         * @code{.cpp}
         * struct Alpha { using Alias = int; struct Nested {}; enum class Mode { One }; };
         *                // nested contains Alias, Nested, and Mode
         * @endcode
         */
        std::vector<JsonDeclaration> nested;

        /**
         * @brief Payload for a top-level function declaration when `kind == "function"`.
         *
         * @code{.cpp}
         * void Free(int count);
         * // function contains Free's function metadata
         * @endcode
         */
        std::optional<JsonFunction> function;

        /**
         * @brief Payload for a top-level variable declaration when `kind == "variable"`.
         *
         * @code{.cpp}
         * inline int globalVar;
         * // variable contains globalVar's variable metadata
         * @endcode
         */
        std::optional<JsonVariable> variable;

        /**
         * @brief Underlying type named by a C++ type alias declaration.
         *
         * @code{.cpp}
         * using TopAlias = ns::Alpha;
         *                  // aliasedType == "ns::Alpha"
         * @endcode
         */
        std::string aliased_type;
    };

    template <typename Builder, typename Value>
    void AppendMember(Builder& builder, bool& needs_comma, const char* key, const Value& value) {
        if (needs_comma) {
            builder.append_comma();
        }
        builder.append_key_value(key, value);
        needs_comma = true;
    }

    template <typename Builder>
    void AppendKey(Builder& builder, bool& needs_comma, const char* key) {
        if (needs_comma) {
            builder.append_comma();
        }
        builder.escape_and_append_with_quotes(key);
        builder.append_colon();
        needs_comma = true;
    }

    template <typename Builder, typename Range>
    void AppendArrayMember(Builder& builder, bool& needs_comma, const char* key, const Range& range) {
        AppendKey(builder, needs_comma, key);
        builder.start_array();

        bool item_needs_comma = false;
        for (const auto& item : range) {
            if (item_needs_comma) {
                builder.append_comma();
            }
            builder.append(item);
            item_needs_comma = true;
        }

        builder.end_array();
    }

    template <typename Builder, typename Value>
    void AppendOptionalMember(Builder& builder, bool& needs_comma, const char* key, const std::optional<Value>& value) {
        if (value) {
            AppendMember(builder, needs_comma, key, *value);
        }
    }

    template <typename Builder>
    void AppendStringIfNotEmpty(Builder& builder, bool& needs_comma, const char* key, const std::string& value) {
        if (!value.empty()) {
            AppendMember(builder, needs_comma, key, value);
        }
    }

    template <typename Builder>
    void AppendBoolIfTrue(Builder& builder, bool& needs_comma, const char* key, bool value) {
        if (value) {
            AppendMember(builder, needs_comma, key, value);
        }
    }

    template <typename Builder>
    void AppendTemplateParameters(Builder& builder, bool& needs_comma, const std::vector<JsonTemplateParameter>& parameters) {
        if (!parameters.empty()) {
            AppendArrayMember(builder, needs_comma, "templateParameters", parameters);
        }
    }

    template <typename Builder>
    void AppendLocationMembers(Builder& builder, bool& needs_comma, const std::string& file,
                               const std::vector<std::string>& scope) {
        AppendMember(builder, needs_comma, "file", file);
        AppendArrayMember(builder, needs_comma, "scope", scope);
    }

    template <typename Builder>
    void AppendFunctionMembers(Builder& builder, bool& needs_comma, const JsonFunction& function,
                               const bool include_location = true) {
        AppendMember(builder, needs_comma, "functionKind", function.kind);
        AppendMember(builder, needs_comma, "name", function.name);
        AppendStringIfNotEmpty(builder, needs_comma, "qualifiedName", function.qualified_name);
        if (include_location) {
            AppendLocationMembers(builder, needs_comma, function.file, function.scope);
        }
        AppendStringIfNotEmpty(builder, needs_comma, "returnType", function.return_type);
        AppendStringIfNotEmpty(builder, needs_comma, "access", function.access);
        AppendStringIfNotEmpty(builder, needs_comma, "storageClass", function.storage_class);
        AppendBoolIfTrue(builder, needs_comma, "isConst", function.is_const);
        AppendBoolIfTrue(builder, needs_comma, "isVolatile", function.is_volatile);
        AppendBoolIfTrue(builder, needs_comma, "isStatic", function.is_static);
        AppendBoolIfTrue(builder, needs_comma, "isVirtual", function.is_virtual);
        AppendBoolIfTrue(builder, needs_comma, "isPure", function.is_pure);
        AppendBoolIfTrue(builder, needs_comma, "isConstexpr", function.is_constexpr);
        AppendBoolIfTrue(builder, needs_comma, "isConsteval", function.is_consteval);
        AppendBoolIfTrue(builder, needs_comma, "isInline", function.is_inline);
        AppendBoolIfTrue(builder, needs_comma, "isDeleted", function.is_deleted);
        AppendBoolIfTrue(builder, needs_comma, "isDefaulted", function.is_defaulted);
        AppendBoolIfTrue(builder, needs_comma, "isExplicit", function.is_explicit);
        AppendStringIfNotEmpty(builder, needs_comma, "refQualifier", function.ref_qualifier);
        AppendStringIfNotEmpty(builder, needs_comma, "exceptionSpec", function.exception_spec);
        AppendTemplateParameters(builder, needs_comma, function.template_parameters);
        AppendArrayMember(builder, needs_comma, "parameters", function.parameters);

        if (function.vtable_index) {
            AppendKey(builder, needs_comma, "vtableIndex");
            builder.append(*function.vtable_index);
        }
    }

    template <typename Builder>
    void AppendVariableMembers(Builder& builder, bool& needs_comma, const JsonVariable& variable,
                               const bool include_location = true) {
        AppendMember(builder, needs_comma, "name", variable.name);
        AppendStringIfNotEmpty(builder, needs_comma, "qualifiedName", variable.qualified_name);
        if (include_location) {
            AppendLocationMembers(builder, needs_comma, variable.file, variable.scope);
        }
        AppendMember(builder, needs_comma, "type", variable.type);
        AppendMember(builder, needs_comma, "declaration", variable.declaration);
        AppendStringIfNotEmpty(builder, needs_comma, "access", variable.access);
        AppendStringIfNotEmpty(builder, needs_comma, "storageClass", variable.storage_class);
        AppendBoolIfTrue(builder, needs_comma, "isConstexpr", variable.is_constexpr);
        AppendBoolIfTrue(builder, needs_comma, "isInline", variable.is_inline);
        AppendBoolIfTrue(builder, needs_comma, "isStaticDataMember", variable.is_static_data_member);
        AppendBoolIfTrue(builder, needs_comma, "isThreadLocal", variable.is_thread_local);
    }

    template <typename Builder>
    void AppendDeclarationArray(Builder& builder, const std::vector<JsonDeclaration>& declarations) {
        builder.start_array();

        bool needs_comma = false;
        for (const auto& declaration : declarations) {
            if (needs_comma) {
                builder.append_comma();
            }
            builder.append(declaration);
            needs_comma = true;
        }

        builder.end_array();
    }

    template <typename Builder>
    void tag_invoke(simdjson::serialize_tag, Builder& builder, const JsonTemplateParameter& parameter) {
        builder.start_object();
        bool needs_comma = false;
        AppendMember(builder, needs_comma, "kind", parameter.kind);
        AppendStringIfNotEmpty(builder, needs_comma, "name", parameter.name);
        AppendStringIfNotEmpty(builder, needs_comma, "type", parameter.type);
        AppendBoolIfTrue(builder, needs_comma, "isParameterPack", parameter.is_parameter_pack);
        if (!parameter.parameters.empty()) {
            AppendArrayMember(builder, needs_comma, "parameters", parameter.parameters);
        }
        builder.end_object();
    }

    template <typename Builder>
    void tag_invoke(simdjson::serialize_tag, Builder& builder, const JsonVTableIndex& vtable_index) {
        builder.start_object();
        bool needs_comma = false;
        AppendMember(builder, needs_comma, "index", vtable_index.index);
        AppendMember(builder, needs_comma, "offset", vtable_index.offset);
        builder.end_object();
    }

    template <typename Builder>
    void tag_invoke(simdjson::serialize_tag, Builder& builder, const JsonParameter& parameter) {
        builder.start_object();
        bool needs_comma = false;
        AppendStringIfNotEmpty(builder, needs_comma, "name", parameter.name);
        AppendMember(builder, needs_comma, "type", parameter.type);
        AppendMember(builder, needs_comma, "declaration", parameter.declaration);
        builder.end_object();
    }

    template <typename Builder>
    void tag_invoke(simdjson::serialize_tag, Builder& builder, const JsonFunction& function) {
        builder.start_object();
        bool needs_comma = false;
        AppendFunctionMembers(builder, needs_comma, function);
        builder.end_object();
    }

    template <typename Builder>
    void tag_invoke(simdjson::serialize_tag, Builder& builder, const JsonVariable& variable) {
        builder.start_object();
        bool needs_comma = false;
        AppendVariableMembers(builder, needs_comma, variable);
        builder.end_object();
    }

    template <typename Builder>
    void tag_invoke(simdjson::serialize_tag, Builder& builder, const JsonEnumerator& enumerator) {
        builder.start_object();
        bool needs_comma = false;
        AppendMember(builder, needs_comma, "name", enumerator.name);
        AppendMember(builder, needs_comma, "value", enumerator.value);
        AppendLocationMembers(builder, needs_comma, enumerator.file, enumerator.scope);
        builder.end_object();
    }

    template <typename Builder>
    void tag_invoke(simdjson::serialize_tag, Builder& builder, const JsonField& field) {
        builder.start_object();
        bool needs_comma = false;
        AppendStringIfNotEmpty(builder, needs_comma, "name", field.name);
        AppendLocationMembers(builder, needs_comma, field.file, field.scope);
        AppendMember(builder, needs_comma, "type", field.type);
        AppendMember(builder, needs_comma, "declaration", field.declaration);
        AppendStringIfNotEmpty(builder, needs_comma, "access", field.access);
        AppendBoolIfTrue(builder, needs_comma, "isMutable", field.is_mutable);
        AppendBoolIfTrue(builder, needs_comma, "isBitfield", field.is_bitfield);
        AppendOptionalMember(builder, needs_comma, "bitWidth", field.bit_width);
        AppendOptionalMember(builder, needs_comma, "offsetBits", field.offset_bits);
        builder.end_object();
    }

    template <typename Builder>
    void tag_invoke(simdjson::serialize_tag, Builder& builder, const JsonBase& base) {
        builder.start_object();
        bool needs_comma = false;
        AppendMember(builder, needs_comma, "type", base.type);
        AppendStringIfNotEmpty(builder, needs_comma, "qualifiedName", base.qualified_name);
        AppendStringIfNotEmpty(builder, needs_comma, "access", base.access);
        AppendBoolIfTrue(builder, needs_comma, "isVirtual", base.is_virtual);
        AppendOptionalMember(builder, needs_comma, "offset", base.offset);
        builder.end_object();
    }

    template <typename Builder>
    void tag_invoke(simdjson::serialize_tag, Builder& builder, const JsonDeclaration& declaration) {
        builder.start_object();
        bool needs_comma = false;

        AppendMember(builder, needs_comma, "kind", declaration.kind);
        const bool payload_has_identity = declaration.kind == "function" || declaration.kind == "variable";
        if (!payload_has_identity) {
            AppendStringIfNotEmpty(builder, needs_comma, "name", declaration.name);
            AppendStringIfNotEmpty(builder, needs_comma, "qualifiedName", declaration.qualified_name);
        }
        AppendLocationMembers(builder, needs_comma, declaration.file, declaration.scope);
        AppendBoolIfTrue(builder, needs_comma, "isAnonymous", declaration.is_anonymous);
        AppendTemplateParameters(builder, needs_comma, declaration.template_parameters);

        if (declaration.kind == "enum") {
            AppendStringIfNotEmpty(builder, needs_comma, "underlyingType", declaration.underlying_type);
            AppendBoolIfTrue(builder, needs_comma, "isScoped", declaration.is_scoped);
            AppendStringIfNotEmpty(builder, needs_comma, "scopedKind", declaration.scoped_kind);
            AppendArrayMember(builder, needs_comma, "enumerators", declaration.enumerators);
        } else if (declaration.kind == "class" || declaration.kind == "struct" || declaration.kind == "union") {
            AppendBoolIfTrue(builder, needs_comma, "isCompleteDefinition", declaration.is_complete_definition);
            AppendOptionalMember(builder, needs_comma, "sizeBytes", declaration.size_bytes);
            AppendOptionalMember(builder, needs_comma, "alignBytes", declaration.align_bytes);
            AppendArrayMember(builder, needs_comma, "bases", declaration.bases);
            AppendArrayMember(builder, needs_comma, "fields", declaration.fields);
            AppendArrayMember(builder, needs_comma, "staticVariables", declaration.static_variables);
            AppendArrayMember(builder, needs_comma, "methods", declaration.methods);
            AppendArrayMember(builder, needs_comma, "nested", declaration.nested);
        } else if (declaration.kind == "function" && declaration.function) {
            AppendFunctionMembers(builder, needs_comma, *declaration.function, false);
        } else if (declaration.kind == "variable" && declaration.variable) {
            AppendVariableMembers(builder, needs_comma, *declaration.variable, false);
        } else if (declaration.kind == "alias") {
            AppendMember(builder, needs_comma, "aliasedType", declaration.aliased_type);
        }

        builder.end_object();
    }
}
