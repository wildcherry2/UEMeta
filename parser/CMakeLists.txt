cmake_minimum_required(VERSION 3.25)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

project(parser LANGUAGES CXX)

include(FetchContent)
find_package(Python3 3.14 REQUIRED COMPONENTS Interpreter)

option(UEMETA_ENABLE_TESTING "Build parser testing harness" OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(FETCHCONTENT_QUIET OFF CACHE BOOL "Show FetchContent population progress" FORCE)

# Keep heavyweight dependency source trees outside individual build preset directories.
# FetchContent still uses each preset's binary dir for subbuild/build artifacts, but the
# downloaded/extracted sources are shared by Ninja Debug, Ninja Release, and VS builds.
set(UEMETA_DEPENDENCY_SOURCE_ROOT
    "${PROJECT_SOURCE_DIR}/intermediate/deps/src"
    CACHE PATH "Shared source directory for fetched dependencies"
)
set(UEMETA_DEPENDENCY_DOWNLOAD_ROOT
    "${PROJECT_SOURCE_DIR}/intermediate/deps/downloads"
    CACHE PATH "Shared download directory for fetched dependency archives"
)
set(UEMETA_DEPENDENCY_LOCK_ROOT
    "${PROJECT_SOURCE_DIR}/intermediate/deps/locks"
    CACHE PATH "Shared lock directory for fetched dependencies"
)
file(MAKE_DIRECTORY
    "${UEMETA_DEPENDENCY_SOURCE_ROOT}"
    "${UEMETA_DEPENDENCY_DOWNLOAD_ROOT}"
    "${UEMETA_DEPENDENCY_LOCK_ROOT}"
)

set(UEMETA_LLVM_VERSION "23.1.0" CACHE STRING "LLVM release fetched by CMake")

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(UEMETA_LLVM_DEFAULT_PACKAGE "LLVM-${UEMETA_LLVM_VERSION}-Linux-X64")
    set(UEMETA_LLVM_DEFAULT_PREBUILT_URL_HASH
        "SHA256=18da30f77f475688a18f7704d23f9f155ae007ed9922dbed6850a9419d9fec8c"
    )
elseif(WIN32)
    set(UEMETA_LLVM_DEFAULT_PACKAGE "clang+llvm-${UEMETA_LLVM_VERSION}-x86_64-pc-windows-msvc")
    set(UEMETA_LLVM_DEFAULT_PREBUILT_URL_HASH
        "SHA256=5799ebeca6870e9e61d5b4c8bc869ca8490e8db55f22d1388c4545404864d9e3"
    )
else()
    message(FATAL_ERROR "No LLVM prebuilt package is configured for ${CMAKE_SYSTEM_NAME}")
endif()

set(UEMETA_LLVM_PACKAGE "${UEMETA_LLVM_DEFAULT_PACKAGE}"
    CACHE STRING "LLVM binary release package"
)
set(UEMETA_LLVM_PREBUILT_URL
    "https://github.com/llvm/llvm-project/releases/download/llvmorg-${UEMETA_LLVM_VERSION}/${UEMETA_LLVM_PACKAGE}.tar.xz"
    CACHE STRING "LLVM binary release archive"
)
set(UEMETA_LLVM_PREBUILT_URL_HASH
    "${UEMETA_LLVM_DEFAULT_PREBUILT_URL_HASH}"
    CACHE STRING "LLVM binary release archive hash"
)
set(UEMETA_LLVM_LIBXML2_VERSION "2.9.12" CACHE STRING "libxml2 release used by the LLVM Windows archive")
set(UEMETA_LLVM_ZLIB_VERSION "1.3.2" CACHE STRING "zlib release used by the LLVM Windows archive")
set(UEMETA_LLVM_ZSTD_VERSION "1.5.7" CACHE STRING "zstd release used by the LLVM Windows archive")
set(UEMETA_CLI11_VERSION "2.6.2" CACHE STRING "CLI11 release fetched by CMake")
set(UEMETA_CLI11_GIT_TAG
    "37bb6edc5317e99af72ef48405e65d9ca5218861"
    CACHE STRING "CLI11 Git commit fetched by CMake"
)
set(UEMETA_QUILL_VERSION "11.1.0" CACHE STRING "Quill release fetched by CMake")
set(UEMETA_QUILL_GIT_TAG
    "v11.1.0"
    CACHE STRING "Quill Git tag fetched by CMake"
)
set(UEMETA_PROTOBUF_VERSION "35.1" CACHE STRING "Protobuf release matched to generated C++ protos")
set(UEMETA_PROTOBUF_GIT_TAG
    "v${UEMETA_PROTOBUF_VERSION}"
    CACHE STRING "Protobuf Git tag fetched by CMake"
)
set(UEMETA_ABSEIL_VERSION "20250512.1" CACHE STRING "Abseil release fetched by CMake")
set(UEMETA_ABSEIL_GIT_TAG
    "${UEMETA_ABSEIL_VERSION}"
    CACHE STRING "Abseil Git tag fetched by CMake"
)
set(UEMETA_BOOST_VERSION "1.91.0" CACHE STRING "Boost release fetched by CMake")
set(UEMETA_BOOST_GIT_TAG
    "boost-${UEMETA_BOOST_VERSION}"
    CACHE STRING "Boost Git tag fetched by CMake"
)
set(UEMETA_GOOGLETEST_VERSION "1.14.0" CACHE STRING "GoogleTest release fetched by CMake")
set(UEMETA_GOOGLETEST_GIT_TAG
    "v${UEMETA_GOOGLETEST_VERSION}"
    CACHE STRING "GoogleTest Git tag fetched by CMake"
)
set(UEMETA_MSVC_DISABLED_WARNINGS
    /wd4100 # unreferenced formal parameter
    /wd4244 # conversion, possible loss of data
    /wd4189 # local variable initialized but not referenced
    /wd4702 # unreachable code
)
set(UEMETA_MSVC_EXTERNAL_DISABLED_WARNINGS
    /w # suppress warnings while compiling non-project targets
    /wd4530 # external code may intentionally compile without unwind semantics
)

FetchContent_Declare(llvm_prebuilt
    URL "${UEMETA_LLVM_PREBUILT_URL}"
    URL_HASH "${UEMETA_LLVM_PREBUILT_URL_HASH}"
    DOWNLOAD_DIR "${UEMETA_DEPENDENCY_DOWNLOAD_ROOT}/llvm_prebuilt"
    DOWNLOAD_NO_PROGRESS FALSE
    SOURCE_DIR "${UEMETA_DEPENDENCY_SOURCE_ROOT}/${UEMETA_LLVM_PACKAGE}"
    SOURCE_SUBDIR cmake-do-not-add
)

if(WIN32)
    # LLVM's Windows library archive was built against these static libraries,
    # but does not bundle them. Its exported targets reference their canonical
    # CMake target names, so make the matching dependencies available first.
    FetchContent_Declare(llvm_libxml2
        URL "https://gitlab.gnome.org/GNOME/libxml2/-/archive/v${UEMETA_LLVM_LIBXML2_VERSION}/libxml2-v${UEMETA_LLVM_LIBXML2_VERSION}.tar.gz"
        URL_HASH "SHA256=98bfa7a9a5e2a75638422050740448ee9f02bf4dc2075c9822d7747d5ff9e617"
        DOWNLOAD_DIR "${UEMETA_DEPENDENCY_DOWNLOAD_ROOT}/llvm_libxml2"
        SOURCE_DIR "${UEMETA_DEPENDENCY_SOURCE_ROOT}/llvm-libxml2-${UEMETA_LLVM_LIBXML2_VERSION}"
        SYSTEM
    )
    FetchContent_Declare(llvm_zlib
        URL "https://github.com/madler/zlib/releases/download/v${UEMETA_LLVM_ZLIB_VERSION}/zlib-${UEMETA_LLVM_ZLIB_VERSION}.tar.gz"
        URL_HASH "SHA256=bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16"
        DOWNLOAD_DIR "${UEMETA_DEPENDENCY_DOWNLOAD_ROOT}/llvm_zlib"
        SOURCE_DIR "${UEMETA_DEPENDENCY_SOURCE_ROOT}/llvm-zlib-${UEMETA_LLVM_ZLIB_VERSION}"
        SYSTEM
    )
    FetchContent_Declare(llvm_zstd
        URL "https://github.com/facebook/zstd/releases/download/v${UEMETA_LLVM_ZSTD_VERSION}/zstd-${UEMETA_LLVM_ZSTD_VERSION}.tar.gz"
        URL_HASH "SHA256=eb33e51f49a15e023950cd7825ca74a4a2b43db8354825ac24fc1b7ee09e6fa3"
        DOWNLOAD_DIR "${UEMETA_DEPENDENCY_DOWNLOAD_ROOT}/llvm_zstd"
        SOURCE_DIR "${UEMETA_DEPENDENCY_SOURCE_ROOT}/llvm-zstd-${UEMETA_LLVM_ZSTD_VERSION}"
        SOURCE_SUBDIR build/cmake
        SYSTEM
    )
endif()

FetchContent_Declare(cli11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG "${UEMETA_CLI11_GIT_TAG}"
    GIT_PROGRESS TRUE
    SOURCE_DIR "${UEMETA_DEPENDENCY_SOURCE_ROOT}/cli11-${UEMETA_CLI11_VERSION}"
    SYSTEM
)

FetchContent_Declare(quill
    GIT_REPOSITORY https://github.com/odygrd/quill.git
    GIT_TAG "${UEMETA_QUILL_GIT_TAG}"
    GIT_PROGRESS TRUE
    SOURCE_DIR "${UEMETA_DEPENDENCY_SOURCE_ROOT}/quill-${UEMETA_QUILL_VERSION}"
    SYSTEM
)

FetchContent_Declare(protobuf
    GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
    GIT_TAG "${UEMETA_PROTOBUF_GIT_TAG}"
    GIT_PROGRESS TRUE
    SOURCE_DIR "${UEMETA_DEPENDENCY_SOURCE_ROOT}/protobuf-${UEMETA_PROTOBUF_VERSION}"
    SYSTEM
)

FetchContent_Declare(absl
    GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
    GIT_TAG "${UEMETA_ABSEIL_GIT_TAG}"
    GIT_PROGRESS TRUE
    SOURCE_DIR "${UEMETA_DEPENDENCY_SOURCE_ROOT}/abseil-${UEMETA_ABSEIL_VERSION}"
    SYSTEM
)

# Hash2 and SmartPtr are header-only, as are the Boost libraries they depend on.
# Fetch the individual modules rather than the full Boost superproject to keep
# downloads small and all revisions locked together.
foreach(UEMETA_BOOST_LIBRARY IN ITEMS
        config
        mp11
        describe
        container_hash
        assert
        static_assert
        core
        throw_exception
        hash2
        smart_ptr)
    FetchContent_Declare(boost_${UEMETA_BOOST_LIBRARY}
        GIT_REPOSITORY "https://github.com/boostorg/${UEMETA_BOOST_LIBRARY}.git"
        GIT_TAG "${UEMETA_BOOST_GIT_TAG}"
        GIT_PROGRESS TRUE
        SOURCE_DIR "${UEMETA_DEPENDENCY_SOURCE_ROOT}/boost-${UEMETA_BOOST_LIBRARY}-${UEMETA_BOOST_VERSION}"
        SYSTEM
    )
endforeach()

function(UEMeta_FetchContent_MakeAvailable name)
    string(TOLOWER "${name}" name_lc)
    FetchContent_GetProperties(${name})

    if(NOT ${name_lc}_POPULATED)
        message(STATUS "FetchContent: preparing ${name}...")
    endif()

    file(LOCK "${UEMETA_DEPENDENCY_LOCK_ROOT}/${name_lc}.lock" GUARD FUNCTION)
    FetchContent_MakeAvailable(${name})
    FetchContent_GetProperties(${name})
    message(STATUS "FetchContent: ${name} source: ${${name_lc}_SOURCE_DIR}")
    set(${name_lc}_SOURCE_DIR "${${name_lc}_SOURCE_DIR}" PARENT_SCOPE)
    set(${name_lc}_BINARY_DIR "${${name_lc}_BINARY_DIR}" PARENT_SCOPE)
    set(${name_lc}_POPULATED "${${name_lc}_POPULATED}" PARENT_SCOPE)
endfunction()

function(UEMeta_ConfigureMSVCDependencyTargets directory runtime_library)
    get_property(_uemeta_targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)

    foreach(_uemeta_target IN LISTS _uemeta_targets)
        get_target_property(_uemeta_imported "${_uemeta_target}" IMPORTED)
        get_target_property(_uemeta_type "${_uemeta_target}" TYPE)

        if(NOT _uemeta_imported
            AND NOT _uemeta_type STREQUAL "INTERFACE_LIBRARY"
            AND NOT _uemeta_type STREQUAL "UTILITY")
            set_target_properties("${_uemeta_target}" PROPERTIES
                MSVC_RUNTIME_LIBRARY "${runtime_library}"
            )
            target_compile_options("${_uemeta_target}" PRIVATE ${ARGN})
        endif()
    endforeach()

    get_property(_uemeta_subdirectories DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)

    foreach(_uemeta_subdirectory IN LISTS _uemeta_subdirectories)
        UEMeta_ConfigureMSVCDependencyTargets("${_uemeta_subdirectory}" "${runtime_library}" ${ARGN})
    endforeach()
endfunction()

function(UEMeta_AppendTargetInterfaceIncludes target)
    get_target_property(_uemeta_aliased_target "${target}" ALIASED_TARGET)

    if(_uemeta_aliased_target)
        set(_uemeta_target "${_uemeta_aliased_target}")
    else()
        set(_uemeta_target "${target}")
    endif()

    if(TARGET "${_uemeta_target}")
        target_include_directories("${_uemeta_target}" INTERFACE ${ARGN})
    endif()
endfunction()

if(MSVC)
    # The official LLVM Windows archive is release-built with /MT and STL debug
    # iterators disabled, so every static dependency in the final binary must
    # use the same runtime even for the Debug preset.
    set(UEMETA_MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()

UEMeta_FetchContent_MakeAvailable(llvm_prebuilt)

if(WIN32)
    block(SCOPE_FOR VARIABLES)
        set(BUILD_SHARED_LIBS OFF)
        set(LIBXML2_WITH_C14N OFF)
        set(LIBXML2_WITH_CATALOG OFF)
        set(LIBXML2_WITH_DEBUG OFF)
        set(LIBXML2_WITH_DOCB OFF)
        set(LIBXML2_WITH_FTP OFF)
        set(LIBXML2_WITH_HTML OFF)
        set(LIBXML2_WITH_HTTP OFF)
        set(LIBXML2_WITH_ICONV OFF)
        set(LIBXML2_WITH_ICU OFF)
        set(LIBXML2_WITH_ISO8859X OFF)
        set(LIBXML2_WITH_LEGACY OFF)
        set(LIBXML2_WITH_LZMA OFF)
        set(LIBXML2_WITH_MEM_DEBUG OFF)
        set(LIBXML2_WITH_MODULES OFF)
        set(LIBXML2_WITH_OUTPUT ON)
        set(LIBXML2_WITH_PATTERN OFF)
        set(LIBXML2_WITH_PROGRAMS OFF)
        set(LIBXML2_WITH_PUSH OFF)
        set(LIBXML2_WITH_PYTHON OFF)
        set(LIBXML2_WITH_READER OFF)
        set(LIBXML2_WITH_REGEXPS OFF)
        set(LIBXML2_WITH_RUN_DEBUG OFF)
        set(LIBXML2_WITH_SAX1 ON)
        set(LIBXML2_WITH_SCHEMAS OFF)
        set(LIBXML2_WITH_SCHEMATRON OFF)
        set(LIBXML2_WITH_TESTS OFF)
        set(LIBXML2_WITH_THREADS ON)
        set(LIBXML2_WITH_THREAD_ALLOC OFF)
        set(LIBXML2_WITH_TREE ON)
        set(LIBXML2_WITH_VALID OFF)
        set(LIBXML2_WITH_WRITER OFF)
        set(LIBXML2_WITH_XINCLUDE OFF)
        set(LIBXML2_WITH_XPATH OFF)
        set(LIBXML2_WITH_XPTR OFF)
        set(LIBXML2_WITH_ZLIB OFF)
        UEMeta_FetchContent_MakeAvailable(llvm_libxml2)

        set(ZLIB_BUILD_TESTING OFF)
        set(ZLIB_BUILD_SHARED OFF)
        set(ZLIB_BUILD_STATIC ON)
        set(ZLIB_INSTALL OFF)
        UEMeta_FetchContent_MakeAvailable(llvm_zlib)

        set(ZSTD_BUILD_PROGRAMS OFF)
        set(ZSTD_BUILD_TESTS OFF)
        set(ZSTD_BUILD_STATIC ON)
        set(ZSTD_BUILD_SHARED OFF)
        UEMeta_FetchContent_MakeAvailable(llvm_zstd)

        if(NOT TARGET LibXml2::LibXml2 AND TARGET LibXml2)
            add_library(LibXml2::LibXml2 ALIAS LibXml2)
        endif()
        if(NOT TARGET ZLIB::ZLIB AND TARGET zlibstatic)
            add_library(ZLIB::ZLIB ALIAS zlibstatic)
        endif()
        if(NOT TARGET zstd::libzstd_static AND TARGET libzstd_static)
            add_library(zstd::libzstd_static ALIAS libzstd_static)
        endif()

        if(MSVC)
            UEMeta_ConfigureMSVCDependencyTargets(
                "${llvm_libxml2_SOURCE_DIR}"
                "${UEMETA_MSVC_RUNTIME_LIBRARY}"
                ${UEMETA_MSVC_EXTERNAL_DISABLED_WARNINGS}
            )
            UEMeta_ConfigureMSVCDependencyTargets(
                "${llvm_zlib_SOURCE_DIR}"
                "${UEMETA_MSVC_RUNTIME_LIBRARY}"
                ${UEMETA_MSVC_EXTERNAL_DISABLED_WARNINGS}
            )
            UEMeta_ConfigureMSVCDependencyTargets(
                "${llvm_zstd_SOURCE_DIR}/build/cmake"
                "${UEMETA_MSVC_RUNTIME_LIBRARY}"
                ${UEMETA_MSVC_EXTERNAL_DISABLED_WARNINGS}
            )
        endif()
    endblock()
endif()

UEMeta_FetchContent_MakeAvailable(cli11)
UEMeta_FetchContent_MakeAvailable(quill)

# Individual Boost library CMake projects use this version from Boost's
# superproject. Keep it defined while their projects are configured.
set(BOOST_SUPERPROJECT_VERSION "${UEMETA_BOOST_VERSION}")
UEMeta_FetchContent_MakeAvailable(boost_config)
UEMeta_FetchContent_MakeAvailable(boost_mp11)
UEMeta_FetchContent_MakeAvailable(boost_describe)
UEMeta_FetchContent_MakeAvailable(boost_container_hash)
UEMeta_FetchContent_MakeAvailable(boost_assert)
UEMeta_FetchContent_MakeAvailable(boost_static_assert)
UEMeta_FetchContent_MakeAvailable(boost_core)
UEMeta_FetchContent_MakeAvailable(boost_throw_exception)
UEMeta_FetchContent_MakeAvailable(boost_hash2)
UEMeta_FetchContent_MakeAvailable(boost_smart_ptr)

set(ABSL_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ABSL_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
UEMeta_FetchContent_MakeAvailable(absl)

set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_CONFORMANCE OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_LIBPROTOC ON CACHE BOOL "" FORCE)
set(protobuf_BUILD_PROTOC_BINARIES ON CACHE BOOL "" FORCE)
set(protobuf_INSTALL OFF CACHE BOOL "" FORCE)
UEMeta_FetchContent_MakeAvailable(protobuf)
include("${protobuf_SOURCE_DIR}/cmake/protobuf-generate.cmake")
UEMeta_AppendTargetInterfaceIncludes(
    protobuf::libprotobuf
    "${protobuf_SOURCE_DIR}/src"
    "${protobuf_SOURCE_DIR}/third_party/utf8_range"
)

if(MSVC)
    UEMeta_ConfigureMSVCDependencyTargets(
        "${absl_SOURCE_DIR}"
        "${UEMETA_MSVC_RUNTIME_LIBRARY}"
        ${UEMETA_MSVC_EXTERNAL_DISABLED_WARNINGS}
    )
    UEMeta_ConfigureMSVCDependencyTargets(
        "${protobuf_SOURCE_DIR}"
        "${UEMETA_MSVC_RUNTIME_LIBRARY}"
        ${UEMETA_MSVC_EXTERNAL_DISABLED_WARNINGS}
    )
endif()

set(LLVM_DIR "${llvm_prebuilt_SOURCE_DIR}/lib/cmake/llvm" CACHE PATH "" FORCE)
set(Clang_DIR "${llvm_prebuilt_SOURCE_DIR}/lib/cmake/clang" CACHE PATH "" FORCE)

if(WIN32)
    # The source targets above satisfy these dependencies. Prevent LLVM's
    # installed config from redundantly searching for already-built packages.
    set(CMAKE_DISABLE_FIND_PACKAGE_ZLIB TRUE)
    set(CMAKE_DISABLE_FIND_PACKAGE_zstd TRUE)
    set(CMAKE_DISABLE_FIND_PACKAGE_LibXml2 TRUE)
    set(CMAKE_DISABLE_FIND_PACKAGE_DIASDK TRUE)
endif()

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

if(WIN32)
    unset(CMAKE_DISABLE_FIND_PACKAGE_ZLIB)
    unset(CMAKE_DISABLE_FIND_PACKAGE_zstd)
    unset(CMAKE_DISABLE_FIND_PACKAGE_LibXml2)
    unset(CMAKE_DISABLE_FIND_PACKAGE_DIASDK)
endif()

separate_arguments(UEMETA_LLVM_DEFINITIONS UNIX_COMMAND "${LLVM_DEFINITIONS}")

# The Windows release archive either records the release builder's DIA SDK path
# or references the DIASDK::Diaguids target without ensuring that it exists.
if(MSVC AND TARGET LLVMDebugInfoPDB)
    find_library(UEMETA_DIAGUIDS_LIBRARY
        NAMES diaguids
        PATHS
            "$ENV{VSINSTALLDIR}/DIA SDK/lib/amd64"
            "C:/Program Files/Microsoft Visual Studio/18/Community/DIA SDK/lib/amd64"
            "C:/Program Files/Microsoft Visual Studio/18/Professional/DIA SDK/lib/amd64"
            "C:/Program Files/Microsoft Visual Studio/18/Enterprise/DIA SDK/lib/amd64"
            "C:/Program Files/Microsoft Visual Studio/2022/Community/DIA SDK/lib/amd64"
            "C:/Program Files/Microsoft Visual Studio/2022/Professional/DIA SDK/lib/amd64"
            "C:/Program Files/Microsoft Visual Studio/2022/Enterprise/DIA SDK/lib/amd64"
    )

    if(UEMETA_DIAGUIDS_LIBRARY)
        if(NOT TARGET DIASDK::Diaguids)
            add_library(DIASDK::Diaguids STATIC IMPORTED)
            set_target_properties(DIASDK::Diaguids PROPERTIES
                IMPORTED_LOCATION "${UEMETA_DIAGUIDS_LIBRARY}"
            )
        endif()

        get_target_property(_uemeta_pdb_links LLVMDebugInfoPDB INTERFACE_LINK_LIBRARIES)
        set(_uemeta_fixed_pdb_links)

        foreach(_uemeta_link IN LISTS _uemeta_pdb_links)
            if(_uemeta_link MATCHES ".*/DIA SDK/lib/amd64/diaguids\\.lib$")
                list(APPEND _uemeta_fixed_pdb_links "${UEMETA_DIAGUIDS_LIBRARY}")
            else()
                list(APPEND _uemeta_fixed_pdb_links "${_uemeta_link}")
            endif()
        endforeach()

        set_target_properties(LLVMDebugInfoPDB PROPERTIES
            INTERFACE_LINK_LIBRARIES "${_uemeta_fixed_pdb_links}"
        )
    endif()
endif()

set(UEMETA_PROTO_ROOT "${PROJECT_SOURCE_DIR}/../proto")
set(UEMETA_PROTO_BUILD_SCRIPT "${UEMETA_PROTO_ROOT}/build.py")
set(UEMETA_PROTO_GENERATED_DIR "${PROJECT_SOURCE_DIR}/intermediate/generated")

file(GLOB UEMETA_PROTO_INPUTS CONFIGURE_DEPENDS "${UEMETA_PROTO_ROOT}/files/*.proto")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${UEMETA_PROTO_BUILD_SCRIPT}"
    "${UEMETA_PROTO_ROOT}/buf.yaml"
    ${UEMETA_PROTO_INPUTS}
)

message(STATUS "Generating parser protobuf C++ sources")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${UEMETA_PROTO_BUILD_SCRIPT}"
        --language cpp
        --output parser/intermediate/generated
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/.."
    RESULT_VARIABLE UEMETA_PROTO_BUILD_RESULT
    OUTPUT_VARIABLE UEMETA_PROTO_BUILD_OUTPUT
    ERROR_VARIABLE UEMETA_PROTO_BUILD_ERROR
)

if(NOT UEMETA_PROTO_BUILD_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Failed to generate parser protobuf C++ sources.\n"
        "stdout:\n${UEMETA_PROTO_BUILD_OUTPUT}\n"
        "stderr:\n${UEMETA_PROTO_BUILD_ERROR}"
    )
endif()

file(GLOB UEMETA_PROTO_GENERATED_SOURCES CONFIGURE_DEPENDS
    "${UEMETA_PROTO_GENERATED_DIR}/*.cc"
)
file(GLOB UEMETA_PROTO_GENERATED_HEADERS CONFIGURE_DEPENDS
    "${UEMETA_PROTO_GENERATED_DIR}/*.h"
)

if(NOT UEMETA_PROTO_GENERATED_SOURCES OR NOT UEMETA_PROTO_GENERATED_HEADERS)
    message(FATAL_ERROR "Expected generated protobuf C++ files in ${UEMETA_PROTO_GENERATED_DIR}")
endif()

source_group("Generated\\Protobuf" FILES
    ${UEMETA_PROTO_GENERATED_SOURCES}
    ${UEMETA_PROTO_GENERATED_HEADERS}
)

add_executable(parser
        src/main.cpp src/ClangHandler.cpp
        src/Cli.cpp
        src/MakeTool.cpp
        src/StablePath.cpp
        ${UEMETA_PROTO_GENERATED_SOURCES}
        ${UEMETA_PROTO_GENERATED_HEADERS}
        src/HeartbeatLogger.cpp
        src/wrappers/EnumDeclWrapper.cpp
        src/wrappers/MessageAllocator.cpp
        src/wrappers/VarDeclWrapper.cpp
        src/wrappers/MethodDeclWrapper.cpp
        src/wrappers/RecordDeclWrapper.cpp
        src/wrappers/Types.cpp
        src/wrappers/DeclDb.cpp)

set_target_properties(parser PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_NAME parser
)

if(MSVC)
    set_target_properties(parser PROPERTIES
        MSVC_RUNTIME_LIBRARY "${UEMETA_MSVC_RUNTIME_LIBRARY}"
    )
    set_source_files_properties(${UEMETA_PROTO_GENERATED_SOURCES} PROPERTIES
        COMPILE_OPTIONS "${UEMETA_MSVC_EXTERNAL_DISABLED_WARNINGS}"
    )
endif()

target_include_directories(parser PRIVATE
    "${PROJECT_SOURCE_DIR}/include"
)

target_include_directories(parser SYSTEM PRIVATE
    "${UEMETA_PROTO_GENERATED_DIR}"
    ${LLVM_INCLUDE_DIRS}
    ${CLANG_INCLUDE_DIRS}
)

target_compile_options(parser PRIVATE
    ${UEMETA_LLVM_DEFINITIONS}
    $<$<CXX_COMPILER_ID:MSVC>:/std:c++latest>
    $<$<CXX_COMPILER_ID:MSVC>:/EHsc>
    $<$<CXX_COMPILER_ID:MSVC>:/Zc:preprocessor>
    $<$<CXX_COMPILER_ID:MSVC>:/bigobj>
    $<$<CXX_COMPILER_ID:MSVC>:${UEMETA_MSVC_DISABLED_WARNINGS}>
    $<$<CXX_COMPILER_ID:Clang>:-std=c++2c>
    $<$<CXX_COMPILER_ID:GNU>:-std=c++2c>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<CXX_COMPILER_ID:MSVC>:/external:anglebrackets>
    $<$<CXX_COMPILER_ID:MSVC>:/external:W0>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/Od>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/Zi>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/RTC1>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/O2>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/Ob2>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wextra>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wpedantic>
    $<$<AND:$<NOT:$<CXX_COMPILER_ID:MSVC>>,$<CONFIG:Debug>>:-O0>
    $<$<AND:$<NOT:$<CXX_COMPILER_ID:MSVC>>,$<CONFIG:Debug>>:-g>
    $<$<AND:$<NOT:$<CXX_COMPILER_ID:MSVC>>,$<CONFIG:Release>>:-O3>
)

target_link_options(parser PRIVATE
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/DEBUG:FULL>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/OPT:REF>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/OPT:ICF>
)

target_link_libraries(parser PRIVATE
    clangTooling
    clangFrontend
    clangASTMatchers
    CLI11::CLI11
    Boost::hash2
    Boost::smart_ptr
    absl::flat_hash_map
    quill::quill
    protobuf::libprotobuf
)

set(PARSER_BUNDLE_DIR "${PROJECT_SOURCE_DIR}/out")
set(PARSER_BUNDLE_CLANG_DIR "${PARSER_BUNDLE_DIR}/Clang")
set(PARSER_LLVM_BIN_DIR "${llvm_prebuilt_SOURCE_DIR}/bin")
target_compile_definitions(parser PRIVATE
    QUILL_DISABLE_FUNCTION_NAME
    QUILL_DISABLE_FILE_INFO
    $<$<CONFIG:Debug>:DEBUG=1>
)

set_property(DIRECTORY APPEND PROPERTY
    ADDITIONAL_CLEAN_FILES "${PARSER_BUNDLE_DIR}"
)

if(NOT IS_DIRECTORY "${PARSER_LLVM_BIN_DIR}")
    message(FATAL_ERROR "Expected LLVM/Clang binary directory at ${PARSER_LLVM_BIN_DIR}")
endif()

add_custom_command(TARGET parser POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "${PARSER_BUNDLE_DIR}"
        "${PARSER_BUNDLE_CLANG_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:parser>"
        "${PARSER_BUNDLE_DIR}/$<TARGET_FILE_NAME:parser>"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${PARSER_LLVM_BIN_DIR}"
        "${PARSER_BUNDLE_CLANG_DIR}"
    COMMENT "Bundling parser executable and LLVM/Clang binaries"
    VERBATIM
)

set(UEMETA_CLANG_CL_CONFIG_DEFINITION "$<IF:$<CONFIG:Debug>,/DDEBUG,/DNDEBUG>")
set(UEMETA_CLANG_CL_CONFIG_OPTIMIZATION "$<IF:$<CONFIG:Debug>,/Od,/O2>")
set(UEMETA_CLANG_CL_CONFIG_INLINE_EXPANSION "$<IF:$<CONFIG:Debug>,/Ob0,/Ob2>")

if(UEMETA_ENABLE_TESTING)
    enable_testing()
    include(GoogleTest)

    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(gtest_force_shared_crt OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG "${UEMETA_GOOGLETEST_GIT_TAG}"
        GIT_PROGRESS TRUE
        SOURCE_DIR "${UEMETA_DEPENDENCY_SOURCE_ROOT}/googletest-${UEMETA_GOOGLETEST_VERSION}"
        SYSTEM
    )

    UEMeta_FetchContent_MakeAvailable(googletest)

    if(MSVC)
        UEMeta_ConfigureMSVCDependencyTargets(
            "${googletest_SOURCE_DIR}"
            "${UEMETA_MSVC_RUNTIME_LIBRARY}"
            ${UEMETA_MSVC_EXTERNAL_DISABLED_WARNINGS}
        )
    endif()

    set(UEMETA_TEST_TARGET_DIR "${PROJECT_SOURCE_DIR}/testing/target")
    set(UEMETA_TEST_TARGET_INCLUDE_DIR "${UEMETA_TEST_TARGET_DIR}/include")
    set(UEMETA_TEST_TARGET_SOURCE_DIR "${UEMETA_TEST_TARGET_DIR}/src")
    set(UEMETA_TEST_HARNESS_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/testing/harness/include")
    set(UEMETA_TEST_HARNESS_SOURCE_DIR "${PROJECT_SOURCE_DIR}/testing/harness/src")
    set(UEMETA_TEST_OUTPUT_DIR "${PROJECT_SOURCE_DIR}/testing/output")
    set(UEMETA_TEST_TARGET_SOURCE "${UEMETA_TEST_TARGET_SOURCE_DIR}/test.cpp")
    set(UEMETA_TEST_PARSER_LAUNCHER "${CMAKE_CURRENT_BINARY_DIR}/RunParserForTests.cmake")
    set(UEMETA_TEST_PARSER_OUTPUT_MARKER "${CMAKE_CURRENT_BINARY_DIR}/uemeta-test-parser-output.ready")

    file(GLOB UEMETA_TEST_TARGET_HEADERS CONFIGURE_DEPENDS
        "${UEMETA_TEST_TARGET_INCLUDE_DIR}/*.h"
        "${UEMETA_TEST_TARGET_INCLUDE_DIR}/*.hpp"
        "${UEMETA_TEST_TARGET_INCLUDE_DIR}/*.cpp"
    )
    file(GLOB UEMETA_TEST_HARNESS_SOURCES CONFIGURE_DEPENDS
        "${UEMETA_TEST_HARNESS_SOURCE_DIR}/*.cpp"
    )
    file(GLOB UEMETA_TEST_HARNESS_HEADERS CONFIGURE_DEPENDS
        "${UEMETA_TEST_HARNESS_INCLUDE_DIR}/*.h"
        "${UEMETA_TEST_HARNESS_INCLUDE_DIR}/*.hpp"
    )

    if(NOT EXISTS "${UEMETA_TEST_TARGET_SOURCE}")
        message(FATAL_ERROR "Expected parser test target source at ${UEMETA_TEST_TARGET_SOURCE}")
    endif()

    if(NOT UEMETA_TEST_TARGET_HEADERS)
        message(FATAL_ERROR "Expected parser test target headers in ${UEMETA_TEST_TARGET_INCLUDE_DIR}")
    endif()

    if(NOT UEMETA_TEST_HARNESS_SOURCES)
        message(FATAL_ERROR "Expected GoogleTest harness sources in ${UEMETA_TEST_HARNESS_SOURCE_DIR}")
    endif()

    file(TO_CMAKE_PATH "${PROJECT_SOURCE_DIR}" UEMETA_TEST_WORKING_DIRECTORY_JSON)
    file(TO_CMAKE_PATH "${UEMETA_TEST_TARGET_SOURCE}" UEMETA_TEST_TARGET_SOURCE_JSON)
    file(TO_CMAKE_PATH "${UEMETA_TEST_TARGET_DIR}" UEMETA_TEST_TARGET_DIR_JSON)
    file(TO_CMAKE_PATH "${UEMETA_TEST_TARGET_INCLUDE_DIR}" UEMETA_TEST_TARGET_INCLUDE_DIR_JSON)
    file(TO_CMAKE_PATH "${UEMETA_TEST_OUTPUT_DIR}" UEMETA_TEST_OUTPUT_DIR_JSON)
    file(TO_CMAKE_PATH "${UEMETA_TEST_PARSER_OUTPUT_MARKER}" UEMETA_TEST_PARSER_OUTPUT_MARKER_JSON)

    set(UEMETA_TEST_CLANG_EXECUTABLE "${PARSER_BUNDLE_CLANG_DIR}/clang-cl${CMAKE_EXECUTABLE_SUFFIX}")
    set(UEMETA_TEST_COMPILE_COMMAND
        "clang-cl${CMAKE_EXECUTABLE_SUFFIX} /nologo /TP /std:c++latest /EHsc /Zc:preprocessor /bigobj /DWIN32 /D_WINDOWS ${UEMETA_CLANG_CL_CONFIG_DEFINITION} /MT ${UEMETA_CLANG_CL_CONFIG_OPTIMIZATION} ${UEMETA_CLANG_CL_CONFIG_INLINE_EXPANSION} /I\"${UEMETA_TEST_TARGET_INCLUDE_DIR_JSON}\" /c \"${UEMETA_TEST_TARGET_SOURCE_JSON}\""
    )
    file(TO_CMAKE_PATH "${UEMETA_TEST_CLANG_EXECUTABLE}" UEMETA_TEST_CLANG_EXECUTABLE_JSON)
    string(REPLACE "\"" "\\\"" UEMETA_TEST_COMPILE_COMMAND_JSON "${UEMETA_TEST_COMPILE_COMMAND}")
    string(CONCAT UEMETA_TEST_COMPILE_COMMANDS_JSON
        "[\n"
        "  {\n"
        "    \"directory\": \"${UEMETA_TEST_WORKING_DIRECTORY_JSON}\",\n"
        "    \"command\": \"${UEMETA_TEST_COMPILE_COMMAND_JSON}\",\n"
        "    \"file\": \"${UEMETA_TEST_TARGET_SOURCE_JSON}\"\n"
        "  }\n"
        "]\n"
    )

    string(CONCAT UEMETA_TEST_PARSER_LAUNCHER_CONTENT
        "file(MAKE_DIRECTORY [===[${UEMETA_TEST_OUTPUT_DIR_JSON}]===])\n"
        "file(GLOB UEMETA_TEST_STALE_OUTPUT_FILES\n"
        "    [===[${UEMETA_TEST_OUTPUT_DIR_JSON}/*bin]===]\n"
        "    [===[${UEMETA_TEST_OUTPUT_DIR_JSON}/*json]===]\n"
        ")\n"
        "if(UEMETA_TEST_STALE_OUTPUT_FILES)\n"
        "    file(REMOVE \${UEMETA_TEST_STALE_OUTPUT_FILES})\n"
        "endif()\n"
        "file(REMOVE [===[${UEMETA_TEST_PARSER_OUTPUT_MARKER_JSON}]===])\n"
        "execute_process(\n"
        "    COMMAND \"$<TARGET_FILE:parser>\"\n"
        "        --compile-commands [===[${UEMETA_TEST_COMPILE_COMMANDS_JSON}]===]\n"
        "        --clang-path [===[${UEMETA_TEST_CLANG_EXECUTABLE_JSON}]===]\n"
        "        --output [===[${UEMETA_TEST_OUTPUT_DIR_JSON}]===]\n"
        "        --format binary\n"
        "        --builtin-subpaths InlinedBuiltinTypes.hpp\n"
        "        --process-implicit-specializations\n"
        "    RESULT_VARIABLE UEMETA_TEST_PARSER_RESULT\n"
        ")\n"
        "if(NOT UEMETA_TEST_PARSER_RESULT EQUAL 0)\n"
        "    message(FATAL_ERROR \"parser test target run failed with exit code \${UEMETA_TEST_PARSER_RESULT}\")\n"
        "endif()\n"
        "file(GLOB UEMETA_TEST_PARSER_OUTPUT_FILES [===[${UEMETA_TEST_OUTPUT_DIR_JSON}/*bin]===])\n"
        "if(NOT UEMETA_TEST_PARSER_OUTPUT_FILES)\n"
        "    message(FATAL_ERROR \"parser test target produced no binary output in ${UEMETA_TEST_OUTPUT_DIR_JSON}\")\n"
        "endif()\n"
        "file(WRITE [===[${UEMETA_TEST_PARSER_OUTPUT_MARKER_JSON}]===] \"ready\\n\")\n"
    )
    file(GENERATE OUTPUT "${UEMETA_TEST_PARSER_LAUNCHER}" CONTENT "${UEMETA_TEST_PARSER_LAUNCHER_CONTENT}")

    add_custom_target(uemeta-test-output
        COMMAND "${CMAKE_COMMAND}" -P "${UEMETA_TEST_PARSER_LAUNCHER}"
        DEPENDS
            parser
            "${UEMETA_TEST_PARSER_LAUNCHER}"
            "${UEMETA_TEST_TARGET_SOURCE}"
            ${UEMETA_TEST_TARGET_HEADERS}
        COMMENT "Preparing parser test output"
        VERBATIM
    )

    add_executable(uemeta-parser-tests
        ${UEMETA_TEST_HARNESS_SOURCES}
        ${UEMETA_TEST_HARNESS_HEADERS}
        ${UEMETA_PROTO_GENERATED_SOURCES}
    )
    add_dependencies(uemeta-parser-tests uemeta-test-output)

    set_target_properties(uemeta-parser-tests PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )

    if(MSVC)
        set_target_properties(uemeta-parser-tests PROPERTIES
            MSVC_RUNTIME_LIBRARY "${UEMETA_MSVC_RUNTIME_LIBRARY}"
        )
    endif()

    target_compile_definitions(uemeta-parser-tests PRIVATE
        UEMETA_TEST_OUTPUT_DIR="${UEMETA_TEST_OUTPUT_DIR_JSON}"
        UEMETA_TEST_TARGET_DIR="${UEMETA_TEST_TARGET_DIR_JSON}"
        UEMETA_TEST_TARGET_INCLUDE_DIR="${UEMETA_TEST_TARGET_INCLUDE_DIR_JSON}"
        UEMETA_TEST_TARGET_SOURCE="${UEMETA_TEST_TARGET_SOURCE_JSON}"
        UEMETA_TEST_PARSER_OUTPUT_MARKER="${UEMETA_TEST_PARSER_OUTPUT_MARKER_JSON}"
    )

    target_include_directories(uemeta-parser-tests SYSTEM PRIVATE
        "${UEMETA_PROTO_GENERATED_DIR}"
    )

    target_include_directories(uemeta-parser-tests PRIVATE
        "${UEMETA_TEST_HARNESS_INCLUDE_DIR}"
    )

    target_compile_options(uemeta-parser-tests PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/EHsc>
        $<$<CXX_COMPILER_ID:MSVC>:/bigobj>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
        $<$<CXX_COMPILER_ID:MSVC>:/external:anglebrackets>
        $<$<CXX_COMPILER_ID:MSVC>:/external:W0>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wextra>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wpedantic>
    )

    target_link_libraries(uemeta-parser-tests PRIVATE
        GTest::gtest_main
        protobuf::libprotobuf
    )

    add_test(
        NAME uemeta-test-output-ctest
        COMMAND "${CMAKE_COMMAND}" -P "${UEMETA_TEST_PARSER_LAUNCHER}"
    )
    set_tests_properties(uemeta-test-output-ctest PROPERTIES
        FIXTURES_SETUP uemeta-parser-output
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )

    gtest_discover_tests(uemeta-parser-tests
        DISCOVERY_MODE PRE_TEST
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        PROPERTIES
            FIXTURES_REQUIRED uemeta-parser-output
    )
endif()

set(UEMETA_PLAYGROUND_SOURCE "${PROJECT_SOURCE_DIR}/playground/pg.cpp")
set(UEMETA_PLAYGROUND_COMPILE_COMMANDS "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/playground_compile_commands.json")
set(UEMETA_PLAYGROUND_LAUNCHER "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/RunPlayground.cmake")

file(TO_CMAKE_PATH "${PROJECT_SOURCE_DIR}" UEMETA_PLAYGROUND_WORKING_DIRECTORY_JSON)
file(TO_CMAKE_PATH "${UEMETA_PLAYGROUND_SOURCE}" UEMETA_PLAYGROUND_SOURCE_JSON)
file(TO_CMAKE_PATH "${UEMETA_PLAYGROUND_COMPILE_COMMANDS}" UEMETA_PLAYGROUND_COMPILE_COMMANDS_JSON)

string(CONCAT UEMETA_PLAYGROUND_COMPILE_COMMANDS_CONTENT
    "[\n"
    "  {\n"
    "    \"directory\": \"${UEMETA_PLAYGROUND_WORKING_DIRECTORY_JSON}\",\n"
    "    \"arguments\": [\n"
    "      \"clang-cl${CMAKE_EXECUTABLE_SUFFIX}\",\n"
    "      \"/nologo\", \"/TP\", \"/std:c++latest\", \"/EHsc\", \"/Zc:preprocessor\", \"/bigobj\",\n"
    "      \"/DWIN32\", \"/D_WINDOWS\", \"${UEMETA_CLANG_CL_CONFIG_DEFINITION}\", \"/MT\",\n"
    "      \"${UEMETA_CLANG_CL_CONFIG_OPTIMIZATION}\", \"${UEMETA_CLANG_CL_CONFIG_INLINE_EXPANSION}\", \"/c\",\n"
    "      \"${UEMETA_PLAYGROUND_SOURCE_JSON}\"\n"
    "    ],\n"
    "    \"file\": \"${UEMETA_PLAYGROUND_SOURCE_JSON}\"\n"
    "  }\n"
    "]\n"
)
file(GENERATE
    OUTPUT "${UEMETA_PLAYGROUND_COMPILE_COMMANDS}"
    CONTENT "${UEMETA_PLAYGROUND_COMPILE_COMMANDS_CONTENT}"
)
string(CONCAT UEMETA_PLAYGROUND_LAUNCHER_CONTENT
    "execute_process(\n"
    "    COMMAND \"$<TARGET_FILE:parser>\"\n"
    "        --compile-commands [===[${UEMETA_PLAYGROUND_COMPILE_COMMANDS_JSON}]===]\n"
    "        --clang-path [===[${PARSER_BUNDLE_CLANG_DIR}/clang-cl${CMAKE_EXECUTABLE_SUFFIX}]===]\n"
    "        --dump\n"
    "    RESULT_VARIABLE UEMETA_PLAYGROUND_RESULT\n"
    ")\n"
    "if(NOT UEMETA_PLAYGROUND_RESULT EQUAL 0)\n"
    "    message(FATAL_ERROR \"playground parser run failed with exit code \${UEMETA_PLAYGROUND_RESULT}\")\n"
    "endif()\n"
)
file(GENERATE OUTPUT "${UEMETA_PLAYGROUND_LAUNCHER}" CONTENT "${UEMETA_PLAYGROUND_LAUNCHER_CONTENT}")

add_custom_target(playground
    COMMAND "${CMAKE_COMMAND}" -P "${UEMETA_PLAYGROUND_LAUNCHER}"
    DEPENDS parser "${UEMETA_PLAYGROUND_SOURCE}"
    COMMENT "Running parser over playground/pg.cpp"
    USES_TERMINAL
    VERBATIM
)
