//==============================================================================
//
//  x_log_private.hpp
//
//==============================================================================
//
//  Copyright (c) 2002-2003 Inevitable Entertainment Inc. All rights reserved.
//  Not to be used without express permission of Inevitable Entertainment, Inc.
//
//==============================================================================

#ifndef X_LOG_PRIVATE_HPP
#define X_LOG_PRIVATE_HPP

//==============================================================================
// Types
//==============================================================================

enum log_type
{
    LOG_TYPE_NULL,
    LOG_TYPE_MESSAGE,
    LOG_TYPE_WARNING,
    LOG_TYPE_ERROR,
    LOG_TYPE_ASSERT,
};

typedef void log_display_fn( const char* pChannel, log_type Type, const char* pMsg,
                             const char* pFileName, s32 LineNumber );

//==============================================================================
// Log control structure
//==============================================================================

struct log_control
{
    bool    Enable;
    bool    EnableMessages;
    bool    EnableWarnings;
    bool    EnableErrors;
    bool    EnableAsserts;
    bool    ImmediateFlush;
    bool    EnableCallbackForMessages;
};

extern log_control g_LogControl;

//==============================================================================
// Legacy log function stubs
//==============================================================================

void    log_RegisterCallBack( log_display_fn* CallBack );

void    log_APP_NAME    ( const char* pName );

void    log_MESSAGE     ( const char* pChannel, const char* pFormatStr, ... );
void    log_WARNING     ( const char* pChannel, const char* pFormatStr, ... );
void    log_ERROR       ( const char* pChannel, const char* pFormatStr, ... );
void    log_ASSERT      ( const char* pMessage, const char* pFile, s32 Line );

void    log_TIMER_PUSH  ( void );
void    log_TIMER_POP   ( const char* pChannel, f32 TimeLimitMS, const char* pFormatStr, ... );

void    clog_MESSAGE    ( xbool Condition, const char* pChannel, const char* pFormatStr, ... );
void    clog_WARNING    ( xbool Condition, const char* pChannel, const char* pFormatStr, ... );
void    clog_ERROR      ( xbool Condition, const char* pChannel, const char* pFormatStr, ... );
void    clog_ASSERT     ( xbool Condition, const char* pMessage, const char* pFile, s32 Line );

void    log_FL          ( const char* pFileName, s32 LineNumber );
void    log_LOCK        ( void );
void    log_UNLOCK      ( void );

void    log_FLUSH       ( void );

//==============================================================================
// Public LOG_* compatibility macros. These intentionally do not evaluate args.
//==============================================================================

#define LOG_APP_NAME(...)       ((void)0)
#define LOG_MESSAGE(...)        ((void)0)
#define LOG_WARNING(...)        ((void)0)
#define LOG_ERROR(...)          ((void)0)
#define LOG_ASSERT(...)         ((void)0)

#define LOG_TIMER_PUSH(...)     ((void)0)
#define LOG_TIMER_POP(...)      ((void)0)

#define LOG_MEMMARK(...)        ((void)0)

#define CLOG_MESSAGE(...)       ((void)0)
#define CLOG_WARNING(...)       ((void)0)
#define CLOG_ERROR(...)         ((void)0)
#define CLOG_ASSERT(...)        ((void)0)

#define LOG_FLUSH(...)          ((void)0)

//==============================================================================
#endif // X_LOG_PRIVATE_HPP
//==============================================================================
