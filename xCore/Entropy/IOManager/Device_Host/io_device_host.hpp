//==============================================================================
//
//  io_device_host.hpp
//
//  Host filesystem device entry points.
//
//==============================================================================

#ifndef IO_DEVICE_HOST_HPP
#define IO_DEVICE_HOST_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "..\io_device.hpp"
#include "..\io_filesystem.hpp"

//==============================================================================
//  IO DEVICE HOST CLASS
//==============================================================================

class io_device_host : public io_device
{

private:

    io_device_file*         m_pLastFile;        // Last file read from (logging).
    s32                     m_LastOffset;       // Offset of last read (logging).
    s32                     m_LastLength;       // Length of last read (logging).
    s32                     m_nSeeks;           // Number of seeks (logging).

public:

    virtual                ~io_device_host              ( void );
                            io_device_host              ( void );
    virtual void            Init                        ( void );
    virtual void            Kill                        ( void );

private:

#if !defined(X_RETAIL)
    void                     LogDeviceRead               ( io_device_file* pFile, s32 Length, s32 Offset );
    void                     LogDeviceWrite              ( io_device_file* pFile, s32 Length, s32 Offset );
#endif        
    virtual device_data*     GetDeviceData               ( void );
    virtual void             CleanFilename               ( char* pClean, const char* pFilename );
    virtual xbool            DeviceOpen                  ( const char* pFilename, io_device_file* pFile, io_device::open_flags OpenFlags );
    virtual xbool            DeviceRead                  ( io_device_file* pFile, void* pBuffer, s32 Length, s32 Offset, s32 AddressSpace );
    virtual xbool            DeviceWrite                 ( io_device_file* pFile, void* pBuffer, s32 Length, s32 Offset, s32 AddressSpace );
    virtual void             DeviceClose                 ( io_device_file* pFile );

};

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

extern io_device_host g_IODeviceHost;

//==============================================================================
#endif //IO_DEVICE_HOST_HPP
//==============================================================================