#==============================================================================
#
#  Android NDK toolchain for Area 51.
#
#  Example:
#    cmake -S . -B build/android-arm64 \
#      -G Ninja \
#      -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/android.cmake \
#      -DA51_ANDROID_NDK=/path/to/android-ndk \
#      -DA51_ANDROID_ABI=arm64-v8a \
#      -DA51_ANDROID_API=21
#
#==============================================================================

set( A51_ANDROID_NDK "" CACHE PATH "Android NDK root directory" )

# CMake propagates the standard Android cache entry into try_compile projects,
# while custom cache entries are not always copied. Reuse it when this file is
# loaded by one of those nested checks.
if( NOT A51_ANDROID_NDK AND CMAKE_ANDROID_NDK )
    set( A51_ANDROID_NDK "${CMAKE_ANDROID_NDK}" CACHE PATH
        "Android NDK root directory" FORCE )
endif()

if( NOT A51_ANDROID_NDK )
    if( DEFINED ENV{ANDROID_NDK_ROOT} )
        set( A51_ANDROID_NDK "$ENV{ANDROID_NDK_ROOT}" CACHE PATH
            "Android NDK root directory" FORCE )
    elseif( DEFINED ENV{ANDROID_NDK_HOME} )
        set( A51_ANDROID_NDK "$ENV{ANDROID_NDK_HOME}" CACHE PATH
            "Android NDK root directory" FORCE )
    endif()
endif()

if( NOT A51_ANDROID_NDK )
    message( FATAL_ERROR
        "A51_ANDROID_NDK is not set. Pass the Android NDK root with "
        "-DA51_ANDROID_NDK=/path/to/android-ndk or set ANDROID_NDK_ROOT." )
endif()

set( A51_ANDROID_NDK_TOOLCHAIN
    "${A51_ANDROID_NDK}/build/cmake/android.toolchain.cmake" )
if( NOT EXISTS "${A51_ANDROID_NDK_TOOLCHAIN}" )
    message( FATAL_ERROR
        "The Android NDK toolchain was not found at "
        "'${A51_ANDROID_NDK_TOOLCHAIN}'." )
endif()

set( A51_ANDROID_ABI arm64-v8a CACHE STRING "Android ABI" )
set_property( CACHE A51_ANDROID_ABI PROPERTY STRINGS
    armeabi-v7a arm64-v8a x86 x86_64 )

set( A51_ANDROID_API 21 CACHE STRING "Android API level" )

# These are consumed by the NDK toolchain. Set them before including it.
set( CMAKE_ANDROID_NDK "${A51_ANDROID_NDK}" CACHE PATH
    "Android NDK used for the target" FORCE )
set( CMAKE_ANDROID_ARCH_ABI "${A51_ANDROID_ABI}" CACHE STRING
    "Android ABI used for the target" FORCE )
set( CMAKE_ANDROID_API "${A51_ANDROID_API}" CACHE STRING
    "Android API level used for the target" FORCE )
set( CMAKE_ANDROID_STL_TYPE c++_static CACHE STRING
    "Android C++ runtime" FORCE )

# NDK 29 still enters its legacy compatibility toolchain by default. That
# path selects the ABI from ANDROID_ABI, not CMAKE_ANDROID_ARCH_ABI.
set( ANDROID_ABI "${A51_ANDROID_ABI}" CACHE STRING
    "Android ABI used by the NDK compatibility toolchain" FORCE )
set( ANDROID_PLATFORM "android-${A51_ANDROID_API}" CACHE STRING
    "Android API level used by the NDK compatibility toolchain" FORCE )
set( ANDROID_STL c++_static CACHE STRING
    "Android C++ runtime used by the NDK compatibility toolchain" FORCE )

set( A51_BUILD_WINDOWS_TARGETS OFF CACHE BOOL
    "Disable Windows-only Area 51 targets" FORCE )

include( "${A51_ANDROID_NDK_TOOLCHAIN}" )

# The NDK compatibility toolchain replaces this list, so append custom values
# after it has been included. This keeps them available to nested try_compile
# projects used by bundled libraries.
list( APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES
    A51_ANDROID_NDK
    A51_ANDROID_ABI
    A51_ANDROID_API )
