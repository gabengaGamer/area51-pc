//==============================================================================
//
//  x_log.cpp
//
//==============================================================================

//TODO: GS: Make something better with this. Maybe something like the logging system in id tech games?

#ifndef X_FILES_HPP
#include "../x_files.hpp"
#endif

#ifndef X_LOG_HPP
#include "../x_log.hpp"
#endif

//==============================================================================
// Legacy logging stubs
//==============================================================================

log_control  g_LogControl = {
                              false,            // Enable logging system
                              false,            // Enable messages
                              false,            // Enable warnings
                              false,            // Enable errors
                              false,            // Enable asserts
                              false,            // Immediate Flush
                              false             // Enable callback for messages
                            };

//==============================================================================

void log_RegisterCallBack( log_display_fn* CallBack )
{
    (void)CallBack;
}

//==============================================================================

void log_APP_NAME( const char* pName )
{
    (void)pName;
}

//==============================================================================

void log_MESSAGE( const char* pChannel, const char* pFormatStr, ... )
{
    (void)pChannel;
    (void)pFormatStr;
}

//==============================================================================

void log_WARNING( const char* pChannel, const char* pFormatStr, ... )
{
    (void)pChannel;
    (void)pFormatStr;
}

//==============================================================================

void log_ERROR( const char* pChannel, const char* pFormatStr, ... )
{
    (void)pChannel;
    (void)pFormatStr;
}

//==============================================================================

void log_ASSERT( const char* pMessage, const char* pFile, s32 Line )
{
    (void)pMessage;
    (void)pFile;
    (void)Line;
}

//==============================================================================

void log_TIMER_PUSH( void )
{
}

//==============================================================================

void log_TIMER_POP( const char* pChannel, f32 TimeLimitMS, const char* pFormatStr, ... )
{
    (void)pChannel;
    (void)TimeLimitMS;
    (void)pFormatStr;
}

//==============================================================================

void clog_MESSAGE( xbool Condition, const char* pChannel, const char* pFormatStr, ... )
{
    (void)Condition;
    (void)pChannel;
    (void)pFormatStr;
}

//==============================================================================

void clog_WARNING( xbool Condition, const char* pChannel, const char* pFormatStr, ... )
{
    (void)Condition;
    (void)pChannel;
    (void)pFormatStr;
}

//==============================================================================

void clog_ERROR( xbool Condition, const char* pChannel, const char* pFormatStr, ... )
{
    (void)Condition;
    (void)pChannel;
    (void)pFormatStr;
}

//==============================================================================

void clog_ASSERT( xbool Condition, const char* pMessage, const char* pFile, s32 Line )
{
    (void)Condition;
    (void)pMessage;
    (void)pFile;
    (void)Line;
}

//==============================================================================

void log_FL( const char* pFileName, s32 LineNumber )
{
    (void)pFileName;
    (void)LineNumber;
}

//==============================================================================

void log_LOCK( void )
{
}

//==============================================================================

void log_UNLOCK( void )
{
}

//==============================================================================

void log_FLUSH( void )
{
}

//==============================================================================
