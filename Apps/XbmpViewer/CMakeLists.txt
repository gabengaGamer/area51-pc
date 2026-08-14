set( _a51_xbmp_viewer_source_root "${CMAKE_CURRENT_LIST_DIR}/../XBMPViewer" )
a51_add_project( XBMPViewer EXECUTABLE
    WIN32
    SOURCE_ROOT "${_a51_xbmp_viewer_source_root}"
    SOURCES
        "${_a51_xbmp_viewer_source_root}/XBMPViewer.qrc"
        "${_a51_xbmp_viewer_source_root}/XBMPViewer.ui"
        "${_a51_xbmp_viewer_source_root}/XBMPViewer.h"
        "${_a51_xbmp_viewer_source_root}/AboutDialog.cpp"
        "${_a51_xbmp_viewer_source_root}/BitmapConvertService.cpp"
        "${_a51_xbmp_viewer_source_root}/BitmapPreviewWidget.cpp"
        "${_a51_xbmp_viewer_source_root}/ConfigManager.cpp"
        "${_a51_xbmp_viewer_source_root}/ConvertSettingsDialog.cpp"
        "${_a51_xbmp_viewer_source_root}/ConvertUtils.cpp"
        "${_a51_xbmp_viewer_source_root}/ExplorerPanel.cpp"
        "${_a51_xbmp_viewer_source_root}/FileListModel.cpp"
        "${_a51_xbmp_viewer_source_root}/FileListView.cpp"
        "${_a51_xbmp_viewer_source_root}/FullscreenPreviewDialog.cpp"
        "${_a51_xbmp_viewer_source_root}/PreviewPanel.cpp"
        "${_a51_xbmp_viewer_source_root}/SettingsDialog.cpp"
        "${_a51_xbmp_viewer_source_root}/ThemeManager.cpp"
        "${_a51_xbmp_viewer_source_root}/XBMPViewer.cpp"
        "${_a51_xbmp_viewer_source_root}/main.cpp"
        "${_a51_xbmp_viewer_source_root}/FileListModel.h"
        "${_a51_xbmp_viewer_source_root}/PreviewPanel.h"
        "${_a51_xbmp_viewer_source_root}/BitmapPreviewWidget.h"
        "${_a51_xbmp_viewer_source_root}/ExplorerPanel.h"
        "${_a51_xbmp_viewer_source_root}/SettingsDialog.h"
        "${_a51_xbmp_viewer_source_root}/ConvertSettingsDialog.h"
        "${_a51_xbmp_viewer_source_root}/AboutDialog.h"
        "${_a51_xbmp_viewer_source_root}/BitmapConvertService.h"
        "${_a51_xbmp_viewer_source_root}/ConfigManager.h"
        "${_a51_xbmp_viewer_source_root}/FullscreenPreviewDialog.h"
        "${_a51_xbmp_viewer_source_root}/ConvertUtils.h"
        "${_a51_xbmp_viewer_source_root}/FileListView.h"
        "${_a51_xbmp_viewer_source_root}/ThemeManager.h"
        "${_a51_xbmp_viewer_source_root}/XBMPViewerConfig.h"
    DEPENDENCIES
        aux_Bitmap
        x_files
)

if( A51_ENABLE_QT AND Qt6_FOUND )
    # Qt AUTOGEN does not support EDITOR-Debug.
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
                -I"${_a51_xbmp_viewer_source_root}"
                -I"${CMAKE_SOURCE_DIR}"
                "${_a51_xbmp_viewer_source_root}/${_header}"
                -o "${_moc_source}"
            DEPENDS "${_a51_xbmp_viewer_source_root}/${_header}"
            VERBATIM
        )
        list( APPEND _a51_qt_moc_sources "${_moc_source}" )
    endforeach()

    set( _a51_qt_ui_header "${CMAKE_CURRENT_BINARY_DIR}/ui_XBMPViewer.h" )
    add_custom_command( OUTPUT "${_a51_qt_ui_header}"
        COMMAND Qt6::uic
            "${_a51_xbmp_viewer_source_root}/XBMPViewer.ui"
            -o "${_a51_qt_ui_header}"
        DEPENDS "${_a51_xbmp_viewer_source_root}/XBMPViewer.ui"
        VERBATIM
    )

    qt_add_resources( _a51_qt_resources
        "${_a51_xbmp_viewer_source_root}/XBMPViewer.qrc"
    )

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
