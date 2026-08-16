//==============================================================================
//
//  x_threads.hpp
//
//==============================================================================

#ifndef _X_THREADS_HPP
#define _X_THREADS_HPP


#ifdef TARGET_DEV
//#define DEBUG_THREADS
#endif

//==============================================================================
//  
//  This file provides basic, cross platform multithreading capability. It requires
//  only four system dependant functions. The host system needs to provide the 
//  following function sets:
//
//      x_BeginAtomic()
//          - Enters the platform's x_files critical section. On non PC
//            targets this may map to an interrupt/context-switch mask; on PC it is
//            a process-local lock for x_files internal state.
//      x_EndAtomic()
//          - Leaves the platform's x_files critical section.
//      xthread::Suspend()
//          -  Suspends the current thread. Moves it from the system running queue
//             to the system suspended queue. These queues are independent from those
//             maintained internally.
//      xthread::Resume()
//          -  Sets a thread back to a running state. The system can reschedule this
//             thread as it sees fit.
//
//  Each thread can also have an additional set of information associated with it.
//  Things such as text format buffers
//==============================================================================

#include "x_types.hpp"
#include "x_array.hpp"

#include "Implementation/x_files_private.hpp"
#include "Implementation/x_threads_private.hpp"

#define X_MAX_THREADS           16
#if defined(TARGET_PC)
#define THREAD_BASE_PRIORITY    THREAD_PRIORITY_NORMAL
#else
#define THREAD_BASE_PRIORITY    0
#endif

#define X_TH_NOBLOCK           (0<<0)
#define X_TH_BLOCK             (1<<0)
#define X_TH_JAM               (1<<1)
#define X_TH_INTERRUPT         (1<<7)

class xthread;
class xmutex;

// Thread snapshots are diagnostic-only. Keep them out of the default API.
#ifndef X_THREADS_DEBUG
#define X_THREADS_DEBUG        1
#endif

#if X_THREADS_DEBUG
#define X_THREAD_DEBUG_MAX_THREADS X_MAX_THREADS

enum x_thread_debug_failure_type
{
    X_THREAD_DEBUG_FAILURE_NONE = 0,
    X_THREAD_DEBUG_FAILURE_MUTEX_CURRENT_THREAD,
    X_THREAD_DEBUG_FAILURE_MUTEX_ENTER,
    X_THREAD_DEBUG_FAILURE_MUTEX_EXIT_OWNER,
    X_THREAD_DEBUG_FAILURE_MUTEX_SEMAPHORE_RELEASE,
};

struct x_thread_debug_failure
{
    x_thread_debug_failure_type Type;
    u32                         Sequence;
    xmutex*                     pMutex;
    xthread*                    pOwner;
    xthread*                    pCurrent;
    xthread*                    pCachedThread;
    s32                         OwnerThreadId;
    s32                         OwnerSystemId;
    s32                         CurrentThreadId;
    s32                         CurrentSystemId;
    s32                         ActualSystemId;
    s32                         CachedSystemId;
    s32                         EnterCount;
    const char*                 pOwnerName;
    const char*                 pCurrentName;
};

struct x_thread_diagnostics
{
    u32                     Sequence;
    x_thread_debug_failure  FirstFailure;
};

extern x_thread_diagnostics g_ThreadDiagnostics;

struct x_thread_debug_info
{
    s32             ThreadId;
    s32             SystemId;
    s32             Priority;
    s32             Status;
    xbool           NeedToTerminate;
    const char*     pName;
};

struct x_thread_debug_snapshot
{
    xbool               IsInitialized;
    s32                 ActiveThreadId;
    s32                 nThreads;
    x_thread_debug_info Threads[X_THREAD_DEBUG_MAX_THREADS];
};
#endif

//==============================================================================
// THREAD LISTS
// These are just essentially internally to keep track of current threads. There
// is a master RunList which contains all threads ready to execute. Each semaphore
// has two lists, one of tasks waiting to acquire the semaphore and another for
// tasks waiting to release the semaphore
//==============================================================================
// Note: Each thread can exist in multiple queues. We keep one for actual running
// and the other for internal tracking (for example, runlist and global thread list
class xthreadlist
{

public:
                        xthreadlist     (void);
                       ~xthreadlist     (void);
            void        Link            (xthread* pThread);
            void        Unlink          (xthread* pThread);
            xthread*    Find            (s32 Id);
            xthread*    GetHead         (void);
protected:
            xthread*                    m_pHead;
            xthread*                    m_pTail;
};

//==============================================================================
// THREAD CLASS
//==============================================================================
typedef void x_thread_entry_fn      ( s32, char** );
typedef void x_thread_entry_fn_noarg( void );

class xthread
{
public:
    enum state {
            RUNNING,
            SUSPENDED,
            LIMBO,
            TERMINATING,
            TERMINATED,
            BLOCKED_ON_MESSAGE_SEND,
            BLOCKED_ON_MESSAGE_RECV,
            BLOCKED_ON_SEMAPHORE_ACQUIRE,
            BLOCKED_ON_SEMAPHORE_RELEASE,
            BLOCKED_ON_MUTEX,
            BLOCKED_ON_TIMER,
            BLOCKED_ON_UNKNOWN,
    };

                                xthread             (   x_thread_entry_fn*  pEntryPoint, 
                                                        const char*         pName,
                                                        s32                 StackSize, 
                                                        s32                 Priority,
                                                        s32                 argc = 0,
                                                        char**              argv=NULL )
                                                    {
                                                        Init( pEntryPoint, pName, StackSize, Priority, argc, argv );
                                                    }

                                xthread             (   x_thread_entry_fn_noarg*    pEntryPoint, 
                                                        const char*                 pName,
                                                        s32                         StackSize, 
                                                        s32                         Priority )
                                                    {
                                                        Init( (x_thread_entry_fn*)pEntryPoint, pName, StackSize, Priority, 0, NULL );
                                                    }

                                xthread             ( void );
            void                InitIdle            ( void* pUserStack, s32 StackSize );

                                xthread             ( s32 StackSize, const char* pName );
                               ~xthread             ( void );
            void                Delay               ( s32 milliseconds );

            xbool               IsActive            ( void );

            void                SetPriority         ( s32 Priority );
            s32                 GetPriority         ( void );
            s32                 GetId               ( void )          { return m_ThreadId;            };
            s32                 GetSystemId         ( void )          { return m_System.ThreadId;     };
            const char*         GetName             ( void )          { return m_pName;               };
            void                Validate            ( void );

            x_thread_globals*   GetGlobals          ( void )          { return &m_Globals;            };
            void                SetFormatBufferSize ( s32 Size );

static      void                DumpThreads         ( void );
            void                DumpState           ( void );
            void                Init                ( x_thread_entry_fn*    pEntryPoint,
                                                      const char*           pName,
                                                      s32                   StackSize,
                                                      s32                   Priority, 
                                                      s32                   argc,
                                                      char**                argv );
            void                Kill                ( void );

protected:
            void                Unlink              ( xthreadlist& List );              // Remove from specific list    
            void                Unlink              ( void );                           // Remove from run list
            void                Link                ( xthreadlist& List );              // Link to specific list
            void                Link                ( void );                           // Link to runlist
            void                Suspend             ( s32 Flags, state State );
            void                Resume              ( s32 Flags );

            xthread*            m_pPrev;
            xthread*            m_pNext;
            xthreadlist*        m_pOwningQueue;

            xthread_private     m_System;
            xbool               m_NeedToTerminate;
            xbool               m_Initialized;
            s32                 m_ThreadId;
            s32                 m_Priority;
            s32                 m_BasePriority;
            x_thread_globals    m_Globals;
const       char*               m_pName;
            void*               m_pStack;
volatile    state               m_Status;
            struct
            {
                s32                 Sentinal;
                x_thread_entry_fn*  pEntry;
                s32                 argc;
                char**              argv;
            } m_Startup;

friend      X_THREAD_BOOT_DECL  x_thread_Root( void * pParams );
friend      class               xthreadlist;
friend      class               xmesgq;
friend      class               xsema;
friend      void                x_KillThreads( void );
#if X_THREADS_DEBUG
friend      void                x_GetThreadDebugSnapshot( x_thread_debug_snapshot& Snapshot );
#endif
protected:
#ifdef DEBUG_THREADS
static      xarray<xthread*> m_MasterThreadList;
#endif
};

#include "Implementation/x_semaphore_private.hpp"
#include "Implementation/x_mutex_private.hpp"
#include "Implementation/x_mqueue_private.hpp"

//==============================================================================
// THREAD FUNCTIONS MAINLY PERFORMED ON THE CURRENT EXECUTING THREAD
//==============================================================================
            void        x_InitThreads       (s32 argc, char** argv);
            void        x_KillThreads       (void);
            xbool       x_IsThreadSystemReady(void);
            void        x_StartMain         (x_thread_entry_fn* pEntry, s32 argc, char** argv);

            void        x_DelayThread       (s32 milliseconds);
            s32         x_GetThreadID       (void);
            xthread*    x_GetCurrentThread  (void);

// Checks thread system integrity. Should catch deadlocks
            void        x_CheckThreads      (xbool ForceDump);
            void        x_WatchdogReset     (void);
            void        x_ValidateThread    (void);

            void        xprof_Enable        (void);
            void        xprof_Disable       (void);
            void        xprof_Flush         (void);
            void        xprof_SetSampleRate (s32 ms);

            void        x_QueueDebugDump    (void);
            void        x_DumpThreads       (void);
#if X_THREADS_DEBUG
            void        x_GetThreadDebugSnapshot( x_thread_debug_snapshot& Snapshot );
            const char* x_GetThreadStateName( s32 State );
            void        x_ResetThreadDiagnostics( void );
            void        x_RecordThreadDebugFailure( x_thread_debug_failure_type Type,
                                                    xmutex* pMutex,
                                                    xthread* pOwner,
                                                    xthread* pCurrent,
                                                    s32 EnterCount );
#endif
            f32         x_GetCPUUtilization (void);
            s64         x_GetIdleTicks      (void);
            void        x_SetCurrentThread  (xthread* pThread);

//==============================================================================
// System specific functions to enter a mutually exclusive section that should not
// be interrupted. On the PS2 this is done by disabling interrupts, on other
// platforms, it can be done using a system mutex or lock of some kind.
//==============================================================================
            void        x_BeginAtomic       (void);
            xbool       x_IsAtomic          (void);
            void        x_EndAtomic         (void);

#endif
