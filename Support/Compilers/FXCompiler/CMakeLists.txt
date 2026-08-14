# Native CMake target for FXCompiler.
a51_add_project( FXCompiler EXECUTABLE
    SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/FXCompiler.cpp"
    PRIVATE_INCLUDE_DIRECTORIES
        "${CMAKE_SOURCE_DIR}/xCore"
        "${CMAKE_SOURCE_DIR}/xCore/x_files"
        "${CMAKE_SOURCE_DIR}/xCore/Entropy"
        "${CMAKE_SOURCE_DIR}/xCore/Auxiliary/fx_Core"
    DEPENDENCIES
        aux_Bitmap
        fx_Core
        Entropy
        MeshUtil
        Parsing
        x_files
)
