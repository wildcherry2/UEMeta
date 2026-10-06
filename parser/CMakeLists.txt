cmake_minimum_required(VERSION 3.25)
project(parser LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(UEMETA_ENABLE_TESTING "Build wrapper unit tests" OFF)
option(UEMETA_ENABLE_COVERAGE "Measure wrapper coverage with Clang" OFF)
if(UEMETA_ENABLE_COVERAGE AND NOT UEMETA_ENABLE_TESTING)
    message(FATAL_ERROR "UEMETA_ENABLE_COVERAGE requires UEMETA_ENABLE_TESTING")
endif()
include(cmake/Dependencies.cmake)

add_library(uemeta-parser-core STATIC
    src/Cli.cpp
    src/clang/MetaASTConsumer.cpp
    src/clang/MetaFrontendAction.cpp
    src/clang/MetaTool.cpp
    src/clang/MetaPreprocessor.cpp
    src/clang/DeclDb.cpp
    src/clang/ReflectionDb.cpp
    src/clang/wrappers/DeclWrapper.cpp
    src/clang/wrappers/EnumDeclWrapper.cpp
    src/clang/wrappers/VarDeclWrapper.cpp
    src/clang/wrappers/MethodDeclWrapper.cpp
    src/clang/wrappers/RecordDeclWrapper.cpp
    src/utility/StablePath.cpp
    src/utility/HeartbeatLogger.cpp
    src/utility/DeclUtility.cpp)

include(cmake/Protobuf.cmake)

target_include_directories(uemeta-parser-core PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_include_directories(uemeta-parser-core SYSTEM PUBLIC
    "${UEMETA_PROTO_GENERATED_DIR}" ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
target_compile_options(uemeta-parser-core PUBLIC ${UEMETA_LLVM_DEFINITIONS})
target_compile_definitions(uemeta-parser-core PUBLIC
    QUILL_DISABLE_FUNCTION_NAME QUILL_DISABLE_FILE_INFO
    $<$<CONFIG:Debug>:DEBUG=1>
    $<$<BOOL:${UEMETA_ENABLE_TESTING}>:UEM_TESTING=1>)

if(MSVC)
    target_compile_options(uemeta-parser-core PUBLIC /EHsc /bigobj /W4 /external:W0
        /wd4100 /wd4244 /wd4189 /wd4702
        $<$<CXX_COMPILER_ID:MSVC>:/std:c++latest;/Zc:preprocessor;/external:anglebrackets>)
else()
    target_compile_options(uemeta-parser-core PUBLIC -Wall -Wextra -Wpedantic)
    # Subclasses of LLVM types must match the binary distribution's RTTI setting.
    if(NOT LLVM_ENABLE_RTTI)
        target_compile_options(uemeta-parser-core PUBLIC -fno-rtti)
    endif()
endif()

target_link_libraries(uemeta-parser-core PUBLIC
    clangTooling clangFrontend clangASTMatchers
    CLI11::CLI11 BS_thread_pool Boost::hash2 absl::flat_hash_map
    quill::quill protobuf::libprotobuf)

add_executable(parser src/main.cpp src/Repl.cpp)
target_link_libraries(parser PRIVATE uemeta-parser-core)
include(cmake/Bundle.cmake)

if(UEMETA_ENABLE_TESTING)
    enable_testing()
    include(GoogleTest)
    add_subdirectory(testing)
endif()
