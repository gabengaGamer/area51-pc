#==========================================================================
#
#  CMakeLists.txt
#
#==========================================================================

cmake_minimum_required( VERSION 3.23 )

# Set CMP0091 before project().
cmake_policy( SET CMP0091 NEW )

project( Area51 LANGUAGES C CXX )

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(
        $<$<CONFIG:Release>:-g>
    )
endif()

# Must precede include( A51ThirdParty ), which add_subdirectory()'s the bundled libs.
if( MSVC AND NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY )
    set( CMAKE_MSVC_RUNTIME_LIBRARY
        "MultiThreaded$<$<OR:$<STREQUAL:$<CONFIG>,Debug>,$<STREQUAL:$<CONFIG>,EDITOR-Debug>>:Debug>"
        CACHE STRING "MSVC runtime library selection" )
endif()

list( APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" )
include( A51Project )
include( A51Platform )
include( A51ThirdParty )

#==========================================================================
#  OPTIONS AND CONFIGURATIONS
#==========================================================================

set( A51_CONFIGURATION_TYPES Debug OptDebug QA Release EDITOR-Debug CACHE STRING "Supported Area 51 configurations" )
if( CMAKE_CONFIGURATION_TYPES )
    set( CMAKE_CONFIGURATION_TYPES "${A51_CONFIGURATION_TYPES}" CACHE STRING "Supported Area 51 configurations" FORCE )
elseif( NOT DEFINED CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "" )
    set( CMAKE_BUILD_TYPE Debug CACHE STRING
        "Default Area 51 configuration for single-config generators" FORCE )
endif()

foreach( _a51_configuration IN LISTS A51_CONFIGURATION_TYPES )
    string( TOUPPER "${_a51_configuration}" _a51_configuration_upper )
    foreach( _a51_linker_kind EXE SHARED MODULE )
        set( _a51_linker_flags_variable
            "CMAKE_${_a51_linker_kind}_LINKER_FLAGS_${_a51_configuration_upper}" )
        if( NOT DEFINED "${_a51_linker_flags_variable}" )
            set( "${_a51_linker_flags_variable}" ""
                CACHE STRING "${_a51_configuration} ${_a51_linker_kind} linker flags" )
        endif()
    endforeach()
endforeach()

option( A51_BUILD_SELECTED_APPS "Build the selected application targets" ON )
option( A51_ENABLE_QT "Enable Qt integration for XBMPViewer when Qt6 is available" ON )
option( A51_EXPORT_COMPILE_COMMANDS "Generate compile_commands.json for tooling" ON )
if( WIN32 )
    set( _a51_default_windows_targets ON )
else()
    set( _a51_default_windows_targets OFF )
endif()
option( A51_BUILD_WINDOWS_TARGETS "Enable targets whose current implementation requires Windows-only dependencies" ${_a51_default_windows_targets} )

if( A51_EXPORT_COMPILE_COMMANDS )
    set( CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL
        "Generate compile_commands.json for tooling" FORCE )
endif()

add_library( a51_build_options INTERFACE )
add_library( a51::build_options ALIAS a51_build_options )

target_compile_features( a51_build_options INTERFACE cxx_std_17 )
target_compile_definitions( a51_build_options INTERFACE
    "$<$<PLATFORM_ID:Windows>:NOMINMAX>"
    "$<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_WARNINGS>"
    "$<$<STREQUAL:$<CONFIG>,EDITOR-Debug>:CONFIG_DEBUG>"
    "$<$<STREQUAL:$<CONFIG>,EDITOR-Debug>:X_EDITOR>"
)

if( MSVC )
    # Allow parallel cl.exe invocations.
    target_compile_options( a51_build_options INTERFACE /FS )

    target_compile_options( a51_build_options INTERFACE
        "$<$<CONFIG:Debug>:/Od>"
        "$<$<STREQUAL:$<CONFIG>,EDITOR-Debug>:/Od>"
        "$<$<CONFIG:OptDebug>:/O2>"
        "$<$<CONFIG:QA>:/O2>"
        "$<$<CONFIG:Release>:/O2>"
        "$<$<CONFIG:Debug>:/Zi>"
        "$<$<STREQUAL:$<CONFIG>,EDITOR-Debug>:/Zi>"
        "$<$<CONFIG:OptDebug>:/Zi>"
        "$<$<CONFIG:QA>:/Zi>"
        "$<$<CONFIG:Release>:/Zi>"
    )
    target_link_options( a51_build_options INTERFACE
        "$<$<CONFIG:Debug>:/DEBUG>"
        "$<$<STREQUAL:$<CONFIG>,EDITOR-Debug>:/DEBUG>"
        "$<$<CONFIG:OptDebug>:/DEBUG>"
        "$<$<CONFIG:QA>:/DEBUG>"
        "$<$<CONFIG:Release>:/DEBUG>"
    )
endif()

if( WIN32 )
    target_link_libraries( a51_build_options INTERFACE
        ws2_32
        user32
        gdi32
        ole32
        comctl32
        comdlg32
    )
endif()

include( CheckIncludeFileCXX )
if( WIN32 )
    check_include_file_cxx( "windows.h" A51_HAS_WINDOWS_H )
    if( NOT A51_HAS_WINDOWS_H )
        message( WARNING "The active Windows toolchain cannot include windows.h. Configure succeeds, but Windows targets require a Windows SDK toolchain before they can be built." )
    endif()
endif()

if( A51_ENABLE_QT )
    find_package( Qt6 COMPONENTS Core Gui Widgets QUIET )
endif()

#==========================================================================
#  PROJECT GRAPH
#==========================================================================

add_subdirectory( xCore )
add_subdirectory( Support )
add_subdirectory( Apps )

set( _a51_tool_targets
    xbmptool
    Art2Code
    dfsTool
    XBMPViewer
    SoundPackager
    ShaderTool
    AnimCompiler
    BitmapCompiler
    DecalCompiler
    FontBuilder
    stringTool
)

set( _a51_configured_tool_targets )
foreach( _a51_tool_target IN LISTS _a51_tool_targets )
    if( TARGET ${_a51_tool_target} )
        list( APPEND _a51_configured_tool_targets ${_a51_tool_target} )
    endif()
endforeach()

if( _a51_configured_tool_targets )
    add_custom_target( a51_tools )
    add_dependencies( a51_tools ${_a51_configured_tool_targets} )
endif()

#==========================================================================
#  PLATFORM DEPENDENCIES
#==========================================================================

if( TARGET Entropy )
    target_compile_definitions( Entropy PRIVATE ENTROPY_RENDER_SDL )
endif()

if( TARGET Script_mgr )
    target_include_directories( Script_mgr PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/xCore/3rdParty/Lua/5.5.0"
    )
endif()

if( TARGET ShaderTool )
    target_include_directories( ShaderTool PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/xCore/3rdParty/DXC/inc"
    )
endif()

message( STATUS "Configured the native Area 51 CMake target graph." )
