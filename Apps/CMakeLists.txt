# Application targets. EDRscDesc is also used as a build-time dependency by
# the compiler tools, so it is kept in the graph independently of the app
# selection switch.

add_subdirectory( EDRscDesc )

set( _a51_portable_app_targets )
set( _a51_windows_app_targets )

if( A51_BUILD_SELECTED_APPS )
    add_subdirectory( XbmpTool )
    add_subdirectory( DfsTool )
    add_subdirectory( Art2Code )
    list( APPEND _a51_portable_app_targets xbmptool dfsTool Art2Code )

    if( A51_ENABLE_QT AND Qt6_FOUND )
        add_subdirectory( XbmpViewer )
        list( APPEND _a51_portable_app_targets XBMPViewer )
    else()
        message( STATUS "Skipping XBMPViewer: Qt6 is not available." )
    endif()

    if( WIN32 AND A51_BUILD_WINDOWS_TARGETS )
        add_subdirectory( GameApp )
        list( APPEND _a51_windows_app_targets A51 )
    elseif( CMAKE_SYSTEM_NAME STREQUAL "Linux" )
        add_subdirectory( GameApp )
        list( APPEND _a51_portable_app_targets A51 )
    endif()

    if( WIN32 AND A51_BUILD_WINDOWS_TARGETS )
        add_subdirectory( SoundPackager )
        add_subdirectory( ShaderTool )
        list( APPEND _a51_windows_app_targets SoundPackager ShaderTool )
    else()
        message( STATUS "Skipping Windows-only application targets: SoundPackager, ShaderTool." )
    endif()

    add_custom_target( a51_portable_apps )
    add_dependencies( a51_portable_apps ${_a51_portable_app_targets} )

    if( _a51_windows_app_targets )
        add_custom_target( a51_windows_apps )
        add_dependencies( a51_windows_apps ${_a51_windows_app_targets} )
    endif()

    add_custom_target( a51_selected_apps )
    add_dependencies( a51_selected_apps
        ${_a51_portable_app_targets}
        ${_a51_windows_app_targets}
    )
endif()
