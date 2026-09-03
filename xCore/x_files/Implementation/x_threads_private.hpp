//==============================================================================
//
//  x_threads_private.hpp
//
//  Machine-specific declarations used by the threading system.
//
//==============================================================================

#ifndef X_THREADS_PRIVATE_HPP
#define X_THREADS_PRIVATE_HPP

#if defined( TARGET_WINDOWS )
#include <windows.h>
#define X_THREAD_BOOT_DECL unsigned __stdcall
#elif defined( TARGET_POSIX )
#define X_THREAD_BOOT_DECL void
#else
#define X_THREAD_BOOT_DECL void
#endif

typedef X_THREAD_BOOT_DECL x_thread_boot_fn( void* );

struct xthread_private
{
    s32 ThreadId;

#if defined( TARGET_WINDOWS )
    HANDLE Handle;
    HANDLE SuspendSemaphore;
    s32    SemaphoreCount;
#elif defined( TARGET_POSIX )
    void* Handle;
#endif

    xbool operator==( xthread_private const& Right ) const
    {
        return ThreadId == Right.ThreadId;
    }

    xbool operator!=( xthread_private const& Right ) const
    {
        return ThreadId != Right.ThreadId;
    }
};

// Thread creation
xthread_private sys_thread_Create( x_thread_boot_fn* pEntry,
                                   void*             pParam,
                                   void*             pStack,
                                   s32               StackSize,
                                   s32               InitialPriority );
void            sys_thread_Start( xthread_private& Private, void* pArg );
void            sys_thread_Destroy( xthread_private& Private );
void            sys_thread_Exit( s32 ExitCode );

// Thread execution control
void sys_thread_Delay( s32 Milliseconds );
void sys_thread_Suspend( xthread_private& Private, s32 Flags );
void sys_thread_Resume( xthread_private& Private, s32 Flags );

// Thread priority control
xthread_private sys_thread_GetId( void );
void            sys_thread_SetPriority( xthread_private& Private, s32 AbsolutePriority );

// System context switch control
void sys_thread_Lock( void );
void sys_thread_Unlock( void );

#endif // X_THREADS_PRIVATE_HPP
