//=============================================================================
//  ALIENSHIELD.CPP
//=============================================================================
//
//=============================================================================

//=============================================================================
// INCLUDES
//=============================================================================

#include "Render/PrimitiveDebug.hpp"
#include "AlienShield.hpp"
#include "CollisionMgr/PolyCache.hpp"
#include "../MiscUtils/SimpleUtils.hpp"
#include "AlienOrb.hpp"
#include "TemplateMgr/TemplateMgr.hpp"
#include "Player/Player.hpp"
#include "Characters/Character.hpp"
#include "Characters/Gray/Gray.hpp"
#include "Loco/LocoUtil.hpp"

extern xbool g_game_running;
extern xbool g_first_person;


struct shield_pitch_envelope_node
{
    f32         Time;
    f32         PitchOffset;
};

static shield_pitch_envelope_node  s_PitchEnvelope[] = { {0.0f,   0.0f},
                                                         {0.5f,  -0.3f},
                                                         {0.6f,   0.3f},
                                                         {0.9f,   0.0f} };
                                                         
static s32 s_PitchEnvelopeCount = 4;


//=============================================================================
// TWEAKS
//=============================================================================
static const f32                    k_THINK_RESET_VALUE             = 5.0f;
static const f32                    k_ORB_ORBIT_RADIUS              = 200.0f;
static const f32                    k_ORB_ORBIT_TIME                = 4.0f;
static const f32                    k_ORB_EVALUATION_TIME           = 1.0f;
static const f32                    k_SHOT_DURATION                 = 2.0f;
static const f32                    k_ACTIVE_PITCH_BLEND_TIME       = 1.5f;

//=============================================================================
// OBJECT DESC.
//=============================================================================

static struct alien_shield_desc : public object_desc
{
    alien_shield_desc( void ) : object_desc( 
        object::TYPE_ALIEN_SHIELD, 
        "Alien Shield",
        "AI",

        object::ATTR_COLLIDABLE             |
        object::ATTR_BLOCKS_ALL_PROJECTILES |
        object::ATTR_RENDERABLE             |
        object::ATTR_TRANSPARENT            |
        object::ATTR_NEEDS_LOGIC_TIME       |
        object::ATTR_DAMAGEABLE             |
        object::ATTR_SPACIAL_ENTRY,

        FLAGS_GENERIC_EDITOR_CREATE | 
        FLAGS_IS_DYNAMIC    ) {}

        virtual object* Create( void ) { return new alien_shield; }

#ifdef X_EDITOR

        virtual s32  OnEditorRender( object& Object ) const
        {
            (void)Object;            
            return static_cast<s32>( EditorIcon::AlienShield );
        }

#endif // X_EDITOR

} s_alien_shield_desc ;

//=============================================================================================

const object_desc&  alien_shield::GetTypeDesc ( void ) const
{
    return s_alien_shield_desc;
}

//=============================================================================================

const object_desc&  alien_shield::GetObjectType ( void )
{
    return s_alien_shield_desc;
}

//=============================================================================================

alien_shield::alien_shield()
{
    m_Radius = 200;
    m_Height = 800;

    m_nStages = 1;
    m_nDeployedOrbsMax = 1;
    m_gGray                  = NULL_GUID;

    s32 i;
    for (i=0;i<MAX_SHIELD_STAGES;i++)
    {
        m_Stage[i].m_ThinkTimer = -1;
        m_Stage[i].m_ActivePitch = 1.0f;
        m_Stage[i].m_PainFXColor = XCOLOR_WHITE;
    }
    m_DestructionFX.m_ThinkTimer = -1;




    m_OrbMaintenanceTimer = 0;

    m_iCurStage = -1;

    m_SoundIDActive             = -1;
    m_SoundIDStageDestroyed     = -1;
    m_SoundIDFinalDestruction   = -1;
    m_ActiveVoiceID             = -1;
    m_SoundIDPain               = -1;
    m_ActivePitchCurrent        = 1.0f;
    m_ActivePitchEnd            = 1.0f;
    m_ActivePitchStart          = 1.0f;
    m_ActivePitchTimer          = 0.0f;
    m_ActivePitchBaseline       = 1.0f;

    m_PitchEnvelopeTime         = 100.0f;

    m_bActive                   = FALSE;
    m_bNeedToBeDestroyed        = FALSE;

    m_iOrbBeingSpawned          = -1;


    m_gActivateOnDestroy        = NULL_GUID;
    m_OrbDelayTime              = 1.0f;

    m_iNextPainFX               = 0;

    // Make sure the alien orb blueprint is in the template dictionary
    g_TemplateStringMgr.Add( "C:\\GameData\\A51\\Source\\Themes\\AI\\Blueprint\\Alien_Orb.bpx" );
}

//=============================================================================================

alien_shield::~alien_shield()
{

}

//=============================================================================

void alien_shield::OnInit( void )
{

}

//=============================================================================================

bbox alien_shield::GetLocalBBox( void ) const 
{ 
    bbox Box;

    f32 R = m_Radius;
    f32 H = m_Height / 2.0f;

    Box.Set( vector3( -R, -H, -R ), vector3( R, H, R ) );


    return Box;
}

//=============================================================================================
volatile f32 ALIEN_SHIELD_VOLUME = 2.0f;
void alien_shield::OnAdvanceSimulation( f32 DeltaTime )
{
    // If we're waiting to die just return
    if( GetAttrBits() & ATTR_DESTROY )
        return;

    if (m_DestructionFX.m_hFX.Validate())
    {        
        m_DestructionFX.m_hFX.AdvanceLogic( DeltaTime );
    }

    // pain fx always get logic, even during destruction  
    s32 i;

    for (i=0;i<MAX_SHIELD_PAIN_FX;i++)
    {
        if (m_hPainFX[ i ].Validate())
        {
            m_hPainFX[ i ].AdvanceLogic( DeltaTime );
        }
    }

    // Bail if we are just waiting for the destruction fx to end
    if (m_bNeedToBeDestroyed)
    {
        if (m_DestructionFX.m_hFX.IsFinished())
        {         
            // Make sure to kill off any pain fx                       
            for (i=0;i<MAX_SHIELD_PAIN_FX;i++)
            {
                if (m_hPainFX[ i ].Validate())
                {
                    m_hPainFX[ i ].KillInstance();
                }
            }

            // Hey, we're finally done.  Destroy me!
            g_ObjMgr.DestroyObject( GetGuid() );           
        }
        return;
    }

    //
    //  Advance audio logic
    //   
    if (m_iCurStage >= 0)
    {
        if (m_iCurStage < (m_nStages-1))
        {
            m_ActivePitchBaseline = m_Stage[ m_iCurStage+1 ].m_ActivePitch - m_Stage[ m_iCurStage ].m_ActivePitch;
            m_ActivePitchBaseline /= 2.0f;
            m_ActivePitchBaseline *= (1.0f - (m_StageHealth / 100.0f));
            m_ActivePitchBaseline += m_Stage[ m_iCurStage ].m_ActivePitch;
        }
        else
            m_ActivePitchBaseline = m_Stage[ m_iCurStage ].m_ActivePitch;
    }
    else
        m_ActivePitchBaseline = 1.0f;

    if (m_ActivePitchCurrent != m_ActivePitchBaseline)
    {
        f32 MaxDelta = 0.2f * DeltaTime;
        f32 Delta = (m_ActivePitchBaseline - m_ActivePitchCurrent);
        if (x_abs( Delta ) < MaxDelta)
        {
            m_ActivePitchCurrent = m_ActivePitchBaseline;
        }
        else
        {
            m_ActivePitchCurrent += (Delta > 0.0f) ? MaxDelta : -MaxDelta;
        }
    }

    if (m_ActiveVoiceID != -1)
    {
        f32 AppliedPitch = m_ActivePitchCurrent;

        m_PitchEnvelopeTime += DeltaTime;

        if (m_PitchEnvelopeTime < s_PitchEnvelope[ s_PitchEnvelopeCount-1 ].Time)
        {
            // Offset by envelope
            s32 i;
            for (i=1;i<s_PitchEnvelopeCount;i++)
            {
                if (m_PitchEnvelopeTime < s_PitchEnvelope[i].Time)
                {
                    f32 Range = s_PitchEnvelope[i].Time - s_PitchEnvelope[i-1].Time;
                    f32 T = (m_PitchEnvelopeTime - s_PitchEnvelope[i-1].Time) / Range;

                    f32 PitchOffset = s_PitchEnvelope[i-1].PitchOffset + (s_PitchEnvelope[i].PitchOffset - s_PitchEnvelope[i-1].PitchOffset) * T;

                    AppliedPitch += PitchOffset;
                    break;
                }
            }
        }
        
        g_AudioMgr.SetPitch( m_ActiveVoiceID, AppliedPitch ); 
        g_AudioMgr.SetVolume( m_ActiveVoiceID, ALIEN_SHIELD_VOLUME ); 
    }
    /*
    if (m_ActivePitchTimer > 0)
    {
        m_ActivePitchTimer -= DeltaTime;
        if (m_ActivePitchTimer < 0)
        {
            m_ActivePitchCurrent = m_ActivePitchEnd;
            g_AudioMgr.SetPitch( m_ActiveVoiceID, m_ActivePitchCurrent );
        }
        else
        {
            f32 T = 1.0f - (m_ActivePitchTimer / k_ACTIVE_PITCH_BLEND_TIME);
            m_ActivePitchCurrent = m_ActivePitchStart + (m_ActivePitchEnd - m_ActivePitchStart) * T;
            g_AudioMgr.SetPitch( m_ActiveVoiceID, m_ActivePitchCurrent );
        }
    }
    */


    //
    //  Advance particle logic
    //
    for (i=0;i<MAX_SHIELD_STAGES;i++)
    {
        stage& Stage = m_Stage[ i ];

        Stage.m_ThinkTimer -= DeltaTime;

        if (i == m_iCurStage)
            Stage.m_ThinkTimer = k_THINK_RESET_VALUE;

        if (Stage.m_ThinkTimer > 0)
        {   
            Stage.m_hFX.AdvanceLogic( DeltaTime );
        }        
    }



    
    //
    //  Advance combat logic
    //
    if (!m_bActive)
        return;

    xbool bValidHostsExist = ValidHostsExist();

    m_OrbMaintenanceTimer -= DeltaTime;

    for (i=0;i<m_nDeployedOrbsMax;i++)
    {
        object_ptr<alien_orb>pOrb( m_Orb[i].m_gOrb );
        if (pOrb)
        {
            if (m_Orb[i].m_OrbitRadius < k_ORB_ORBIT_RADIUS)
            {
                m_Orb[i].m_OrbitRadius = MIN( m_Orb[i].m_OrbitRadius + (k_ORB_ORBIT_RADIUS / k_ORB_ORBIT_TIME) * DeltaTime, k_ORB_ORBIT_RADIUS);
                pOrb->SetOrbitRadius( m_Orb[i].m_OrbitRadius );                
            }
            else
            {
                // Release the hounds!
                if (pOrb->HasScriptedPosition())
                    pOrb->SetScriptedPosition( NULL_GUID );
            }
        }
        else if (bValidHostsExist)
        {
            if (m_iOrbBeingSpawned >= 0)
            {
                // A specific orb is spawning
                if (i == m_iOrbBeingSpawned)
                {
                    // And it is this orb
                    if (m_OrbMaintenanceTimer <= 0)
                    {
                        CreateOrb( i );
                       m_OrbMaintenanceTimer = 0;
                       m_iOrbBeingSpawned = -1;
                    }
                }
            }
            else
            {
                // No orb is presently spawning, let's take over the slot
                m_iOrbBeingSpawned = i;
                m_OrbMaintenanceTimer = m_OrbDelayTime;
            }     
        }
    }   

    //
    //  Handle animation of the gray
    //
    object_ptr<character>Gray(m_gGray);
    if (Gray.IsValid())
    {
        if (Gray->IsAnimAtEnd())
        {
            PlayGrayAnim( GRAY_ANIM_IDLE );
        }
    }

}

//=============================================================================================

void alien_shield::OnColCheck( void )
{    
    if( m_iCurStage >= 0 )
    {     
        bbox Box;

        f32 R = m_Radius;
        f32 H = m_Height / 2.0f;

        vector3 Pos = GetL2W().GetTranslation();

        Box.Set( Pos + vector3( -R, -H, -R ), Pos + vector3( R, H, R ) );
        g_CollisionMgr.StartApply( GetGuid() );
        g_CollisionMgr.ApplyAABBox( Box );
        g_CollisionMgr.EndApply();
    }    
}

//=============================================================================================

void alien_shield::OnMove( const vector3& NewPos )
{
    object::OnMove( NewPos );
}

//=============================================================================================

void alien_shield::OnRender( void )
{
}

//=============================================================================================

void alien_shield::OnRenderTransparent( void )
{
    const matrix4& L2W = GetL2W();
    vector3 Pos = L2W.GetTranslation();

    s32 i;
    for (i=0;i<m_nStages;i++)
    {
        if (m_Stage[i].m_ThinkTimer > 0)
        {
            m_Stage[i].m_hFX.SetTranslation( Pos );
            m_Stage[i].m_hFX.Render();
        }
    }
    if (m_DestructionFX.m_hFX.Validate())
        m_DestructionFX.m_hFX.Render();

    for (i=0;i<MAX_SHIELD_PAIN_FX;i++)
    {
        if (m_hPainFX[i].Validate())
        {
            m_hPainFX[i].Render();
        }
    }

#ifdef X_EDITOR
    if (!g_game_running || (GetAttrBits() & ATTR_EDITOR_SELECTED) )
    {
        // Only draw the debug cylinder when:
        // 1. the game isn't running
        // 2. the game is running, but this object is selected
        //
        render::debug::Cylinder( Pos, m_Radius, m_Height, 16, xcolor(200,0,50,64) );
    }

#endif
}

//=============================================================================

void alien_shield::OnEnumProp      ( prop_enum&    List )
{
    object::OnEnumProp( List );

    List.PropEnumHeader( "Alien Shield",           "Alien Shield Properties", 0 );

    List.PropEnumFloat ( "Alien Shield\\Height", "Height of the shield cylinder", 0 );
    List.PropEnumFloat ( "Alien Shield\\Radius", "Radius of the shield cylinder", 0 );


    List.PropEnumHeader  ( "Alien Shield\\Sound", "Sounds for the alien shield", 0 );    
    List.PropEnumString  ( "Alien Shield\\Sound\\Audio Package Name", "The audio package associated with this alien shield object.", PROP_TYPE_READ_ONLY | PROP_TYPE_DONT_SAVE | PROP_TYPE_DONT_EXPORT );
    List.PropEnumExternal( "Alien Shield\\Sound\\Audio Package",      "Resource\0audiopkg\0", "The audio package associated with this alien shield object.", PROP_TYPE_DONT_SHOW );
    List.PropEnumExternal( "Alien Shield\\Sound\\Active Sound",         "Sound\0soundexternal\0","The sound to play when the shield is active",     PROP_TYPE_MUST_ENUM );
    List.PropEnumExternal( "Alien Shield\\Sound\\Stage Destroyed",      "Sound\0soundexternal\0","The sound to play when the shield is damaged enough to change stages", PROP_TYPE_MUST_ENUM );
    List.PropEnumExternal( "Alien Shield\\Sound\\Final Destruction",    "Sound\0soundexternal\0","The sound to play when the shield is destroyed",  PROP_TYPE_MUST_ENUM );
    List.PropEnumExternal( "Alien Shield\\Sound\\Pain",                 "Sound\0soundexternal\0","The sound to play when the shield takes pain",    PROP_TYPE_MUST_ENUM );


    List.PropEnumGuid  ( "Alien Shield\\Gray",      "Gray standing inside the shield", PROP_TYPE_EXPOSE );
    List.PropEnumInt   ( "Alien Shield\\Deployed orbs", "How many orbs can be out in the world at once", PROP_TYPE_EXPOSE );
    List.PropEnumFloat ( "Alien Shield\\Orb Delay", "How long the shield must wait before launching an orb", PROP_TYPE_EXPOSE );
    List.PropEnumInt   ( "Alien Shield\\Stage Count", "How many stages does the shield have", PROP_TYPE_MUST_ENUM );
    
    List.PropEnumGuid  ( "Alien Shield\\Limit SpawnTube Volume","The specified object's bounding box defines the volum that spawn tubes have to be in to be considered valid targets", PROP_TYPE_EXPOSE );
    List.PropEnumGuid  ( "Alien Shield\\Limit Turret Volume",   "The specified object's bounding box defines the volum that turrets have to be in to be considered valid targets", PROP_TYPE_EXPOSE );
    List.PropEnumGuid  ( "Alien Shield\\Host Group",            "The group containing all possible hosts for orbs spawned by this spawner", PROP_TYPE_EXPOSE );

    List.PropEnumGuid  ( "Alien Shield\\Activate On Destroy",   "Activate this object when the shield is destroyed", PROP_TYPE_EXPOSE );
    
    List.PropEnumHeader  ( "Alien Shield\\Anim TeleportIn", "Animation for gray to play when teleporting in", 0 );
    {
        s32 iPath = List.PushPath( "Alien Shield\\Anim TeleportIn\\" );        
        LocoUtil_OnEnumPropAnimFlags(   List, loco::ANIM_FLAG_PLAY_TYPE_ALL | loco::ANIM_FLAG_END_STATE_ALL | loco::ANIM_FLAG_INTERRUPT_BLEND | loco::ANIM_FLAG_TURN_OFF_AIMER | loco::ANIM_FLAG_RESTART_IF_SAME_ANIM, m_GrayAnimData[ GRAY_ANIM_TELEPORT_IN ].m_AnimFlags );
        List.PopPath(iPath);
    }
    List.PropEnumHeader  ( "Alien Shield\\Anim ReleaseOrb", "Animation for gray to play when the shield releases an orb", 0 );
    {
        s32 iPath = List.PushPath( "Alien Shield\\Anim ReleaseOrb\\" );        
        LocoUtil_OnEnumPropAnimFlags(   List, loco::ANIM_FLAG_PLAY_TYPE_ALL | loco::ANIM_FLAG_END_STATE_ALL | loco::ANIM_FLAG_INTERRUPT_BLEND | loco::ANIM_FLAG_TURN_OFF_AIMER | loco::ANIM_FLAG_RESTART_IF_SAME_ANIM, m_GrayAnimData[ GRAY_ANIM_RELEASE_ORB ].m_AnimFlags );
        List.PopPath(iPath);
    }
    List.PropEnumHeader  ( "Alien Shield\\Anim Damaged", "Animation for gray to play when the shield gets damaged enough to switch stages", 0 );
    {
        s32 iPath = List.PushPath( "Alien Shield\\Anim Damaged\\" );        
        LocoUtil_OnEnumPropAnimFlags(   List, loco::ANIM_FLAG_PLAY_TYPE_ALL | loco::ANIM_FLAG_END_STATE_ALL | loco::ANIM_FLAG_INTERRUPT_BLEND | loco::ANIM_FLAG_TURN_OFF_AIMER | loco::ANIM_FLAG_RESTART_IF_SAME_ANIM, m_GrayAnimData[ GRAY_ANIM_DAMAGE ].m_AnimFlags );
        List.PopPath(iPath);
    }
    List.PropEnumHeader  ( "Alien Shield\\Anim Idle", "Animation for gray to play when idle within the shield", 0 );
    {
        s32 iPath = List.PushPath( "Alien Shield\\Anim Idle\\" );        
        LocoUtil_OnEnumPropAnimFlags(   List, loco::ANIM_FLAG_PLAY_TYPE_ALL | loco::ANIM_FLAG_END_STATE_ALL | loco::ANIM_FLAG_INTERRUPT_BLEND | loco::ANIM_FLAG_TURN_OFF_AIMER | loco::ANIM_FLAG_RESTART_IF_SAME_ANIM, m_GrayAnimData[ GRAY_ANIM_IDLE ].m_AnimFlags );
        List.PopPath(iPath);
    }

    s32 i;
    for (i=0;i<m_nStages;i++)
    {
        List.PropEnumString( xfs("Alien Shield\\Stage [%d]",i),"", PROP_TYPE_HEADER );
        s32 iHeader = List.PushPath(xfs("Alien Shield\\Stage [%d]\\",i));

        List.PropEnumExternal( "StageFX",          "Resource\0fxo\0",  "Particle effect for this stage.", 0 );

        List.PropEnumFloat( "Active Pitch", "Pitch for active sound while this stage is up", 0 );
        List.PropEnumColor( "Pain FX Color", "Color to tint the pain fx", 0 );
        List.PopPath(iHeader);          
    }  

    List.PropEnumExternal( "Alien Shield\\DestructionFX",          "Resource\0fxo\0",  "Particle effect for the shield's death.", 0 );
    List.PropEnumExternal( "Alien Shield\\PainFX",                 "Resource\0fxo\0",  "Particle effect for pain response. This will be positioned and oriented by the shield to match the point of impact", 0 );
}

//=============================================================================

xbool alien_shield::OnProperty      ( prop_query&   I    )
{
    if( object::OnProperty( I ) )
    {
        // do nothing
    }
    else if (I.IsSimilarPath( "Alien Shield\\Anim TeleportIn" ))
    {        
        s32 iPath = I.PushPath( "Alien Shield\\Anim TeleportIn\\" );
        xbool bRet = FALSE;
        if (LocoUtil_OnPropertyAnimFlags(I, 
            m_GrayAnimData[ GRAY_ANIM_TELEPORT_IN ].m_AnimGroupName, 
            m_GrayAnimData[ GRAY_ANIM_TELEPORT_IN ].m_AnimName, 
            m_GrayAnimData[ GRAY_ANIM_TELEPORT_IN ].m_AnimFlags, 
            m_GrayAnimData[ GRAY_ANIM_TELEPORT_IN ].m_AnimPlayTime))
        {
            bRet = TRUE;
        }
        I.PopPath(iPath); 
        return bRet;
    }
    else if (I.IsSimilarPath( "Alien Shield\\Anim ReleaseOrb" ))
    {        
        s32 iPath = I.PushPath( "Alien Shield\\Anim ReleaseOrb\\" );
        xbool bRet = FALSE;
        if (LocoUtil_OnPropertyAnimFlags(I, 
            m_GrayAnimData[ GRAY_ANIM_RELEASE_ORB ].m_AnimGroupName, 
            m_GrayAnimData[ GRAY_ANIM_RELEASE_ORB ].m_AnimName, 
            m_GrayAnimData[ GRAY_ANIM_RELEASE_ORB ].m_AnimFlags, 
            m_GrayAnimData[ GRAY_ANIM_RELEASE_ORB ].m_AnimPlayTime))
        {
            bRet = TRUE;
        }
        I.PopPath(iPath); 
        return bRet;
    }
    else if (I.IsSimilarPath( "Alien Shield\\Anim Damaged" ))
    {        
        s32 iPath = I.PushPath( "Alien Shield\\Anim Damaged\\" );
        xbool bRet = FALSE;
        if (LocoUtil_OnPropertyAnimFlags(I, 
            m_GrayAnimData[ GRAY_ANIM_DAMAGE ].m_AnimGroupName, 
            m_GrayAnimData[ GRAY_ANIM_DAMAGE ].m_AnimName, 
            m_GrayAnimData[ GRAY_ANIM_DAMAGE ].m_AnimFlags, 
            m_GrayAnimData[ GRAY_ANIM_DAMAGE ].m_AnimPlayTime))
        {
            bRet = TRUE;
        }
        I.PopPath(iPath); 
        return bRet;
    }
    else if (I.IsSimilarPath( "Alien Shield\\Anim Idle" ))
    {        
        s32 iPath = I.PushPath( "Alien Shield\\Anim Idle\\" );
        xbool bRet = FALSE;
        if (LocoUtil_OnPropertyAnimFlags(I, 
            m_GrayAnimData[ GRAY_ANIM_IDLE ].m_AnimGroupName, 
            m_GrayAnimData[ GRAY_ANIM_IDLE ].m_AnimName, 
            m_GrayAnimData[ GRAY_ANIM_IDLE ].m_AnimFlags, 
            m_GrayAnimData[ GRAY_ANIM_IDLE ].m_AnimPlayTime))
        {
            bRet = TRUE;
        }
        I.PopPath(iPath); 
        return bRet;
    }
    else if (I.VarFloat( "Alien Shield\\Height", m_Height ))
    {}
    else if (I.VarFloat( "Alien Shield\\Radius", m_Radius))
    {}
    else if (I.VarGUID( "Alien Shield\\Activate On Destroy", m_gActivateOnDestroy ))
    {}
    else if (I.VarFloat( "Alien Shield\\Orb Delay", m_OrbDelayTime ))
    {}
    else if( I.IsVar( "Alien Shield\\Sound\\Audio Package" ) )
    {
        if( I.IsRead() )
        {
            I.SetVarExternal( m_hAudioPackage.GetName(), RESOURCE_NAME_SIZE );
        }
        else
        {
            // Get the FileName
            const char* pString = I.GetVarExternal();

            if( pString[0] )
            {
                if( xstring(pString) == "<null>" )
                {
                    m_hAudioPackage.SetName( "" );
                }
                else
                {
                    m_hAudioPackage.SetName( pString );                

                    // Load the audio package.
                    if( m_hAudioPackage.IsLoaded() == FALSE )
                        m_hAudioPackage.GetPointer();
                }
            }
        }
    }
    else if( I.IsVar( "Alien Shield\\Sound\\Audio Package Name" ) )
    {
        // You can only read.
        if( I.IsRead() )
        {
            I.SetVarString( m_hAudioPackage.GetName(), RESOURCE_NAME_SIZE );
        }
    }
    else if (SMP_UTIL_IsAudioVar( I, "Alien Shield\\Sound\\Active Sound", m_hAudioPackage, m_SoundIDActive ))
    {}
    else if (SMP_UTIL_IsAudioVar( I, "Alien Shield\\Sound\\Stage Destroyed", m_hAudioPackage, m_SoundIDStageDestroyed ))
    {}
    else if (SMP_UTIL_IsAudioVar( I, "Alien Shield\\Sound\\Final Destruction", m_hAudioPackage, m_SoundIDFinalDestruction ))
    {}    
    else if (SMP_UTIL_IsAudioVar( I, "Alien Shield\\Sound\\Pain", m_hAudioPackage, m_SoundIDPain ))
    {}    
    else if (I.VarGUID( "Alien Shield\\Limit SpawnTube Volume", m_SpawnTubeLimitVolume ))
    {}
    else if (I.VarGUID( "Alien Shield\\Limit Turret Volume", m_TurretLimitVolume ))
    {}
    else if (I.VarGUID ( "Alien Shield\\Host Group", m_HostGroup ))
    {}
    else if (I.VarGUID( "Alien Shield\\Gray", m_gGray ))
    {}
    else if (I.VarInt( "Alien Shield\\Deployed orbs", m_nDeployedOrbsMax ))
    {
        if (!I.IsRead())
        {
            // If we were in the process of spawning an orb from the range
            // that may have just been cut off, we need to abandon it
            // and reset the spawn index back to -1
            if (m_iOrbBeingSpawned >= m_nDeployedOrbsMax)
                m_iOrbBeingSpawned = -1;
        }
    }
    else if (I.IsVar( "Alien Shield\\Stage Count" ))
    {
        if (I.IsRead())
        {
            I.SetVarInt( m_nStages );
        }
        else
        {

            m_nStages = I.GetVarInt(1,MAX_SHIELD_STAGES);            
        }
    }
    else
    if (I.IsSimilarPath( "Alien Shield\\Stage" ) )
    {
        s32 i = I.GetIndex(0);
        if (i>=0)
        {
            if ((i < MAX_SHIELD_STAGES) && (i >= 0))
            {
                if( SMP_UTIL_IsParticleFxVar( I, "Alien Shield\\Stage []\\StageFX", m_Stage[i].m_hParticleEffect) )
                {}
                else if (I.VarFloat("Alien Shield\\Stage []\\Active Pitch", m_Stage[i].m_ActivePitch))
                {}
                else if (I.VarColor("Alien Shield\\Stage []\\Pain FX Color", m_Stage[i].m_PainFXColor))
                {}
                else
                {
                    return FALSE;
                }
            }
        }
    }
    else
    if( SMP_UTIL_IsParticleFxVar( I, "Alien Shield\\DestructionFX", m_DestructionFX.m_hParticleEffect) )
    {}
    else if( SMP_UTIL_IsParticleFxVar( I, "Alien Shield\\PainFX", m_hPain ) )
    {}
    else 
    {
        return FALSE;
    }

    return TRUE;
}

//=========================================================================

void alien_shield::OnPain( const pain& Pain )
{   
    (void)Pain;
    if (!m_bActive || m_bNeedToBeDestroyed)
        return;

    m_PitchEnvelopeTime = 0;

    Pain.ComputeDamageAndForce(GetLogicalName(), GetGuid(), GetBBox().GetCenter());
    m_StageHealth -= Pain.GetDamage();
    if (m_SoundIDPain != -1)
    {
        const char* pSoundName = g_StringMgr.GetString( m_SoundIDPain );
        if (pSoundName)
        {
            voice_id VID = g_AudioMgr.PlayVolumeClipped( pSoundName, Pain.GetPosition(), GetZone1(), TRUE );
            g_AudioMgr.SetVolume(VID,ALIEN_SHIELD_VOLUME);
        }
    }

    PlayPainFX( Pain.GetPosition() );
    
    const matrix4& L2W = GetL2W();

    if (m_StageHealth <= 0)
    {
        SwitchStage( m_iCurStage + 1 );

        if (m_iCurStage == -1)
        {
            // We have run out of stages and are dead.  Play the "FinalDestruction" sound
            if (m_SoundIDFinalDestruction != -1)
            {
                const char* pSoundName = g_StringMgr.GetString( m_SoundIDFinalDestruction );
                if (pSoundName)
                {
                    voice_id VID = g_AudioMgr.PlayVolumeClipped( pSoundName, L2W.GetTranslation(), GetZone1(), TRUE );
                    g_AudioMgr.SetVolume( VID, ALIEN_SHIELD_VOLUME );
                }
            }
            // Kill the active sound
            if (m_ActiveVoiceID != -1)
            {
                g_AudioMgr.Release( m_ActiveVoiceID, 0 );
                m_ActiveVoiceID = -1;
            }
            // Activate the remote object if required
            if (NULL_GUID != m_gActivateOnDestroy)
            {
                object* pObj = g_ObjMgr.GetObjectByGuid( m_gActivateOnDestroy );
                if (pObj)
                {
                    pObj->OnActivate( TRUE );
                }
            }
            // Smack the gray
            if (NULL_GUID != m_gGray)
            {
                object_ptr<character> pGray(m_gGray);
                if (pGray.IsValid())
                {
                    pGray->SetIgnorePain( FALSE );
                    vector3 Dir = GetPosition() - Pain.GetPosition();
                    pain GrayPain;
                    pain_handle hPain = GetPainHandleForGenericPain( TYPE_LETHAL );
                    GrayPain.Setup( hPain, GetGuid(), GetPosition() );
                    GrayPain.SetDirection( Dir );
                    GrayPain.SetDirectHitGuid( m_gGray );
                    if (Pain.HasCollision())
                        GrayPain.SetCollisionInfo( Pain.GetCollision() );
                    GrayPain.SetCustomScalar( 1 );
                    GrayPain.ApplyToObject( m_gGray );
                }
            }
            // Release any spiraling orbs
            s32 i;
            for (i=0;i<m_nDeployedOrbsMax;i++)
            {
                object_ptr<alien_orb>pOrb( m_Orb[i].m_gOrb );
                if (pOrb.IsValid())
                {                    
                    if (pOrb->HasScriptedPosition())
                        pOrb->SetScriptedPosition( NULL_GUID );                    
                }
            }
            // Kick off the destruction fx
            m_bNeedToBeDestroyed = TRUE;
            if( ( m_DestructionFX.m_hFX.Validate() == FALSE ) && ( m_DestructionFX.m_hParticleEffect.GetPointer() ) )
            {
                m_DestructionFX.m_hFX.InitInstance( m_DestructionFX.m_hParticleEffect.GetPointer() );        
            }   
            m_DestructionFX.m_hFX.Restart();
            m_DestructionFX.m_hFX.SetSuspended( FALSE );

            const matrix4& L2W = GetL2W();

            m_DestructionFX.m_hFX.SetTranslation( L2W.GetTranslation() );
            m_DestructionFX.m_hFX.SetRotation   ( L2W.GetRotation()    );
        }
        else
        {
            // We have switched to a new stage, play the "StageDestroyed" sound
            if (m_SoundIDStageDestroyed != -1)
            {
                const char* pSoundName = g_StringMgr.GetString( m_SoundIDStageDestroyed );
                if (pSoundName)
                {
                    voice_id VID = g_AudioMgr.PlayVolumeClipped( pSoundName, L2W.GetTranslation(), GetZone1(), TRUE );
                    g_AudioMgr.SetVolume( VID, ALIEN_SHIELD_VOLUME );
                }
            }

            PlayGrayAnim( GRAY_ANIM_DAMAGE );
        }
    }
}

//=========================================================================

void alien_shield::OnActivate( xbool bOn )
{
    if (bOn)
    {
        m_bActive = TRUE;
        SwitchStage( 0 );    
        if (m_ActiveVoiceID != -1)
        {
            g_AudioMgr.Release( m_ActiveVoiceID, 0 );
            m_ActiveVoiceID = -1;
        }
        if (m_SoundIDActive != -1)
        {
            const char* pSoundName = g_StringMgr.GetString( m_SoundIDActive );
            if (pSoundName)
            {
                const matrix4& L2W = GetL2W();
                m_ActiveVoiceID = g_AudioMgr.Play( pSoundName, L2W.GetTranslation(), GetZone1(), TRUE );                
                g_AudioMgr.SetVolume( m_ActiveVoiceID, ALIEN_SHIELD_VOLUME );
            }
        }
        PlayGrayAnim( GRAY_ANIM_TELEPORT_IN );
        if (m_gGray)
        {
            object_ptr<gray> pGray( m_gGray );
            if (pGray.IsValid())
                pGray->SetIgnorePain( TRUE );
            pGray->SetShieldGuid( GetGuid() );
        }
    }
    else
    {
        if (m_bActive)
        {
            DeactivateCurrentStage();
            m_iCurStage = -1;
        }

        if (m_gGray)
        {
            object_ptr<character> pGray( m_gGray );
            if (pGray.IsValid())
                pGray->SetIgnorePain( TRUE );
        }

        m_bActive = FALSE;
    }
}

//=========================================================================

void alien_shield::SwitchStage( s32 iStage )
{
    DeactivateCurrentStage();
    m_iCurStage = iStage;

    if (m_iCurStage == m_nStages)
    {
        m_bActive = FALSE;
        m_iCurStage = -1;
        return;
    }
    else
    if (( m_iCurStage < 0 ) || ( m_iCurStage >= m_nStages ))
    {
        // Invalid. Deactivate 
        m_iCurStage = -1;
        m_bActive = FALSE;
        return;
    }

    m_StageHealth = 100;
    ActivateCurrentStage();
}

//=========================================================================

void alien_shield::DeactivateCurrentStage( void )
{
    if ((m_iCurStage < -1) || (m_iCurStage >= m_nStages))
    {
        // Stage is invalid, should we complain?
        return;
    }

    if (m_iCurStage == -1)
        return;
    
    stage& Stage = m_Stage[ m_iCurStage ];

    if( ( Stage.m_hFX.Validate()) )
    {
        Stage.m_hFX.SetSuspended( TRUE );        
    }   
}

//=========================================================================

void alien_shield::ActivateCurrentStage( void )
{
    if ((m_iCurStage < -1) || (m_iCurStage >= m_nStages))
    {
        // Stage is invalid, should we complain?
        return;
    }

    if (m_iCurStage == -1)
        return;

    stage& Stage = m_Stage[ m_iCurStage ];

    if( ( Stage.m_hFX.Validate() == FALSE ) && ( Stage.m_hParticleEffect.GetPointer() ) )
    {
        Stage.m_hFX.InitInstance( Stage.m_hParticleEffect.GetPointer() );        
    }   
    Stage.m_hFX.Restart();
    Stage.m_hFX.SetSuspended( FALSE );

    const matrix4& L2W = GetL2W();

    Stage.m_hFX.SetTranslation( L2W.GetTranslation() );
    Stage.m_hFX.SetRotation   ( L2W.GetRotation()    );

    Stage.m_ThinkTimer = k_THINK_RESET_VALUE;

    if (m_ActiveVoiceID != -1)
    {
        m_ActivePitchStart = m_ActivePitchCurrent;
        m_ActivePitchEnd   = Stage.m_ActivePitch;
        m_ActivePitchTimer = k_ACTIVE_PITCH_BLEND_TIME;
    }        
}

//=============================================================================

void alien_shield::PlayGrayAnim( gray_anim Anim )
{
    gray_anim_data D = m_GrayAnimData[ Anim ];
   
    object_ptr<character> Gray( m_gGray );
    if (!Gray.IsValid())
        return;
    
    if ((D.m_AnimGroupName == -1) || (D.m_AnimName == -1))
        return;

    // setup all the data
    character_trigger_state::TriggerData pData;
    pData.m_ActionType = action_ai_base::AI_PLAY_ANIMATION;
    pData.m_MustSucceed = TRUE;
    pData.m_ResponseList.AddFlags( response_list::RF_IGNORE_ATTACKS | 
                                   response_list::RF_IGNORE_SIGHT | 
                                   response_list::RF_IGNORE_SOUND | 
                                   response_list::RF_IGNORE_ALERTS );
    pData.m_TriggerGuid = 0;
    pData.m_Blocking = FALSE;
    pData.m_NextAiState = character_state::STATE_IDLE;
    x_strcpy( pData.m_UnionData.m_PlayAnimData.m_AnimGroupName, g_StringMgr.GetString( D.m_AnimGroupName ) );
    x_strcpy( pData.m_UnionData.m_PlayAnimData.m_AnimName, g_StringMgr.GetString( D.m_AnimName ) );
    pData.m_UnionData.m_PlayAnimData.m_AnimPlayTime = D.m_AnimPlayTime;
    pData.m_UnionData.m_PlayAnimData.m_AnimFlags    = D.m_AnimFlags;
    pData.m_ActionFocus = 0 ;

    Gray->TriggerState(pData);
}

//=========================================================================

void alien_shield::CreateOrb( s32 iSlot )
{
    // Don't create if there are no valid targets
    if (!ValidHostsExist())
        return;


    ASSERT( (iSlot >= 0) && (iSlot < m_nDeployedOrbsMax) );
    if ((iSlot < 0) || (iSlot > m_nDeployedOrbsMax))
        return;

    // Don't replace it if the old one is still alive
    object_ptr<alien_orb>pOrb(m_Orb[iSlot].m_gOrb);
    if (pOrb)
        return;

    const matrix4& L2W = GetL2W();

    guid gObj = NULL_GUID;

    gObj = g_TemplateMgr.CreateSingleTemplate( "C:\\GameData\\A51\\Source\\Themes\\AI\\Blueprint\\Alien_Orb.bpx", L2W.GetTranslation(), radian3(0,0,0), GetZone1(), GetZone2() );

    object* pObj = g_ObjMgr.GetObjectByGuid( gObj );
    if (pObj)
    {        
        alien_orb& Orb = alien_orb::GetSafeType( *pObj );  
        Orb.SetSpawnTubeLimit( m_SpawnTubeLimitVolume );
        Orb.SetTurretLimit( m_TurretLimitVolume );
        Orb.ActivateOrb();
        Orb.SetScriptedPosition( GetGuid() );
        Orb.SetOrbitOffset( x_frand(0,R_360) );
        Orb.SetOrbitRadius( 0 );

        m_Orb[ iSlot ].m_gOrb = gObj;
        m_Orb[ iSlot ].m_OrbitRadius = 0;    
    }

    PlayGrayAnim( GRAY_ANIM_RELEASE_ORB );    
}

//=========================================================================

xbool alien_shield::ValidHostsExist( void )
{
    xbool bValidTargets = FALSE;
    if (m_SpawnTubeLimitVolume != NULL_GUID)
    {        
        object* pObj = g_ObjMgr.GetObjectByGuid( m_SpawnTubeLimitVolume );
        if (pObj)
        {
            if (alien_orb::SelectTargetInVolume( object::TYPE_ALIEN_SPAWN_TUBE, pObj->GetBBox(), GetGuid(), m_HostGroup ))
            {
                bValidTargets = TRUE;
            }
        }
    }

    if ((m_TurretLimitVolume != NULL_GUID) && (!bValidTargets))
    {        
        object* pObj = g_ObjMgr.GetObjectByGuid( m_TurretLimitVolume );
        if (pObj)
        {
            if (alien_orb::SelectTargetInVolume( object::TYPE_TURRET, pObj->GetBBox(), GetGuid(), m_HostGroup ))
            {
                bValidTargets = TRUE;
            }
        }
    }

    // Bail if there are no targets around
    return bValidTargets;
}

//=============================================================================================

void alien_shield::PlayPainFX( const vector3& PainPos )
{
    fx_handle& FX = m_hPainFX[ m_iNextPainFX ];

    if( ( FX.Validate() == FALSE ) && ( m_hPain.GetPointer() ) )
    {
        FX.InitInstance( m_hPain.GetPointer() );        
    } 

    const matrix4& L2W = GetL2W();

    vector3 Pos     = L2W.GetTranslation();    
    vector3 Delta   = PainPos - Pos;
    Delta.GetY()    = 0;

    radian3 Rot(0,0,0);
    Rot.Yaw = Delta.GetYaw();

    FX.Restart();
    FX.SetSuspended( FALSE );
    FX.SetTranslation( PainPos );
    FX.SetRotation( Rot );
    FX.SetColor( m_Stage[ m_iCurStage ].m_PainFXColor );

    m_iNextPainFX = (m_iNextPainFX + 1) % MAX_SHIELD_PAIN_FX;
}

//=============================================================================================

vector3 alien_shield::GetAimAtPoint( const vector3& AimFromHere )
{
    vector3 Delta = AimFromHere - GetPosition();
    Delta.Normalize();
    vector3 PlanarDelta = Delta;
    PlanarDelta.GetY() = 0;
    
    f32 PlanarLength = PlanarDelta.Length();

    f32 DeltaLength = 1.0f / PlanarLength;
    Delta *= DeltaLength;

    f32 HeightRelToCenterline = Delta.GetY();

    HeightRelToCenterline = MAX(-m_Height,MIN(m_Height,HeightRelToCenterline));
    HeightRelToCenterline += x_frand(-30,30);
    HeightRelToCenterline = MAX(-m_Height,MIN(m_Height,HeightRelToCenterline));

    PlanarDelta *= m_Radius;
    PlanarDelta.RotateY( x_frand(-R_15,R_15) );
    PlanarDelta.GetY() = HeightRelToCenterline;

    return PlanarDelta + GetPosition();
}

//=============================================================================================


//=========================================================================

#ifdef X_EDITOR
void alien_shield::EditorPreGame( void )
{
    const matrix4& L2W = GetL2W();

    guid ObjGuid = g_TemplateMgr.EditorCreateSingleTemplateFromPath( "C:\\GameData\\A51\\Source\\Themes\\AI\\Blueprint\\Alien_Orb.bpx", L2W.GetTranslation(), L2W.GetRotation(), -1, -1 );
    g_ObjMgr.GetObjectByGuid( ObjGuid )->EditorPreGame();
    g_ObjMgr.DestroyObject( ObjGuid );    
}
#endif // X_EDITOR

//=========================================================================
