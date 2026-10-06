include("${CMAKE_CURRENT_LIST_DIR}/DependencyCache.cmake")

set(BUILD_SHARED_LIBS OFF)
set(BUILD_TESTING OFF)
if(MSVC)
    # Match the release LLVM archive even in Debug builds.
    set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)
    add_compile_definitions(_ITERATOR_DEBUG_LEVEL=0)
endif()

include("${CMAKE_CURRENT_LIST_DIR}/LLVM.cmake")

uemeta_github(cli11 CLIUtils/CLI11 37bb6edc5317e99af72ef48405e65d9ca5218861
    62ecd3cd7e8ce36506ea8c19e20554a933c82f45d6a75a39cc8778e0c5170a66)
uemeta_github(quill odygrd/quill v11.1.0
    a4c41068ec51979e1c6d95ae9ab6efc09e654b9815dcb7a1b58b7a430a5cbd13)
uemeta_archive(bs_thread_pool https://codeload.github.com/bshoshany/thread-pool/tar.gz/v5.1.0
    54378fb9cbeaee72996d3f8761469c43bb7dd2d4b07faec6d29a68277bd91a51)
find_package(Threads REQUIRED)
add_library(BS_thread_pool INTERFACE)
target_include_directories(BS_thread_pool SYSTEM INTERFACE "${bs_thread_pool_SOURCE_DIR}/include")
target_link_libraries(BS_thread_pool INTERFACE Threads::Threads)

# Hash2 and its Boost dependencies are header-only; no Boost build is needed.
add_library(uemeta-boost INTERFACE)
add_library(Boost::hash2 ALIAS uemeta-boost)
set(boost_modules
    config:e69618fa862927db69b4dd8e6b070c647a799b892fd0ee141d28db0dab025531
    mp11:6ab871e0ef397a2e7b0602f13a5f473a7381043c91b57fe907ca4925a6a1ee58
    describe:755e216f0f36379dc87e32ff5ab16d87fa9df4276562f241669924298899fc2b
    container_hash:5ec8bf37a75bef0bbac5f9e6e5f95d22a8e8a0034ce225d40ba701a483b34d27
    assert:9145fba14048a46c0f65e5b28e68176d568148957ebf9bdf9e82bcc5d5a703a9
    static_assert:23217831b80926140ac0cfb62d0cde5b8c878cf5b568852b2f3c6366fdb75820
    core:fbc69a21a0b3c839a2657ed803f1e7c1e6426d3d6e7c8bddb6b7a498382b5cd1
    throw_exception:bba826d1380ccedbcf0468ae4b74012ac14c3830be30d9174fbaf8583b56ed67
    hash2:b1a135d5e32c6e0088e566c3aef6cadd68caa311db2d0585a3c89651b8863f37)
foreach(module IN LISTS boost_modules)
    string(REPLACE ":" ";" module "${module}")
    list(GET module 0 name)
    list(GET module 1 sha256)
    uemeta_archive(boost_${name} "https://codeload.github.com/boostorg/${name}/tar.gz/boost-1.91.0" "${sha256}")
    target_include_directories(uemeta-boost SYSTEM INTERFACE "${boost_${name}_SOURCE_DIR}/include")
endforeach()

set(ABSL_BUILD_TESTING OFF)
set(ABSL_ENABLE_INSTALL OFF)
uemeta_github(absl abseil/abseil-cpp 20250512.1
    9b7a064305e9fd94d124ffa6cc358592eb42b5da588fb4e07d09254aa40086db)
set(UEMETA_PROTOBUF_VERSION 35.1) # Must match protoc_version in ../proto/build.py.
set(protobuf_BUILD_TESTS OFF)
set(protobuf_BUILD_CONFORMANCE OFF)
set(protobuf_BUILD_EXAMPLES OFF)
set(protobuf_BUILD_LIBPROTOC OFF)
set(protobuf_BUILD_PROTOC_BINARIES OFF)
set(protobuf_BUILD_LIBUPB OFF)
set(protobuf_INSTALL OFF)
set(protobuf_WITH_ZLIB OFF)
uemeta_github(protobuf protocolbuffers/protobuf "v${UEMETA_PROTOBUF_VERSION}"
    22775f9376938295efa2d59a59bde4cd075a42df5a9b4d27aa9b99fa6a413bd2)
target_include_directories(libprotobuf INTERFACE
    "${protobuf_SOURCE_DIR}/src" "${protobuf_SOURCE_DIR}/third_party/utf8_range")

if(UEMETA_ENABLE_TESTING)
    set(INSTALL_GTEST OFF)
    set(gtest_force_shared_crt OFF)
    uemeta_github(googletest google/googletest v1.14.0
        8ad598c73ad796e0d8280b082cebd82a630d73e73cd3c70057938a6501bba5d7)
endif()

# Abseil and GoogleTest override the runtime default in their own directories.
function(uemeta_msvc_dependencies directory)
    get_property(targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach(target IN LISTS targets)
        get_target_property(type "${target}" TYPE)
        if(NOT type MATCHES "^(INTERFACE_LIBRARY|UTILITY)$")
            set_property(TARGET "${target}" PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded)
            target_compile_options("${target}" PRIVATE /w /wd4530)
        endif()
    endforeach()
    get_property(subdirectories DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
    foreach(subdirectory IN LISTS subdirectories)
        uemeta_msvc_dependencies("${subdirectory}")
    endforeach()
endfunction()
if(MSVC)
    uemeta_msvc_dependencies("${CMAKE_CURRENT_SOURCE_DIR}")
endif()
