# Native CMake target for xbmptool.
a51_add_project( xbmptool EXECUTABLE
    SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/xbmpTool.cpp"
    PRIVATE_INCLUDE_DIRECTORIES
        "${CMAKE_SOURCE_DIR}/xCore/x_files"
        "${CMAKE_SOURCE_DIR}/xCore"
    DEPENDENCIES
        aux_Bitmap
        CommandLine
        x_files
)
