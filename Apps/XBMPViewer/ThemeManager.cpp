//==============================================================================
//
//  ThemeManager.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "ThemeManager.h"

#include <QApplication>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

void ThemeManager::ApplyTheme(const QString& ThemeType)
{
    if (ThemeType == "Fusion Dark")
    {
        QApplication::setStyle("Fusion");
        QPalette Palette;
        Palette.setColor(QPalette::Window, QColor(53, 53, 53));
        Palette.setColor(QPalette::WindowText, Qt::white);
        Palette.setColor(QPalette::Base, QColor(35, 35, 35));
        Palette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
        Palette.setColor(QPalette::ToolTipBase, Qt::white);
        Palette.setColor(QPalette::ToolTipText, Qt::white);
        Palette.setColor(QPalette::Text, Qt::white);
        Palette.setColor(QPalette::Button, QColor(53, 53, 53));
        Palette.setColor(QPalette::ButtonText, Qt::white);
        Palette.setColor(QPalette::BrightText, Qt::red);
        Palette.setColor(QPalette::Link, QColor(42, 130, 218));
        Palette.setColor(QPalette::Highlight, QColor(42, 130, 218));
        Palette.setColor(QPalette::HighlightedText, Qt::black);
        //Palette.setColor(QPalette::Disabled, QPalette::Text, QColor(127, 127, 127));
        //Palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(127, 127, 127));		
        QApplication::setPalette(Palette);
        qApp->setStyleSheet("");
        return;
    }

    if (ThemeType == "Fusion White")
    {
        QApplication::setStyle("Fusion");
        QPalette Palette;
        Palette.setColor(QPalette::Window, Qt::white);
        Palette.setColor(QPalette::WindowText, Qt::black);
        Palette.setColor(QPalette::Base, Qt::white);
        Palette.setColor(QPalette::AlternateBase, QColor(245, 245, 245));
        Palette.setColor(QPalette::ToolTipBase, Qt::white);
        Palette.setColor(QPalette::ToolTipText, Qt::black);
        Palette.setColor(QPalette::Text, Qt::black);
        Palette.setColor(QPalette::Button, QColor(240, 240, 240));
        Palette.setColor(QPalette::ButtonText, Qt::black);
        Palette.setColor(QPalette::BrightText, Qt::red);
        Palette.setColor(QPalette::Link, QColor(0, 0, 255));
        Palette.setColor(QPalette::Highlight, QColor(0, 120, 215));
        Palette.setColor(QPalette::HighlightedText, Qt::white);
        //Palette.setColor(QPalette::Disabled, QPalette::Text, QColor(127, 127, 127));
        //Palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(127, 127, 127));		
        QApplication::setPalette(Palette);
        qApp->setStyleSheet("");
        return;
    }

    if (ThemeType == "Windows Dark")
    {
        QApplication::setStyle("windows");
        QPalette Palette;
        Palette.setColor(QPalette::Window, QColor(22, 32, 32));
        Palette.setColor(QPalette::WindowText, Qt::white);
        Palette.setColor(QPalette::Base, QColor(25, 25, 25));
        Palette.setColor(QPalette::AlternateBase, QColor(42, 42, 42));
        Palette.setColor(QPalette::ToolTipBase, QColor(53, 53, 53));
        Palette.setColor(QPalette::ToolTipText, Qt::white);
        Palette.setColor(QPalette::Text, Qt::white);
        Palette.setColor(QPalette::Button, QColor(45, 45, 45));
        Palette.setColor(QPalette::ButtonText, Qt::white);
        Palette.setColor(QPalette::BrightText, Qt::red);
        Palette.setColor(QPalette::Link, QColor(42, 130, 218));
        Palette.setColor(QPalette::Highlight, QColor(42, 130, 218));
        Palette.setColor(QPalette::HighlightedText, Qt::white);
        //Palette.setColor(QPalette::Disabled, QPalette::Text, QColor(127, 127, 127));
        //Palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(127, 127, 127));
        QApplication::setPalette(Palette);
        qApp->setStyleSheet("");
        return;
    }

    if (ThemeType == "Windows White" || ThemeType.isEmpty())
    {
        QApplication::setStyle("windows");
        QPalette Palette;
        Palette.setColor(QPalette::Window, QColor(240, 240, 240));
        Palette.setColor(QPalette::WindowText, Qt::black);
        Palette.setColor(QPalette::Base, Qt::white);
        Palette.setColor(QPalette::AlternateBase, QColor(233, 233, 233));
        Palette.setColor(QPalette::ToolTipBase, QColor(255, 255, 220));
        Palette.setColor(QPalette::ToolTipText, Qt::black);
        Palette.setColor(QPalette::Text, Qt::black);
        Palette.setColor(QPalette::Button, QColor(240, 240, 240));
        Palette.setColor(QPalette::ButtonText, Qt::black);
        Palette.setColor(QPalette::BrightText, Qt::red);
        Palette.setColor(QPalette::Link, QColor(0, 0, 255));
        Palette.setColor(QPalette::Highlight, QColor(0, 120, 215));
        Palette.setColor(QPalette::HighlightedText, Qt::white);
        //Palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(160, 160, 160));
        //Palette.setColor(QPalette::Disabled, QPalette::Text, QColor(160, 160, 160));
        //Palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(160, 160, 160));		
        QApplication::setPalette(Palette);
        qApp->setStyleSheet("");
        return;
    }

    if (ThemeType == "VGUI")
    {
        QApplication::setStyle("windows");      
        QPalette Palette;
        Palette.setColor(QPalette::Window, QColor(48, 62, 48));           
        Palette.setColor(QPalette::WindowText, QColor(200, 220, 180));    
        Palette.setColor(QPalette::Base, QColor(35, 48, 38));             
        Palette.setColor(QPalette::AlternateBase, QColor(42, 55, 45));    
        Palette.setColor(QPalette::Text, QColor(200, 220, 180));          
        Palette.setColor(QPalette::Button, QColor(58, 72, 58));           
        Palette.setColor(QPalette::ButtonText, QColor(200, 220, 180));    
        Palette.setColor(QPalette::Highlight, QColor(90, 140, 90));       
        Palette.setColor(QPalette::HighlightedText, QColor(255, 255, 220));
        Palette.setColor(QPalette::ToolTipBase, QColor(70, 90, 70));
        Palette.setColor(QPalette::ToolTipText, QColor(220, 240, 200));
        Palette.setColor(QPalette::BrightText, QColor(255, 240, 120));
        Palette.setColor(QPalette::Link, QColor(130, 190, 130));
        //Palette.setColor(QPalette::LinkVisited, QColor(100, 160, 100));
        //Palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(100, 120, 100));
        //Palette.setColor(QPalette::Disabled, QPalette::Text, QColor(100, 120, 100));
        //Palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(100, 120, 100));        
        QApplication::setPalette(Palette);
        qApp->setStyleSheet(
            "QMenuBar { background-color: #303E30; color: #C8DCB4; }"
            "QMenuBar::item:selected { background-color: #5A8C5A; }"
            "QMenu { background-color: #303E30; color: #C8DCB4; border: 1px solid #486848; }"
            "QMenu::item:selected { background-color: #5A8C5A; }"
            "QToolTip { background-color: #465A46; color: #DCF0C8; border: 1px solid #304830; }"
        );
        return;
    }
}
