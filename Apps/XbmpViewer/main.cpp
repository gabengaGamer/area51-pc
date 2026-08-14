//==============================================================================
//  
//  main.cpp
//  
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "XBMPViewerConfig.h"
#include "XBMPViewer.h"

#include <QApplication>

//==============================================================================
//  MAIN APPLICATION ENTRY POINT
//==============================================================================

s32 main(s32 argc, char** argv)
{
    x_Init(argc, argv);

    QApplication App(argc, argv);
    App.setApplicationName("XBMPViewer");

    XBMPViewer Window;
    Window.resize(1280, 720);
    Window.show();

    const s32 Result = App.exec();

    x_Kill();

    return Result;
}