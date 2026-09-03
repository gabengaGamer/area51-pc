//==============================================================================
//
//  x_threads_windows.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "../x_types.hpp"

#if !defined(TARGET_WINDOWS)
#error "This is only for the PC target platform. Please check build exclusion rules"
#endif

#include "../x_threads.hpp"
#include "x_threads_private.hpp"
#include <process.h>
#include <windows.h>

//==============================================================================
//  DATA
//==============================================================================

static CRITICAL_SECTION s_Critical;
static INIT_ONCE        s_CriticalInitOnce = INIT_ONCE_STATIC_INIT;
static xbool            s_Critical_Initialized = FALSE;

//==============================================================================
//  HELPER FUNCTIONS
//==============================================================================

static 
BOOL CALLBACK sys_thread_InitCritical( PINIT_ONCE pInitOnce,
                                              PVOID       pParameter,
                                              PVOID*      ppContext )
{
    (void)pInitOnce;
    (void)pParameter;
    (void)ppContext;

    if( !InitializeCriticalSectionAndSpinCount( &s_Critical, 0x00000400 ) )
    {
        return FALSE;
    }

    s_Critical_Initialized = TRUE;
    return TRUE;
}

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

void sys_thread_Delay( s32 Milliseconds )
{
    Sleep( Milliseconds );
}

//==============================================================================

xthread_private sys_thread_GetId( void )
{
    xthread_private Private;

    Private.ThreadId         = GetCurrentThreadId();
    Private.Handle           = GetCurrentThread();
    Private.SuspendSemaphore = NULL;
    Private.SemaphoreCount   = 0;
    return Private;
}

//==============================================================================

// Sets thread priority from the x_files absolute priority. Windows uses
// THREAD_BASE_PRIORITY as the normal priority; positive values raise it and
// negative values lower it.

void sys_thread_SetPriority( xthread_private& Private, s32 AbsolutePriority )
{
    SetThreadPriority( Private.Handle, AbsolutePriority - THREAD_BASE_PRIORITY );
}

//==============================================================================

void sys_thread_Suspend( xthread_private& Private, s32 Flags )
{
    (void)Flags;
    ASSERTS( (Flags & X_TH_INTERRUPT) == 0, "Cannot sleep in an interrupt context" );
    ASSERT( Private.SuspendSemaphore );
    VERIFY( WaitForSingleObject( Private.SuspendSemaphore, INFINITE ) == WAIT_OBJECT_0 );
}

//==============================================================================

void sys_thread_Resume( xthread_private& Private, s32 Flags )
{
    (void)Flags;
    ASSERT( Private.SuspendSemaphore );
    VERIFY( ReleaseSemaphore( Private.SuspendSemaphore, 1, NULL ) );
}

//==============================================================================

xthread_private sys_thread_Create( x_thread_boot_fn* pEntry,
                                   void*             pParam,
                                   void*             pStack,
                                   s32               StackSize,
                                   s32               InitialPriority )
{
    (void)pStack;

    xthread_private Private;
    unsigned        Tid = 0;

    Private.ThreadId         = 0;
    Private.Handle           = NULL;
    Private.SuspendSemaphore = NULL;
    Private.SemaphoreCount   = 0;

    Private.SuspendSemaphore = CreateSemaphore( NULL, 0, 128, NULL );
    ASSERT( Private.SuspendSemaphore );

    if( !Private.SuspendSemaphore )
    {
        return Private;
    }

    if( pEntry )
    {
        Private.Handle = (HANDLE)_beginthreadex( NULL,
                                                 (unsigned)StackSize,
                                                 pEntry,
                                                 pParam,
                                                 CREATE_SUSPENDED,
                                                 &Tid );
        Private.ThreadId = (s32)Tid;
        if( Private.Handle )
        {
            sys_thread_SetPriority( Private, InitialPriority );
        }
        else
        {
            CloseHandle( Private.SuspendSemaphore );
            Private.SuspendSemaphore = NULL;
        }
    }
    else
    {
        Private.Handle   = GetCurrentThread();
        Private.ThreadId = GetCurrentThreadId();
    }

    ASSERT( Private.ThreadId );
    ASSERT( Private.Handle );
    return Private;
}

//==============================================================================

void sys_thread_Start( xthread_private& Private, void* pArg )
{
    (void)pArg;

    if( Private.Handle )
    {
        ResumeThread( Private.Handle );
    }
}

//==============================================================================

void sys_thread_Destroy( xthread_private& Private )
{
    if( Private.Handle && (Private.Handle != GetCurrentThread()) )
    {
        CloseHandle( Private.Handle );
    }

    if( Private.SuspendSemaphore )
    {
        CloseHandle( Private.SuspendSemaphore );
    }

    Private.Handle           = NULL;
    Private.SuspendSemaphore = NULL;
    Private.ThreadId         = 0;
    Private.SemaphoreCount   = 0;
}

//==============================================================================

void sys_thread_Lock( void )
{
    VERIFY( InitOnceExecuteOnce( &s_CriticalInitOnce,
                                 sys_thread_InitCritical,
                                 NULL,
                                 NULL ) );
    EnterCriticalSection( &s_Critical );
}

//==============================================================================

void sys_thread_Unlock( void )
{
    ASSERT( s_Critical_Initialized );
    LeaveCriticalSection( &s_Critical );
}

//==============================================================================

void sys_thread_Exit( s32 ReturnCode )
{
    _endthreadex( (unsigned)ReturnCode );
}
