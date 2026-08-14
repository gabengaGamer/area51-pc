//==============================================================================
//
//  audio_descriptor_runtime.hpp
//
//==============================================================================

#ifndef AUDIO_DESCRIPTOR_RUNTIME_HPP
#define AUDIO_DESCRIPTOR_RUNTIME_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Audio/audio_types.hpp"

//==============================================================================
//  TYPES
//==============================================================================

class audio_package;
class audio_package_registry;
struct audio_runtime;

//==============================================================================
//  AUDIO DESCRIPTOR RUNTIME CLASS
//==============================================================================

class audio_descriptor_runtime
{
public:

                            audio_descriptor_runtime ( void );
                           ~audio_descriptor_runtime ( void );

            void            Init                    ( audio_runtime&            Runtime,
                                                      audio_package_registry&   Packages );
            void            Kill                    ( void );

            f32             GetLengthSeconds        ( const char*               pIdentifier );
            f32             GetDescriptorLengthSeconds
                                                    ( u16*                      pDescriptor,
                                                      audio_package*            pPackage );

            s32             AppendDescriptor        ( f32                       BaseTime,
                                                      u16*                      pDescriptor,
                                                      voice*                    pVoice,
                                                      audio_package*            pPackage );

            s32             IsCold                  ( char*                     pIdentifier );
            s32             IsDescriptorCold        ( u16*                      pDescriptor,
                                                      audio_package*            pPackage );

            void            GetVoiceParameters      ( uncompressed_parameters*  pParams,
                                                      u16*                      pDescriptor,
                                                      audio_package*            pPackage,
                                                      char*                     pDescriptorName );
            xbool           GetVoiceParameters      ( const char*               pIdentifier,
                                                      uncompressed_parameters&  Params );
            u32             GetUserData             ( const char*               pIdentifier );
            s32             GetPriority             ( const char*               pIdentifier );
            f32             GetFarFalloff           ( const char*               pIdentifier );
            f32             GetNearFalloff          ( const char*               pIdentifier );

private:

            class           descriptor_visitor;
            class           append_descriptor_visitor;
            class           cold_descriptor_visitor;
            class           length_descriptor_visitor;

inline      audio_runtime&  Runtime                 ( void ) { ASSERT( m_pRuntime ); return *m_pRuntime; }
inline      audio_package_registry& Packages        ( void ) { ASSERT( m_pPackages ); return *m_pPackages; }

            void            DecodeParameters        ( uncompressed_parameters*  pParams,
                                                      u16*                      pDescriptor );
            void            GetElementParameters    ( uncompressed_parameters*  pParams,
                                                      u16*                      pDescriptor,
                                                      voice*                    pVoice );

            s32             WalkDescriptor          ( f32                       BaseTime,
                                                      u16*                      pDescriptor,
                                                      audio_package*            pPackage,
                                                      descriptor_visitor&       Visitor,
                                                      u32&                      RecursionDepth );
            s32             WalkSimple              ( f32                       BaseTime,
                                                      u16*                      pDescriptor,
                                                      audio_package*            pPackage,
                                                      descriptor_visitor&       Visitor,
                                                      u32&                      RecursionDepth );
            s32             WalkComplex             ( f32                       BaseTime,
                                                      u16*                      pDescriptor,
                                                      audio_package*            pPackage,
                                                      descriptor_visitor&       Visitor,
                                                      u32&                      RecursionDepth );
            s32             WalkRandomList          ( f32                       BaseTime,
                                                      u16*                      pDescriptor,
                                                      audio_package*            pPackage,
                                                      descriptor_visitor&       Visitor,
                                                      u32&                      RecursionDepth );
            s32             WalkWeightedList        ( f32                       BaseTime,
                                                      u16*                      pDescriptor,
                                                      audio_package*            pPackage,
                                                      descriptor_visitor&       Visitor,
                                                      u32&                      RecursionDepth );
            s32             WalkElement             ( f32                       BaseTime,
                                                      u16*                      pDescriptor,
                                                      audio_package*            pPackage,
                                                      descriptor_visitor&       Visitor,
                                                      u32&                      RecursionDepth );
            f32             GetSampleLengthSeconds  ( index_type                Type,
                                                      u32                       Index,
                                                      audio_package*            pPackage );
            s32             AppendHot               ( u32                       Index,
                                                      f32                       DeltaTime,
                                                      u16*                      pDescriptor,
                                                      voice*                    pVoice,
                                                      audio_package*            pPackage );
            s32             AppendCold              ( u32                       Index,
                                                      f32                       DeltaTime,
                                                      u16*                      pDescriptor,
                                                      voice*                    pVoice,
                                                      audio_package*            pPackage );

private:

            audio_runtime*          m_pRuntime;
            audio_package_registry* m_pPackages;
};

//==============================================================================
#endif // AUDIO_DESCRIPTOR_RUNTIME_HPP
//==============================================================================
