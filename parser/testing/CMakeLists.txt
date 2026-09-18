add_executable(uemeta-parser-tests harness/src/TestMain.cpp)
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
target_include_directories(uemeta-parser-tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/harness/include")
target_link_libraries(uemeta-parser-tests PRIVATE uemeta-parser-core GTest::gtest)

# Register each wrapper's tests and measured production files together.
# Wrappers may be header-only, or share a header registered by another wrapper.
function(uemeta_add_wrapper_tests wrapper)
    target_sources(uemeta-parser-tests PRIVATE "harness/src/wrappers/${wrapper}Tests.cpp")
    set(sources)
    if(EXISTS "../include/UEMeta/clang/wrappers/${wrapper}.hpp")
        list(APPEND sources "../include/UEMeta/clang/wrappers/${wrapper}.hpp")
    endif()
    if(EXISTS "../src/clang/wrappers/${wrapper}.cpp")
        list(APPEND sources "../src/clang/wrappers/${wrapper}.cpp")
    endif()
    set(UEMETA_COVERAGE_SOURCES ${UEMETA_COVERAGE_SOURCES} ${sources} PARENT_SCOPE)
endfunction()

uemeta_add_wrapper_tests(EnumDeclWrapper)
uemeta_add_wrapper_tests(VarDeclWrapper)
uemeta_add_wrapper_tests(FunctionDeclWrapper)
# MethodDeclWrapper is declared in FunctionDeclWrapper.hpp, already measured above.
uemeta_add_wrapper_tests(MethodDeclWrapper)
uemeta_add_wrapper_tests(RecordDeclWrapper)

gtest_discover_tests(uemeta-parser-tests
    DISCOVERY_MODE PRE_TEST
    WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
)

if(UEMETA_ENABLE_COVERAGE)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Coverage.cmake")
endif()
