set( _a51_xbmp_tool_source_root "${CMAKE_CURRENT_LIST_DIR}/../xbmpTool" )
a51_add_project( xbmptool EXECUTABLE
    SOURCE_ROOT "${_a51_xbmp_tool_source_root}"
    SOURCES
        "${_a51_xbmp_tool_source_root}/xbmpTool.cpp"
    PRIVATE_INCLUDE_DIRECTORIES
        "${CMAKE_SOURCE_DIR}/xCore/x_files"
        "${CMAKE_SOURCE_DIR}/xCore"
    DEPENDENCIES
        aux_Bitmap
        CommandLine
        x_files
)
