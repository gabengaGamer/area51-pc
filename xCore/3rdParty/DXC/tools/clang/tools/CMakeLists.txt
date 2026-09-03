add_subdirectory(driver)
# add_subdirectory(clang-format)    # HLSL Change
# add_subdirectory(clang-format-vs) # HLSL Change
# add_subdirectory(clang-fuzzer)    # HLSL Change

# add_subdirectory(c-index-test)    # HLSL Change
option(CLANG_BUILD_LIBCLANG "Build the optional libclang indexing library" OFF)
if(CLANG_BUILD_LIBCLANG)
  add_subdirectory(libclang)
endif()

if(0 AND CLANG_ENABLE_ARCMT)        # HLSL Change
  add_subdirectory(arcmt-test)
  add_subdirectory(c-arcmt-test)
endif()

if(CLANG_ENABLE_STATIC_ANALYZER)
  add_subdirectory(clang-check)
endif()

# We support checking out the clang-tools-extra repository into the 'extra'
# subdirectory. It contains tools developed as part of the Clang/LLVM project
# on top of the Clang tooling platform. We keep them in a separate repository
# to keep the primary Clang repository small and focused.
# It also may be included by LLVM_EXTERNAL_CLANG_TOOLS_EXTRA_SOURCE_DIR.
add_llvm_external_project(clang-tools-extra extra)

# HLSL Change Starts
add_subdirectory(dxcompiler)
add_subdirectory(dxclib)
add_subdirectory(dxc)
add_subdirectory(dxcvalidator)
add_subdirectory(dxildll)
add_subdirectory(dxa)
add_subdirectory(dxopt)
add_subdirectory(dxl)
add_subdirectory(dxr)
add_subdirectory(dxv)

# These targets can currently only be built on Windows.
if (MSVC)
add_subdirectory(d3dcomp)
add_subdirectory(dxrfallbackcompiler)
endif (MSVC)
# HLSL Change Ends
