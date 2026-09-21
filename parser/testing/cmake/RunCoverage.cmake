include("${COVERAGE_SOURCES_FILE}")
list(LENGTH sources expected_file_count)
if(expected_file_count EQUAL 0)
    message(FATAL_ERROR "No sources registered for coverage")
endif()

if(NOT DEFINED TEST_FILTER)
    set(TEST_FILTER "*")
endif()

file(MAKE_DIRECTORY "${COVERAGE_DIR}")
set(raw_profile "${COVERAGE_DIR}/tests.profraw")
set(profile "${COVERAGE_DIR}/tests.profdata")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "LLVM_PROFILE_FILE=${raw_profile}"
        "${TEST_EXECUTABLE}" "--gtest_filter=${TEST_FILTER}"
    COMMAND_ERROR_IS_FATAL ANY
)
execute_process(
    COMMAND "${LLVM_PROFDATA}" merge -sparse "${raw_profile}" -o "${profile}"
    COMMAND_ERROR_IS_FATAL ANY
)
execute_process(
    COMMAND "${LLVM_COV}" report "${TEST_EXECUTABLE}" "-instr-profile=${profile}" ${sources}
        -show-branch-summary -show-region-summary
    COMMAND_ERROR_IS_FATAL ANY
)
execute_process(
    COMMAND "${LLVM_COV}" show "${TEST_EXECUTABLE}" "-instr-profile=${profile}" ${sources}
        -format=html "-output-dir=${COVERAGE_DIR}/html" -show-branches=count
    COMMAND_ERROR_IS_FATAL ANY
)
execute_process(
    COMMAND "${LLVM_COV}" export "${TEST_EXECUTABLE}" "-instr-profile=${profile}" ${sources} -summary-only
    OUTPUT_VARIABLE report
    COMMAND_ERROR_IS_FATAL ANY
)
file(WRITE "${COVERAGE_DIR}/summary.json" "${report}")
string(JSON file_count LENGTH "${report}" data 0 files)
if(NOT file_count EQUAL expected_file_count)
    message(FATAL_ERROR "Expected coverage for ${expected_file_count} registered source files, got ${file_count}")
endif()
if(DEFINED REQUIRE_FULL_COVERAGE AND NOT REQUIRE_FULL_COVERAGE)
    message(STATUS "Coverage report: ${COVERAGE_DIR}/html/index.html")
    return()
endif()
foreach(metric IN ITEMS lines regions branches functions)
    string(JSON total GET "${report}" data 0 totals ${metric} count)
    string(JSON covered GET "${report}" data 0 totals ${metric} covered)
    if(NOT covered EQUAL total OR total EQUAL 0)
        message(FATAL_ERROR "Wrapper ${metric}: ${covered}/${total}; expected 100%. See ${COVERAGE_DIR}/html/index.html")
    endif()
endforeach()
message(STATUS "Registered wrappers: 100% lines, regions, branches and functions. Report: ${COVERAGE_DIR}/html/index.html")
