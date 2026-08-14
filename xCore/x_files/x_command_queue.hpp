//==============================================================================
//
//  x_command_queue.hpp
//
//==============================================================================

#ifndef X_COMMAND_QUEUE_HPP
#define X_COMMAND_QUEUE_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#ifndef X_DEBUG_HPP
#include "x_debug.hpp"
#endif

#ifndef _X_THREADS_HPP
#include "x_threads.hpp"
#endif

//==============================================================================
//  X COMMAND QUEUE
//==============================================================================

template<class T, s32 MAX_CMD_ENTRIES>
class x_command_queue
{
public:
                    x_command_queue    ( void );

    void            Clear              ( void );
    xbool           Push               ( const T& Entry );
    xbool           Pop                ( T& Entry );
    // Blocks until an entry is available or Wake() is called.
    // Returns FALSE only when woken without an entry.
    xbool           PopWait            ( T& Entry );
    xbool           IsEmpty            ( void );
    // Wakes one blocked PopWait() without adding an entry.
    void            Wake               ( void );

private:
                    x_command_queue    ( const x_command_queue& );
    x_command_queue& operator=         ( const x_command_queue& );

    xmutex          m_Lock;
    xsema           m_Signal;
    T               m_Entries[MAX_CMD_ENTRIES];
    s32             m_Read;
    s32             m_Write;
    s32             m_Count;
    xbool           m_WakePending;
};

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

template<class T, s32 MAX_CMD_ENTRIES>
x_command_queue<T, MAX_CMD_ENTRIES>::x_command_queue( void ) :
    m_Signal( MAX_CMD_ENTRIES, 0 )
{
    ASSERT( MAX_CMD_ENTRIES > 0 );
    Clear();
}

//==============================================================================

template<class T, s32 MAX_CMD_ENTRIES>
void x_command_queue<T, MAX_CMD_ENTRIES>::Clear( void )
{
    m_Lock.Acquire();
    m_Read  = 0;
    m_Write = 0;
    m_Count = 0;
    m_WakePending = FALSE;

    while( m_Signal.Acquire( X_TH_NOBLOCK ) )
    {
    }

    m_Lock.Release();
}

//==============================================================================

template<class T, s32 MAX_CMD_ENTRIES>
xbool x_command_queue<T, MAX_CMD_ENTRIES>::Push( const T& Entry )
{
    xbool Result;

    Result = FALSE;

    m_Lock.Acquire();

    if( m_Count < MAX_CMD_ENTRIES )
    {
        m_Entries[m_Write] = Entry;
        m_Write++;
        if( m_Write >= MAX_CMD_ENTRIES )
            m_Write = 0;

        m_Count++;
        Result = TRUE;
        if( m_WakePending )
        {
            m_WakePending = FALSE;
        }
        else
        {
            VERIFY( m_Signal.Release( X_TH_NOBLOCK ) );
        }
    }

    m_Lock.Release();

    return Result;
}

//==============================================================================

template<class T, s32 MAX_CMD_ENTRIES>
xbool x_command_queue<T, MAX_CMD_ENTRIES>::Pop( T& Entry )
{
    xbool Result;

    Result = FALSE;

    if( !m_Signal.Acquire( X_TH_NOBLOCK ) )
        return FALSE;

    m_Lock.Acquire();

    if( m_Count > 0 )
    {
        Entry = m_Entries[m_Read];
        m_Read++;
        if( m_Read >= MAX_CMD_ENTRIES )
            m_Read = 0;

        m_Count--;
        Result = TRUE;
    }
    else
    {
        m_WakePending = FALSE;
    }

    m_Lock.Release();

    return Result;
}

//==============================================================================

template<class T, s32 MAX_CMD_ENTRIES>
xbool x_command_queue<T, MAX_CMD_ENTRIES>::PopWait( T& Entry )
{
    xbool Result;

    Result = FALSE;

    if( !m_Signal.Acquire( X_TH_BLOCK ) )
        return FALSE;

    m_Lock.Acquire();

    if( m_Count > 0 )
    {
        Entry = m_Entries[m_Read];
        m_Read++;
        if( m_Read >= MAX_CMD_ENTRIES )
            m_Read = 0;

        m_Count--;
        Result = TRUE;
    }
    else
    {
        m_WakePending = FALSE;
    }

    m_Lock.Release();

    return Result;
}

//==============================================================================

template<class T, s32 MAX_CMD_ENTRIES>
xbool x_command_queue<T, MAX_CMD_ENTRIES>::IsEmpty( void )
{
    xbool Result;

    m_Lock.Acquire();
    Result = (m_Count == 0);
    m_Lock.Release();

    return Result;
}

//==============================================================================

template<class T, s32 MAX_CMD_ENTRIES>
void x_command_queue<T, MAX_CMD_ENTRIES>::Wake( void )
{
    m_Lock.Acquire();

    if( (m_Count == 0) && !m_WakePending )
    {
        m_WakePending = TRUE;
        VERIFY( m_Signal.Release( X_TH_NOBLOCK ) );
    }

    m_Lock.Release();
}

//==============================================================================
#endif // X_COMMAND_QUEUE_HPP
//==============================================================================
