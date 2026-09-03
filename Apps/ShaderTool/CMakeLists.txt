a51_add_project( ShaderTool EXECUTABLE
    SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/DxcCompiler.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ParseScript.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ShaderTool.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ShaderTool.hpp"
    PRIVATE_INCLUDE_DIRECTORIES
        "${CMAKE_SOURCE_DIR}/xCore/x_files"
        "${CMAKE_SOURCE_DIR}/xCore"
        "${CMAKE_SOURCE_DIR}/xCore/Parsing"
    DEPENDENCIES
        CommandLine
        Parsing
        x_files
        dxcompiler
)
