//==============================================================================
//
//  audio_channel_mgr.hpp
//
//==============================================================================

#ifndef AUDIO_CHANNEL_MGR_HPP
#define AUDIO_CHANNEL_MGR_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Audio/audio_types.hpp"

//==============================================================================
//  STRUCTS
//==============================================================================

struct audio_runtime;

//==============================================================================
//  AUDIO CHANNEL MGR CLASS
//==============================================================================

class audio_channel_mgr
{
public:

                        audio_channel_mgr   ( void );
                       ~audio_channel_mgr   ( void );
        
        void            Init                ( audio_runtime& Runtime );
        void            Kill                ( void );

        xbool           Acquire             ( element*      pElement );
        xbool           Release             ( channel*      pChannel );
                                             
        void            Start               ( channel*      pChannel );
        void            Pause               ( channel*      pChannel );
        void            Resume              ( channel*      pChannel );
        xbool           IsPlaying           ( channel*      pChannel );

        s32             GetPriority         ( channel*      pChannel );
        void            SetPriority         ( channel*      pChannel,
                                              s32           Priority );  

        f32             GetVolume           ( channel*      pChannel );
        void            SetVolume           ( channel*      pChannel,
                                              f32           Volume );

        void            GetPan              ( channel*      pChannel,
                                              vector4&      Pan);
        void            SetPan              ( channel*      pChannel,
                                              vector4&      Pan );

        f32             GetPitch            ( channel*      pChannel );
        void            SetPitch            ( channel*      pChannel,
                                              f32           Pitch );

        f32             GetEffectSend       ( channel*      pChannel );
        void            SetEffectSend       ( channel*      pChannel,
                                              f32           EffectSend );

        void            Update              ( void );

inline  channel*        FreeList            ( void )        { return &m_FreeChannels; }
inline  channel*        UsedList            ( void )        { return &m_UsedChannels; }

private:

inline  audio_runtime&  Runtime             ( void ) { ASSERT( m_pRuntime ); return *m_pRuntime; }

        void            UpdatePriorityList  ( channel* pChannel, xbool RemoveFromFreeList );
        xbool           Free                ( channel* pChannel, xbool PutInFreeList, xbool FreeParent );

private:
        
        channel         m_FreeChannels;
        channel         m_UsedChannels;
        audio_runtime*  m_pRuntime;
};

//==============================================================================
#endif // AUDIO_CHANNEL_MGR_HPP
//==============================================================================
