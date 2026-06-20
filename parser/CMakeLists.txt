cmake_minimum_required(VERSION 3.25)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

project(parser LANGUAGES CXX)

include(FetchContent)
find_package(Python3 3.14 REQUIRED COMPONENTS Interpreter)

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

set(UEMETA_LLVM_VERSION "22.1.6" CACHE STRING "LLVM release fetched by CMake")

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(UEMETA_LLVM_DEFAULT_PACKAGE "LLVM-${UEMETA_LLVM_VERSION}-Linux-X64")
    set(UEMETA_LLVM_DEFAULT_PREBUILT_URL_HASH
        "SHA256=c5ac8ef89ca39d30cb32e9b83772f995dd891c685ebc188d593c943a64d5f8b5"
    )
elseif(WIN32)
    set(UEMETA_LLVM_DEFAULT_PACKAGE "clang+llvm-${UEMETA_LLVM_VERSION}-x86_64-pc-windows-msvc")
    set(UEMETA_LLVM_DEFAULT_PREBUILT_URL_HASH
        "SHA256=657343edf361ca463bd642e39c74b251c6338b96cdbd55ff277555298b027696"
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
set(UEMETA_MSVC_DISABLED_WARNINGS
    /wd4100 # unreferenced formal parameter
    /wd4244 # conversion, possible loss of data
    /wd4189 # local variable initialized but not referenced
    /wd4702 # unreachable code
)

FetchContent_Declare(llvm_prebuilt
    URL "${UEMETA_LLVM_PREBUILT_URL}"
    URL_HASH "${UEMETA_LLVM_PREBUILT_URL_HASH}"
    DOWNLOAD_DIR "${UEMETA_DEPENDENCY_DOWNLOAD_ROOT}/llvm_prebuilt"
    DOWNLOAD_NO_PROGRESS FALSE
    SOURCE_DIR "${UEMETA_DEPENDENCY_SOURCE_ROOT}/${UEMETA_LLVM_PACKAGE}"
    SOURCE_SUBDIR cmake-do-not-add
)

FetchContent_Declare(cli11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG "${UEMETA_CLI11_GIT_TAG}"
    GIT_PROGRESS TRUE
    SOURCE_DIR "${UEMETA_DEPENDENCY_SOURCE_ROOT}/cli11-${UEMETA_CLI11_VERSION}"
)

FetchContent_Declare(quill
    GIT_REPOSITORY https://github.com/odygrd/quill.git
    GIT_TAG "${UEMETA_QUILL_GIT_TAG}"
    GIT_PROGRESS TRUE
    SOURCE_DIR "${UEMETA_DEPENDENCY_SOURCE_ROOT}/quill-${UEMETA_QUILL_VERSION}"
)

FetchContent_Declare(protobuf
    GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
    GIT_TAG "${UEMETA_PROTOBUF_GIT_TAG}"
    GIT_PROGRESS TRUE
    SOURCE_DIR "${UEMETA_DEPENDENCY_SOURCE_ROOT}/protobuf-${UEMETA_PROTOBUF_VERSION}"
)

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

UEMeta_FetchContent_MakeAvailable(llvm_prebuilt)
UEMeta_FetchContent_MakeAvailable(cli11)
UEMeta_FetchContent_MakeAvailable(quill)

set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_CONFORMANCE OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_LIBPROTOC OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_PROTOC_BINARIES OFF CACHE BOOL "" FORCE)
set(protobuf_INSTALL OFF CACHE BOOL "" FORCE)
UEMeta_FetchContent_MakeAvailable(protobuf)

set(LLVM_DIR "${llvm_prebuilt_SOURCE_DIR}/lib/cmake/llvm" CACHE PATH "" FORCE)
set(Clang_DIR "${llvm_prebuilt_SOURCE_DIR}/lib/cmake/clang" CACHE PATH "" FORCE)

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

separate_arguments(UEMETA_LLVM_DEFINITIONS UNIX_COMMAND "${LLVM_DEFINITIONS}")

# The Windows release archive records the release builder's DIA SDK path.
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
        --modules parser
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
        ${UEMETA_PROTO_GENERATED_HEADERS})

set_target_properties(parser PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_NAME parser
)

target_include_directories(parser PRIVATE
    "${PROJECT_SOURCE_DIR}/include"
    "${UEMETA_PROTO_GENERATED_DIR}"
)

target_include_directories(parser SYSTEM PRIVATE
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
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/Od>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/Zi>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/RTC1>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/O2>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/Ob2>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/DNDEBUG>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wextra>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wpedantic>
    $<$<AND:$<NOT:$<CXX_COMPILER_ID:MSVC>>,$<CONFIG:Debug>>:-O0>
    $<$<AND:$<NOT:$<CXX_COMPILER_ID:MSVC>>,$<CONFIG:Debug>>:-g>
    $<$<AND:$<NOT:$<CXX_COMPILER_ID:MSVC>>,$<CONFIG:Release>>:-O3>
    $<$<AND:$<NOT:$<CXX_COMPILER_ID:MSVC>>,$<CONFIG:Release>>:-DNDEBUG>
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
    quill::quill
    protobuf::libprotobuf
)

set(PARSER_BUNDLE_DIR "${PROJECT_SOURCE_DIR}/out")
set(PARSER_BUNDLE_CLANG_DIR "${PARSER_BUNDLE_DIR}/Clang")
set(PARSER_LLVM_BIN_DIR "${llvm_prebuilt_SOURCE_DIR}/bin")
target_compile_definitions(parser PRIVATE
    QUILL_DISABLE_FUNCTION_NAME
    QUILL_DISABLE_FILE_INFO
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
