set( CMAKE_SYSTEM_NAME Linux )
set( A51_BUILD_WINDOWS_TARGETS OFF CACHE BOOL "Disable Windows-only Area 51 targets" FORCE )

set( A51_LINUX_SYSROOT "" CACHE PATH
    "Optional sysroot for the selected Linux target" )
if( A51_LINUX_SYSROOT )
    set( CMAKE_SYSROOT "${A51_LINUX_SYSROOT}" CACHE PATH
        "Linux target sysroot" FORCE )
    set( CMAKE_FIND_ROOT_PATH "${A51_LINUX_SYSROOT}" CACHE STRING
        "Linux target root paths" FORCE )
endif()

set( CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER )
if( A51_LINUX_SYSROOT OR NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" )
    set( CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY )
    set( CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY )
    set( CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY )
else()
    set( CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH )
    set( CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH )
    set( CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH )
endif()

# Use caller-provided compilers when set.
function( a51_select_linux_gnu_compilers CompilerPrefix )
    if( NOT CMAKE_C_COMPILER )
        find_program( _a51_c_compiler NAMES "${CompilerPrefix}gcc" )
        if( NOT _a51_c_compiler )
            message( FATAL_ERROR
                "Could not find the Linux C compiler '${CompilerPrefix}gcc'. "
                "Install the matching GNU cross toolchain or pass "
                "-DCMAKE_C_COMPILER=/path/to/compiler."
            )
        endif()
        set( CMAKE_C_COMPILER "${_a51_c_compiler}" CACHE FILEPATH
            "Linux C compiler" )
    endif()

    if( NOT CMAKE_CXX_COMPILER )
        find_program( _a51_cxx_compiler NAMES "${CompilerPrefix}g++" )
        if( NOT _a51_cxx_compiler )
            message( FATAL_ERROR
                "Could not find the Linux C++ compiler '${CompilerPrefix}g++'. "
                "Install the matching GNU cross toolchain or pass "
                "-DCMAKE_CXX_COMPILER=/path/to/compiler."
            )
        endif()
        set( CMAKE_CXX_COMPILER "${_a51_cxx_compiler}" CACHE FILEPATH
            "Linux C++ compiler" )
    endif()
endfunction()
