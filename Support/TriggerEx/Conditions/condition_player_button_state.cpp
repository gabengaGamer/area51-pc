///////////////////////////////////////////////////////////////////////////////
//
//  condition_player_button_state.cpp
//
///////////////////////////////////////////////////////////////////////////////

//=========================================================================
//  INCLUDES
//=========================================================================

#include "condition_player_button_state.hpp"
#include "..\xcore\auxiliary\MiscUtils\Property.hpp"
#include "..\MiscUtils\SimpleUtils.hpp"
#include "..\Support\Objects\Player.hpp"
#include "..\triggerex_object.hpp"

//=========================================================================
// CLASS FUNCTIONS
//=========================================================================

condition_player_button_state::condition_player_button_state( conditional_affecter* pParent ):  conditional_ex_base(  pParent ),
m_Code(BUTTON_CODE_PRESSED),
m_ButtonID(ingame_pad::ACTION_USE)
{
}

//=============================================================================

xbool condition_player_button_state::Execute( guid TriggerGuid )
{    
    (void) TriggerGuid;
    
    player* pPlayer = SMP_UTIL_GetActivePlayer();
    if ( !pPlayer )
        return FALSE;

    const s32 PlayerPad = pPlayer->GetActivePlayerPad();

    if( (PlayerPad < 0) || (PlayerPad >= MAX_LOCAL_PLAYERS) )
        return FALSE;

    if( (m_ButtonID < ingame_pad::GAMEPLAY_ACTION_FIRST) || (m_ButtonID >= ingame_pad::GAMEPLAY_ACTION_END) )
        return FALSE;

    f32 WasVal = g_IngamePad[ PlayerPad ].GetLogical( m_ButtonID ).GetWasValue();
    f32 IsVal  = g_IngamePad[ PlayerPad ].GetLogical( m_ButtonID ).GetIsValue();
    xbool WasPressed = WasVal > 0.25f;
    xbool IsPressed  = IsVal  > 0.25f;

    switch ( m_Code )
    {
    case BUTTON_CODE_PRESSED:  
        return ( WasPressed || IsPressed );
        break;
    case BUTTON_CODE_NOT_PRESSED: 
        return !( WasPressed || IsPressed );
        break;
    default:
        ASSERT(0);
        break;
    }

    return FALSE;
}    

//=============================================================================

void condition_player_button_state::OnEnumProp ( prop_enum& rPropList )
{ 
    rPropList.PropEnumEnum( "Logical Input", ingame_pad::GetLogicalIDEnum(), "Logical mapping to watch.", 0 ) ;
    rPropList.PropEnumEnum( "Operation",     "Pressed\0NOT Pressed\0",       "Logic available to this condtional.", PROP_TYPE_MUST_ENUM );

    conditional_ex_base::OnEnumProp( rPropList );
}

//=============================================================================

xbool condition_player_button_state::OnProperty ( prop_query& rPropQuery )
{
    if( rPropQuery.IsVar( "Logical Input" ) )
    {
        if ( rPropQuery.IsRead() )
        {
            ingame_pad::logical_id ButtonID = m_ButtonID;

            if( (ButtonID < ingame_pad::GAMEPLAY_ACTION_FIRST) || (ButtonID >= ingame_pad::GAMEPLAY_ACTION_END) )
                ButtonID = ingame_pad::ACTION_USE;

            rPropQuery.SetVarEnum( ingame_pad::GetLogicalIDName( ButtonID ) );
        }
        else
        {
            ingame_pad::logical_id ButtonID = ingame_pad::GetLogicalIDByName( rPropQuery.GetVarEnum() );

            if( ButtonID != ingame_pad::ACTION_NULL )
                m_ButtonID = ButtonID;
        }
        return TRUE;
    }

    if ( rPropQuery.VarInt ( "Code" , m_Code ) )
        return TRUE;

    if ( rPropQuery.IsVar( "Operation") )
    {
        if( rPropQuery.IsRead() )
        {
            switch ( m_Code )
            {
            case BUTTON_CODE_PRESSED:      rPropQuery.SetVarEnum( "Pressed" );     break;
            case BUTTON_CODE_NOT_PRESSED:  rPropQuery.SetVarEnum( "NOT Pressed" ); break;
            default:
                ASSERT(0);
                break;
            }
            
            return( TRUE );
        }
        else
        {
            const char* pString = rPropQuery.GetVarEnum();
            
            if( x_stricmp( pString, "Pressed" )==0)      { m_Code = BUTTON_CODE_PRESSED;}
            if( x_stricmp( pString, "NOT Pressed" )==0)  { m_Code = BUTTON_CODE_NOT_PRESSED;}
            
            return( TRUE );
        }
    }

    if( conditional_ex_base::OnProperty( rPropQuery ) )
        return TRUE;

    return FALSE;
}

//=============================================================================

const char* condition_player_button_state::GetDescription( void )
{
    static big_string   Info;
    ingame_pad::logical_id ButtonID = m_ButtonID;

    if( (ButtonID < ingame_pad::GAMEPLAY_ACTION_FIRST) || (ButtonID >= ingame_pad::GAMEPLAY_ACTION_END) )
        ButtonID = ingame_pad::ACTION_USE;

    switch ( m_Code )
    {
    case BUTTON_CODE_PRESSED:   
        Info.Set(xfs("%s Pressed.", ingame_pad::GetLogicalIDName(ButtonID) ) );
        break;
    case BUTTON_CODE_NOT_PRESSED: 
        Info.Set(xfs("%s NOT Pressed.", ingame_pad::GetLogicalIDName(ButtonID) ) );
        break;
    default:
        ASSERT(0);
        break;
    }

    return Info.Get();
}
