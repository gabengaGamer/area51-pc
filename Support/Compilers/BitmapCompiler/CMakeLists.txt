# Native CMake target for BitmapCompiler.
a51_add_project( BitmapCompiler EXECUTABLE
    SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/main.cpp"
    PRIVATE_INCLUDE_DIRECTORIES
        "${CMAKE_SOURCE_DIR}/xCore"
        "${CMAKE_SOURCE_DIR}/xCore/x_files"
        "${CMAKE_CURRENT_LIST_DIR}/./"
        "${CMAKE_SOURCE_DIR}/xCore/Auxiliary/CommandLine"
        "${CMAKE_SOURCE_DIR}/xCore/../Support"
    DEPENDENCIES
        aux_Bitmap
        CommandLine
        x_files
)
