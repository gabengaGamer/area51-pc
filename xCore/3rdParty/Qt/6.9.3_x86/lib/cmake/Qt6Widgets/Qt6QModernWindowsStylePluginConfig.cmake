# Copyright (C) 2024 The Qt Company Ltd.
# SPDX-License-Identifier: BSD-3-Clause


####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was QtPluginConfig.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
  endif()
endmacro()

macro(check_required_components _NAME)
  foreach(comp ${${_NAME}_FIND_COMPONENTS})
    if(NOT ${_NAME}_${comp}_FOUND)
      if(${_NAME}_FIND_REQUIRED_${comp})
        set(${_NAME}_FOUND FALSE)
      endif()
    endif()
  endforeach()
endmacro()

####################################################################################

cmake_minimum_required(VERSION 3.16...3.21)

include(CMakeFindDependencyMacro)

# Do the checks inside Targets.cmake even when the file is still being generated
include("${CMAKE_CURRENT_LIST_DIR}/Qt6QModernWindowsStylePluginTargetsPrecheck.cmake")

if(NOT __qt_QModernWindowsStylePlugin_skip_include_targets_file)
    # Find required dependencies, if any.
    if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/Qt6QModernWindowsStylePluginDependencies.cmake")
        include("${CMAKE_CURRENT_LIST_DIR}/Qt6QModernWindowsStylePluginDependencies.cmake")
    else()
        set(QModernWindowsStylePlugin_FOUND TRUE)
    endif()

    if(QModernWindowsStylePlugin_FOUND)
        include("${CMAKE_CURRENT_LIST_DIR}/Qt6QModernWindowsStylePluginTargets.cmake")
        include("${CMAKE_CURRENT_LIST_DIR}/Qt6QModernWindowsStylePluginAdditionalTargetInfo.cmake")
        set(__qt_QModernWindowsStylePlugin_targets_file_included ON)
    endif()
endif()
