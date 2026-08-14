# Native CMake target for aux_Bitmap.
a51_add_project( aux_Bitmap STATIC
    SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/aux_Bitmap.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/DefaultBMP.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Load_BMP.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Load_PSD.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Load_TGA.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/aux_Bitmap.hpp"
    PRIVATE_INCLUDE_DIRECTORIES
        "${CMAKE_SOURCE_DIR}/xCore/x_files"
)
