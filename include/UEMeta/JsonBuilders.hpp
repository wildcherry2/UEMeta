#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <glaze/glaze.hpp>

#include "Cli.hpp"

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

    namespace JsonDetail {
        template <typename T>
        std::span<const T> SpanOf(const std::vector<T>& values) noexcept {
            return {values.data(), values.size()};
        }

        template <typename T>
        std::optional<std::span<const T>> NonEmptySpanOf(const std::vector<T>& values) noexcept {
            if (values.empty()) {
                return std::nullopt;
            }
            return SpanOf(values);
        }

        inline std::optional<std::string_view> NonEmptyString(const std::string& value) noexcept {
            if (value.empty()) {
                return std::nullopt;
            }
            return std::string_view{value};
        }

        inline std::optional<bool> TrueOnly(const bool value) noexcept {
            if (!value) {
                return std::nullopt;
            }
            return true;
        }

        inline std::string ScrubFilePath(const std::string_view file) noexcept {
            std::string original_file;
            try {
                original_file = std::string{file};
                std::string out_file = original_file;
                const auto& delimiters = Config::GetConfig().PathDelimiters();
                const auto& blacklist = Config::GetConfig().PathBlacklist();
                bool changed = false;

                if (!delimiters.empty()) {
                    size_t highest_delim = std::string::npos;
                    for (const auto& delimiter : delimiters) {
                        if (delimiter.empty()) {
                            continue;
                        }

                        const auto delim_loc = out_file.rfind(delimiter);
                        if (delim_loc != std::string::npos &&
                            (highest_delim == std::string::npos || delim_loc > highest_delim)) {
                            highest_delim = delim_loc;
                        }
                    }
                    if (highest_delim != std::string::npos) {
                        out_file = out_file.substr(highest_delim);
                        changed = true;
                    }
                }

                if (!blacklist.empty()) {
                    constexpr std::string_view replacement = "removed";

                    for (const auto& token : blacklist) {
                        if (token.empty()) {
                            continue;
                        }

                        for (auto begin = out_file.find(token);
                            begin != std::string::npos;
                            begin = out_file.find(token, begin + replacement.size())) {
                            out_file.replace(begin, token.size(), replacement);
                            changed = true;
                        }
                    }
                }

                if (changed) {
                    out_file = std::filesystem::path{out_file}.lexically_normal().string();
                }

                return out_file;
            } catch (std::exception& ex) {
                UEM_ERROR("Error scrubbing file {}: '{}', will replace with empty string!", original_file, ex.what());
                return "";
            } catch (...) {
                UEM_ERROR("Unknown error scrubbing file {}, will replace with empty string!", original_file);
                return "";
            }
        }

        template<typename T>
        std::string FileScrubber(const T& object) noexcept {
            return ScrubFilePath(object.file);
        }
    }
}

template <>
struct glz::meta<UEMeta::JsonTemplateParameter> {
    using T = UEMeta::JsonTemplateParameter;

    static constexpr auto value = object(
        "kind", &T::kind,
        "name", &T::name,
        "type", &T::type,
        "isParameterPack", &T::is_parameter_pack,
        "parameters", &T::parameters
    );

    template <typename Value>
    static constexpr bool skip_if(Value&& value, const std::string_view key, const meta_context&) {
        using V = std::decay_t<Value>;
        if constexpr (std::same_as<V, std::string>) {
            return (key == "name" || key == "type") && value.empty();
        } else if constexpr (std::same_as<V, bool>) {
            return !value;
        } else if constexpr (std::same_as<V, std::vector<UEMeta::JsonTemplateParameter>>) {
            return key == "parameters" && value.empty();
        }
        return false;
    }
};

template <>
struct glz::meta<UEMeta::JsonVTableIndex> {
    using T = UEMeta::JsonVTableIndex;

    static constexpr auto value = object(
        "index", &T::index,
        "offset", &T::offset
    );
};

template <>
struct glz::meta<UEMeta::JsonParameter> {
    using T = UEMeta::JsonParameter;

    static constexpr auto value = object(
        "name", &T::name,
        "type", &T::type,
        "declaration", &T::declaration
    );

    template <typename Value>
    static constexpr bool skip_if(Value&& value, const std::string_view key, const meta_context&) {
        using V = std::decay_t<Value>;
        if constexpr (std::same_as<V, std::string>) {
            return key == "name" && value.empty();
        }
        return false;
    }
};

template <>
struct glz::meta<UEMeta::JsonFunction> {
    using T = UEMeta::JsonFunction;

    static constexpr auto value = object(
        "functionKind", &T::kind,
        "name", &T::name,
        "qualifiedName", &T::qualified_name,
        "file", glz::custom<nullptr, UEMeta::JsonDetail::FileScrubber<UEMeta::JsonFunction>>,
        "scope", &T::scope,
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
        "parameters", &T::parameters,
        "vtableIndex", &T::vtable_index
    );

    template <typename Value>
    static constexpr bool skip_if(Value&& value, const std::string_view key, const meta_context&) {
        using V = std::decay_t<Value>;
        if constexpr (std::same_as<V, std::string>) {
            return (key == "qualifiedName" || key == "returnType" || key == "access" ||
                    key == "storageClass" || key == "refQualifier" || key == "exceptionSpec") &&
                   value.empty();
        } else if constexpr (std::same_as<V, bool>) {
            return !value;
        } else if constexpr (std::same_as<V, std::vector<UEMeta::JsonTemplateParameter>>) {
            return key == "templateParameters" && value.empty();
        }
        return false;
    }
};

template <>
struct glz::meta<UEMeta::JsonVariable> {
    using T = UEMeta::JsonVariable;

    static constexpr auto value = object(
        "name", &T::name,
        "qualifiedName", &T::qualified_name,
        "file", glz::custom<nullptr, UEMeta::JsonDetail::FileScrubber<UEMeta::JsonVariable>>,
        "scope", &T::scope,
        "type", &T::type,
        "declaration", &T::declaration,
        "access", &T::access,
        "storageClass", &T::storage_class,
        "isConstexpr", &T::is_constexpr,
        "isInline", &T::is_inline,
        "isStaticDataMember", &T::is_static_data_member,
        "isThreadLocal", &T::is_thread_local
    );

    template <typename Value>
    static constexpr bool skip_if(Value&& value, const std::string_view key, const meta_context&) {
        using V = std::decay_t<Value>;
        if constexpr (std::same_as<V, std::string>) {
            return (key == "qualifiedName" || key == "access" || key == "storageClass") && value.empty();
        } else if constexpr (std::same_as<V, bool>) {
            return !value;
        }
        return false;
    }
};

template <>
struct glz::meta<UEMeta::JsonEnumerator> {
    using T = UEMeta::JsonEnumerator;

    static constexpr auto value = object(
        "name", &T::name,
        "value", &T::value,
        "file", glz::custom<nullptr, UEMeta::JsonDetail::FileScrubber<UEMeta::JsonEnumerator>>,
        "scope", &T::scope
    );
};

template <>
struct glz::meta<UEMeta::JsonField> {
    using T = UEMeta::JsonField;

    static constexpr auto value = object(
        "name", &T::name,
        "file", glz::custom<nullptr, UEMeta::JsonDetail::FileScrubber<UEMeta::JsonField>>,
        "scope", &T::scope,
        "type", &T::type,
        "declaration", &T::declaration,
        "access", &T::access,
        "isMutable", &T::is_mutable,
        "isBitfield", &T::is_bitfield,
        "bitWidth", &T::bit_width,
        "offsetBits", &T::offset_bits
    );

    template <typename Value>
    static constexpr bool skip_if(Value&& value, const std::string_view key, const meta_context&) {
        using V = std::decay_t<Value>;
        if constexpr (std::same_as<V, std::string>) {
            return (key == "name" || key == "access") && value.empty();
        } else if constexpr (std::same_as<V, bool>) {
            return !value;
        }
        return false;
    }
};

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
struct glz::meta<UEMeta::JsonIncludeOrder> {
    using T = UEMeta::JsonIncludeOrder;

    static constexpr auto value = object(
        "file", &T::file,
        "inclusions", &T::inclusions
    );
};

template <>
struct glz::meta<UEMeta::JsonDeclaration> {
    static constexpr auto custom_write = true;
};

template <>
struct glz::to<glz::JSON, UEMeta::JsonDeclaration> {
    template <auto Opts>
    static void op(const UEMeta::JsonDeclaration& declaration, is_context auto&& ctx, auto&& b, auto&& ix) noexcept {
        using namespace UEMeta::JsonDetail;

        const bool payload_has_identity = declaration.kind == "function" || declaration.kind == "variable";
        auto name = payload_has_identity ? std::optional<std::string_view>{} : NonEmptyString(declaration.name);
        auto qualified_name = payload_has_identity ? std::optional<std::string_view>{} : NonEmptyString(declaration.qualified_name);
        auto scope = SpanOf(declaration.scope);
        auto is_anonymous = TrueOnly(declaration.is_anonymous);
        auto template_parameters = NonEmptySpanOf(declaration.template_parameters);
        // glz::obj stores string-like values as string_view, so keep the scrubbed string alive through serialization.
        auto file = FileScrubber(declaration);
        auto common = glz::obj{
            "kind", declaration.kind,
            "name", name,
            "qualifiedName", qualified_name,
            "file", file,
            "scope", scope,
            "isAnonymous", is_anonymous,
            "templateParameters", template_parameters
        };

        if (declaration.kind == "enum") {
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
            auto object = glz::merge{common, payload};
            serialize<JSON>::op<Opts>(object, ctx, b, ix);
        } else if (declaration.kind == "class" || declaration.kind == "struct" || declaration.kind == "union") {
            auto is_complete_definition = TrueOnly(declaration.is_complete_definition);
            auto bases = SpanOf(declaration.bases);
            auto fields = SpanOf(declaration.fields);
            auto static_variables = SpanOf(declaration.static_variables);
            auto methods = SpanOf(declaration.methods);
            auto nested = SpanOf(declaration.nested);
            auto payload = glz::obj{
                "isCompleteDefinition", is_complete_definition,
                "sizeBytes", declaration.size_bytes,
                "alignBytes", declaration.align_bytes,
                "bases", bases,
                "fields", fields,
                "staticVariables", static_variables,
                "methods", methods,
                "nested", nested
            };
            auto object = glz::merge{common, payload};
            serialize<JSON>::op<Opts>(object, ctx, b, ix);
        } else if (declaration.kind == "function" && declaration.function) {
            const auto& function = *declaration.function;
            auto function_qualified_name = NonEmptyString(function.qualified_name);
            auto return_type = NonEmptyString(function.return_type);
            auto access = NonEmptyString(function.access);
            auto storage_class = NonEmptyString(function.storage_class);
            auto is_const = TrueOnly(function.is_const);
            auto is_volatile = TrueOnly(function.is_volatile);
            auto is_static = TrueOnly(function.is_static);
            auto is_virtual = TrueOnly(function.is_virtual);
            auto is_pure = TrueOnly(function.is_pure);
            auto is_constexpr = TrueOnly(function.is_constexpr);
            auto is_consteval = TrueOnly(function.is_consteval);
            auto is_inline = TrueOnly(function.is_inline);
            auto is_deleted = TrueOnly(function.is_deleted);
            auto is_defaulted = TrueOnly(function.is_defaulted);
            auto is_explicit = TrueOnly(function.is_explicit);
            auto ref_qualifier = NonEmptyString(function.ref_qualifier);
            auto exception_spec = NonEmptyString(function.exception_spec);
            auto function_template_parameters = NonEmptySpanOf(function.template_parameters);
            auto parameters = SpanOf(function.parameters);
            auto payload = glz::obj{
                "functionKind", function.kind,
                "name", function.name,
                "qualifiedName", function_qualified_name,
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
                "templateParameters", function_template_parameters,
                "parameters", parameters,
                "vtableIndex", function.vtable_index
            };
            auto object = glz::merge{common, payload};
            serialize<JSON>::op<Opts>(object, ctx, b, ix);
        } else if (declaration.kind == "variable" && declaration.variable) {
            const auto& variable = *declaration.variable;
            auto variable_qualified_name = NonEmptyString(variable.qualified_name);
            auto access = NonEmptyString(variable.access);
            auto storage_class = NonEmptyString(variable.storage_class);
            auto is_constexpr = TrueOnly(variable.is_constexpr);
            auto is_inline = TrueOnly(variable.is_inline);
            auto is_static_data_member = TrueOnly(variable.is_static_data_member);
            auto is_thread_local = TrueOnly(variable.is_thread_local);
            auto payload = glz::obj{
                "name", variable.name,
                "qualifiedName", variable_qualified_name,
                "type", variable.type,
                "declaration", variable.declaration,
                "access", access,
                "storageClass", storage_class,
                "isConstexpr", is_constexpr,
                "isInline", is_inline,
                "isStaticDataMember", is_static_data_member,
                "isThreadLocal", is_thread_local
            };
            auto object = glz::merge{common, payload};
            serialize<JSON>::op<Opts>(object, ctx, b, ix);
        } else if (declaration.kind == "alias") {
            auto payload = glz::obj{"aliasedType", declaration.aliased_type};
            auto object = glz::merge{common, payload};
            serialize<JSON>::op<Opts>(object, ctx, b, ix);
        } else {
            serialize<JSON>::op<Opts>(common, ctx, b, ix);
        }
    }
};
