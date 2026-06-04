//=========================================================================
//
//  GamePad.hpp
//
//=========================================================================

#ifndef GAME_PAD_HPP
#define GAME_PAD_HPP

//=========================================================================
// INCLUDES
//=========================================================================

#include "Entropy.hpp"
#ifdef CONFIG_VIEWER
#include "../../Apps/ArtistViewer/Config.hpp"
#else
#include "../../Apps/GameApp/Config.hpp"
#endif

#include "../Menu/DebugMenuDefine.hpp"

#ifndef MAX_LOCAL_PLAYERS
#define MAX_LOCAL_PLAYERS 4
#endif

//=========================================================================
// DEFINES
//=========================================================================

enum input_context
{
    INGAME_CONTEXT     = (1 << 0),
    FRONTEND_CONTEXT   = (1 << 1),
#if defined( ENABLE_DEBUG_MENU )
    DEBUG_MENU_CONTEXT = (1 << 2),
#endif
    ALL_CONTEXTS       = INGAME_CONTEXT | FRONTEND_CONTEXT
#if defined( ENABLE_DEBUG_MENU )
                       | DEBUG_MENU_CONTEXT
#endif
};

//=========================================================================
// CLASSES
//=========================================================================

class ingame_pad : public input_action_map
{
public:

    typedef input_action_state logical;

    enum logical_id
    {
        ACTION_NULL = -1,

        GAMEPLAY_ACTION_FIRST = 0,
        MOVE_FORWARD = GAMEPLAY_ACTION_FIRST,
        MOVE_BACKWARD,
        STRAFE_LEFT,
        STRAFE_RIGHT,

        LOOK_HORIZONTAL,
        LOOK_VERTICAL,
        ACTION_JUMP,
        ACTION_CROUCH,
        ACTION_PRIMARY,
        ACTION_SECONDARY,
        ACTION_RELOAD,
        ACTION_MUTATION,
        ACTION_CYCLE_RIGHT,
        ACTION_USE,
        ACTION_FLASHLIGHT,

        ACTION_SPEAK_FOLLOW_STAY,
        ACTION_SPEAK_USE_ACTIVATE,
        ACTION_SPEAK_COVER_ME,
        ACTION_SPEAK_ATTACK_COVER,

        ACTION_THROW_GRENADE,
        ACTION_MELEE_ATTACK,
        ACTION_CYCLE_LEFT,

        ACTION_VOTE_MENU_ON,
        ACTION_VOTE_MENU_OFF,
        ACTION_VOTE_YES,
        ACTION_VOTE_NO,
        ACTION_VOTE_ABSTAIN,

        ACTION_CHAT,

        LEAN_LEFT,
        LEAN_RIGHT,

        ACTION_TALK_MODE_TOGGLE,
        ACTION_FIRE_PARASITES,
        ACTION_FIRE_CONTAGION,
        ACTION_MUTANT_MELEE,

        ACTION_MP_FLASHLIGHT,
        ACTION_MP_MUTATE,
        ACTION_DROP_FLAG,
        ACTION_PAUSE_CONTEXT,

        GAMEPLAY_ACTION_END,

        FRONTEND_ACTION_FIRST = GAMEPLAY_ACTION_END,
        UI_UP = FRONTEND_ACTION_FIRST,
        UI_DOWN,
        UI_LEFT,
        UI_RIGHT,
        UI_SELECT,
        UI_BACK,
        UI_DELETE,
        UI_ACTIVATE,
        UI_SHOULDER_L,
        UI_SHOULDER_R,
        UI_SHOULDER_L2,
        UI_SHOULDER_R2,
        UI_HELP,

        FRONTEND_ACTION_END,

#if defined( ENABLE_DEBUG_MENU )
        DEBUG_MENU_ACTION_FIRST = FRONTEND_ACTION_END,
        DEBUG_MENU_NEXT_PAGE = DEBUG_MENU_ACTION_FIRST,
        DEBUG_MENU_PREV_PAGE,
        DEBUG_MENU_NEXT_ITEM,
        DEBUG_MENU_PREV_ITEM,
        DEBUG_MENU_INCREMENT,
        DEBUG_MENU_DECREMENT,
        DEBUG_MENU_ACTION,
        DEBUG_MENU_EXIT_MODIFIER,
        DEBUG_MENU_EXIT,

        DEBUG_MENU_ACTION_END,
        MAX_ACTION = DEBUG_MENU_ACTION_END
#else
        MAX_ACTION = FRONTEND_ACTION_END
#endif
    };

                        ingame_pad      ( void );

    logical&            GetLogical      ( s32 I );
    const logical&      GetLogical      ( s32 I ) const;

    virtual void        SampleFrame     ( f32 DeltaTime );

    static  const char* GetLogicalIDName    ( s32 Index );
    static  const char* GetLogicalIDEnum    ( void );
    static  logical_id  GetLogicalIDByName  ( const char* pName );
    static  input_gadget GetInputPromptGadget( const xwchar* pToken );

protected:

    void                OnInitialize    ( void );
    xbool               ShouldPollInput ( void ) const;
    virtual void        OnActionValue   ( s32 ActionID, f32 Value );
};

//=========================================================================
// GLOBAL STATE
//=========================================================================

extern ingame_pad g_IngamePad[ MAX_LOCAL_PLAYERS ];

//=========================================================================
#endif // GAME_PAD_HPP
//=========================================================================
