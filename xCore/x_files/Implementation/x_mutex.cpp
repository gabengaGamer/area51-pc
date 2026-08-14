#include "../x_types.hpp"
#include "../x_debug.hpp"
#include "../x_threads.hpp"

//==============================================================================
// This file contains our base level atomic synchronisation primitives. All other
// expanded synchronisation methods (such as message queues or potentially semaphores)
// use these functions to lock a mutex. They are guaranteed to be atomic.
//
// Since we can disable interrupts for short periods of time, it makes sense to
// wrap the target platform signalling with our own since, unless we need to block,
// we will eliminate the need for a system call. If we need to block, we don't
// really care about the time it takes anyway. This will help make it very efficient
// to use message queues for general queuing.

s32 s_AcquiresDone=0;
s32 s_ReleasesDone=0;
s32 s_EntersDone=0;
s32 s_ExitsDone=0;

//==============================================================================
//--- Mutexes are implemented as single entry semaphores. The code optimizations
// for using mutexes in a special case are minimal and not worth it.
//==============================================================================

xmutex::xmutex( void )
       :m_Semaphore( 1, 1 )
{
    m_Initialized = TRUE;
    m_EnterCount  = 0;
    m_pOwner      = NULL;
#ifdef DEBUG_THREADS
    m_MasterMutexList.Append( this );
#endif
}

//==============================================================================

xmutex::~xmutex( void )
{
    ASSERT(m_Initialized);
    ASSERT(m_EnterCount == 0);
    ASSERT(m_pOwner == NULL);
#ifdef DEBUG_THREADS
    m_MasterMutexList.Delete( m_MasterMutexList.Find(this) );
#endif
}

//==============================================================================

xbool xmutex::Enter( s32 Flags )
{
    xthread* pCurrent;

    ASSERT(m_Initialized);
    pCurrent = x_GetCurrentThread();

    if( !pCurrent )
    {
        #if X_THREADS_DEBUG
        x_RecordThreadDebugFailure( X_THREAD_DEBUG_FAILURE_MUTEX_CURRENT_THREAD, this, m_pOwner, NULL, m_EnterCount );
        #endif
        ASSERTS( FALSE, "xmutex::Enter could not identify the current thread" );
        return FALSE;
    }

    x_BeginAtomic();

    if( m_pOwner == pCurrent )
    {
        m_EnterCount++;
        x_EndAtomic();
        return TRUE;
    }

    x_EndAtomic();

    if( !m_Semaphore.Acquire( Flags ) )
    {
        #if X_THREADS_DEBUG
        if( Flags & X_TH_BLOCK )
        {
            x_RecordThreadDebugFailure( X_THREAD_DEBUG_FAILURE_MUTEX_ENTER, this, m_pOwner, pCurrent, m_EnterCount );
        }
        #endif
        return FALSE;
    }

    x_BeginAtomic();
    ASSERT( m_pOwner == NULL );
    ASSERT( m_EnterCount == 0 );
    m_pOwner     = pCurrent;
    m_EnterCount = 1;
    x_EndAtomic();

    return TRUE;
}

//==============================================================================

xbool xmutex::Exit( s32 Flags )
{
    xbool Release;
    xthread* pCurrent;

    ASSERT(m_Initialized);
    x_BeginAtomic();

    pCurrent = x_GetCurrentThread();

    ASSERT(m_EnterCount > 0);

    if( m_pOwner != pCurrent )
    {
        #if X_THREADS_DEBUG
        x_RecordThreadDebugFailure( X_THREAD_DEBUG_FAILURE_MUTEX_EXIT_OWNER, this, m_pOwner, pCurrent, m_EnterCount );
        #endif
        ASSERTS( FALSE, "xmutex::Exit called by a thread that does not own the mutex" );
        x_EndAtomic();
        return FALSE;
    }

    Release = FALSE;
    m_EnterCount--;
    if( m_EnterCount == 0 )
    {
        m_pOwner = NULL;
        Release  = TRUE;
    }

    x_EndAtomic();

    if( Release )
    {
        xbool const Released = m_Semaphore.Release( Flags );
        #if X_THREADS_DEBUG
        if( !Released )
        {
            x_RecordThreadDebugFailure( X_THREAD_DEBUG_FAILURE_MUTEX_SEMAPHORE_RELEASE, this, NULL, pCurrent, 0 );
        }
        #endif
        return Released;
    }

    return TRUE;
}

//==============================================================================

xsema::xsema( s32 count, s32 initial )
{
    ASSERT( count > 0 );
    ASSERT( initial >= 0 );
    ASSERT( initial <= count );

    if( count <= 0 )
        count = 1;

    if( initial < 0 )
        initial = 0;

    if( initial > count )
        initial = count;

    m_Initialized       = TRUE;
    m_Count             = count;
    m_Available         = initial;

#ifdef DEBUG_THREADS
    m_MasterSemaphoreList.Append( this );
#endif
}

//==============================================================================

xsema::~xsema( void )
{
    ASSERT( m_Initialized );
#ifdef DEBUG_THREADS
    m_MasterSemaphoreList.Delete( m_MasterSemaphoreList.Find(this) );
#endif
}

//==============================================================================

xbool xsema::Acquire( s32 Flags )
{
    xthread* pThread;

    ASSERT(m_Initialized);
    x_BeginAtomic();
    s_EntersDone++;

    if( Flags & X_TH_BLOCK )
    {
        xthread *pThread;
        pThread = x_GetCurrentThread();

        while( m_Available==0 )
        {
            pThread->Unlink();
            pThread->Link( m_WaitingAcquire );
            pThread->Suspend( Flags, xthread::BLOCKED_ON_SEMAPHORE_ACQUIRE );
            if( (pThread->IsActive()==FALSE) && (m_Available==0) )
            {
                if( pThread->m_pOwningQueue==&m_WaitingAcquire )
                {
                    pThread->Unlink( m_WaitingAcquire );
                }
                else
                {
                    ASSERT( pThread->m_pOwningQueue == NULL );
                }
                pThread->Link();
                x_EndAtomic();
                return FALSE;
            }
            ASSERT( pThread->m_pOwningQueue==NULL );
            pThread->Link();
        }
    }
    else
    {
        if( m_Available==0 )
        {
            x_EndAtomic();
            return FALSE;
        }
    }

    m_Available--;

    pThread = m_WaitingRelease.GetHead();

    if( pThread )
    {
        s_ReleasesDone++;
        pThread->Unlink( m_WaitingRelease );
        x_EndAtomic();
        pThread->Resume( Flags );
    }
    else
    {
        x_EndAtomic();
    }
    return TRUE;

}

//==============================================================================

xbool xsema::Release(s32 Flags)
{
    xthread* pThread;

    ASSERT(m_Initialized);
    x_BeginAtomic();
    s_ExitsDone++;

    if( Flags & X_TH_BLOCK )
    {
        xthread *pThread;
        pThread = x_GetCurrentThread();

        while( m_Available==m_Count )
        {
            pThread->Unlink();
            pThread->Link( m_WaitingRelease );
            pThread->Suspend( Flags, xthread::BLOCKED_ON_SEMAPHORE_RELEASE );
            if( (pThread->IsActive()==FALSE) && (m_Available==m_Count) )
            {
                if( pThread->m_pOwningQueue==&m_WaitingRelease )
                {
                    pThread->Unlink( m_WaitingRelease );
                }
                else
                {
                    ASSERT( pThread->m_pOwningQueue == NULL );
                }
                pThread->Link();
                x_EndAtomic();
                return FALSE;
            }
            ASSERT( pThread->m_pOwningQueue==NULL );
            pThread->Link();
        }
    }
    else
    {
        if( m_Available==m_Count )
        {
            x_EndAtomic();
            return FALSE;
        }
    }

    m_Available++;

    pThread = m_WaitingAcquire.GetHead();

    if( pThread )
    {
        pThread->Unlink( m_WaitingAcquire );
        s_AcquiresDone++;
        x_EndAtomic();
        pThread->Resume( Flags );
    }
    else
    {
        x_EndAtomic();
    }
    return TRUE;
}
