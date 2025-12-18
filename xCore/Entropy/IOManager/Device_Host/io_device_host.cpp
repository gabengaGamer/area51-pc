//==============================================================================
//
//  io_device_host.cpp
//
//  Host filesystem device entry points.
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "..\io_mgr.hpp"
#include "..\io_filesystem.hpp"
#include "io_device_host.hpp"
#include "x_log.hpp"

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

io_device_host g_IODeviceHost;

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

io_device_host::io_device_host( void )
{
    m_pLastFile  = NULL;
    m_LastOffset = -1;
    m_LastLength = 0;
    m_nSeeks     = 0;
}

//==============================================================================

io_device_host::~io_device_host( void )
{
}

//==============================================================================

#if !defined(X_RETAIL)
void io_device_host::LogDeviceRead( io_device_file* pFile, s32 Length, s32 Offset )
{
    (void)pFile;
    (void)Length;
    (void)Offset;

#ifdef LOG_DEVICE_SEEK
    // Same file?
    if( m_pLastFile != pFile )
    {
        LOG_MESSAGE( LOG_PHYSICAL_SEEK, "SEEK! Different File: %s", pFile->Filename );
        m_nSeeks++;
    }
    // Need to seek?
    else if( Offset != (m_LastOffset+m_LastLength) )
    {
        LOG_MESSAGE( LOG_PHYSICAL_SEEK, "SEEK! Prev Offset: %d, Len: %d, Curr Offset: %d, Len: %d, Delta %d", m_LastOffset, m_LastLength, Offset, Length, Offset-m_LastOffset );
        m_nSeeks++;
    }

    // Update 'em
    m_pLastFile  = pFile;
    m_LastOffset = Offset;
    m_LastLength = Length;
#endif // LOG_DEVICE_SEEK

#ifdef LOG_DEVICE_READ
    LOG_MESSAGE( LOG_PHYSICAL_READ, "READ! File: %s, Offset: %d, Length: %d", pFile->Filename, Offset, Length );
#endif // LOG_DEVICE_READ
}

//==============================================================================

void io_device_host::LogDeviceWrite( io_device_file* pFile, s32 Length, s32 Offset )
{
    (void)pFile;
    (void)Length;
    (void)Offset;

#ifdef LOG_DEVICE_WRITE
    LOG_MESSAGE( LOG_PHYSICAL_WRITE, "WRITE! File: %s, Offset: %d, Length: %d", pFile->Filename, Offset, Length );
#endif
}
#endif
