//=========================================================================
//
//  PlayerRotation.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "Player.hpp"
#include "Objects//NewWeapon.hpp"
#include "StateMgr//StateMgr.hpp"

//=========================================================================
//  DEFINES
//=========================================================================

static const f32 s_DistanceAtR25 = 700.0f;

tweak_handle LookSpring_ReturnSpeedTweak( "LookSpring_ReturnSpeed" );

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

void player::UpdateRotation( const f32& rDeltaTime )
{
    X_PROFILE_SCOPE_CATEGORY( "Context", "player::UpdateRotation" );

    radian OldPitch = m_Pitch;
    radian OldYaw   = m_Yaw;

    f32 fAimYawOffset = R_0;

    // make sure we load in tweaks in case they've changed
    LoadAimAssistTweaks();

    CalculatePitchLimits( rDeltaTime );
    UpdateAimAssistance ( rDeltaTime );
    UpdateAimOffset     ( rDeltaTime );

    PlayerLookSample LookSample = m_LookSample;
    ApplyZoomLookModifiers( LookSample );

    if( rDeltaTime < F32_MIN )
    {
        ASSERT(0);
    }

    if( IsAimAssistInputActive() && (m_AimAssistData.TargetGuid != 0) )
    {
        static const f32 s_AimAssistTune  = 1.06f;

        fAimYawOffset   = m_YawAimOffset
            * m_MoveInput.GamepadStrafe
            * rDeltaTime 
            * ( -s_DistanceAtR25 / m_AimAssistData.LOFPtDist )
            //* ( -s_DistanceAtR25 / m_DistanceToAimAssistTarget ) 
            * m_AimAssistData.TurnDampeningT //m_AimAssistPct 
            * s_AimAssistTune;
    }

    PlayerLookRotation LookResult;
    m_Look.EvaluateRotation( LookSample,
                             rDeltaTime,
                             m_fCurrentYawAimModifier,
                             m_fCurrentPitchAimModifier,
                             LookResult );

    m_Yaw   += LookResult.YawDelta;
    m_Pitch += LookResult.PitchDelta;

    m_Yaw += fAimYawOffset;
    m_Pitch  = MIN( m_PitchMax, MAX( m_PitchMin , m_Pitch ) );

    if ( m_bInTurret )
    {
        // Make sure we're within the turret's boundaries
        object* pObj;
        f32 MinAngleDiff = x_MinAngleDiff( m_Yaw, OldYaw );
        vector3 EyePos( m_Turret.AnchorL2W.GetTranslation() );
        EyePos.GetY() += 150.0f;

        // Left
        if (   (MinAngleDiff > 0.0f) // rotating left
            && (pObj = g_ObjMgr.GetObjectByGuid( m_Turret.LeftBoundaryGuid )) )
        {
            const vector3 Pos( pObj->GetPosition() );
            const vector3 ToPos( Pos - EyePos );
            const f32 LeftYawBound = ToPos.GetYaw();

            if ( (x_MinAngleDiff( m_Yaw, LeftYawBound )    >  0.0f)      // Yaw is to the left of the bound
                && (x_MinAngleDiff( LeftYawBound, OldYaw ) >= 0.0f) )    // OldYaw is to the right of the bound
            {
                // We've just crossed the boundary
                m_Yaw = LeftYawBound;
            }
        }

        // Right
        if (   (MinAngleDiff < 0.0f) // rotating right
            && (pObj = g_ObjMgr.GetObjectByGuid( m_Turret.RightBoundaryGuid )) )
        {
            const vector3 Pos( pObj->GetPosition() );
            const vector3 ToPos( Pos - EyePos );
            const f32 RightYawBound = ToPos.GetYaw();

            if ( (x_MinAngleDiff( m_Yaw, RightYawBound )   <  0.0f)      // Yaw is to the right of the bound
                && (x_MinAngleDiff( RightYawBound, OldYaw )<= 0.0f) )    // OldYaw is to the left of the bound
            {
                // We've just crossed the boundary
                m_Yaw = RightYawBound;
            }
        }

        MinAngleDiff = x_MinAngleDiff( m_Pitch, OldPitch );
        // Upper
        if (   (MinAngleDiff < 0.0f) // rotating up
            && (pObj = g_ObjMgr.GetObjectByGuid( m_Turret.UpperBoundaryGuid )) )
        {
            const vector3 Pos( pObj->GetPosition() );
            const vector3 ToPos( Pos - EyePos );
            const f32 UpperPitchBound = ToPos.GetPitch();

            if (   (x_MinAngleDiff( m_Pitch, UpperPitchBound )  <  0.0f)      // Pitch is above the bound
                && (x_MinAngleDiff( UpperPitchBound, OldPitch ) <= 0.0f) )    // OldPitch is below the bound
            {
                // We've just crossed the boundary
                m_Pitch = UpperPitchBound;
            }
        }

        // Lower
        if (   (MinAngleDiff > 0.0f) // rotating down
            && (pObj = g_ObjMgr.GetObjectByGuid( m_Turret.LowerBoundaryGuid )) )
        {
            const vector3 Pos( pObj->GetPosition() );
            const vector3 ToPos( Pos - EyePos );
            const f32 LowerPitchBound = ToPos.GetPitch();

            if (   (x_MinAngleDiff( m_Pitch, LowerPitchBound )  >  0.0f)      // Pitch is below the bound
                && (x_MinAngleDiff( LowerPitchBound, OldPitch ) >= 0.0f) )    // OldPitch is above the bound
            {
                // We've just crossed the boundary
                m_Pitch = LowerPitchBound;
            }
        }
    }

#ifndef X_EDITOR
    if( x_abs( m_Input.GetState().GetAction( PlayerAction::LookVertical ).Value ) <= 0.25f )
    {
        player_profile& Profile = g_StateMgr.GetActiveProfile( g_StateMgr.GetProfileListIndex( m_LocalSlot ) );

        // if lookspring is on, recenter vertically over time
        if( Profile.GetLookspringOn() )
        {
            f32 SpringSpeed = LookSpring_ReturnSpeedTweak.GetF32();
            m_Pitch -= (m_Pitch * SpringSpeed * rDeltaTime);
            m_Pitch  = MIN( m_PitchMax, MAX( m_PitchMin, m_Pitch ) );
        }
    }
#endif

    if( rDeltaTime > F32_MIN )
    {
        m_PitchRate = x_MinAngleDiff( m_Pitch, OldPitch ) / rDeltaTime;
        m_YawRate   = x_MinAngleDiff( m_Yaw,   OldYaw   ) / rDeltaTime;
    }
    else
    {
        m_PitchRate = 0.0f;
        m_YawRate   = 0.0f;
    }

    UpdateCameraShake( rDeltaTime );

    if( (m_Pitch != OldPitch) || 
        (m_Yaw   != OldYaw  ) )
    {
#ifndef X_EDITOR
        m_NetDirtyBits |= ORIENTATION_BIT;
#endif
    }
}

//===========================================================================
void player::CalculateRigOffset( f32 DeltaTime )
{
    ( void )DeltaTime;

    CalculateStrafeRigOffset( DeltaTime );
    CalculateMoveRigOffset( DeltaTime );

    // Figure out where to put the rig in relation to the camera.
    vector3 vDesiredOffsetStrafe( m_fCurrentStrafeRigOffset, 0.0f, 0.0f );
    vector3 vDesiredOffsetMove( 0.0f, 0.0f, m_fCurrentMoveRigOffset );

    vDesiredOffsetStrafe.RotateY( m_EyesYaw );
    vDesiredOffsetMove.RotateY( m_EyesYaw );

    m_vRigOffset = vDesiredOffsetStrafe + vDesiredOffsetMove;
}   

//===========================================================================

void player::CalculateStrafeRigOffset( f32 DeltaTime )
{
    xbool bMovingLeft = FALSE;
    xbool bMovingRight = FALSE;
    f32 PreviousStrafeOffset = m_fCurrentStrafeRigOffset;

    // The desired offsets for where the controller is currently placed.
    f32 DesiredStrafeOffset = m_fRigMaxStrafeOffset * m_fStrafeValue;

    new_weapon* pWeapon = GetCurrentWeaponPtr();
    if( pWeapon && pWeapon->IsZoomEnabled() )
        DesiredStrafeOffset = 0.0f;

    // Update the offsets.
    if ( DesiredStrafeOffset < m_fCurrentStrafeRigOffset )
    {
        m_fCurrentStrafeRigOffset -= ( m_fRigStrafeOffsetVelocity * DeltaTime );
        bMovingLeft = TRUE;
    }
    else
        if ( DesiredStrafeOffset > m_fCurrentStrafeRigOffset )
        {
            m_fCurrentStrafeRigOffset += ( m_fRigStrafeOffsetVelocity * DeltaTime );
            bMovingRight = TRUE;
        }


        if ( bMovingLeft )
        {
            m_fCurrentStrafeRigOffset = MAX( m_fCurrentStrafeRigOffset, DesiredStrafeOffset );
        }
        if ( bMovingRight )
        {
            m_fCurrentStrafeRigOffset = MIN( m_fCurrentStrafeRigOffset, DesiredStrafeOffset );
        }

        if ( DesiredStrafeOffset == 0.0f )
        {
            if ( PreviousStrafeOffset <= 0.0f && m_fCurrentStrafeRigOffset >= 0.0f )
            {
                m_fCurrentStrafeRigOffset = 0.0f;
            }
            if ( PreviousStrafeOffset >= 0.0f && m_fCurrentStrafeRigOffset <= 0.0f )
            {
                m_fCurrentStrafeRigOffset = 0.0f;
            }

        }

}

//===========================================================================

void player::CalculateMoveRigOffset( f32 DeltaTime )
{
    xbool bMovingForward = FALSE;
    xbool bMovingBackward = FALSE;
    f32 PreviousMoveOffset = m_fCurrentMoveRigOffset;

    // The desired offsets for where the controller is currently placed.
    f32 DesiredMoveOffset = m_fRigMaxMoveOffset * m_fMoveValue;

    new_weapon* pWeapon = GetCurrentWeaponPtr();
    if( pWeapon && pWeapon->IsZoomEnabled() )
        DesiredMoveOffset = 0.0f;

    // Update the offsets.
    if ( DesiredMoveOffset < m_fCurrentMoveRigOffset )
    {
        m_fCurrentMoveRigOffset -= ( m_fRigMoveOffsetVelocity * DeltaTime );
        bMovingForward = TRUE;
    }
    else
        if ( DesiredMoveOffset > m_fCurrentMoveRigOffset )
        {
            m_fCurrentMoveRigOffset += ( m_fRigMoveOffsetVelocity * DeltaTime );
            bMovingBackward = TRUE;
        }

        if ( bMovingForward )
        {
            m_fCurrentMoveRigOffset = MAX( m_fCurrentMoveRigOffset, DesiredMoveOffset );
        }
        else
            if ( bMovingBackward )
            {
                m_fCurrentMoveRigOffset = MIN( m_fCurrentMoveRigOffset, DesiredMoveOffset );
            }

            if ( DesiredMoveOffset == 0.0f )
            {
                if ( PreviousMoveOffset <= 0.0f && m_fCurrentMoveRigOffset >= 0.0f )
                {
                    m_fCurrentMoveRigOffset = 0.0f;
                }
            }
}

//===========================================================================

void player::CalculateLookHorozOffset( f32 DeltaTime ) // YAW
{
    xbool bLookingLeft = FALSE;
    xbool bLookingRight = FALSE;
    f32 PreviousLookOffset = m_CurrentHorozRigOffset;

    // The desired offsets for where the camera is currently placed.
    f32 const MaxYawRate = MAX( F32_MIN, m_Look.GetYawStickSensitivity() );
    f32 DesiredHorozOffset = m_RigLookMaxHorozOffset * (m_YawRate / MaxYawRate);
    DesiredHorozOffset = MAX( -m_RigLookMaxHorozOffset, MIN( m_RigLookMaxHorozOffset, DesiredHorozOffset ) );

    // Update the offsets.
    if ( DesiredHorozOffset < m_CurrentHorozRigOffset )
    {
        m_CurrentHorozRigOffset -= ( m_RigLookHorozVelocity * DeltaTime );
        bLookingLeft = TRUE;
    }
    else if ( DesiredHorozOffset > m_CurrentHorozRigOffset )
    {
        m_CurrentHorozRigOffset += ( m_RigLookHorozVelocity * DeltaTime );
        bLookingRight = TRUE;
    }

    if ( bLookingLeft )
    {
        m_CurrentHorozRigOffset = MAX( m_CurrentHorozRigOffset, DesiredHorozOffset );
    }
    else if ( bLookingRight )
    {
        m_CurrentHorozRigOffset = MIN( m_CurrentHorozRigOffset, DesiredHorozOffset );
    }

    if ( DesiredHorozOffset == 0.0f )
    {
        if ( PreviousLookOffset <= 0.0f && m_CurrentHorozRigOffset >= 0.0f )
        {
            m_CurrentHorozRigOffset = 0.0f;
        }
    }
}

//===========================================================================

void player::CalculateLookVertOffset( f32 DeltaTime ) // PITCH
{
    xbool bLookingUp = FALSE;
    xbool bLookingDown = FALSE;
    f32 PreviousLookOffset = m_CurrentVertRigOffset;

    // The desired offsets for where the camera is currently placed.
    f32 MaxPitchRate = MAX( F32_MIN, m_Look.GetPitchStickSensitivity() );
    f32 DesiredVertOffset = m_RigLookMaxVertOffset * (m_PitchRate / MaxPitchRate);
    DesiredVertOffset = MAX( -m_RigLookMaxVertOffset, MIN( m_RigLookMaxVertOffset, DesiredVertOffset ) );

    // Update the offsets.
    if ( DesiredVertOffset < m_CurrentVertRigOffset )
    {
        m_CurrentVertRigOffset -= ( m_RigLookVertVelocity * DeltaTime );
        bLookingUp = TRUE;
    }
    else
        if ( DesiredVertOffset > m_CurrentVertRigOffset )
        {
            m_CurrentVertRigOffset += ( m_RigLookVertVelocity * DeltaTime );
            bLookingDown = TRUE;
        }

        if ( bLookingUp )
        {
            m_CurrentVertRigOffset = MAX( m_CurrentVertRigOffset, DesiredVertOffset );
        }
        else
            if ( bLookingDown )
            {
                m_CurrentVertRigOffset = MIN( m_CurrentVertRigOffset, DesiredVertOffset );
            }

            if ( DesiredVertOffset == 0.0f )
            {
                if ( PreviousLookOffset <= 0.0f && m_CurrentVertRigOffset >= 0.0f )
                {
                    m_CurrentVertRigOffset = 0.0f;
                }
            }
}


void player::CalculatePitchLimits( const f32& rDeltaTime )
{
    m_PitchMin += 2.f;
    m_PitchMax += 2.f;
    m_DesiredPitchMin += 2.f;
    m_DesiredPitchMax += 2.f;

    //Handle changes to m_DesiredPitchMin
    if ( m_PitchMin < m_DesiredPitchMin )
    {
        m_PitchMin = MIN( m_PitchMin + m_fPitchChangeSpeed * rDeltaTime , m_DesiredPitchMin ); 
    }
    else
        if ( m_PitchMin > m_DesiredPitchMin )
        {
            m_PitchMin = MAX( m_PitchMin - m_fPitchChangeSpeed * rDeltaTime , m_DesiredPitchMin );         
        }

        //Handle changes to m_DesiredPitchMax
        if ( m_PitchMax < m_DesiredPitchMax )
        {
            m_PitchMax = MIN( m_PitchMax + m_fPitchChangeSpeed * rDeltaTime , m_DesiredPitchMax ); 
        }
        else
            if ( m_PitchMax > m_DesiredPitchMax )
            {
                m_PitchMax = MAX( m_PitchMax - m_fPitchChangeSpeed * rDeltaTime , m_DesiredPitchMax );         
            }

            m_PitchMin -= 2.f;
            m_PitchMax -= 2.f;

            m_DesiredPitchMin -= 2.f;
            m_DesiredPitchMax -= 2.f;
}
