# Copyright (C) 2024 The Qt Company Ltd.
# SPDX-License-Identifier: BSD-3-Clause

include_guard(DIRECTORY)


# Distributions should probably change this default.
if(NOT DEFINED QT_SKIP_AUTO_PLUGIN_INCLUSION)
    set(QT_SKIP_AUTO_PLUGIN_INCLUSION OFF)
endif()

if(__qt_Sql_targets_file_included AND NOT QT_SKIP_AUTO_PLUGIN_INCLUSION)
    __qt_internal_include_plugin_packages(Sql)
endif()
