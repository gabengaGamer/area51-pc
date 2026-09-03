#==========================================================================
#
#  A51Platform.cmake
#
#==========================================================================

include_guard( GLOBAL )

if( CMAKE_SYSTEM_NAME STREQUAL "Windows" )
    set( A51_TARGET_PLATFORM Windows )
elseif( CMAKE_SYSTEM_NAME STREQUAL "Linux" )
    set( A51_TARGET_PLATFORM Linux )
elseif( CMAKE_SYSTEM_NAME STREQUAL "Android" )
    set( A51_TARGET_PLATFORM Android )
else()
    message( FATAL_ERROR
        "Area 51 supports Windows, Linux, and Android targets; "
        "CMAKE_SYSTEM_NAME is '${CMAKE_SYSTEM_NAME}'." )
endif()

if( A51_TARGET_PLATFORM STREQUAL "Linux" AND
    CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" )
    if( CMAKE_C_COMPILER_TARGET )
        set( _a51_compiler_target "${CMAKE_C_COMPILER_TARGET}" )
    else()
        execute_process(
            COMMAND "${CMAKE_C_COMPILER}" -dumpmachine
            OUTPUT_VARIABLE _a51_compiler_target
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
    endif()

    string( TOLOWER "${_a51_compiler_target}" _a51_compiler_target_lower )
    if( _a51_compiler_target_lower MATCHES "(mingw|cygwin|windows)" )
        message( FATAL_ERROR
            "The Linux target is using a Windows compiler "
            "('${_a51_compiler_target}'). Select a native Linux compiler, "
            "a Linux GNU cross compiler, or a Linux toolchain/sysroot."
        )
    endif()
endif()

string( TOLOWER "${CMAKE_GENERATOR_PLATFORM}" _a51_generator_platform )
string( TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _a51_system_processor )

if( _a51_generator_platform STREQUAL "win32" OR
    _a51_system_processor STREQUAL "i386" OR
    _a51_system_processor STREQUAL "i686" OR
    _a51_system_processor STREQUAL "x86" )
    set( A51_TARGET_ARCHITECTURE x86 )
elseif( _a51_generator_platform STREQUAL "arm" OR
        _a51_system_processor STREQUAL "arm" OR
        _a51_system_processor STREQUAL "arm32" OR
        _a51_system_processor MATCHES "^armv[4-8]" )
    set( A51_TARGET_ARCHITECTURE arm32 )
elseif( _a51_generator_platform STREQUAL "arm64" OR
        _a51_system_processor STREQUAL "aarch64" OR
        _a51_system_processor STREQUAL "arm64" )
    set( A51_TARGET_ARCHITECTURE arm64 )
elseif( _a51_generator_platform STREQUAL "x64" OR
        _a51_system_processor STREQUAL "x86_64" OR
        _a51_system_processor STREQUAL "amd64" )
    set( A51_TARGET_ARCHITECTURE x64 )
elseif( CMAKE_SIZEOF_VOID_P EQUAL 4 )
    set( A51_TARGET_ARCHITECTURE x86 )
else()
    set( A51_TARGET_ARCHITECTURE x64 )
endif()

if( CMAKE_SIZEOF_VOID_P EQUAL 4 AND
    NOT A51_TARGET_ARCHITECTURE MATCHES "^(x86|arm32)$" )
    message( FATAL_ERROR
        "Area 51 selected ${A51_TARGET_ARCHITECTURE}, but the compiler "
        "uses a 32-bit pointer size." )
elseif( CMAKE_SIZEOF_VOID_P EQUAL 8 AND
        NOT A51_TARGET_ARCHITECTURE MATCHES "^(x64|arm64)$" )
    message( FATAL_ERROR
        "Area 51 selected ${A51_TARGET_ARCHITECTURE}, but the compiler "
        "uses a 64-bit pointer size." )
endif()

if( A51_TARGET_PLATFORM STREQUAL "Linux" AND _a51_compiler_target_lower )
    if( A51_TARGET_ARCHITECTURE STREQUAL "x86" AND
        NOT _a51_compiler_target_lower MATCHES "(^|[-_])(i[3-6]86|x86)([-_]|$)" )
        message( FATAL_ERROR
            "Linux x86 was selected, but the compiler target is "
            "'${_a51_compiler_target}'." )
    elseif( A51_TARGET_ARCHITECTURE STREQUAL "x64" AND
            NOT _a51_compiler_target_lower MATCHES "(x86_64|amd64)" )
        message( FATAL_ERROR
            "Linux x64 was selected, but the compiler target is "
            "'${_a51_compiler_target}'." )
    elseif( A51_TARGET_ARCHITECTURE STREQUAL "arm32" AND
            NOT _a51_compiler_target_lower MATCHES "(^|[-_])arm(v[4-8][^ -_]*|hf)?([-_]|$)" )
        message( FATAL_ERROR
            "Linux ARM32 was selected, but the compiler target is "
            "'${_a51_compiler_target}'." )
    elseif( A51_TARGET_ARCHITECTURE STREQUAL "arm64" AND
            NOT _a51_compiler_target_lower MATCHES "(aarch64|arm64)" )
        message( FATAL_ERROR
            "Linux ARM64 was selected, but the compiler target is "
            "'${_a51_compiler_target}'." )
    endif()
endif()

set( A51_TARGET_PLATFORM "${A51_TARGET_PLATFORM}" CACHE INTERNAL "Area 51 target platform" FORCE )
set( A51_TARGET_ARCHITECTURE "${A51_TARGET_ARCHITECTURE}" CACHE INTERNAL "Area 51 target architecture" FORCE )

message( STATUS "Area 51 target: ${A51_TARGET_PLATFORM} / ${A51_TARGET_ARCHITECTURE}" )
