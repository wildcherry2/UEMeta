find_package(Python3 3.14 REQUIRED COMPONENTS Interpreter)
set(UEMETA_PROTO_ROOT "${PROJECT_SOURCE_DIR}/../proto")
set(UEMETA_PROTO_GENERATED_DIR "${PROJECT_BINARY_DIR}/generated")

file(GLOB proto_inputs CONFIGURE_DEPENDS "${UEMETA_PROTO_ROOT}/files/*.proto")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${UEMETA_PROTO_ROOT}/build.py" "${UEMETA_PROTO_ROOT}/buf.yaml" ${proto_inputs})

# The shared Python script owns Buf configuration, tool versions and generation.
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${UEMETA_PROTO_ROOT}/build.py"
        --language cpp --output "${UEMETA_PROTO_GENERATED_DIR}"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/.."
    COMMAND_ERROR_IS_FATAL ANY)

file(GLOB proto_sources CONFIGURE_DEPENDS "${UEMETA_PROTO_GENERATED_DIR}/*.pb.cc")
if(NOT proto_sources)
    message(FATAL_ERROR "proto/build.py produced no C++ sources in ${UEMETA_PROTO_GENERATED_DIR}")
endif()
target_sources(uemeta-parser-core PRIVATE ${proto_sources})
