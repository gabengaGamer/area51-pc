# Native Qt bitmap viewer target.
a51_add_project( XBMPViewer EXECUTABLE
    WIN32
    SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/XBMPViewer.qrc"
        "${CMAKE_CURRENT_LIST_DIR}/XBMPViewer.ui"
        "${CMAKE_CURRENT_LIST_DIR}/XBMPViewer.h"
        "${CMAKE_CURRENT_LIST_DIR}/AboutDialog.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/BitmapConvertService.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/BitmapPreviewWidget.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ConfigManager.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ConvertSettingsDialog.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ConvertUtils.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ExplorerPanel.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/FileListModel.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/FileListView.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/FullscreenPreviewDialog.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/PreviewPanel.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/SettingsDialog.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ThemeManager.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/XBMPViewer.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/main.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/FileListModel.h"
        "${CMAKE_CURRENT_LIST_DIR}/PreviewPanel.h"
        "${CMAKE_CURRENT_LIST_DIR}/BitmapPreviewWidget.h"
        "${CMAKE_CURRENT_LIST_DIR}/ExplorerPanel.h"
        "${CMAKE_CURRENT_LIST_DIR}/SettingsDialog.h"
        "${CMAKE_CURRENT_LIST_DIR}/ConvertSettingsDialog.h"
        "${CMAKE_CURRENT_LIST_DIR}/AboutDialog.h"
        "${CMAKE_CURRENT_LIST_DIR}/BitmapConvertService.h"
        "${CMAKE_CURRENT_LIST_DIR}/ConfigManager.h"
        "${CMAKE_CURRENT_LIST_DIR}/FullscreenPreviewDialog.h"
        "${CMAKE_CURRENT_LIST_DIR}/ConvertUtils.h"
        "${CMAKE_CURRENT_LIST_DIR}/FileListView.h"
        "${CMAKE_CURRENT_LIST_DIR}/ThemeManager.h"
        "${CMAKE_CURRENT_LIST_DIR}/XBMPViewerConfig.h"
    DEPENDENCIES
        aux_Bitmap
        x_files
)

if( A51_ENABLE_QT AND Qt6_FOUND )
    # Qt AUTOGEN currently emits `$<CONFIG:EDITOR-Debug>`, which is rejected
    # by the active CMake generator. Keep the configuration name and invoke
    # the Qt tools explicitly instead.
    set( _a51_qt_moc_headers
        XBMPViewer.h
        FileListModel.h
        PreviewPanel.h
        BitmapPreviewWidget.h
        ExplorerPanel.h
        SettingsDialog.h
        ConvertSettingsDialog.h
        AboutDialog.h
        FullscreenPreviewDialog.h
    )

    set( _a51_qt_moc_sources )
    foreach( _header IN LISTS _a51_qt_moc_headers )
        get_filename_component( _header_name "${_header}" NAME_WE )
        set( _moc_source "${CMAKE_CURRENT_BINARY_DIR}/moc_${_header_name}.cpp" )
        add_custom_command( OUTPUT "${_moc_source}"
            COMMAND Qt6::moc
                -I"${CMAKE_CURRENT_LIST_DIR}"
                -I"${CMAKE_SOURCE_DIR}"
                "${CMAKE_CURRENT_LIST_DIR}/${_header}"
                -o "${_moc_source}"
            DEPENDS "${CMAKE_CURRENT_LIST_DIR}/${_header}"
            VERBATIM
        )
        list( APPEND _a51_qt_moc_sources "${_moc_source}" )
    endforeach()

    set( _a51_qt_ui_header "${CMAKE_CURRENT_BINARY_DIR}/ui_XBMPViewer.h" )
    add_custom_command( OUTPUT "${_a51_qt_ui_header}"
        COMMAND Qt6::uic
            "${CMAKE_CURRENT_LIST_DIR}/XBMPViewer.ui"
            -o "${_a51_qt_ui_header}"
        DEPENDS "${CMAKE_CURRENT_LIST_DIR}/XBMPViewer.ui"
        VERBATIM
    )

    qt_add_resources( _a51_qt_resources XBMPViewer.qrc )

    target_sources( XBMPViewer PRIVATE
        ${_a51_qt_moc_sources}
        "${_a51_qt_ui_header}"
        ${_a51_qt_resources}
    )
    target_include_directories( XBMPViewer PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" )
    target_link_libraries( XBMPViewer PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets )
else()
    message( STATUS "Qt6 was not found; XBMPViewer remains configured but cannot be built until Qt6 is provided." )
endif()
