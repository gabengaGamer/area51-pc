a51_add_project( FontBuilder EXECUTABLE
    SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/FontBuilder.cpp"
    PRIVATE_INCLUDE_DIRECTORIES
        "${CMAKE_SOURCE_DIR}/xCore/x_files"
        "${CMAKE_SOURCE_DIR}/xCore/Auxiliary"
        "${CMAKE_SOURCE_DIR}/xCore/Parsing"
    DEPENDENCIES
        aux_Bitmap
        CommandLine
        Parsing
        x_files
)
