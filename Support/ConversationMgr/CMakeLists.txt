a51_add_project( ConversationMgr STATIC
    SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/ConversationMgr.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ConversationMgr.hpp"
    PRIVATE_INCLUDE_DIRECTORIES
        "${CMAKE_SOURCE_DIR}/xCore"
        "${CMAKE_SOURCE_DIR}/xCore/Entropy"
        "${CMAKE_SOURCE_DIR}/xCore/x_files"
        "${CMAKE_SOURCE_DIR}/Support/ConversationMgr"
        "${CMAKE_SOURCE_DIR}/xCore/../Support"
        "${CMAKE_SOURCE_DIR}/xCore/Auxiliary"
)
