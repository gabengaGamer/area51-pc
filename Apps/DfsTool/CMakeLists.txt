set( _a51_dfs_tool_source_root "${CMAKE_CURRENT_LIST_DIR}" )
a51_add_project( dfsTool EXECUTABLE
    SOURCE_ROOT "${_a51_dfs_tool_source_root}"
    SOURCES
        "${_a51_dfs_tool_source_root}/dfs.cpp"
        "${_a51_dfs_tool_source_root}/dfsTool.cpp"
        "${_a51_dfs_tool_source_root}/dfs_Build.cpp"
        "${_a51_dfs_tool_source_root}/dictionary.cpp"
        "${_a51_dfs_tool_source_root}/dfs.hpp"
        "${_a51_dfs_tool_source_root}/dictionary.hpp"
    PRIVATE_INCLUDE_DIRECTORIES
        "${CMAKE_SOURCE_DIR}/xCore/x_files"
        "${CMAKE_SOURCE_DIR}/xCore"
    DEPENDENCIES
        CommandLine
        x_files
)
