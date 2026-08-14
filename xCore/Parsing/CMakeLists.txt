a51_add_project( Parsing STATIC
    SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}"
    DEPENDENCIES
        x_files
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/TextIn.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/TextOut.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/tokenizer.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/TextIn.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/TextOut.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/tokenizer.hpp"
    PRIVATE_INCLUDE_DIRECTORIES
        "${CMAKE_SOURCE_DIR}/xCore/x_files"
        "${CMAKE_CURRENT_LIST_DIR}"
        "${CMAKE_SOURCE_DIR}/Support"
)
