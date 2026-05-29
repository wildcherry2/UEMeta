cmake_minimum_required(VERSION 3.25)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

project(UEMeta LANGUAGES CXX)

include(FetchContent)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(UEMETA_LLVM_VERSION "22.1.6" CACHE STRING "LLVM release fetched by CMake")
set(UEMETA_LLVM_PACKAGE "clang+llvm-${UEMETA_LLVM_VERSION}-x86_64-pc-windows-msvc"
    CACHE STRING "LLVM binary release package"
)
set(UEMETA_LLVM_PREBUILT_URL
    "https://github.com/llvm/llvm-project/releases/download/llvmorg-${UEMETA_LLVM_VERSION}/${UEMETA_LLVM_PACKAGE}.tar.xz"
    CACHE STRING "LLVM binary release archive"
)
set(UEMETA_LLVM_PREBUILT_URL_HASH
    "SHA256=657343edf361ca463bd642e39c74b251c6338b96cdbd55ff277555298b027696"
    CACHE STRING "LLVM binary release archive hash"
)
set(UEMETA_SIMDJSON_VERSION "4.6.4" CACHE STRING "simdjson release fetched by CMake")
set(UEMETA_SIMDJSON_SINGLEHEADER_URL
    "https://github.com/simdjson/simdjson/releases/download/v${UEMETA_SIMDJSON_VERSION}/singleheader.zip"
    CACHE STRING "simdjson single-header release archive"
)
set(UEMETA_SIMDJSON_SINGLEHEADER_URL_HASH
    "SHA256=ccd85a1f1f9e43485c5661742b52bebf4806d1c7aa617cdb658a56dfa356e5a7"
    CACHE STRING "simdjson single-header release archive hash"
)
set(UEMETA_CLI11_VERSION "2.6.2" CACHE STRING "CLI11 release fetched by CMake")
set(UEMETA_CLI11_GIT_TAG
    "37bb6edc5317e99af72ef48405e65d9ca5218861"
    CACHE STRING "CLI11 Git commit fetched by CMake"
)

FetchContent_Declare(llvm_prebuilt
    URL "${UEMETA_LLVM_PREBUILT_URL}"
    URL_HASH "${UEMETA_LLVM_PREBUILT_URL_HASH}"
    SOURCE_SUBDIR cmake-do-not-add
)

FetchContent_Declare(simdjson_singleheader
    URL "${UEMETA_SIMDJSON_SINGLEHEADER_URL}"
    URL_HASH "${UEMETA_SIMDJSON_SINGLEHEADER_URL_HASH}"
    SOURCE_SUBDIR cmake-do-not-add
)

FetchContent_Declare(cli11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG "${UEMETA_CLI11_GIT_TAG}"
)

FetchContent_MakeAvailable(llvm_prebuilt)
FetchContent_MakeAvailable(simdjson_singleheader)
FetchContent_MakeAvailable(cli11)

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

add_executable(uemeta src/main.cpp src/ClangHandler.cpp
        src/Cli.cpp
        src/MakeTool.cpp)

add_library(simdjson STATIC "${simdjson_singleheader_SOURCE_DIR}/simdjson.cpp")
target_include_directories(simdjson PUBLIC "${simdjson_singleheader_SOURCE_DIR}")
target_compile_options(simdjson PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/EHsc>)
target_compile_features(simdjson PUBLIC cxx_std_17)

set_target_properties(uemeta PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_include_directories(uemeta PRIVATE "${PROJECT_SOURCE_DIR}/include")

target_include_directories(uemeta SYSTEM PRIVATE
    ${LLVM_INCLUDE_DIRS}
    ${CLANG_INCLUDE_DIRS}
)

target_compile_options(uemeta PRIVATE
    ${UEMETA_LLVM_DEFINITIONS}
    $<$<CXX_COMPILER_ID:MSVC>:/std:c++latest>
    $<$<CXX_COMPILER_ID:MSVC>:/EHsc>
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

target_link_options(uemeta PRIVATE
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/DEBUG:FULL>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/OPT:REF>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/OPT:ICF>
)

target_link_libraries(uemeta PRIVATE
    clangTooling
    clangFrontend
    clangASTMatchers
    simdjson
    CLI11::CLI11
)
