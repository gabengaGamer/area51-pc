a51_add_project( MeshUtil STATIC
    SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}"
    DEPENDENCIES
        Parsing
        x_files
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/MatxMisc.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/RawAnim.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/RawMaterial.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/RawMaterial2.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/RawMesh.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/RawMesh2.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/RawSettings.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/MATXMisc.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/RawAnim.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/RawMaterial.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/RawMaterial2.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/RawMesh.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/RawMesh2.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/RawSettings.hpp"
    PRIVATE_INCLUDE_DIRECTORIES
        "${CMAKE_SOURCE_DIR}/Support"
        "${CMAKE_SOURCE_DIR}/xCore/x_files"
        "${CMAKE_SOURCE_DIR}/xCore/Parsing"
        "${CMAKE_SOURCE_DIR}/xCore"
)
