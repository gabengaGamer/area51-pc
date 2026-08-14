//==============================================================================
//  DebugMenuPageAdvCheckpoints.cpp
//
//  Copyright (c) 2002-2003 Inevitable Entertainment Inc. All rights reserved.
//
//  This is the implementation for the Debug menu Advance Checkpoints page.
//  
//==============================================================================

#include "DebugMenu2.hpp"
//#include "SaveMgr/SaveMgr.hpp"
//#include "Objects/Player/Player.hpp"
//#include "StateMgr/mapList.hpp"
//#include "Ui/ui_manager.hpp"
//#include "Ui/ui_font.hpp"
//#include "Configuration/GameConfig.hpp"
//#include "PhysicsMgr/PhysicsMgr.hpp"

//==============================================================================

#if defined( ENABLE_DEBUG_MENU )

//==============================================================================
// Data
xbool   dummy[5];

//==============================================================================

debug_menu_page_adv_checkpoints::debug_menu_page_adv_checkpoints( ) : debug_menu_page()
{
    m_pTitle = "Advance Checkpoints";

    AddItemBool     ( "<Not Available>", dummy[0] );
    AddItemBool     ( "<Not Available>", dummy[1] );
    AddItemBool     ( "<Not Available>", dummy[2] );
    AddItemBool     ( "<Not Available>", dummy[3] );
    AddItemBool     ( "<Not Available>", dummy[4] );
}

#endif // defined( ENABLE_DEBUG_MENU )
