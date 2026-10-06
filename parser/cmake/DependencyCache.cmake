include_guard(GLOBAL)

set(UEMETA_DEPENDENCY_CACHE "${CMAKE_CURRENT_LIST_DIR}/../intermediate/deps"
    CACHE PATH "Archives and extracted sources shared by all builds and versions")
get_filename_component(UEMETA_DEPENDENCY_CACHE "${UEMETA_DEPENDENCY_CACHE}" ABSOLUTE)

# Only archives and sources are shared. Every build keeps its own object files.
# The checksum identifies both the archive and extraction, so changing a URL,
# release or platform cannot silently reuse another dependency's contents.
function(uemeta_archive name url sha256)
    string(LENGTH "${sha256}" hash_length)
    if(NOT hash_length EQUAL 64 OR NOT sha256 MATCHES "^[0-9a-f]+$")
        message(FATAL_ERROR "${name}: expected a SHA256 checksum")
    endif()
    set(archive "${UEMETA_DEPENDENCY_CACHE}/downloads/${sha256}/archive")
    set(source "${UEMETA_DEPENDENCY_CACHE}/src/${sha256}")
    file(MAKE_DIRECTORY "${UEMETA_DEPENDENCY_CACHE}/locks")
    file(LOCK "${UEMETA_DEPENDENCY_CACHE}/locks/${sha256}.lock" GUARD FUNCTION)

    if(NOT EXISTS "${source}/.complete")
        message(STATUS "Preparing ${name}")
        file(MAKE_DIRECTORY "${UEMETA_DEPENDENCY_CACHE}/downloads/${sha256}")
        # DOWNLOAD verifies an existing file and skips the transfer if it matches.
        file(DOWNLOAD "${url}" "${archive}" EXPECTED_HASH "SHA256=${sha256}"
            TLS_VERIFY ON SHOW_PROGRESS)
        # An interrupted extraction is never published as a usable source tree.
        cmake_path(IS_PREFIX UEMETA_DEPENDENCY_CACHE "${source}.tmp" NORMALIZE inside_cache)
        if(NOT inside_cache)
            message(FATAL_ERROR "Extraction path must be inside the dependency cache")
        endif()
        file(REMOVE_RECURSE "${source}.tmp")
        file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${source}.tmp")
        file(WRITE "${source}.tmp/.complete" "${sha256}\n")
        file(RENAME "${source}.tmp" "${source}")
    endif()

    # GitHub/source archives have one enclosing directory; protoc archives do not.
    file(GLOB entries LIST_DIRECTORIES TRUE "${source}/*")
    list(REMOVE_ITEM entries "${source}/.complete")
    list(LENGTH entries count)
    if(count EQUAL 1 AND IS_DIRECTORY "${entries}")
        set(source "${entries}")
    endif()
    set(${name}_SOURCE_DIR "${source}" PARENT_SCOPE)
endfunction()

function(uemeta_dependency name url sha256)
    uemeta_archive(${name} "${url}" "${sha256}")
    set(source "${${name}_SOURCE_DIR}")
    # Some upstream configure steps write to their source tree (e.g. zlib).
    file(LOCK "${UEMETA_DEPENDENCY_CACHE}/locks/${sha256}.lock" GUARD FUNCTION)
    add_subdirectory("${source}/${ARGN}" "${CMAKE_BINARY_DIR}/_deps/${name}-build" EXCLUDE_FROM_ALL SYSTEM)
    set(${name}_SOURCE_DIR "${source}" PARENT_SCOPE)
endfunction()

function(uemeta_github name repository revision sha256)
    uemeta_dependency(${name} "https://codeload.github.com/${repository}/tar.gz/${revision}" "${sha256}")
    set(${name}_SOURCE_DIR "${${name}_SOURCE_DIR}" PARENT_SCOPE)
endfunction()
