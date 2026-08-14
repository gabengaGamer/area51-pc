//==============================================================================
//
//  io_mgr.hpp
//
//  Core IO manager control and dispatcher.
//
//==============================================================================

#ifndef IO_MGR_HPP
#define IO_MGR_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_types.hpp"
#include "io_device.hpp"

//==============================================================================
//  ENUMS
//==============================================================================

enum devices
{
    IO_DEVICE_HOST = 0,
    NUM_IO_DEVICES,
};

//==============================================================================

class   io_request;

//==============================================================================
//  IO MANAGER CLASS
//==============================================================================

class io_mgr
{

    friend  void            ProcessEndOfRequest     ( io_device* pDevice, s32 Status );
    friend  void            io_dispatch_job         ( void* pData );
    friend  class           io_device;
    friend  class           io_cache;
    friend  class           io_request;
    friend  class           io_fs;

public:
private:

    enum                        { DISPATCHER_QUEUE_SIZE = 16 };

    io_device*                  m_Devices[ NUM_IO_DEVICES ];
    io_request*                 m_DispatcherRequests[ DISPATCHER_QUEUE_SIZE ];
    volatile s32                m_DispatcherRead;
    volatile s32                m_DispatcherWrite;
    volatile s32                m_DispatcherCount;
    volatile xbool              m_DispatcherJobActive;
    volatile xbool              m_DispatcherKilling;

    xbool           QueueDispatcherRequest  ( io_request* pRequest, xbool IsCompletion );
    xbool           PopDispatcherRequest    ( io_request*& pRequest );
    xbool           StartDispatcher         ( void );
    void            DispatchRequests        ( void );
    void            DispatchRequest         ( io_request* pRequest );

public:

                    io_mgr                  ( void );
                   ~io_mgr                  ( void );
    s32             Init                    ( void );
    s32             Kill                    ( void );
    s32             QueueRequest            ( io_request* pRequest );
    s32             CancelRequest           ( io_request* pRequest );
    io_device_file* OpenDeviceFile          ( const char* pFileName, s32 DeviceIndex, io_device::open_flags );
    void            CloseDeviceFile         ( io_device_file* pFile );
    s32             GetDeviceQueueStatus    ( s32 Device ) const;
    void            SetDevicePathPrefix     ( const char* pPrefix, s32 DeviceIndex );
    void            GetDevicePathPrefix     ( char* pBuffer, s32 DeviceIndex );
};

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

extern io_mgr g_IoMgr;

//==============================================================================
#endif // IO_MGR_HPP
//==============================================================================
