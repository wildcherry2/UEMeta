set(PARSER_BUNDLE_DIR "${PROJECT_SOURCE_DIR}/out" CACHE PATH "Parser executable and runtime resources")
# The pinned LLVM/Clang packages link statically on both supported platforms.
add_custom_command(TARGET parser POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${PARSER_BUNDLE_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:parser>" "${PARSER_BUNDLE_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${llvm_prebuilt_SOURCE_DIR}/lib/clang/${LLVM_VERSION_MAJOR}" "${PARSER_BUNDLE_DIR}/resources"
    COMMENT "Bundling parser and Clang resources"
    VERBATIM)

if(MSVC)
    add_custom_command(TARGET parser POST_BUILD
        COMMAND "$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:${CMAKE_COMMAND};-E;copy_if_different;$<TARGET_PDB_FILE:parser>;${PARSER_BUNDLE_DIR}>"
        COMMAND_EXPAND_LISTS VERBATIM)
endif()
