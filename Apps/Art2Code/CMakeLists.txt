a51_add_project( Art2Code EXECUTABLE
    SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/main.cpp"
    PRIVATE_INCLUDE_DIRECTORIES
        "${CMAKE_SOURCE_DIR}/xCore"
        "${CMAKE_SOURCE_DIR}/xCore/x_files"
        "${CMAKE_SOURCE_DIR}/xCore/Entropy"
        "${CMAKE_SOURCE_DIR}/xCore/MeshUtil"
        "${CMAKE_SOURCE_DIR}/xCore/Auxiliary/CommandLine"
        "${CMAKE_SOURCE_DIR}/xCore/Parsing"
        "${CMAKE_SOURCE_DIR}/xCore/Auxiliary/Bitmap"
    DEPENDENCIES
        aux_Bitmap
        CommandLine
        MeshUtil
        Parsing
        x_files
)
