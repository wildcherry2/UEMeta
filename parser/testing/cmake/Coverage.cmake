if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "UEMETA_ENABLE_COVERAGE requires the testing preset to use clang++ or clang-cl")
endif()

get_filename_component(_coverage_compiler_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
find_program(UEMETA_LLVM_COV NAMES llvm-cov HINTS "${_coverage_compiler_dir}" "${PARSER_LLVM_BIN_DIR}" REQUIRED)
find_program(UEMETA_LLVM_PROFDATA NAMES llvm-profdata HINTS "${_coverage_compiler_dir}" "${PARSER_LLVM_BIN_DIR}" REQUIRED)

# Instrument production code, keeping third-party libraries uninstrumented.
if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_options(uemeta-parser-core PUBLIC /Od /clang:-fprofile-instr-generate /clang:-fcoverage-mapping)
    # clang-cl embeds the profile runtime's default-library directive in its objects.
else()
    target_compile_options(uemeta-parser-core PUBLIC -O0 -fprofile-instr-generate -fcoverage-mapping)
    target_link_options(uemeta-parser-core INTERFACE -fprofile-instr-generate)
endif()

# The wrapper registrations in testing/CMakeLists.txt define the coverage scope.
set(_coverage_sources_file "${CMAKE_CURRENT_BINARY_DIR}/CoverageSources.cmake")
file(WRITE "${_coverage_sources_file}" "set(sources\n")
foreach(source IN LISTS UEMETA_COVERAGE_SOURCES)
    file(APPEND "${_coverage_sources_file}" "    [==[${source}]==]\n")
endforeach()
file(APPEND "${_coverage_sources_file}" ")\n")

add_custom_target(uemeta-test-coverage
    COMMAND "${CMAKE_COMMAND}"
        "-DTEST_EXECUTABLE=$<TARGET_FILE:uemeta-parser-tests>"
        "-DLLVM_COV=${UEMETA_LLVM_COV}"
        "-DLLVM_PROFDATA=${UEMETA_LLVM_PROFDATA}"
        "-DCOVERAGE_SOURCES_FILE=${_coverage_sources_file}"
        "-DCOVERAGE_DIR=${PROJECT_BINARY_DIR}/coverage"
        -P "${CMAKE_CURRENT_LIST_DIR}/RunCoverage.cmake"
    DEPENDS uemeta-parser-tests
    COMMENT "Running all tests and enforcing 100% coverage for registered wrappers"
    USES_TERMINAL
    VERBATIM
)

# Reflection tests exercise only synthetic inputs and the detection database.
# Report their coverage separately: defensive failures and platform-specific
# filesystem paths are not all reachable in a single host's valid ASTs.
set(_reflection_sources_file "${CMAKE_CURRENT_BINARY_DIR}/ReflectionCoverageSources.cmake")
file(WRITE "${_reflection_sources_file}"
    "set(sources\n"
    "    [==[${PROJECT_SOURCE_DIR}/src/clang/ReflectionDb.cpp]==]\n"
    "    [==[${PROJECT_SOURCE_DIR}/include/UEMeta/clang/ReflectionDb.hpp]==]\n"
    ")\n"
)
add_custom_target(uemeta-reflection-coverage
    COMMAND "${CMAKE_COMMAND}"
        "-DTEST_EXECUTABLE=$<TARGET_FILE:uemeta-parser-tests>"
        "-DTEST_FILTER=*Reflection*"
        "-DREQUIRE_FULL_COVERAGE=OFF"
        "-DLLVM_COV=${UEMETA_LLVM_COV}"
        "-DLLVM_PROFDATA=${UEMETA_LLVM_PROFDATA}"
        "-DCOVERAGE_SOURCES_FILE=${_reflection_sources_file}"
        "-DCOVERAGE_DIR=${PROJECT_BINARY_DIR}/reflection-coverage"
        -P "${CMAKE_CURRENT_LIST_DIR}/RunCoverage.cmake"
    DEPENDS uemeta-parser-tests
    COMMENT "Running synthetic reflection detection tests and reporting coverage"
    USES_TERMINAL
    VERBATIM
)
