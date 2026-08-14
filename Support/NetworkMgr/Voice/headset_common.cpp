//==============================================================================
//
//  Headset_Common.cpp
//
//==============================================================================

#include "x_types.hpp"
#include "x_log.hpp"
#include "x_string.hpp"

#include "Headset.hpp"

//==============================================================================

void headset::ProvideUpdate( netstream& BitStream, s32 MaxLength )
{
    m_ReadFifo.ProvideUpdate( BitStream, MaxLength, m_EncodeBlockSize );
}

//==============================================================================

void headset::AcceptUpdate( netstream& BitStream )
{
    m_WriteFifo.AcceptUpdate( BitStream, m_EncodeBlockSize );
}

//==============================================================================

void headset::UpdateLoopBack( void )
{
    if( m_LoopbackEnabled == TRUE )
    {
        char Buffer[ 256 ];

        if( m_ReadFifo.Remove( Buffer, m_EncodeBlockSize, m_EncodeBlockSize ) )
        {
            m_WriteFifo.Insert( Buffer, m_EncodeBlockSize, m_EncodeBlockSize );
        }
    }
}

//==============================================================================

s32 headset::Read( void* pBuffer, s32 Length )
{
    s32     Available;

    if( (pBuffer == NULL) ||
        (Length <= 0) ||
        (m_EncodeBlockSize <= 0) )
    {
        return 0;
    }

    Available = m_ReadFifo.GetBytesUsed();
    Available = MIN( Available, Length );
    Available -= Available % m_EncodeBlockSize;

    if( (Available <= 0) ||
        !m_ReadFifo.Remove( pBuffer, Available, m_EncodeBlockSize ) )
    {
        return 0;
    }

    return Available;
}

//==============================================================================

s32 headset::Write( const void* pBuffer, s32 Length )
{
    if( (pBuffer == NULL) ||
        (Length <= 0) ||
        (m_EncodeBlockSize <= 0) ||
        ((Length % m_EncodeBlockSize) != 0) )
    {
        return 0;
    }

    if( !m_WriteFifo.Insert( pBuffer, Length, m_EncodeBlockSize ) )
    {
        return 0;
    }

    return Length;
}

//==============================================================================

void headset::ClearReadFifo( void )
{
    m_ReadFifo.Clear();
}

//==============================================================================

void headset::ClearWriteFifo( void )
{
    m_WriteFifo.Clear();
}

//==============================================================================

void headset::UpdateTalkingState( void )
{
    m_IsTalking = m_TalkingRequested && (m_HeadsetCount > 0);
}

//==============================================================================

void headset::SetTalking( xbool IsTalking )
{
    m_TalkingRequested = IsTalking;
    UpdateTalkingState();
}

//==============================================================================

void headset::SetVolume( f32 Headset, f32 MicrophoneSensitivity )
{
    m_HeadsetVolume         = MAX( 0.0f, MIN( 1.0f, Headset ) );
    m_MicrophoneSensitivity = MAX( 0.0f, MIN( 1.0f, MicrophoneSensitivity ) );
    m_VolumeChanged = TRUE;

}

//==============================================================================

void headset::GetVolume( f32& Headset, f32& Microphone )
{
    Headset = m_HeadsetVolume;
    Microphone = m_MicrophoneSensitivity;
}

//==============================================================================

s32 headset::GetNumBytesInWriteFifo( void )
{
    return( m_WriteFifo.GetBytesUsed() );
}

//==============================================================================

void headset::SetActiveHeadset( s32 HeadsetIndex )
{
    if( HeadsetIndex != m_ActiveHeadset )
    {
        //
        // If the old headset was there, then we tell the system it has been removed
        //
        if( (m_ActiveHeadset != -1) && (m_HeadsetMask & (1<<m_ActiveHeadset)) )
        {
            OnHeadsetRemove();
        }

        m_ActiveHeadset = HeadsetIndex;
        //
        // If the new headset is present, insert it.
        //
        if( (m_ActiveHeadset != -1) && (m_HeadsetMask & (1<<m_ActiveHeadset)) )
        {
            OnHeadsetInsert();
        }
    }
}
