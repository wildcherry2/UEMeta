set(UEMETA_LLVM_VERSION 23.1.0)
if(NOT DEFINED CMAKE_SIZEOF_VOID_P)
    message(FATAL_ERROR "CMake could not determine the compiler ABI; fix the compiler/configure errors above first")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8 OR NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(AMD64|amd64|x86_64|X86_64)$")
    message(FATAL_ERROR "The bundled LLVM packages require x86-64")
endif()
if(WIN32 AND MSVC)
    set(llvm_package "clang+llvm-${UEMETA_LLVM_VERSION}-x86_64-pc-windows-msvc")
    set(llvm_sha256 5799ebeca6870e9e61d5b4c8bc869ca8490e8db55f22d1388c4545404864d9e3)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(llvm_package "LLVM-${UEMETA_LLVM_VERSION}-Linux-X64")
    set(llvm_sha256 18da30f77f475688a18f7704d23f9f155ae007ed9922dbed6850a9419d9fec8c)
else()
    message(FATAL_ERROR "Supported platforms: Windows with MSVC/clang-cl, and Linux x86-64")
endif()
uemeta_archive(llvm_prebuilt
    "https://github.com/llvm/llvm-project/releases/download/llvmorg-${UEMETA_LLVM_VERSION}/${llvm_package}.tar.xz"
    "${llvm_sha256}")
set(PARSER_LLVM_BIN_DIR "${llvm_prebuilt_SOURCE_DIR}/bin")

if(WIN32)
    # The Windows LLVM archive references these libraries but does not ship them.
    foreach(feature C14N CATALOG DEBUG DOCB FTP HTML HTTP ICONV ICU ISO8859X LEGACY LZMA
            MEM_DEBUG MODULES PATTERN PROGRAMS PUSH PYTHON READER REGEXPS RUN_DEBUG
            SCHEMAS SCHEMATRON TESTS THREAD_ALLOC VALID WRITER XINCLUDE XPATH XPTR ZLIB)
        set(LIBXML2_WITH_${feature} OFF)
    endforeach()
    foreach(feature OUTPUT SAX1 THREADS TREE)
        set(LIBXML2_WITH_${feature} ON)
    endforeach()
    uemeta_dependency(llvm_libxml2
        https://gitlab.gnome.org/GNOME/libxml2/-/archive/v2.9.12/libxml2-v2.9.12.tar.gz
        98bfa7a9a5e2a75638422050740448ee9f02bf4dc2075c9822d7747d5ff9e617)
    set(ZLIB_BUILD_TESTING OFF)
    set(ZLIB_BUILD_SHARED OFF)
    set(ZLIB_BUILD_STATIC ON)
    set(ZLIB_INSTALL OFF)
    uemeta_dependency(llvm_zlib
        https://github.com/madler/zlib/releases/download/v1.3.2/zlib-1.3.2.tar.gz
        bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16)
    set(ZSTD_BUILD_PROGRAMS OFF)
    set(ZSTD_BUILD_TESTS OFF)
    set(ZSTD_BUILD_STATIC ON)
    set(ZSTD_BUILD_SHARED OFF)
    uemeta_dependency(llvm_zstd
        https://github.com/facebook/zstd/releases/download/v1.5.7/zstd-1.5.7.tar.gz
        eb33e51f49a15e023950cd7825ca74a4a2b43db8354825ac24fc1b7ee09e6fa3 build/cmake)
    add_library(LibXml2::LibXml2 ALIAS LibXml2)
    add_library(ZLIB::ZLIB ALIAS zlibstatic)
    add_library(zstd::libzstd_static ALIAS libzstd_static)
    foreach(package ZLIB zstd LibXml2)
        set(CMAKE_DISABLE_FIND_PACKAGE_${package} TRUE)
    endforeach()

    # Infer the local DIA SDK for Ninja and Visual Studio without hard-coded editions.
    if(NOT DEFINED MSVC_DIA_SDK_DIR)
        string(REGEX REPLACE "/VC/Tools/.*$" "/DIA SDK" MSVC_DIA_SDK_DIR "${CMAKE_CXX_COMPILER}")
        if(CMAKE_VS_INSTALL_ROOT)
            set(MSVC_DIA_SDK_DIR "${CMAKE_VS_INSTALL_ROOT}/DIA SDK")
        endif()
    endif()
endif()

# Explicit paths keep stale package cache entries from selecting another LLVM.
set(LLVM_DIR "${llvm_prebuilt_SOURCE_DIR}/lib/cmake/llvm")
set(Clang_DIR "${llvm_prebuilt_SOURCE_DIR}/lib/cmake/clang")
find_package(LLVM REQUIRED CONFIG NO_DEFAULT_PATH PATHS "${LLVM_DIR}")
find_package(Clang REQUIRED CONFIG NO_DEFAULT_PATH PATHS "${Clang_DIR}")
separate_arguments(UEMETA_LLVM_DEFINITIONS UNIX_COMMAND "${LLVM_DEFINITIONS}")

if(WIN32)
    foreach(package ZLIB zstd LibXml2)
        unset(CMAKE_DISABLE_FIND_PACKAGE_${package})
    endforeach()
    # Some LLVM releases export the release builder's absolute DIA library path.
    get_target_property(pdb_links LLVMDebugInfoPDB INTERFACE_LINK_LIBRARIES)
    list(TRANSFORM pdb_links REPLACE ".*/DIA SDK/lib/amd64/diaguids\\.lib$" "DIASDK::Diaguids")
    set_property(TARGET LLVMDebugInfoPDB PROPERTY INTERFACE_LINK_LIBRARIES "${pdb_links}")
endif()
