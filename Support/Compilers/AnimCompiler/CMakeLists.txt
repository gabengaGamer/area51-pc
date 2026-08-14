# Native CMake target for AnimCompiler.
a51_add_project( AnimCompiler EXECUTABLE
    SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/../../Animation/AnimData.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../ResourceMgr/inline_ResourceMgr.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../ResourceMgr/ResourceMgr.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/AnimCompiler.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../Animation/AnimCompress.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../Animation/AnimDecompress.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../Animation/AnimEvent.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../Animation/AnimGroup.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../Animation/AnimInfo.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../Animation/AnimKeyData.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../ResourceMgr/ResourceMgr.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/AnimCompiler.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/main.cpp"
    PRIVATE_INCLUDE_DIRECTORIES
        "${CMAKE_SOURCE_DIR}/xCore/.."
        "${CMAKE_SOURCE_DIR}/xCore"
        "${CMAKE_SOURCE_DIR}/xCore/x_files"
        "${CMAKE_SOURCE_DIR}/xCore/Entropy"
        "${CMAKE_CURRENT_LIST_DIR}/./"
        "${CMAKE_SOURCE_DIR}/xCore/MeshUtil"
        "${CMAKE_SOURCE_DIR}/xCore/Auxiliary/CommandLine"
        "${CMAKE_SOURCE_DIR}/xCore/../Support/Animation"
        "${CMAKE_SOURCE_DIR}/xCore/../Support/ResourceMgr"
        "${CMAKE_SOURCE_DIR}/xCore/Auxiliary/MiscUtils"
        "${CMAKE_SOURCE_DIR}/xCore/../Support"
        "${CMAKE_SOURCE_DIR}/xCore/../Support/Render"
    DEPENDENCIES
        CommandLine
        Entropy
        MeshUtil
        Parsing
        x_files
)
