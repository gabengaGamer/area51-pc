# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/mnt/Mount2/DeveloperSurface/area51-pc/xCore/3rdParty/Libvpx")
  file(MAKE_DIRECTORY "/mnt/Mount2/DeveloperSurface/area51-pc/xCore/3rdParty/Libvpx")
endif()
file(MAKE_DIRECTORY
  "/mnt/Mount2/DeveloperSurface/area51-pc/Apps/GameApp/android/app/.cxx/RelWithDebInfo/4b5x6w5g/arm64-v8a/thirdparty/Libvpx"
  "/mnt/Mount2/DeveloperSurface/area51-pc/Apps/GameApp/android/app/.cxx/RelWithDebInfo/4b5x6w5g/arm64-v8a/a51_libvpx_external-prefix"
  "/mnt/Mount2/DeveloperSurface/area51-pc/Apps/GameApp/android/app/.cxx/RelWithDebInfo/4b5x6w5g/arm64-v8a/a51_libvpx_external-prefix/tmp"
  "/mnt/Mount2/DeveloperSurface/area51-pc/Apps/GameApp/android/app/.cxx/RelWithDebInfo/4b5x6w5g/arm64-v8a/a51_libvpx_external-prefix/src/a51_libvpx_external-stamp"
  "/mnt/Mount2/DeveloperSurface/area51-pc/Apps/GameApp/android/app/.cxx/RelWithDebInfo/4b5x6w5g/arm64-v8a/a51_libvpx_external-prefix/src"
  "/mnt/Mount2/DeveloperSurface/area51-pc/Apps/GameApp/android/app/.cxx/RelWithDebInfo/4b5x6w5g/arm64-v8a/a51_libvpx_external-prefix/src/a51_libvpx_external-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/mnt/Mount2/DeveloperSurface/area51-pc/Apps/GameApp/android/app/.cxx/RelWithDebInfo/4b5x6w5g/arm64-v8a/a51_libvpx_external-prefix/src/a51_libvpx_external-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/mnt/Mount2/DeveloperSurface/area51-pc/Apps/GameApp/android/app/.cxx/RelWithDebInfo/4b5x6w5g/arm64-v8a/a51_libvpx_external-prefix/src/a51_libvpx_external-stamp${cfgdir}") # cfgdir has leading slash
endif()
