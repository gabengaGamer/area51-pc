a51_add_project( Music_mgr STATIC
    SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/music_mgr.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/music_mgr.hpp"
    PRIVATE_INCLUDE_DIRECTORIES
        "${CMAKE_SOURCE_DIR}/xCore"
        "${CMAKE_SOURCE_DIR}/xCore/Entropy"
        "${CMAKE_SOURCE_DIR}/Support/Music_mgr"
        "${CMAKE_SOURCE_DIR}/xCore/x_files"
        "${CMAKE_SOURCE_DIR}/Support"
        "${CMAKE_SOURCE_DIR}/xCore/Auxiliary"
        "${CMAKE_SOURCE_DIR}/xCore/Auxiliary/MiscUtils"
)
