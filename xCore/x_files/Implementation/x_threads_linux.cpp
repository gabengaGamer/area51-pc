//==============================================================================
//
//  x_threads_linux.cpp
//
//==============================================================================

// pthread_timedjoin_np is a Linux extension declared with _GNU_SOURCE.
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "../x_types.hpp"

#if !defined(TARGET_LINUX)
#error "This is only for the Linux target platform. Please check build exclusion rules"
#endif

#include "../x_threads.hpp"
#include "x_threads_private.hpp"

#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

//==============================================================================
// TYPES
//==============================================================================

struct linux_thread_state
{
    pthread_t          Thread;
    sem_t              StartSemaphore;
    sem_t              SuspendSemaphore;
    x_thread_boot_fn*  pEntry;
    void*              pParam;
    s32                ThreadId;
    s32                InitialPriority;
};

//==============================================================================
// DEFINES
//==============================================================================

static const s32 THREAD_DESTROY_GRACE_MS = 100;

//==============================================================================
// DATA
//==============================================================================

static std::atomic<s32> s_NextThreadId( 1 );
static thread_local s32 s_CurrentThreadId = 0;

static pthread_mutex_t s_Critical;
static pthread_once_t  s_CriticalOnce = PTHREAD_ONCE_INIT;

//==============================================================================
// HELPER FUNCTIONS
//==============================================================================

static 
s32 AllocateThreadId( void )
{
    return s_NextThreadId.fetch_add( 1, std::memory_order_relaxed );
}

//==============================================================================

static 
s32 GetCurrentThreadId( void )
{
    if( s_CurrentThreadId == 0 )
        s_CurrentThreadId = AllocateThreadId();

    return s_CurrentThreadId;
}

//==============================================================================

static 
void WaitForSemaphore( sem_t& Semaphore )
{
    while( sem_wait( &Semaphore ) != 0 )
    {
        if( errno != EINTR )
            break;
    }
}

//==============================================================================

static 
void InitializeCritical( void )
{
    pthread_mutexattr_t Attributes;
    pthread_mutexattr_init( &Attributes );
    pthread_mutexattr_settype( &Attributes, PTHREAD_MUTEX_RECURSIVE );
    pthread_mutex_init( &s_Critical, &Attributes );
    pthread_mutexattr_destroy( &Attributes );
}

//==============================================================================

static 
xbool GetJoinDeadline( timespec& Deadline )
{
    if( clock_gettime( CLOCK_REALTIME, &Deadline ) != 0 )
        return FALSE;

    Deadline.tv_sec  += THREAD_DESTROY_GRACE_MS / 1000;
    Deadline.tv_nsec += (THREAD_DESTROY_GRACE_MS % 1000) * 1000000L;
    if( Deadline.tv_nsec >= 1000000000L )
    {
        Deadline.tv_sec++;
        Deadline.tv_nsec -= 1000000000L;
    }

    return TRUE;
}

//==============================================================================

static 
pid_t GetSystemThreadId( void )
{
    return (pid_t)syscall( SYS_gettid );
}

//==============================================================================

static 
s32 GetNicePriority( s32 AbsolutePriority )
{
    s32 NicePriority = THREAD_BASE_PRIORITY - AbsolutePriority;

    if( NicePriority < -20 )
        NicePriority = -20;
    if( NicePriority > 19 )
        NicePriority = 19;

    return NicePriority;
}

//==============================================================================

static 
xbool SetCurrentNicePriority( s32 AbsolutePriority )
{
    const s32 NicePriority = GetNicePriority( AbsolutePriority );
    return (setpriority( PRIO_PROCESS,
                         (id_t)GetSystemThreadId(),
                         (int)NicePriority ) == 0);
}

//==============================================================================

static 
xbool SetSchedulingPolicy( pthread_t Thread, s32 AbsolutePriority, int& AppliedPolicy )
{
    sched_param Parameters;
    Parameters.sched_priority = 0;
    AppliedPolicy = (AbsolutePriority < THREAD_BASE_PRIORITY) ? SCHED_IDLE : SCHED_OTHER;

    int Result = pthread_setschedparam( Thread, AppliedPolicy, &Parameters );
    if( (Result != 0) && (AppliedPolicy == SCHED_IDLE) )
    {
        // SCHED_IDLE is available on supported Linux kernels, but retain a
        // normal-scheduler fallback for older or restricted environments.
        AppliedPolicy = SCHED_OTHER;
        Result = pthread_setschedparam( Thread, AppliedPolicy, &Parameters );
    }

    return (Result == 0);
}

//==============================================================================

static 
void ApplyThreadPriority( pthread_t Thread, s32 AbsolutePriority )
{
    int AppliedPolicy;
    if( SetSchedulingPolicy( Thread, AbsolutePriority, AppliedPolicy ) == FALSE )
    {
        return;
    }

    if( (AppliedPolicy == SCHED_OTHER) && pthread_equal( Thread, pthread_self() ) )
    {
        // Raising priority requires CAP_SYS_NICE or a suitable RLIMIT_NICE.
        // If it is unavailable, keep the scheduler policy and inherited nice.
        SetCurrentNicePriority( AbsolutePriority );
    }
}

//==============================================================================

static 
xbool JoinThread( pthread_t Thread )
{
    timespec Deadline;
    if( GetJoinDeadline( Deadline ) == FALSE )
    {
        return FALSE;
    }

    return (pthread_timedjoin_np( Thread, NULL, &Deadline ) == 0);
}

//==============================================================================

static 
void* ThreadEntry( void* pData )
{
    linux_thread_state* pState = (linux_thread_state*)pData;
    s_CurrentThreadId = pState->ThreadId;
    ApplyThreadPriority( pState->Thread, pState->InitialPriority );
    WaitForSemaphore( pState->StartSemaphore );
    pState->pEntry( pState->pParam );
    return NULL;
}

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

void sys_thread_Delay( s32 Milliseconds )
{
    timespec Remaining;
    Remaining.tv_sec  = Milliseconds / 1000;
    Remaining.tv_nsec = (Milliseconds % 1000) * 1000000L;

    while( nanosleep( &Remaining, &Remaining ) != 0 )
    {
        if( errno != EINTR )
            break;
    }
}

//==============================================================================

xthread_private sys_thread_GetId( void )
{
    xthread_private Private;
    Private.ThreadId = GetCurrentThreadId();
    Private.Handle   = NULL;
    return Private;
}

//==============================================================================

void sys_thread_SetPriority( xthread_private& Private, s32 AbsolutePriority )
{
    // Linux has no useful static priority range for SCHED_OTHER. Map lower
    // x_files priorities to SCHED_IDLE and use nice values for normal/high
    // priorities without entering a privileged real-time scheduling policy.
    linux_thread_state* pState = (linux_thread_state*)Private.Handle;
    if( pState )
    {
        pState->InitialPriority = AbsolutePriority;
        ApplyThreadPriority( pState->Thread, AbsolutePriority );
        return;
    }

    ApplyThreadPriority( pthread_self(), AbsolutePriority );
}

//==============================================================================

void sys_thread_Suspend( xthread_private& Private, s32 Flags )
{
    (void)Flags;
    ASSERTS( (Flags & X_TH_INTERRUPT) == 0, "Cannot sleep in an interrupt context" );

    linux_thread_state* pState = (linux_thread_state*)Private.Handle;
    if( pState )
        WaitForSemaphore( pState->SuspendSemaphore );
}

//==============================================================================

void sys_thread_Resume( xthread_private& Private, s32 Flags )
{
    (void)Flags;

    linux_thread_state* pState = (linux_thread_state*)Private.Handle;
    if( pState )
        sem_post( &pState->SuspendSemaphore );
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
    Private.ThreadId = 0;
    Private.Handle   = NULL;

    if( !pEntry )
        return sys_thread_GetId();

    linux_thread_state* pState = (linux_thread_state*)std::malloc( sizeof(linux_thread_state) );
    if( !pState )
        return Private;

    pState->pEntry   = pEntry;
    pState->pParam   = pParam;
    pState->ThreadId = AllocateThreadId();
    pState->InitialPriority = InitialPriority;

    const xbool StartInitialized   = (sem_init( &pState->StartSemaphore, 0, 0 ) == 0);
    const xbool SuspendInitialized = (StartInitialized && (sem_init( &pState->SuspendSemaphore, 0, 0 ) == 0));
    if( !SuspendInitialized )
    {
        if( StartInitialized )
            sem_destroy( &pState->StartSemaphore );
        std::free( pState );
        return Private;
    }

    pthread_attr_t Attributes;
    pthread_attr_init( &Attributes );
    if( StackSize > 0 )
        pthread_attr_setstacksize( &Attributes, (size_t)StackSize );

    const int CreateResult = pthread_create( &pState->Thread, &Attributes, ThreadEntry, pState );
    pthread_attr_destroy( &Attributes );

    if( CreateResult != 0 )
    {
        sem_destroy( &pState->SuspendSemaphore );
        sem_destroy( &pState->StartSemaphore );
        std::free( pState );
        return Private;
    }

    Private.ThreadId = pState->ThreadId;
    Private.Handle   = pState;
    return Private;
}

//==============================================================================

void sys_thread_Start( xthread_private& Private, void* pArg )
{
    (void)pArg;

    linux_thread_state* pState = (linux_thread_state*)Private.Handle;
    if( pState )
        sem_post( &pState->StartSemaphore );
}

//==============================================================================

void sys_thread_Destroy( xthread_private& Private )
{
    linux_thread_state* pState = (linux_thread_state*)Private.Handle;
    if( !pState )
    {
        Private.ThreadId = 0;
        return;
    }

    if( !pthread_equal( pState->Thread, pthread_self() ) )
    {
        // The common destructor already gave the worker time to terminate
        // cooperatively. Never turn that timeout back into an infinite wait.
        const xbool Joined = JoinThread( pState->Thread );
        ASSERTS( Joined, "Linux thread did not terminate before native join timeout." );
        if( Joined == FALSE )
        {
            std::abort();
        }
    }
    else
    {
        // A thread cannot join itself. Detach it before releasing the state.
        const int DetachResult = pthread_detach( pState->Thread );
        ASSERTS( DetachResult == 0, "Could not detach the current Linux thread." );
    }

    sem_destroy( &pState->SuspendSemaphore );
    sem_destroy( &pState->StartSemaphore );
    std::free( pState );

    Private.ThreadId = 0;
    Private.Handle   = NULL;
}

//==============================================================================

void sys_thread_Lock( void )
{
    pthread_once( &s_CriticalOnce, InitializeCritical );
    pthread_mutex_lock( &s_Critical );
}

//==============================================================================

void sys_thread_Unlock( void )
{
    pthread_mutex_unlock( &s_Critical );
}

//==============================================================================

void sys_thread_Exit( s32 ReturnCode )
{
    pthread_exit( (void*)(intptr_t)ReturnCode );
}
