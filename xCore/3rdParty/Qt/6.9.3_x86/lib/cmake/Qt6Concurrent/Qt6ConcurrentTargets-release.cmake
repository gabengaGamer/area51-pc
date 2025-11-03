#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "Qt6::Concurrent" for configuration "Release"
set_property(TARGET Qt6::Concurrent APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(Qt6::Concurrent PROPERTIES
  IMPORTED_IMPLIB_RELEASE "${_IMPORT_PREFIX}/lib/Qt6Concurrent.lib"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/Qt6Concurrent.dll"
  )

list(APPEND _cmake_import_check_targets Qt6::Concurrent )
list(APPEND _cmake_import_check_files_for_Qt6::Concurrent "${_IMPORT_PREFIX}/lib/Qt6Concurrent.lib" "${_IMPORT_PREFIX}/bin/Qt6Concurrent.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
