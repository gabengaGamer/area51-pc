//=========================================================================
//
//  GamePad.cpp
//
//=========================================================================

// TODO: GS: Implement ini loading + key mapping in settings

//=========================================================================
//  INCLUDES
//=========================================================================

#include "GamePad.hpp"

//=========================================================================
//  GLOBAL STATE
//=========================================================================

ingame_pad g_IngamePad[ MAX_LOCAL_PLAYERS ];

#if CONFIG_IS_DEMO && !defined( X_EDITOR )
extern xtimer g_DemoIdleTimer;
#endif

//=========================================================================
//  STRUCTS
//=========================================================================

struct logical_definition
{
    ingame_pad::logical_id  LogicalID;
    const char*             pName;
};

//-------------------------------------------------------------------------

struct mapping_definition
{
    ingame_pad::logical_id  LogicalID;
    input_gadget            GadgetID;
    f32                     Scale;
    u32                     ContextMask;
};

//-------------------------------------------------------------------------

struct prompt_definition
{
    const char*             pToken;
    ingame_pad::logical_id  LogicalID;
};

//=========================================================================
//  LOGICAL DEFINITIONS
//=========================================================================

static
const logical_definition s_GameplayLogicals[] =
{
    { ingame_pad::MOVE_FORWARD,                "Move Forward" },
    { ingame_pad::MOVE_BACKWARD,               "Move Backward" },
    { ingame_pad::STRAFE_LEFT,                 "Strafe Left" },
    { ingame_pad::STRAFE_RIGHT,                "Strafe Right" },
    { ingame_pad::LOOK_HORIZONTAL,             "Horizontal Look" },
    { ingame_pad::LOOK_VERTICAL,               "Vertical Look" },
    { ingame_pad::LEAN_LEFT,                   "Lean Left" },
    { ingame_pad::LEAN_RIGHT,                  "Lean Right" },

    { ingame_pad::ACTION_RELOAD,               "Reload" },
    { ingame_pad::ACTION_PRIMARY,              "Primary Fire" },
    { ingame_pad::ACTION_SECONDARY,            "Secondary Fire" },
    { ingame_pad::ACTION_JUMP,                 "Jump" },
    { ingame_pad::ACTION_CROUCH,               "Crouch" },
    { ingame_pad::ACTION_MUTATION,             "Toggle Mutation" },
    { ingame_pad::ACTION_FIRE_PARASITES,       "Fire Parasites" },
    { ingame_pad::ACTION_FIRE_CONTAGION,       "Fire Contagion" },
    { ingame_pad::ACTION_MUTANT_MELEE,         "Mutant Melee" },
    { ingame_pad::ACTION_CYCLE_RIGHT,          "Cycle Weapons Right" },
    { ingame_pad::ACTION_CYCLE_LEFT,           "Cycle Weapons Left" },
    { ingame_pad::ACTION_USE,                  "Use Object" },
    { ingame_pad::ACTION_FLASHLIGHT,           "Toggle Flashlight" },
    { ingame_pad::ACTION_THROW_GRENADE,        "Throw Grenade" },
    { ingame_pad::ACTION_MELEE_ATTACK,         "Melee Attack" },

    { ingame_pad::ACTION_SPEAK_FOLLOW_STAY,    "Speak: Follow Me" },
    { ingame_pad::ACTION_SPEAK_USE_ACTIVATE,   "Speak: Use / Activate" },
    { ingame_pad::ACTION_SPEAK_COVER_ME,       "Speak: Cover Me" },
    { ingame_pad::ACTION_SPEAK_ATTACK_COVER,   "Speak: Attack / Take Cover" },

    { ingame_pad::ACTION_VOTE_MENU_ON,         "Vote: Menu On" },
    { ingame_pad::ACTION_VOTE_MENU_OFF,        "Vote: Menu Off" },
    { ingame_pad::ACTION_VOTE_YES,             "Vote: Yes" },
    { ingame_pad::ACTION_VOTE_NO,              "Vote: No" },
    { ingame_pad::ACTION_VOTE_ABSTAIN,         "Vote: Abstain" },

    { ingame_pad::ACTION_CHAT,                 "Voice Chat" },
    { ingame_pad::ACTION_TALK_MODE_TOGGLE,     "Talk: Mode Toggle" },
    { ingame_pad::ACTION_MP_FLASHLIGHT,        "Multiplayer Toggle Flashlight" },
    { ingame_pad::ACTION_MP_MUTATE,            "Multiplayer Toggle Mutation" },
    { ingame_pad::ACTION_DROP_FLAG,            "Drop Flag" },

    { ingame_pad::ACTION_PAUSE_CONTEXT,        "Pause" },
};

//-------------------------------------------------------------------------

static
const logical_definition s_FrontendLogicals[] =
{
    { ingame_pad::UI_UP,                       "UI Up" },
    { ingame_pad::UI_DOWN,                     "UI Down" },
    { ingame_pad::UI_LEFT,                     "UI Left" },
    { ingame_pad::UI_RIGHT,                    "UI Right" },
    { ingame_pad::UI_SELECT,                   "UI Select" },
    { ingame_pad::UI_BACK,                     "UI Back" },
    { ingame_pad::UI_DELETE,                   "UI Delete" },
    { ingame_pad::UI_ACTIVATE,                 "UI Activate" },
    { ingame_pad::UI_SHOULDER_L,               "UI Shoulder Left" },
    { ingame_pad::UI_SHOULDER_R,               "UI Shoulder Right" },
    { ingame_pad::UI_SHOULDER_L2,              "UI Shoulder Left 2" },
    { ingame_pad::UI_SHOULDER_R2,              "UI Shoulder Right 2" },
    { ingame_pad::UI_HELP,                     "UI Help" },
};

//-------------------------------------------------------------------------

#if defined( ENABLE_DEBUG_MENU )
static
const logical_definition s_DebugMenuLogicals[] =
{
    { ingame_pad::DEBUG_MENU_NEXT_PAGE,        "Debug Menu Next Page" },
    { ingame_pad::DEBUG_MENU_PREV_PAGE,        "Debug Menu Previous Page" },
    { ingame_pad::DEBUG_MENU_NEXT_ITEM,        "Debug Menu Next Item" },
    { ingame_pad::DEBUG_MENU_PREV_ITEM,        "Debug Menu Previous Item" },
    { ingame_pad::DEBUG_MENU_INCREMENT,        "Debug Menu Increment" },
    { ingame_pad::DEBUG_MENU_DECREMENT,        "Debug Menu Decrement" },
    { ingame_pad::DEBUG_MENU_ACTION,           "Debug Menu Action" },
    { ingame_pad::DEBUG_MENU_EXIT_MODIFIER,    "Debug Menu Exit Modifier" },
    { ingame_pad::DEBUG_MENU_EXIT,             "Debug Menu Exit" },
};
#endif // defined( ENABLE_DEBUG_MENU )

//=========================================================================
//  MAPPING DEFINITIONS
//=========================================================================

static
const mapping_definition s_GameplayMappings[] =
{
    { ingame_pad::MOVE_FORWARD,                          INPUT_XBOX_STICK_LEFT_Y,   1.0f,   INGAME_CONTEXT },
    { ingame_pad::MOVE_BACKWARD,                         INPUT_XBOX_STICK_LEFT_Y,   -1.0f,  INGAME_CONTEXT },
    { ingame_pad::STRAFE_LEFT,                           INPUT_XBOX_STICK_LEFT_X,   -1.0f,  INGAME_CONTEXT },
    { ingame_pad::STRAFE_RIGHT,                          INPUT_XBOX_STICK_LEFT_X,   1.0f,   INGAME_CONTEXT },
    { ingame_pad::LOOK_HORIZONTAL,                       INPUT_XBOX_STICK_RIGHT_X,  1.0f,   INGAME_CONTEXT },
    { ingame_pad::LOOK_VERTICAL,                         INPUT_XBOX_STICK_RIGHT_Y,  -1.0f,  INGAME_CONTEXT },
    { ingame_pad::ACTION_JUMP,                           INPUT_XBOX_BTN_A,          1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_CROUCH,                         INPUT_XBOX_BTN_L_STICK,    1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_PRIMARY,                        INPUT_XBOX_R_TRIGGER,      1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_SECONDARY,                      INPUT_XBOX_L_TRIGGER,      1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_RELOAD,                         INPUT_XBOX_BTN_X,          1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_MUTATION,                       INPUT_XBOX_BTN_UP,         1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_CYCLE_RIGHT,                    INPUT_XBOX_BTN_Y,          1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_USE,                            INPUT_XBOX_BTN_X,          1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_FLASHLIGHT,                     INPUT_XBOX_BTN_DOWN,       1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_THROW_GRENADE,                  INPUT_XBOX_BTN_B,          1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_MELEE_ATTACK,                   INPUT_XBOX_BTN_R_STICK,    1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_CYCLE_LEFT,                     INPUT_XBOX_BTN_BLACK,      1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_VOTE_MENU_ON,                   INPUT_XBOX_BTN_DOWN,       1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_VOTE_MENU_OFF,                  INPUT_XBOX_BTN_DOWN,       1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_VOTE_YES,                       INPUT_XBOX_BTN_RIGHT,      1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_VOTE_NO,                        INPUT_XBOX_BTN_LEFT,       1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_VOTE_ABSTAIN,                   INPUT_XBOX_BTN_UP,         1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_CHAT,                           INPUT_XBOX_BTN_WHITE,      1.0f,   INGAME_CONTEXT },
    { ingame_pad::LEAN_LEFT,                             INPUT_XBOX_BTN_LEFT,       1.0f,   INGAME_CONTEXT },
    { ingame_pad::LEAN_RIGHT,                            INPUT_XBOX_BTN_RIGHT,      1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_TALK_MODE_TOGGLE,               INPUT_XBOX_BTN_WHITE,      1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_FIRE_PARASITES,                 INPUT_XBOX_R_TRIGGER,      1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_FIRE_CONTAGION,                 INPUT_XBOX_L_TRIGGER,      1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_MUTANT_MELEE,                   INPUT_XBOX_BTN_R_STICK,    1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_MP_FLASHLIGHT,                  INPUT_XBOX_BTN_DOWN,       1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_MP_MUTATE,                      INPUT_XBOX_BTN_UP,         1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_DROP_FLAG,                      INPUT_XBOX_BTN_DOWN,       1.0f,   INGAME_CONTEXT },

    { ingame_pad::MOVE_FORWARD,                          INPUT_KBD_W,               1.0f,   INGAME_CONTEXT },
    { ingame_pad::MOVE_BACKWARD,                         INPUT_KBD_S,               1.0f,   INGAME_CONTEXT },
    { ingame_pad::STRAFE_LEFT,                           INPUT_KBD_A,               1.0f,   INGAME_CONTEXT },
    { ingame_pad::STRAFE_RIGHT,                          INPUT_KBD_D,               1.0f,   INGAME_CONTEXT },
    { ingame_pad::LOOK_HORIZONTAL,                       INPUT_MOUSE_X_REL,         1.0f,   INGAME_CONTEXT },
    { ingame_pad::LOOK_VERTICAL,                         INPUT_MOUSE_Y_REL,         1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_JUMP,                           INPUT_KBD_SPACE,           1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_CROUCH,                         INPUT_KBD_LCONTROL,        1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_PRIMARY,                        INPUT_MOUSE_BTN_L,         1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_SECONDARY,                      INPUT_MOUSE_BTN_R,         1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_RELOAD,                         INPUT_KBD_R,               1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_MUTATION,                       INPUT_KBD_X,               1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_CYCLE_RIGHT,                    INPUT_MOUSE_WHEEL_REL,     1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_USE,                            INPUT_KBD_TAB,             1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_FLASHLIGHT,                     INPUT_KBD_F,               1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_THROW_GRENADE,                  INPUT_KBD_G,               1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_MELEE_ATTACK,                   INPUT_KBD_V,               1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_CYCLE_LEFT,                     INPUT_MOUSE_WHEEL_REL,     -1.0f,  INGAME_CONTEXT },
    { ingame_pad::ACTION_VOTE_MENU_ON,                   INPUT_KBD_F1,              1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_VOTE_MENU_OFF,                  INPUT_KBD_F1,              1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_VOTE_YES,                       INPUT_KBD_F2,              1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_VOTE_NO,                        INPUT_KBD_F3,              1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_VOTE_ABSTAIN,                   INPUT_KBD_F4,              1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_CHAT,                           INPUT_KBD_T,               1.0f,   INGAME_CONTEXT },
    { ingame_pad::LEAN_LEFT,                             INPUT_KBD_Q,               1.0f,   INGAME_CONTEXT },
    { ingame_pad::LEAN_RIGHT,                            INPUT_KBD_E,               1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_TALK_MODE_TOGGLE,               INPUT_KBD_Y,               1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_FIRE_PARASITES,                 INPUT_MOUSE_BTN_L,         1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_FIRE_CONTAGION,                 INPUT_MOUSE_BTN_R,         1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_MUTANT_MELEE,                   INPUT_KBD_V,               1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_MP_FLASHLIGHT,                  INPUT_KBD_F,               1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_MP_MUTATE,                      INPUT_KBD_X,               1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_DROP_FLAG,                      INPUT_KBD_N,               1.0f,   INGAME_CONTEXT },

    { ingame_pad::MOVE_FORWARD,                          INPUT_PS2_STICK_LEFT_Y,    1.0f,   INGAME_CONTEXT },
    { ingame_pad::MOVE_BACKWARD,                         INPUT_PS2_STICK_LEFT_Y,    -1.0f,  INGAME_CONTEXT },
    { ingame_pad::STRAFE_LEFT,                           INPUT_PS2_STICK_LEFT_X,    -1.0f,  INGAME_CONTEXT },
    { ingame_pad::STRAFE_RIGHT,                          INPUT_PS2_STICK_LEFT_X,    1.0f,   INGAME_CONTEXT },
    { ingame_pad::LOOK_HORIZONTAL,                       INPUT_PS2_STICK_RIGHT_X,   1.0f,   INGAME_CONTEXT },
    { ingame_pad::LOOK_VERTICAL,                         INPUT_PS2_STICK_RIGHT_Y,   -1.0f,  INGAME_CONTEXT },
    { ingame_pad::ACTION_JUMP,                           INPUT_PS2_BTN_L1,          1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_CROUCH,                         INPUT_PS2_BTN_L2,          1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_PRIMARY,                        INPUT_PS2_BTN_R1,          1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_SECONDARY,                      INPUT_PS2_BTN_R2,          1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_RELOAD,                         INPUT_PS2_BTN_CROSS,       1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_MUTATION,                       INPUT_PS2_BTN_L_UP,        1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_CYCLE_RIGHT,                    INPUT_PS2_BTN_CIRCLE,      1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_USE,                            INPUT_PS2_BTN_CROSS,       1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_FLASHLIGHT,                     INPUT_PS2_BTN_L_STICK,     1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_SPEAK_USE_ACTIVATE,             INPUT_PS2_BTN_CROSS,       1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_THROW_GRENADE,                  INPUT_PS2_BTN_SQUARE,      1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_MELEE_ATTACK,                   INPUT_PS2_BTN_R_STICK,     1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_CYCLE_LEFT,                     INPUT_PS2_BTN_TRIANGLE,    1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_VOTE_MENU_ON,                   INPUT_PS2_BTN_L_DOWN,      1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_VOTE_MENU_OFF,                  INPUT_PS2_BTN_L_DOWN,      1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_VOTE_YES,                       INPUT_PS2_BTN_L_RIGHT,     1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_VOTE_NO,                        INPUT_PS2_BTN_L_LEFT,      1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_VOTE_ABSTAIN,                   INPUT_PS2_BTN_L_UP,        1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_CHAT,                           INPUT_PS2_BTN_L_STICK,     1.0f,   INGAME_CONTEXT },
    { ingame_pad::LEAN_LEFT,                             INPUT_PS2_BTN_L_LEFT,      1.0f,   INGAME_CONTEXT },
    { ingame_pad::LEAN_RIGHT,                            INPUT_PS2_BTN_L_RIGHT,     1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_TALK_MODE_TOGGLE,               INPUT_PS2_BTN_SELECT,      1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_FIRE_PARASITES,                 INPUT_PS2_BTN_R1,          1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_FIRE_CONTAGION,                 INPUT_PS2_BTN_R2,          1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_MUTANT_MELEE,                   INPUT_PS2_BTN_R_STICK,     1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_MP_FLASHLIGHT,                  INPUT_PS2_BTN_L_STICK,     1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_MP_MUTATE,                      INPUT_PS2_BTN_L_UP,        1.0f,   INGAME_CONTEXT },
    { ingame_pad::ACTION_DROP_FLAG,                      INPUT_PS2_BTN_L_DOWN,      1.0f,   INGAME_CONTEXT },
};

//-------------------------------------------------------------------------

static
const mapping_definition s_SystemMappings[] =
{
    { ingame_pad::ACTION_PAUSE_CONTEXT,                  INPUT_XBOX_BTN_START,      1.0f,   ALL_CONTEXTS },
    { ingame_pad::ACTION_PAUSE_CONTEXT,                  INPUT_KBD_ESCAPE,          1.0f,   ALL_CONTEXTS },
    { ingame_pad::ACTION_PAUSE_CONTEXT,                  INPUT_PS2_BTN_START,       1.0f,   ALL_CONTEXTS },

#if defined( ENABLE_DEBUG_MENU )
    { ingame_pad::DEBUG_MENU_EXIT_MODIFIER,              INPUT_XBOX_BTN_BACK,       1.0f,   INGAME_CONTEXT | DEBUG_MENU_CONTEXT },
    { ingame_pad::DEBUG_MENU_EXIT,                       INPUT_XBOX_BTN_START,      1.0f,   INGAME_CONTEXT | DEBUG_MENU_CONTEXT },
    { ingame_pad::DEBUG_MENU_EXIT_MODIFIER,              INPUT_KBD_TAB,             1.0f,   INGAME_CONTEXT | DEBUG_MENU_CONTEXT },
    { ingame_pad::DEBUG_MENU_EXIT,                       INPUT_KBD_RETURN,          1.0f,   INGAME_CONTEXT | DEBUG_MENU_CONTEXT },
    { ingame_pad::DEBUG_MENU_EXIT_MODIFIER,              INPUT_PS2_BTN_SELECT,      1.0f,   INGAME_CONTEXT | DEBUG_MENU_CONTEXT },
    { ingame_pad::DEBUG_MENU_EXIT,                       INPUT_PS2_BTN_START,       1.0f,   INGAME_CONTEXT | DEBUG_MENU_CONTEXT },
#endif
};

//-------------------------------------------------------------------------

static
const mapping_definition s_FrontendMappings[] =
{
    { ingame_pad::UI_UP,                                 INPUT_XBOX_BTN_UP,         1.0f,   FRONTEND_CONTEXT },
    { ingame_pad::UI_DOWN,                               INPUT_XBOX_BTN_DOWN,       1.0f,   FRONTEND_CONTEXT },
    { ingame_pad::UI_LEFT,                               INPUT_XBOX_BTN_LEFT,       1.0f,   FRONTEND_CONTEXT },
    { ingame_pad::UI_RIGHT,                              INPUT_XBOX_BTN_RIGHT,      1.0f,   FRONTEND_CONTEXT },
    { ingame_pad::UI_UP,                                 INPUT_XBOX_STICK_LEFT_Y,   1.0f,   FRONTEND_CONTEXT },
    { ingame_pad::UI_DOWN,                               INPUT_XBOX_STICK_LEFT_Y,   -1.0f,  FRONTEND_CONTEXT },
    { ingame_pad::UI_LEFT,                               INPUT_XBOX_STICK_LEFT_X,   -1.0f,  FRONTEND_CONTEXT },
    { ingame_pad::UI_RIGHT,                              INPUT_XBOX_STICK_LEFT_X,   1.0f,   FRONTEND_CONTEXT },
    { ingame_pad::UI_SELECT,                             INPUT_XBOX_BTN_A,          1.0f,   FRONTEND_CONTEXT },
    { ingame_pad::UI_BACK,                               INPUT_XBOX_BTN_B,          1.0f,   FRONTEND_CONTEXT },
    { ingame_pad::UI_BACK,                               INPUT_XBOX_BTN_BACK,       1.0f,   FRONTEND_CONTEXT },
    { ingame_pad::UI_DELETE,                             INPUT_XBOX_BTN_X,          1.0f,   FRONTEND_CONTEXT },
    { ingame_pad::UI_ACTIVATE,                           INPUT_XBOX_BTN_Y,          1.0f,   FRONTEND_CONTEXT },
    { ingame_pad::UI_SHOULDER_L,                         INPUT_XBOX_BTN_WHITE,      1.0f,   FRONTEND_CONTEXT },
    { ingame_pad::UI_SHOULDER_R,                         INPUT_XBOX_BTN_BLACK,      1.0f,   FRONTEND_CONTEXT },
    { ingame_pad::UI_SHOULDER_L2,                        INPUT_XBOX_L_TRIGGER,      1.0f,   FRONTEND_CONTEXT },
    { ingame_pad::UI_SHOULDER_R2,                        INPUT_XBOX_R_TRIGGER,      1.0f,   FRONTEND_CONTEXT },
    { ingame_pad::UI_HELP,                               INPUT_XBOX_BTN_START,      1.0f,   FRONTEND_CONTEXT },

    { ingame_pad::UI_UP,                                 INPUT_KBD_UP,              1.0f,   FRONTEND_CONTEXT },
    { ingame_pad::UI_DOWN,                               INPUT_KBD_DOWN,            1.0f,   FRONTEND_CONTEXT },
    { ingame_pad::UI_LEFT,                               INPUT_KBD_LEFT,            1.0f,   FRONTEND_CONTEXT },
    { ingame_pad::UI_RIGHT,                              INPUT_KBD_RIGHT,           1.0f,   FRONTEND_CONTEXT },
    { ingame_pad::UI_SELECT,                             INPUT_KBD_RETURN,          1.0f,   FRONTEND_CONTEXT },
    { ingame_pad::UI_BACK,                               INPUT_KBD_ESCAPE,          1.0f,   FRONTEND_CONTEXT },
    { ingame_pad::UI_DELETE,                             INPUT_KBD_DELETE,          1.0f,   FRONTEND_CONTEXT },
    { ingame_pad::UI_DELETE,                             INPUT_KBD_BACK,            1.0f,   FRONTEND_CONTEXT },
    { ingame_pad::UI_ACTIVATE,                           INPUT_KBD_R,               1.0f,   FRONTEND_CONTEXT },
    { ingame_pad::UI_HELP,                               INPUT_KBD_RETURN,          1.0f,   FRONTEND_CONTEXT },
    { ingame_pad::UI_UP,                                 INPUT_PS2_BTN_L_UP,        1.0f,   FRONTEND_CONTEXT },
    { ingame_pad::UI_DOWN,                               INPUT_PS2_BTN_L_DOWN,      1.0f,   FRONTEND_CONTEXT },
    { ingame_pad::UI_LEFT,                               INPUT_PS2_BTN_L_LEFT,      1.0f,   FRONTEND_CONTEXT },
    { ingame_pad::UI_RIGHT,                              INPUT_PS2_BTN_L_RIGHT,     1.0f,   FRONTEND_CONTEXT },
    { ingame_pad::UI_UP,                                 INPUT_PS2_STICK_LEFT_Y,    1.0f,   FRONTEND_CONTEXT },
    { ingame_pad::UI_DOWN,                               INPUT_PS2_STICK_LEFT_Y,    -1.0f,  FRONTEND_CONTEXT },
    { ingame_pad::UI_LEFT,                               INPUT_PS2_STICK_LEFT_X,    -1.0f,  FRONTEND_CONTEXT },
    { ingame_pad::UI_RIGHT,                              INPUT_PS2_STICK_LEFT_X,    1.0f,   FRONTEND_CONTEXT },
    { ingame_pad::UI_SELECT,                             INPUT_PS2_BTN_CROSS,       1.0f,   FRONTEND_CONTEXT },
    { ingame_pad::UI_BACK,                               INPUT_PS2_BTN_SQUARE,      1.0f,   FRONTEND_CONTEXT },
    { ingame_pad::UI_DELETE,                             INPUT_PS2_BTN_CIRCLE,      1.0f,   FRONTEND_CONTEXT },
    { ingame_pad::UI_ACTIVATE,                           INPUT_PS2_BTN_TRIANGLE,    1.0f,   FRONTEND_CONTEXT },
    { ingame_pad::UI_SHOULDER_L,                         INPUT_PS2_BTN_L1,          1.0f,   FRONTEND_CONTEXT },
    { ingame_pad::UI_SHOULDER_R,                         INPUT_PS2_BTN_R1,          1.0f,   FRONTEND_CONTEXT },
    { ingame_pad::UI_SHOULDER_L2,                        INPUT_PS2_BTN_L2,          1.0f,   FRONTEND_CONTEXT },
    { ingame_pad::UI_SHOULDER_R2,                        INPUT_PS2_BTN_R2,          1.0f,   FRONTEND_CONTEXT },
    { ingame_pad::UI_HELP,                               INPUT_PS2_BTN_START,       1.0f,   FRONTEND_CONTEXT },
};

//-------------------------------------------------------------------------

#if defined( ENABLE_DEBUG_MENU )
static
const mapping_definition s_DebugMenuMappings[] =
{
    { ingame_pad::DEBUG_MENU_NEXT_PAGE,                  INPUT_XBOX_BTN_BLACK,      1.0f,   DEBUG_MENU_CONTEXT },
    { ingame_pad::DEBUG_MENU_NEXT_PAGE,                  INPUT_XBOX_R_TRIGGER,      1.0f,   DEBUG_MENU_CONTEXT },
    { ingame_pad::DEBUG_MENU_PREV_PAGE,                  INPUT_XBOX_BTN_WHITE,      1.0f,   DEBUG_MENU_CONTEXT },
    { ingame_pad::DEBUG_MENU_PREV_PAGE,                  INPUT_XBOX_L_TRIGGER,      1.0f,   DEBUG_MENU_CONTEXT },
    { ingame_pad::DEBUG_MENU_NEXT_ITEM,                  INPUT_XBOX_BTN_DOWN,       1.0f,   DEBUG_MENU_CONTEXT },
    { ingame_pad::DEBUG_MENU_NEXT_ITEM,                  INPUT_XBOX_STICK_LEFT_Y,  -1.0f,   DEBUG_MENU_CONTEXT },
    { ingame_pad::DEBUG_MENU_PREV_ITEM,                  INPUT_XBOX_BTN_UP,         1.0f,   DEBUG_MENU_CONTEXT },
    { ingame_pad::DEBUG_MENU_PREV_ITEM,                  INPUT_XBOX_STICK_LEFT_Y,   1.0f,   DEBUG_MENU_CONTEXT },
    { ingame_pad::DEBUG_MENU_INCREMENT,                  INPUT_XBOX_BTN_RIGHT,      1.0f,   DEBUG_MENU_CONTEXT },
    { ingame_pad::DEBUG_MENU_INCREMENT,                  INPUT_XBOX_STICK_LEFT_X,   1.0f,   DEBUG_MENU_CONTEXT },
    { ingame_pad::DEBUG_MENU_DECREMENT,                  INPUT_XBOX_BTN_LEFT,       1.0f,   DEBUG_MENU_CONTEXT },
    { ingame_pad::DEBUG_MENU_DECREMENT,                  INPUT_XBOX_STICK_LEFT_X,  -1.0f,   DEBUG_MENU_CONTEXT },
    { ingame_pad::DEBUG_MENU_ACTION,                     INPUT_XBOX_BTN_A,          1.0f,   DEBUG_MENU_CONTEXT },
    { ingame_pad::DEBUG_MENU_ACTION,                     INPUT_XBOX_BTN_X,          1.0f,   DEBUG_MENU_CONTEXT },

    { ingame_pad::DEBUG_MENU_NEXT_ITEM,                  INPUT_KBD_DOWN,            1.0f,   DEBUG_MENU_CONTEXT },
    { ingame_pad::DEBUG_MENU_PREV_ITEM,                  INPUT_KBD_UP,              1.0f,   DEBUG_MENU_CONTEXT },
    { ingame_pad::DEBUG_MENU_INCREMENT,                  INPUT_KBD_RIGHT,           1.0f,   DEBUG_MENU_CONTEXT },
    { ingame_pad::DEBUG_MENU_DECREMENT,                  INPUT_KBD_LEFT,            1.0f,   DEBUG_MENU_CONTEXT },
    { ingame_pad::DEBUG_MENU_ACTION,                     INPUT_KBD_RETURN,          1.0f,   DEBUG_MENU_CONTEXT },
    { ingame_pad::DEBUG_MENU_ACTION,                     INPUT_KBD_DELETE,          1.0f,   DEBUG_MENU_CONTEXT },
    { ingame_pad::DEBUG_MENU_ACTION,                     INPUT_KBD_BACK,            1.0f,   DEBUG_MENU_CONTEXT },

    { ingame_pad::DEBUG_MENU_NEXT_PAGE,                  INPUT_PS2_BTN_R1,          1.0f,   DEBUG_MENU_CONTEXT },
    { ingame_pad::DEBUG_MENU_NEXT_PAGE,                  INPUT_PS2_BTN_R2,          1.0f,   DEBUG_MENU_CONTEXT },
    { ingame_pad::DEBUG_MENU_PREV_PAGE,                  INPUT_PS2_BTN_L1,          1.0f,   DEBUG_MENU_CONTEXT },
    { ingame_pad::DEBUG_MENU_PREV_PAGE,                  INPUT_PS2_BTN_L2,          1.0f,   DEBUG_MENU_CONTEXT },
    { ingame_pad::DEBUG_MENU_NEXT_ITEM,                  INPUT_PS2_BTN_L_DOWN,      1.0f,   DEBUG_MENU_CONTEXT },
    { ingame_pad::DEBUG_MENU_NEXT_ITEM,                  INPUT_PS2_STICK_LEFT_Y,   -1.0f,   DEBUG_MENU_CONTEXT },
    { ingame_pad::DEBUG_MENU_PREV_ITEM,                  INPUT_PS2_BTN_L_UP,        1.0f,   DEBUG_MENU_CONTEXT },
    { ingame_pad::DEBUG_MENU_PREV_ITEM,                  INPUT_PS2_STICK_LEFT_Y,    1.0f,   DEBUG_MENU_CONTEXT },
    { ingame_pad::DEBUG_MENU_INCREMENT,                  INPUT_PS2_BTN_L_RIGHT,     1.0f,   DEBUG_MENU_CONTEXT },
    { ingame_pad::DEBUG_MENU_INCREMENT,                  INPUT_PS2_STICK_LEFT_X,    1.0f,   DEBUG_MENU_CONTEXT },
    { ingame_pad::DEBUG_MENU_DECREMENT,                  INPUT_PS2_BTN_L_LEFT,      1.0f,   DEBUG_MENU_CONTEXT },
    { ingame_pad::DEBUG_MENU_DECREMENT,                  INPUT_PS2_STICK_LEFT_X,   -1.0f,   DEBUG_MENU_CONTEXT },
    { ingame_pad::DEBUG_MENU_ACTION,                     INPUT_PS2_BTN_CROSS,       1.0f,   DEBUG_MENU_CONTEXT },
    { ingame_pad::DEBUG_MENU_ACTION,                     INPUT_PS2_BTN_CIRCLE,      1.0f,   DEBUG_MENU_CONTEXT },
};
#endif // defined( ENABLE_DEBUG_MENU )

//=========================================================================
//  PROMPT DEFINITIONS
//=========================================================================

static
const prompt_definition s_PromptDefinitions[] =
{
    { "x",          ingame_pad::UI_SELECT             },
    { "q",          ingame_pad::UI_DELETE             },
    { "a",          ingame_pad::UI_BACK               },
    { "o",          ingame_pad::UI_ACTIVATE           },
    { "X",          ingame_pad::UI_DELETE             },
    { "Y",          ingame_pad::UI_ACTIVATE           },
    { "A",          ingame_pad::UI_SELECT             },
    { "B",          ingame_pad::UI_BACK               },
    { "S",          ingame_pad::UI_HELP               },
    { "SQUARE",     ingame_pad::UI_DELETE             },
    { "CROSS",      ingame_pad::UI_SELECT             },
    { "TRIANGLE",   ingame_pad::UI_ACTIVATE           },
    { "CIRCLE",     ingame_pad::UI_BACK               },
    { "DOWN",       ingame_pad::UI_DOWN               },
    { "LEFT",       ingame_pad::UI_LEFT               },
    { "UP",         ingame_pad::UI_UP                 },
    { "RIGHT",      ingame_pad::UI_RIGHT              },
    { "UPDOWN",     ingame_pad::UI_UP                 },
    { "LEFTRIGHT",  ingame_pad::UI_LEFT               },
    { "PAUSE",      ingame_pad::ACTION_PAUSE_CONTEXT  },
    { "START",      ingame_pad::UI_HELP               },
    { "SELECT",     ingame_pad::UI_SELECT             },
    { "BACK",       ingame_pad::UI_BACK               },
    { "DELETE",     ingame_pad::UI_DELETE             },
    { "ACTIVATE",   ingame_pad::UI_ACTIVATE           },
    { "GRENADE",    ingame_pad::ACTION_THROW_GRENADE  },
    { "RELOAD",     ingame_pad::ACTION_RELOAD         },
    { "USE",        ingame_pad::ACTION_USE            },
    { "PREVWEAPON", ingame_pad::ACTION_CYCLE_LEFT     },
    { "NEXTWEAPON", ingame_pad::ACTION_CYCLE_RIGHT    },
    { "LEANLEFT",   ingame_pad::LEAN_LEFT             },
    { "LEANRIGHT",  ingame_pad::LEAN_RIGHT            },
    { "LEAN",       ingame_pad::LEAN_LEFT             },
    { "MUTATE",     ingame_pad::ACTION_MUTATION       },
    { "TRANSFORM",  ingame_pad::ACTION_MUTATION       },
    { "JUMP",       ingame_pad::ACTION_JUMP           },
    { "CROUCH",     ingame_pad::ACTION_CROUCH         },
    { "FLASHLIGHT", ingame_pad::ACTION_FLASHLIGHT     },
    { "FIRE",       ingame_pad::ACTION_PRIMARY        },
    { "SECONDARY",  ingame_pad::ACTION_SECONDARY      },
    { "MELEE",      ingame_pad::ACTION_MELEE_ATTACK   },
    { "PARASITE",   ingame_pad::ACTION_FIRE_PARASITES },
    { "CONTAGION",  ingame_pad::ACTION_FIRE_CONTAGION },
};

//=========================================================================
//  HELPER FUNCTIONS
//=========================================================================

template< class T, int Count >
static
s32 CountOf( const T (&)[Count] )
{
    return Count;
}

//=========================================================================

template< int Count >
static
void RegisterLogicalNames( ingame_pad& Pad, const logical_definition (&Definitions)[Count] )
{
    for( s32 i = 0; i < Count; i++ )
    {
        Pad.SetActionName( Definitions[i].LogicalID, Definitions[i].pName );
    }
}

//=========================================================================

static
input_action_value_mode GetMappingValueMode( ingame_pad::logical_id LogicalID, input_gadget GadgetID )
{
    if( input_system::GetGadgetValueKind( GadgetID ) != INPUT_VALUE_ABSOLUTE_AXIS )
        return INPUT_ACTION_VALUE_AUTO;

    if( (LogicalID == ingame_pad::LOOK_HORIZONTAL) ||
        (LogicalID == ingame_pad::LOOK_VERTICAL) )
    {
        return INPUT_ACTION_VALUE_SIGNED_AXIS;
    }

    return INPUT_ACTION_VALUE_POSITIVE_AXIS;
}

//=========================================================================

template< int Count >
static
void RegisterInputMappings( ingame_pad& Pad, const mapping_definition (&Definitions)[Count] )
{
    for( s32 i = 0; i < Count; i++ )
    {
        const mapping_definition& Mapping = Definitions[i];
        Pad.AddBinding( Mapping.LogicalID,
                        Mapping.GadgetID,
                        Mapping.Scale,
                        Mapping.ContextMask,
                        GetMappingValueMode( Mapping.LogicalID, Mapping.GadgetID ) );
    }
}

//=========================================================================

template< int Count >
static
const char* FindRegisteredLogicalName( const logical_definition (&Definitions)[Count], s32 LogicalID )
{
    for( s32 i = 0; i < Count; i++ )
    {
        if( Definitions[i].LogicalID == LogicalID )
            return Definitions[i].pName;
    }

    return NULL;
}

//=========================================================================

template< int Count >
static
ingame_pad::logical_id FindRegisteredLogicalID( const logical_definition (&Definitions)[Count], const char* pName )
{
    ASSERT( pName );

    if( !pName )
        return ingame_pad::ACTION_NULL;

    for( s32 i = 0; i < Count; i++ )
    {
        if( x_stricmp( pName, Definitions[i].pName ) == 0 )
            return Definitions[i].LogicalID;
    }

    return ingame_pad::ACTION_NULL;
}

//=========================================================================

static
xbool PromptTokenMatches( const xwchar* pToken, const char* pReference )
{
    ASSERT( pToken );
    ASSERT( pReference );

    if( !pToken || !pReference )
        return FALSE;

    while( *pReference )
    {
        if( (*pToken == 0) || (*pToken == 0xBB) )
            return FALSE;

        if( (char)*pToken != *pReference )
            return FALSE;

        pToken++;
        pReference++;
    }

    return( (*pToken == 0) || (*pToken == 0xBB) );
}

//=========================================================================

static
xbool IsPromptTokenComposite( const xwchar* pToken )
{
    if( !pToken )
        return FALSE;

    return(   PromptTokenMatches( pToken, "UPDOWN" )
           || PromptTokenMatches( pToken, "LEFTRIGHT" )
           || PromptTokenMatches( pToken, "LEAN" ) );
}

//=========================================================================

static
const prompt_definition* FindPromptToken( const xwchar* pToken )
{
    if( !pToken )
        return NULL;

    for( s32 i = 0; i < CountOf( s_PromptDefinitions ); i++ )
    {
        if( PromptTokenMatches( pToken, s_PromptDefinitions[i].pToken ) )
            return &s_PromptDefinitions[i];
    }

    return NULL;
}

//=========================================================================

template< int Count >
static
const mapping_definition* FindLogicalMapping( const mapping_definition (&Mappings)[Count], ingame_pad::logical_id LogicalID, input_platform Platform )
{
    for( s32 i = 0; i < Count; i++ )
    {
        if( (Mappings[i].LogicalID == LogicalID) &&
            (Mappings[i].GadgetID != INPUT_UNDEFINED) &&
            (input_system::GetGadgetPlatform( Mappings[i].GadgetID ) == Platform) )
        {
            return &Mappings[i];
        }
    }

    return NULL;
}

//=========================================================================

static
const mapping_definition* FindPromptLogicalMapping( ingame_pad::logical_id LogicalID, input_platform Platform )
{
    const mapping_definition* pMapping = FindLogicalMapping( s_FrontendMappings, LogicalID, Platform );
    if( pMapping )
        return pMapping;

    pMapping = FindLogicalMapping( s_SystemMappings, LogicalID, Platform );
    if( pMapping )
        return pMapping;

    return FindLogicalMapping( s_GameplayMappings, LogicalID, Platform );
}

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

ingame_pad::ingame_pad( void )
{
    SetActiveContext( INGAME_CONTEXT );
    OnInitialize();
}

//=========================================================================

ingame_pad::logical& ingame_pad::GetLogical( s32 I )
{
    return GetActionState( I );
}

//=========================================================================

const ingame_pad::logical& ingame_pad::GetLogical( s32 I ) const
{
    return GetActionState( I );
}

//=========================================================================

xbool ingame_pad::ShouldPollInput( void ) const
{
    if( !HasDeviceID() )
    {
        u32 UnassignedContexts = FRONTEND_CONTEXT;
#if defined( ENABLE_DEBUG_MENU )
        UnassignedContexts |= DEBUG_MENU_CONTEXT;
#endif

        if( (GetActiveContext() & UnassignedContexts) == 0 )
            return FALSE;
    }

    return TRUE;
}

//=========================================================================

void ingame_pad::SampleFrame( f32 DeltaTime )
{
    if( !ShouldPollInput() )
    {
        ClearAllActions();
        return;
    }

    input_action_map::SampleDevice( DeltaTime, GetResolvedDeviceID( 0 ) );
}

//=========================================================================

void ingame_pad::OnActionValue( s32 ActionID, f32 Value )
{
    (void)ActionID;

#if CONFIG_IS_DEMO && !defined( X_EDITOR )
    if( x_abs( Value ) > 0.2f )
    {
        g_DemoIdleTimer.Trip();
    }
#else
    (void)Value;
#endif
}

//=========================================================================

void ingame_pad::OnInitialize( void )
{
    ASSERT( CountOf( s_GameplayLogicals ) == (GAMEPLAY_ACTION_END - GAMEPLAY_ACTION_FIRST) );
    ASSERT( CountOf( s_FrontendLogicals ) == (FRONTEND_ACTION_END - FRONTEND_ACTION_FIRST) );
#if defined( ENABLE_DEBUG_MENU )
    ASSERT( CountOf( s_DebugMenuLogicals ) == (DEBUG_MENU_ACTION_END - DEBUG_MENU_ACTION_FIRST) );
#endif

    SetActionCount( MAX_ACTION );

    RegisterLogicalNames( *this, s_GameplayLogicals );
    RegisterLogicalNames( *this, s_FrontendLogicals );
#if defined( ENABLE_DEBUG_MENU )
    RegisterLogicalNames( *this, s_DebugMenuLogicals );
#endif

    RegisterInputMappings( *this, s_GameplayMappings );
    RegisterInputMappings( *this, s_SystemMappings );
    RegisterInputMappings( *this, s_FrontendMappings );
#if defined( ENABLE_DEBUG_MENU )
    RegisterInputMappings( *this, s_DebugMenuMappings );
#endif
}

//=========================================================================

const char* ingame_pad::GetLogicalIDName( s32 Index )
{
    const char* pName = FindRegisteredLogicalName( s_GameplayLogicals, Index );
    if( pName )
        return pName;

    pName = FindRegisteredLogicalName( s_FrontendLogicals, Index );
    if( pName )
        return pName;

#if defined( ENABLE_DEBUG_MENU )
    pName = FindRegisteredLogicalName( s_DebugMenuLogicals, Index );
    if( pName )
        return pName;
#endif

    ASSERTS( 0, "Unknown logical input id" );
    return "";
}

//===========================================================================

const char* ingame_pad::GetLogicalIDEnum( void )
{
    static xarray<char> s_Enum;

    if( s_Enum.GetCount() )
        return s_Enum.GetPtr();

    for( s32 i = 0; i < CountOf( s_GameplayLogicals ); i++ )
    {
        const char* pName = s_GameplayLogicals[i].pName;
        const s32 Length  = x_strlen( pName );

        for( s32 j = 0; j <= Length; j++ )
        {
            s_Enum.Append( pName[j] );
        }
    }

    s_Enum.Append( 0 );
    return s_Enum.GetPtr();
}

//=========================================================================

ingame_pad::logical_id ingame_pad::GetLogicalIDByName( const char* pName )
{
    return FindRegisteredLogicalID( s_GameplayLogicals, pName );
}

//=========================================================================

input_gadget ingame_pad::GetInputPromptGadget( const xwchar* pToken )
{
    ASSERT( pToken );

    if( !pToken )
        return INPUT_UNDEFINED;

    const prompt_definition* pPrompt = FindPromptToken( pToken );
    if( !pPrompt || IsPromptTokenComposite( pToken ) )
        return INPUT_UNDEFINED;

    if( g_Input.GetCurrentInputDevice() != INPUT_DEVICE_GAMEPAD )
        return INPUT_UNDEFINED;

    input_platform Platform = g_Input.GetCurrentInputPlatform();

    if( (Platform != INPUT_PLATFORM_XBOX) && (Platform != INPUT_PLATFORM_PS2) )
        return INPUT_UNDEFINED;

    const mapping_definition* pMapping = FindPromptLogicalMapping( pPrompt->LogicalID, Platform );
    return pMapping ? pMapping->GadgetID : INPUT_UNDEFINED;
}
