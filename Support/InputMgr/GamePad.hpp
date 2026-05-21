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

#include "InputMgr.hpp"
#ifndef MAX_LOCAL_PLAYERS
#define MAX_LOCAL_PLAYERS 4
#endif

//=========================================================================
// CLASSES
//=========================================================================

class ingame_pad : public input_pad
{
public:

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
        MAX_ACTION = FRONTEND_ACTION_END
    };

public:

                        ingame_pad      ( void );
    virtual void        OnUpdate        ( f32 DeltaTime );
    virtual void        OnInitialize    ( void );

    static  const char* GetLogicalIDName    ( s32 Index );
    static  const char* GetLogicalIDEnum    ( void );
    static  logical_id  GetLogicalIDByName  ( const char* pName );

protected:

    virtual xbool       IsPausePressed  ( void ) const;
};

//=========================================================================
// GLOBAL VARS
//=========================================================================

extern ingame_pad g_IngamePad[ MAX_LOCAL_PLAYERS ];

//=========================================================================
#endif // GAME_PAD_HPP
//=========================================================================
