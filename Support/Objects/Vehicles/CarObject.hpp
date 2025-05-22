//==============================================================================
//
//  CarObject.hpp
//
//==============================================================================

#ifndef CAR_OBJECT_HPP
#define CAR_OBJECT_HPP

//==============================================================================
// INCLUDES
//==============================================================================

#include "VehicleObject.hpp"
#include "AudioMgr\AudioMgr.hpp"

//==============================================================================
// DEFINES
//==============================================================================

#define WHEEL_HISTORY_LENGTH        128

//==============================================================================
// FORWARD DECLARATIONS
//==============================================================================

class wheel;

//==============================================================================
// TYPES
//==============================================================================

// Class that represents a wheel
class wheel
{
// Friends
friend class car_object;

// Data
protected:
    // Wheel state
    s32         m_iBone;                    // Bone index
    vector3     m_Offset;                   // Offset relative to car
    radian      m_Steer;                    // Steer yaw
    vector3     m_Velocity;                 // Current velocity
    vector3     m_Position;                 // Current position
    vector3     m_OldPosition;              // Previous position
    guid        m_ObjectGuid;               // Guid of car object
    radian      m_Spin;                     // Current rotation of wheel
    radian      m_SpinSpeed;                // Current rate of rotation
    xbool       m_bPassive;                 // Is this a passive wheel
    f32         m_LastShockLength;          // Previous shock length
    f32         m_CurrentShockLength;       // Current shock length
    xbool       m_bHasTraction;             // Does wheel have traction
    f32         m_DebrisTimer;              // Timer for debris creation

    // History data
    struct history
    {
        f32     ShockLength;
        f32     ShockSpeed;
        xbool   bHasTraction;
    };

    s32         m_nLogicLoops;
    history     m_History[WHEEL_HISTORY_LENGTH];

// Functions
public:
                wheel           ( void );
                
    void        Init            ( s32 iBone, 
                                  const matrix4& VehicleL2W, 
                                  const vector3& WheelOffset, 
                                  guid ObjectGuid,
                                  xbool bPassive );

    void        Reset           ( const matrix4& VehicleL2W );

    void        Advance         ( f32 DeltaTime, const matrix4& VehicleL2W, f32 Accel, f32 Brake );
    void        FinishAdvance   ( f32 DeltaTime, const matrix4& VehicleL2W );

protected:
    void        ApplyInput      ( f32 DeltaTime, f32 Accel, f32 Brake );
    void        EnforceCollision( void );
    void        ApplyTraction   ( f32 DeltaTime, const matrix4& VehicleL2W );
};

//==============================================================================
// CLASS
//==============================================================================

class car_object : public vehicle_object
{
public:
    CREATE_RTTI( car_object, vehicle_object, object )

    //--------------------------------------------------------------------------
    // Public Types
    //--------------------------------------------------------------------------
    
    enum car_flags
    {
        CAR_FLAG_DESTROYED = BIT(0),
    };

    //--------------------------------------------------------------------------
    // Constructor / Destructor
    //--------------------------------------------------------------------------
    
                                car_object          ( void );
    virtual                     ~car_object         ( void );

    //--------------------------------------------------------------------------
    // Virtual functions from object
    //--------------------------------------------------------------------------

    virtual void                OnInit              ( void );
    virtual void                OnKill              ( void );
    virtual void                OnEnumProp          ( prop_enum&    List );
    virtual xbool               OnProperty          ( prop_query&   I    );
    virtual void                OnPain              ( const pain& Pain );
    
    virtual void                OnPolyCacheGather   ( void );
    
    virtual const object_desc&  GetTypeDesc         ( void ) const;
    static  const object_desc&  GetObjectType       ( void );
    
    virtual const char*         GetLogicalName      ( void );

    //--------------------------------------------------------------------------
    // Public Interface
    //--------------------------------------------------------------------------
    
            f32                 GetHealth           ( void ) const { return m_Health; }
            void                SetHealth           ( f32 Health ) { m_Health = Health; }
            
            f32                 GetMaxHealth        ( void ) const { return m_MaxHealth; }
            void                SetMaxHealth        ( f32 MaxHealth ) { m_MaxHealth = MaxHealth; }

protected:

    //--------------------------------------------------------------------------
    // Protected Virtual functions from vehicle_object
    //--------------------------------------------------------------------------
    
    virtual void                VehicleInit         ( void );
    virtual void                VehicleKill         ( void );
    virtual void                VehicleReset        ( void );
    virtual void                VehicleUpdate       ( f32 DeltaTime );
    virtual void                VehicleRender       ( void );

    //--------------------------------------------------------------------------
    // Protected Functions
    //--------------------------------------------------------------------------
    
            void                AdvanceSteering     ( f32 DeltaTime );
            void                EnforceConstraints  ( void );
            void                UpdateAirStabilization( f32 DeltaTime );
            void                RenderShockHistory  ( void );
            void                UpdateAudio         ( f32 DeltaTime );
            void                ComputeL2W          ( void );

    //--------------------------------------------------------------------------
    // Protected Data - Wheels
    //--------------------------------------------------------------------------
    
protected:
    wheel                       m_Wheels[4];            // FL, FR, RL, RR
    matrix4                     m_BackupL2W;            // Backup transform for reset
    
    //--------------------------------------------------------------------------
    // Protected Data - Physics
    //--------------------------------------------------------------------------
    
    radian                      m_OldStabilizationTheta;
    radian                      m_StabilizationSpeed;
    
    vector3                     m_BubbleOffset[8];      // Collision bubble offsets
    f32                         m_BubbleRadius[8];      // Collision bubble radii
    s32                         m_nBubbles;             // Number of collision bubbles

    //--------------------------------------------------------------------------
    // Protected Data - Audio
    //--------------------------------------------------------------------------
    
    voice_id                    m_AudioIdle;            // Idle engine sound
    voice_id                    m_AudioRev;             // Rev engine sound
    f32                         m_RevTurbo;             // Turbo rev amount

    //--------------------------------------------------------------------------
    // Protected Data - Gameplay
    //--------------------------------------------------------------------------
    
    f32                         m_Health;               // Current health
    f32                         m_MaxHealth;            // Maximum health
    u32                         m_CarFlags;             // Various flags
    
    //--------------------------------------------------------------------------
    // Protected Data - Debug
    //--------------------------------------------------------------------------
    
    xbool                       m_bDisplayShockInfo;    // Display shock debug info
};

//==============================================================================
#endif // CAR_OBJECT_HPP
//==============================================================================