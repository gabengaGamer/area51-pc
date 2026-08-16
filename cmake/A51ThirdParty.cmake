#==========================================================================
#
#  A51ThirdParty.cmake
#
#  Build bundled third-party libraries.
#
#==========================================================================

include_guard( GLOBAL )

include( ExternalProject )

option( A51_USE_BUNDLED_DEPENDENCIES
    "Build the bundled third-party dependencies from source" ON )

if( NOT A51_USE_BUNDLED_DEPENDENCIES )
    message( FATAL_ERROR
        "A51_USE_BUNDLED_DEPENDENCIES=OFF is not supported by this build." )
endif()

set( A51_THIRDPARTY_ROOT "${CMAKE_SOURCE_DIR}/xCore/3rdParty" )

set( BUILD_SHARED_LIBS OFF CACHE BOOL
    "Build bundled third-party libraries as static libraries" FORCE )
set( BUILD_TESTING OFF CACHE BOOL
    "Disable bundled third-party tests" FORCE )

# SDL
set( SDL_SHARED OFF CACHE BOOL "Build SDL3 shared library" FORCE )
set( SDL_STATIC ON CACHE BOOL "Build SDL3 static library" FORCE )
set( SDL_TEST_LIBRARY OFF CACHE BOOL "Build SDL3 test library" FORCE )
set( SDL_TESTS OFF CACHE BOOL "Build SDL3 tests" FORCE )
set( SDL_EXAMPLES OFF CACHE BOOL "Build SDL3 examples" FORCE )
set( SDL_INSTALL OFF CACHE BOOL "Install SDL3" FORCE )
add_subdirectory( "${A51_THIRDPARTY_ROOT}/SDL3"
    "${CMAKE_BINARY_DIR}/thirdparty/SDL3" EXCLUDE_FROM_ALL )
add_library( a51::SDL3 ALIAS SDL3-static )

# Xiph libraries
set( INSTALL_DOCS OFF CACHE BOOL "Install bundled third-party docs" FORCE )
set( INSTALL_PKG_CONFIG_MODULE OFF CACHE BOOL
    "Install bundled third-party pkg-config files" FORCE )
set( INSTALL_CMAKE_PACKAGE_MODULE OFF CACHE BOOL
    "Install bundled third-party CMake packages" FORCE )
add_subdirectory( "${A51_THIRDPARTY_ROOT}/Libogg"
    "${CMAKE_BINARY_DIR}/thirdparty/Libogg" EXCLUDE_FROM_ALL )
add_library( a51::ogg ALIAS ogg )

set( OPUS_BUILD_SHARED_LIBRARY OFF CACHE BOOL
    "Build Opus shared library" FORCE )
set( OPUS_BUILD_TESTING OFF CACHE BOOL "Build Opus tests" FORCE )
set( OPUS_BUILD_PROGRAMS OFF CACHE BOOL "Build Opus programs" FORCE )
set( OPUS_INSTALL_PKG_CONFIG_MODULE OFF CACHE BOOL
    "Install Opus pkg-config module" FORCE )
set( OPUS_INSTALL_CMAKE_CONFIG_MODULE OFF CACHE BOOL
    "Install Opus CMake package module" FORCE )
add_subdirectory( "${A51_THIRDPARTY_ROOT}/Libopus"
    "${CMAKE_BINARY_DIR}/thirdparty/Libopus" EXCLUDE_FROM_ALL )
add_library( a51::opus ALIAS opus )

add_subdirectory( "${A51_THIRDPARTY_ROOT}/Libvorbis"
    "${CMAKE_BINARY_DIR}/thirdparty/Libvorbis" EXCLUDE_FROM_ALL )
add_library( a51::vorbis ALIAS vorbis )
add_library( a51::vorbisenc ALIAS vorbisenc )
add_library( a51::vorbisfile ALIAS vorbisfile )

# libvpx
set( A51_LIBVPX_ROOT "${A51_THIRDPARTY_ROOT}/Libvpx" )
add_library( a51_libvpx STATIC IMPORTED GLOBAL )

if( A51_TARGET_PLATFORM STREQUAL "Windows" )
    if( A51_TARGET_ARCHITECTURE STREQUAL "x86" )
        set( A51_LIBVPX_LIBRARY "${A51_LIBVPX_ROOT}/bin/x32/vpx.lib" )
    elseif( A51_TARGET_ARCHITECTURE STREQUAL "x64" )
        set( A51_LIBVPX_LIBRARY "${A51_LIBVPX_ROOT}/bin/x64/vpx.lib" )
    else()
        message( FATAL_ERROR
            "Windows ${A51_TARGET_ARCHITECTURE} has no bundled libvpx library." )
    endif()

    if( NOT EXISTS "${A51_LIBVPX_LIBRARY}" )
        message( FATAL_ERROR
            "Bundled Windows libvpx library is missing: ${A51_LIBVPX_LIBRARY}" )
    endif()

    set_target_properties( a51_libvpx PROPERTIES
        IMPORTED_LOCATION "${A51_LIBVPX_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${A51_LIBVPX_ROOT}"
    )
else()
    if( NOT EXISTS "${A51_LIBVPX_ROOT}/configure" )
        message( FATAL_ERROR "Bundled libvpx source tree is missing its configure script." )
    endif()
    find_program( A51_LIBVPX_MAKE_PROGRAM NAMES make REQUIRED )

    set( A51_LIBVPX_BINARY_DIR "${CMAKE_BINARY_DIR}/thirdparty/Libvpx" )
    ExternalProject_Add( a51_libvpx_external
        SOURCE_DIR "${A51_LIBVPX_ROOT}"
        BINARY_DIR "${A51_LIBVPX_BINARY_DIR}"
        CONFIGURE_COMMAND
            "${CMAKE_COMMAND}" -E env
            "CC=${CMAKE_C_COMPILER}"
            "CXX=${CMAKE_CXX_COMPILER}"
            "${A51_LIBVPX_ROOT}/configure"
            --target=generic-gnu
            --disable-shared
            --enable-static
            --disable-examples
            --disable-tools
            --disable-docs
            --disable-unit-tests
            --disable-vp8-encoder
            --disable-vp9-encoder
            --disable-webm-io
            --disable-libyuv
            --enable-pic
        BUILD_COMMAND "${A51_LIBVPX_MAKE_PROGRAM}" -j2
        INSTALL_COMMAND ""
        BUILD_BYPRODUCTS "${A51_LIBVPX_BINARY_DIR}/libvpx.a"
        LOG_CONFIGURE ON
        LOG_BUILD ON
    )
    set_target_properties( a51_libvpx PROPERTIES
        IMPORTED_LOCATION "${A51_LIBVPX_BINARY_DIR}/libvpx.a"
        INTERFACE_INCLUDE_DIRECTORIES
            "${A51_LIBVPX_ROOT};${A51_LIBVPX_BINARY_DIR}"
    )
    add_dependencies( a51_libvpx a51_libvpx_external )
endif()

add_library( a51::vpx ALIAS a51_libvpx )

# opusfile is not used by the current movie backend.

# libwebm
set( ENABLE_WEBMTS OFF CACHE BOOL "Build libwebm MPEG-TS tools" FORCE )
set( ENABLE_WEBMINFO OFF CACHE BOOL "Build libwebm info tool" FORCE )
set( ENABLE_TESTS OFF CACHE BOOL "Build libwebm tests" FORCE )
set( ENABLE_SAMPLE_PROGRAMS OFF CACHE BOOL "Build libwebm samples" FORCE )
set( A51_CMAKE_CXX_FLAGS_BEFORE_LIBWEBM "${CMAKE_CXX_FLAGS}" )
add_subdirectory( "${A51_THIRDPARTY_ROOT}/Libwebm"
    "${CMAKE_BINARY_DIR}/thirdparty/Libwebm" EXCLUDE_FROM_ALL )
set( CMAKE_CXX_FLAGS "${A51_CMAKE_CXX_FLAGS_BEFORE_LIBWEBM}"
    CACHE STRING "C++ compiler flags" FORCE )
unset( A51_CMAKE_CXX_FLAGS_BEFORE_LIBWEBM )
foreach( A51_LIBWEBM_TARGET mkvmuxer mkvparser webvtt_common webm )
    if( TARGET ${A51_LIBWEBM_TARGET} )
        target_compile_features( ${A51_LIBWEBM_TARGET} PRIVATE cxx_std_17 )
        target_compile_definitions( ${A51_LIBWEBM_TARGET} PRIVATE
            __STDC_CONSTANT_MACROS
            __STDC_FORMAT_MACROS
            __STDC_LIMIT_MACROS
        )
    endif()
endforeach()
target_include_directories( webm PUBLIC
    "${A51_THIRDPARTY_ROOT}/Libwebm"
    "${A51_THIRDPARTY_ROOT}/Libwebm/webm_parser/include"
)
add_library( a51::webm ALIAS webm )

if( A51_TARGET_PLATFORM STREQUAL "Windows" )
    message( STATUS "Using bundled SDL3, Ogg, Opus, Vorbis, WebM and Windows libvpx." )
else()
    message( STATUS "Using bundled SDL3, Ogg, Opus, Vorbis, libvpx and WebM sources." )
endif()
