//=========================================================================
//
//  PlayerInputState.cpp
//
//=========================================================================

#include "PlayerInput.hpp"

//=========================================================================

PlayerActionState::PlayerActionState( void ) :
    Value        ( 0.0f ),
    PressedValue ( 0.0f ),
    GamepadValue ( 0.0f ),
    GamepadPressedValue( 0.0f ),
    IsHeld       ( FALSE ),
    WasPressed   ( FALSE ),
    WasReleased  ( FALSE ),
    GamepadIsHeld( FALSE ),
    GamepadWasPressed( FALSE )
{
}

//=========================================================================

PlayerMoveInput::PlayerMoveInput( void ) :
    KeyboardForward ( 0.0f ),
    KeyboardStrafe  ( 0.0f ),
    GamepadForward  ( 0.0f ),
    GamepadStrafe   ( 0.0f )
{
}

//=========================================================================

PlayerLookInput::PlayerLookInput( void ) :
    MouseYaw     ( 0.0f ),
    MousePitch   ( 0.0f ),
    GamepadYaw   ( 0.0f ),
    GamepadPitch ( 0.0f )
{
}

//=========================================================================

PlayerInputState::PlayerInputState( void )
{
    Clear();
}

//=========================================================================

void PlayerInputState::Clear( void )
{
    Move = PlayerMoveInput();
    Look = PlayerLookInput();

    for( s32 i = 0; i < static_cast<s32>( PlayerAction::Count ); i++ )
        m_Actions[i] = PlayerActionState();
}

//=========================================================================

PlayerActionState const& PlayerInputState::GetAction( PlayerAction Action ) const
{
    s32 const Index = static_cast<s32>( Action );
    ASSERT( (Index >= 0) && (Index < static_cast<s32>( PlayerAction::Count )) );
    return m_Actions[((Index >= 0) && (Index < static_cast<s32>( PlayerAction::Count ))) ? Index : 0];
}

//=========================================================================

xbool PlayerInputState::IsHeld( PlayerAction Action ) const
{
    return GetAction( Action ).IsHeld;
}

//=========================================================================

xbool PlayerInputState::WasPressed( PlayerAction Action ) const
{
    return GetAction( Action ).WasPressed;
}

//=========================================================================

xbool PlayerInputState::WasReleased( PlayerAction Action ) const
{
    return GetAction( Action ).WasReleased;
}

//=========================================================================

PlayerInputStateBuilder::PlayerInputStateBuilder( void )
{
    Clear();
}

//=========================================================================

void PlayerInputStateBuilder::Clear( void )
{
    m_State.Clear();
}

//=========================================================================

void PlayerInputStateBuilder::SetAction( PlayerAction Action,
                                         const PlayerActionState& State )
{
    s32 const Index = static_cast<s32>( Action );
    ASSERT( (Index >= 0) && (Index < static_cast<s32>( PlayerAction::Count )) );
    if( (Index >= 0) && (Index < static_cast<s32>( PlayerAction::Count )) )
        m_State.m_Actions[Index] = State;
}

//=========================================================================

void PlayerInputStateBuilder::SetMove( const PlayerMoveInput& Move )
{
    m_State.Move = Move;
}

//=========================================================================

void PlayerInputStateBuilder::SetLook( const PlayerLookInput& Look )
{
    m_State.Look = Look;
}

//=========================================================================

PlayerInputState const& PlayerInputStateBuilder::GetState( void ) const
{
    return m_State;
}
