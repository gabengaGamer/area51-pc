//==============================================================================
//
//  io_device_host_pc.cpp
//
//  Host filesystem device layer for PC.
//
//==============================================================================

//==============================================================================
//  PLATFORM CHECK
//==============================================================================

#include "x_types.hpp"

#if !defined(TARGET_PC)
#error "This is only for the PC target platform. Please check build exclusion rules"
#endif

//==============================================================================
//  INCLUDES
//==============================================================================

#include "..\io_mgr.hpp"
#include "..\io_filesystem.hpp"
#include "x_memory.hpp"
#include "x_math.hpp"
#include "io_device_host.hpp"
#include <stdio.h>

//==============================================================================
//  HOSTFS: DEFINES, BUFFERS, ETC...
//==============================================================================

#define HOSTFS_CACHE_SIZE    (32*1024)
#define HOSTFS_NUM_FILES     (32)                        // TODO: CJG - Increase this to accomodate multiple CDFS's
#define HOSTFS_INFO_SIZE     (32)
#define HOSTFS_CACHE         ((void*)s_HostCache)
#define HOSTFS_FILES         ((void*)s_HostFiles)
#define HOSTFS_BUFFER_ALIGN  (32)
#define HOSTFS_OFFSET_ALIGN  (4)
#define HOSTFS_LENGTH_ALIGN  (32)

static char           s_HostCache[ HOSTFS_CACHE_SIZE ] GCN_ALIGNMENT(HOSTFS_BUFFER_ALIGN);
static io_device_file s_HostFiles[ HOSTFS_NUM_FILES ];

//==============================================================================
// DEVICE DEFINITION.
//==============================================================================

io_device::device_data s_DeviceData =
{
    "PC HostFS",        // Name
    TRUE,               // IsSupported
    TRUE,               // IsReadable
    FALSE,              // IsWriteable
    HOSTFS_CACHE_SIZE,  // CacheSize
    HOSTFS_BUFFER_ALIGN,// BufferAlign
    HOSTFS_OFFSET_ALIGN,// OffsetAlign
    HOSTFS_LENGTH_ALIGN,// LengthAlign
    HOSTFS_NUM_FILES,   // NumFiles
    HOSTFS_INFO_SIZE,   // InfoSize
    HOSTFS_CACHE,       // pCache
    HOSTFS_FILES        // pFilesBuffer
};

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

static void ReadCallback( s32 Result, void* pFileInfo )
{
    (void)pFileInfo;

    // We are in the callback
    g_IODeviceHost.EnterCallback();

    // Success?
    if( Result >= 0 )
    {
        // Its all good!
        ProcessEndOfRequest( &g_IODeviceHost, io_request::COMPLETED );
    }
    else
    {
        // Ack failed!
        ProcessEndOfRequest( &g_IODeviceHost, io_request::FAILED );
    }

    // Done with callback
    g_IODeviceHost.LeaveCallback();
}

//==============================================================================

void io_device_host::CleanFilename( char* pClean, const char* pFilename )
{
    // Gotta fit.
    ASSERT( x_strlen(pFilename) + x_strlen(m_Prefix) < IO_DEVICE_FILENAME_LIMIT );

    // If hard disk drive is specified, do NOT prepend the prefix.
    if( strnicmp( pFilename, "C:", 2 ) != 0 )
    {
        // Pre-pend the prefix.
        x_strcpy( pClean, m_Prefix );
    }
    else
    {
        *pClean = 0;
    }
    
    // Move to end of string.
    pClean += x_strlen( pClean );

    // Now clean it.
    while( *pFilename )
    {
        if( (*pFilename == '\\') || (*pFilename == '/') )
        {
            *pClean++ = '\\';
            pFilename++;

            while( *pFilename && ((*pFilename == '\\') || (*pFilename == '/')) )
                pFilename++;
        }
        else
        {
            *pClean++ = *pFilename++;
        }
    }

    // Terminate it.
    *pClean = 0;
}

//==============================================================================

void io_device_host::Init( void )
{
    // Base class initialization
    io_device::Init();

    // Set default path so current apps will function without modification
    x_strcpy( m_Prefix, "C:\\GAMEDATA\\A51\\RELEASE\\PC\\DVD\\" );
}

//==============================================================================

void io_device_host::Kill( void )
{
    io_device::Kill();
}

//==============================================================================

io_device::device_data* io_device_host::GetDeviceData( void )
{
    return &s_DeviceData;
}

//==============================================================================

xbool io_device_host::DeviceOpen( const char* pFilename, io_device_file* pFile, open_flags OpenFlags )
{
    (void)OpenFlags;
    // Clean the filename.
    char CleanFile[IO_DEVICE_FILENAME_LIMIT];
    CleanFilename( CleanFile, (char *)pFilename );

    pFile->Handle = fopen(CleanFile,"rb");
    // Open the file on the host filesystem.
    if( pFile->Handle )
    {
        // Get the length of the file.
        fseek( (FILE*)pFile->Handle, 0, SEEK_END );
        pFile->Length = ftell( (FILE*)pFile->Handle );
        fseek( (FILE*)pFile->Handle,0,SEEK_SET );
        // Woot!
        return TRUE;
    }

    return FALSE;
}

//==============================================================================

xbool io_device_host::DeviceRead( io_device_file* pFile, void* pBuffer, s32 Length, s32 Offset, s32 AddressSpace )
{
    s32 ReadLength;
    (void)AddressSpace;
#if !defined(X_RETAIL)
    // Log the read.
    LogDeviceRead( pFile, Length, Offset );   
#endif    

    fseek( (FILE*)pFile->Handle, Offset, SEEK_SET );
    ReadLength = fread(pBuffer,1,Length,(FILE*)pFile->Handle);
    ReadCallback((ReadLength==Length),pFile->pHardwareData);

    // Tell the world.
    return ReadLength == Length;
}

//==============================================================================

xbool io_device_host::DeviceWrite( io_device_file* pFile, void* pBuffer, s32 Length, s32 Offset, s32 AddressSpace )
{
    (void)pFile;
    (void)pBuffer;
    (void)Length;
    (void)Offset;
    (void)AddressSpace;
    ASSERT( 0 );
    return 0;
}

//==============================================================================

void io_device_host::DeviceClose ( io_device_file* pFile )
{
    fclose((FILE*)pFile->Handle);
}
