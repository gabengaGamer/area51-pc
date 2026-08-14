# Native GameApp target.

set( A51_GAME_APP_PLATFORM_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/GameAppPlatform.hpp"
)

set( A51_GAME_APP_WINDOWS_RESOURCES )

if( WIN32 )
    list( APPEND A51_GAME_APP_PLATFORM_SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/GameAppPlatform_Windows.cpp"
    )
    list( APPEND A51_GAME_APP_WINDOWS_RESOURCES
        "${CMAKE_CURRENT_LIST_DIR}/A51.rc"
    )
elseif( CMAKE_SYSTEM_NAME STREQUAL "Linux" )
    list( APPEND A51_GAME_APP_PLATFORM_SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/GameAppPlatform_Linux.cpp"
    )
else()
    message( FATAL_ERROR "GameApp has no platform backend for ${CMAKE_SYSTEM_NAME}" )
endif()

a51_add_project( A51 EXECUTABLE
    SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/Config.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/FramePacer.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/FrameTiming.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/IniFile.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/main.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Version.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Config.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/FramePacer.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/FrameTiming.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/IniFile.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/main.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/main_desktop.inl"
        "${CMAKE_CURRENT_LIST_DIR}/A51Resources.hpp"
        ${A51_GAME_APP_PLATFORM_SOURCES}
        ${A51_GAME_APP_WINDOWS_RESOURCES}
    DEPENDENCIES
        AudioMgr
        ConversationMgr
        UserInterface
        EventMgr
        Gamelib
        Music_mgr
        Script_mgr
        NetworkMgr
        Render
        StringMgr
        aux_Bitmap
        FX
        Entropy
        Parsing
        x_files
    PRIVATE_INCLUDE_DIRECTORIES
        "${CMAKE_SOURCE_DIR}/xCore/3rdParty/SDL3/include"
)
