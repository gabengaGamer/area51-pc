# Native CMake target for dfsTool.
a51_add_project( dfsTool EXECUTABLE
    SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/dfs.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/dfsTool.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/dfs_Build.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/dictionary.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/dfs.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/dictionary.hpp"
    PRIVATE_INCLUDE_DIRECTORIES
        "${CMAKE_SOURCE_DIR}/xCore/x_files"
        "${CMAKE_SOURCE_DIR}/xCore"
    DEPENDENCIES
        CommandLine
        x_files
)
