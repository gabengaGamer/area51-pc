# Install script for directory: /mnt/Mount2/DeveloperSurface/area51-pc/xCore/3rdParty/Libwebm

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "0")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "TRUE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/home/gamespy/Android/Sdk/ndk/29.0.14206865/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "/mnt/Mount2/DeveloperSurface/area51-pc/Apps/GameApp/android/app/.cxx/RelWithDebInfo/4b5x6w5g/arm64-v8a/thirdparty/Libwebm/libwebm.a")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/webm" TYPE FILE FILES
    "/mnt/Mount2/DeveloperSurface/area51-pc/xCore/3rdParty/Libwebm/webm_parser/include/webm/buffer_reader.h"
    "/mnt/Mount2/DeveloperSurface/area51-pc/xCore/3rdParty/Libwebm/webm_parser/include/webm/callback.h"
    "/mnt/Mount2/DeveloperSurface/area51-pc/xCore/3rdParty/Libwebm/webm_parser/include/webm/dom_types.h"
    "/mnt/Mount2/DeveloperSurface/area51-pc/xCore/3rdParty/Libwebm/webm_parser/include/webm/element.h"
    "/mnt/Mount2/DeveloperSurface/area51-pc/xCore/3rdParty/Libwebm/webm_parser/include/webm/file_reader.h"
    "/mnt/Mount2/DeveloperSurface/area51-pc/xCore/3rdParty/Libwebm/webm_parser/include/webm/id.h"
    "/mnt/Mount2/DeveloperSurface/area51-pc/xCore/3rdParty/Libwebm/webm_parser/include/webm/istream_reader.h"
    "/mnt/Mount2/DeveloperSurface/area51-pc/xCore/3rdParty/Libwebm/webm_parser/include/webm/reader.h"
    "/mnt/Mount2/DeveloperSurface/area51-pc/xCore/3rdParty/Libwebm/webm_parser/include/webm/status.h"
    "/mnt/Mount2/DeveloperSurface/area51-pc/xCore/3rdParty/Libwebm/webm_parser/include/webm/webm_parser.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/webm/common" TYPE FILE FILES "/mnt/Mount2/DeveloperSurface/area51-pc/xCore/3rdParty/Libwebm/common/webmids.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/webm/mkvmuxer" TYPE FILE FILES
    "/mnt/Mount2/DeveloperSurface/area51-pc/xCore/3rdParty/Libwebm/mkvmuxer/mkvmuxer.h"
    "/mnt/Mount2/DeveloperSurface/area51-pc/xCore/3rdParty/Libwebm/mkvmuxer/mkvmuxertypes.h"
    "/mnt/Mount2/DeveloperSurface/area51-pc/xCore/3rdParty/Libwebm/mkvmuxer/mkvmuxerutil.h"
    "/mnt/Mount2/DeveloperSurface/area51-pc/xCore/3rdParty/Libwebm/mkvmuxer/mkvwriter.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/webm/mkvparser" TYPE FILE FILES
    "/mnt/Mount2/DeveloperSurface/area51-pc/xCore/3rdParty/Libwebm/mkvparser/mkvparser.h"
    "/mnt/Mount2/DeveloperSurface/area51-pc/xCore/3rdParty/Libwebm/mkvparser/mkvreader.h"
    )
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/mnt/Mount2/DeveloperSurface/area51-pc/Apps/GameApp/android/app/.cxx/RelWithDebInfo/4b5x6w5g/arm64-v8a/thirdparty/Libwebm/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
