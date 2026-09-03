#==========================================================================
#
#  A51Project.cmake
#
#==========================================================================

include_guard( GLOBAL )

function( a51_add_project TargetName ProjectType )
    set( _options WIN32 )
    set( _one_value_arguments SOURCE_ROOT )
    set( _multi_value_arguments
        SOURCES
        DEPENDENCIES
        PUBLIC_INCLUDE_DIRECTORIES
        PRIVATE_INCLUDE_DIRECTORIES
        COMPILE_DEFINITIONS
        LINK_OPTIONS
    )
    cmake_parse_arguments(
        A51_PROJECT
        "${_options}"
        "${_one_value_arguments}"
        "${_multi_value_arguments}"
        ${ARGN}
    )

    if( NOT A51_PROJECT_SOURCE_ROOT )
        set( A51_PROJECT_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}" )
    endif()

    if( NOT A51_PROJECT_SOURCES )
        message( FATAL_ERROR "A51 project ${TargetName} has no explicit source list." )
    endif()

    set( _missing_sources )
    foreach( _source IN LISTS A51_PROJECT_SOURCES )
        if( NOT EXISTS "${_source}" )
            list( APPEND _missing_sources "${_source}" )
        endif()
    endforeach()
    if( _missing_sources )
        string( JOIN "\n  " _missing_source_report ${_missing_sources} )
        message( FATAL_ERROR
            "A51 project ${TargetName} references missing sources:\n  ${_missing_source_report}"
        )
    endif()

    if( ProjectType STREQUAL "EXECUTABLE" )
        if( A51_PROJECT_WIN32 )
            add_executable( ${TargetName} WIN32 ${A51_PROJECT_SOURCES} )
        else()
            add_executable( ${TargetName} ${A51_PROJECT_SOURCES} )
        endif()
    elseif( ProjectType STREQUAL "SHARED" )
        add_library( ${TargetName} SHARED ${A51_PROJECT_SOURCES} )
    elseif( ProjectType STREQUAL "STATIC" )
        add_library( ${TargetName} STATIC ${A51_PROJECT_SOURCES} )
    else()
        message( FATAL_ERROR "Unknown A51 project type for ${TargetName}: ${ProjectType}" )
    endif()

    target_include_directories( ${TargetName} PRIVATE
        "${CMAKE_SOURCE_DIR}"
        "${CMAKE_SOURCE_DIR}/xCore"
        "${CMAKE_SOURCE_DIR}/xCore/x_files"
        "${CMAKE_SOURCE_DIR}/xCore/Entropy"
        "${CMAKE_SOURCE_DIR}/xCore/Parsing"
        "${CMAKE_SOURCE_DIR}/xCore/MeshUtil"
        "${CMAKE_SOURCE_DIR}/xCore/Auxiliary"
        "${CMAKE_SOURCE_DIR}/xCore/Auxiliary/Bitmap"
        "${CMAKE_SOURCE_DIR}/xCore/Auxiliary/CommandLine"
        "${CMAKE_SOURCE_DIR}/xCore/Auxiliary/fx_Core"
        "${CMAKE_SOURCE_DIR}/xCore/3rdParty"
        "${CMAKE_SOURCE_DIR}/Support"
        "${CMAKE_SOURCE_DIR}/MiscUtils"
        "${A51_PROJECT_SOURCE_ROOT}"
        ${A51_PROJECT_PRIVATE_INCLUDE_DIRECTORIES}
    )

    if( A51_PROJECT_PUBLIC_INCLUDE_DIRECTORIES )
        target_include_directories( ${TargetName} PUBLIC
            ${A51_PROJECT_PUBLIC_INCLUDE_DIRECTORIES}
        )
    endif()

    target_link_libraries( ${TargetName} PRIVATE
        a51::build_options
        ${A51_PROJECT_DEPENDENCIES}
    )

    target_compile_features( ${TargetName} PRIVATE cxx_std_17 )
    set_property( TARGET ${TargetName} PROPERTY C_STANDARD 11 )

    # Platform target definitions. TARGET_DESKTOP and TARGET_POSIX are
    # derived by x_target.hpp from the selected leaf target.
    target_compile_definitions( ${TargetName} PRIVATE
        "$<$<PLATFORM_ID:Windows>:TARGET_WINDOWS>"
        "$<$<PLATFORM_ID:Linux>:TARGET_LINUX>"
        "$<$<PLATFORM_ID:Android>:TARGET_ANDROID>"
        "$<$<STREQUAL:$<CONFIG>,Debug>:CONFIG_DEBUG>"
        "$<$<STREQUAL:$<CONFIG>,OptDebug>:CONFIG_OPTDEBUG>"
        "$<$<STREQUAL:$<CONFIG>,QA>:CONFIG_QA>"
        "$<$<STREQUAL:$<CONFIG>,Release>:CONFIG_RETAIL>"
        "$<$<STREQUAL:$<CONFIG>,EDITOR-Debug>:CONFIG_DEBUG>"
        "$<$<STREQUAL:$<CONFIG>,EDITOR-Debug>:X_EDITOR>"
        ${A51_PROJECT_COMPILE_DEFINITIONS}
    )

    if( A51_PROJECT_LINK_OPTIONS )
        target_link_options( ${TargetName} PRIVATE ${A51_PROJECT_LINK_OPTIONS} )
    endif()

    source_group( TREE "${CMAKE_SOURCE_DIR}" FILES ${A51_PROJECT_SOURCES} )
endfunction()
