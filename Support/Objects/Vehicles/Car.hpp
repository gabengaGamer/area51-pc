//==============================================================================
//
//  Car.hpp
//
//==============================================================================

#ifndef __CAR_HPP__
#define __CAR_HPP__

//==============================================================================
// INCLUDES
//==============================================================================
#include "VehicleRigidBody.hpp"
#include "VehicleObject.hpp"

#define MAX_DOORS 4 //ЗАЛУПА ПЕНИС ЕЛДА

//==============================================================================
// CLASSES
//==============================================================================

// Class that represents a wheel
class car_wheel
{
// Friends
friend class car;

// Data
protected:
    s32     m_iBone ;           // Bone index
    f32     m_Side ;            // 1=Left, -1=Right
    vector3 m_ShockOffset ;     // Offset relative to car
    f32     m_ShockLength ;     // Length of shock
    f32     m_Radius ;          // Radius of wheel

    f32     m_Length ;          // Current length of shock
    radian  m_Steer ;           // Steer yaw
    radian  m_Spin ;            // Spin roll
    radian  m_SpinSpeed ;       // Spinning speed
    matrix4 m_L2W ;             // Local -> world


// Functions
public:
            car_wheel();
            void Init           ( s32 iBone, f32 Side, const vector3& WheelOffset, f32 ShockLength, f32 Radius ) ;
            void Reset          ( void ) ;
            void ComputeForces  ( vehicle_rigid_body& Car, f32 DeltaTime ) ;
            void Advance        ( vehicle_rigid_body& Car, f32 DeltaTime, f32 Steer, f32 Accel ) ;

};

// Class that represents a doors
class car_door
{
// Friends
friend class car;	

// Data	
protected:
    s32     m_iBone;           // Bone index
    matrix4 m_L2W;             // Local -> world
    matrix4 m_InvBindL2W;      // Local -> world
    vector3 m_DoorPos;         // Door position
    vector3 m_DoorLastPos;     // Last door position
    vector3 m_HingeOffsets[2]; // Idk
   
// Functions
public:
            car_door();
            void Init     ( car& Car, s32 iBone, f32 Side, vector3* HingeOffsets, vector3& DoorOffset );
            void Reset    ( car& Car );
            void Advance  ( car& Car, f32 DeltaTime );
};

//==============================================================================

// Class that represents a car
class car : public vehicle_object
{
// Structures
private:


// Data
protected:

            // Wheel vars
            car_wheel           m_Wheels[4] ;        // FL, FR, RL, RR
            s32                 m_NDoors;            // Door count
            car_door            m_Doors[MAX_DOORS];  // Maximum doors, probably 4 ?
            vehicle_rigid_body  m_VehicleRigidBody;  // Xyeta

// Functions
public:
            // Constructor/Destructor
            car() ;
    virtual ~car() ;

            // Initialization
    virtual void                Init            ( const char* pGeometryName,
                                                  const char* pAnimName, 
                                                  const matrix4& L2W,
                                                  guid  ObjectGuid = 0 ) ;
    virtual void                Reset           ( const matrix4& L2W ) ;

            // Computes forces
    virtual void                ComputeForces   ( f32 DeltaTime ) ;

            // Advances logic               
    virtual void                AdvanceSimulation( f32 DeltaTime ) ;
                                            
            // Renders geometry             
    virtual void                Render          ( void ) ;

            // Control functions
            void                SetSteer        ( f32 Steer ) ;
            void                SetAccel        ( f32 Accel ) ;
    virtual void                ZeroVelocities  ( void ) ;

                                           
} ;


//==============================================================================

#endif  // #ifndef __CAR_HPP__
