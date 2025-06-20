//==============================================================================
//  
//  x_debug.cpp
//  
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#ifndef X_DEBUG_HPP
#include "../x_debug.hpp"
#endif

#ifndef X_PLUS_HPP
#include "../x_plus.hpp"
#endif

#ifndef X_STDIO_HPP
#include "../x_stdio.hpp"
#endif

#ifndef X_STRING_HPP
#include "../x_string.hpp"
#endif

#ifndef X_FILES_PRIVATE_HPP
#include "x_files_private.hpp"
#endif

#include "../x_files.hpp"

#include "../x_threads.hpp"

//==============================================================================
// PLATFORM HEADERS
//==============================================================================

#ifdef TARGET_PC
#include <stdio.h>  // for printf
#include "../x_bytestream.hpp"
#include <richedit.h>
#endif

//==============================================================================

#if defined( X_RETAIL ) && !defined( X_QA ) && !defined( TARGET_PC )
#error Exclude x_debug.cpp from retail builds.
#endif

//==============================================================================
// DEFINES
//==============================================================================
#define ERROR_BUFFER_SIZE ( 1024 * 1 )
#define CAUSE_BUFFER_SIZE ( 256 )

enum rtf_actions
{
    RTF_BREAK = 1,
    RTF_CONTINUE,
    RTF_IGNORE,
    RTF_IGNORE_ALL,
    RTF_EMAIL
};

//==============================================================================
//  VARIABLES
//==============================================================================
static void s_DefaultLogHandler(const char *pString);

volatile    s32                 DDBZ;
static      rtf_fn              s_DefaultRTFHandler;
static      rtf_fn*             s_pRTFHandler = s_DefaultRTFHandler;
static      rtfmailer_fn*       s_pRTFMailer  = NULL;
static      log_fn*             s_pLogHandler = s_DefaultLogHandler;
static      xbool               s_Asserting   = FALSE;
static      x_debug_crash_fn*   s_DebugCrashFunction=NULL;

#ifndef X_RETAIL
static      char                s_DebugVersion[128]=__DATE__ __TIME__;
static      x_debug_msg_fn*     s_DebugMsgFunction = NULL;
#endif

#if !defined( CONFIG_RETAIL )
static      char                s_Cause[CAUSE_BUFFER_SIZE] = {0};
#endif

#ifdef TARGET_PC
static      char                s_ErrorBuffer[ ERROR_BUFFER_SIZE ];
static      s32                 s_iErrorBuffer = 0;
static      s32                 s_iErrorLast   = 0;
#endif

#ifdef X_DEBUG
            xbool   g_bErrorBreak  = TRUE;
#else
            xbool   g_bErrorBreak  = FALSE;
#endif

//==============================================================================
//  FUNCTIONS
//==============================================================================

//==============================================================================
//  Debugger Message

#ifdef TARGET_PC
#include <windows.h>    // OutputDebugString()
#endif

xbool g_bSkipAllThrowDialogs    = TRUE;
xbool g_bSkipAllCatchDialogs    = FALSE;
xbool g_bSkipAllAsserts         = FALSE;
xbool g_bSkipThrowCatchAssertDialogs = FALSE;

//==============================================================================

#ifdef TARGET_PC
const char* xExceptionGetErrorString( void )
{
    return s_ErrorBuffer;
}
#endif

//==============================================================================

#ifdef TARGET_PC

const char* s_pRTF_Title = NULL;
const char* s_pRTF_Message = NULL;

INT_PTR CALLBACK RTFDialogProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
    switch( uMsg )
    {
        case WM_INITDIALOG:
            {
                // Set Dialog title
                if( s_pRTF_Title )
                    ::SetWindowText( hWnd, s_pRTF_Title );

                // Find edit control and set message
                HWND hWndEdit = ::GetDlgItem( hWnd, 6 );
                if( hWndEdit && s_pRTF_Message )
                {
                    ::SendMessage( hWndEdit, EM_SETBKGNDCOLOR, 0, ::GetSysColor( COLOR_BTNFACE ) );
                    ::SetWindowText( hWndEdit, s_pRTF_Message );
                }
            }
            break;

        case WM_CLOSE:
            ::EndDialog( hWnd, RTF_CONTINUE );
            return TRUE;
            break;

        case WM_KEYDOWN:
            {
                if( wParam == VK_ESCAPE )
                {
                    ::EndDialog( hWnd, RTF_CONTINUE );
                    return TRUE;
                }
            }
            break;

        case WM_COMMAND:
            {
                switch( wParam )
                {
                case RTF_BREAK:
                case RTF_CONTINUE:
                case RTF_IGNORE:
                case RTF_IGNORE_ALL:
                case RTF_EMAIL:
                    ::EndDialog( hWnd, wParam );
                    return TRUE;
                }
            }
            break;
    }

    return FALSE;
}

#define RTF_DLG_BW  70
#define RTF_DLG_BH  15
#define RTF_DLG_S   8
#define RTF_DLG_W   (RTF_DLG_S + (RTF_DLG_BW + RTF_DLG_S) * 5)
#define RTF_DLG_H   250

s32 RTFDialog( const char* pTitle, const char* pMessage )
{
    xbytestream b;

    // Send the RTF through the mailer if one is installed
    if( s_pRTFMailer )
        s_pRTFMailer( pTitle, pMessage );

    // Create the dialog box template
    /* dlgVer */    b << (u16)1;
    /* signature */ b << (u16)0xFFFF;
    /* helpID */    b << (u32)0;
    /* exStyle */   b << (u32)0;
    /* dwStyle */   b << (u32)( DS_MODALFRAME | DS_SHELLFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU );
    /* cDlgItems */ b << (u16)6;
    /* x */         b << (u16)0;
    /* y */         b << (u16)0;
    /* cx */        b << (u16)RTF_DLG_W;
    /* cy */        b << (u16)RTF_DLG_H;
    /* menu */      b << (u16)0;
    /* class */     b << (u16)0;
    /* title */     b << (u16)0;
    /* pointsize */ b << (u16)8;
    /* weight */    b << (u16)400;
    /* italic */    b << (u8)0;
    /* charset */   b << (u8)1;
                    xwstring Name = "MS Shell Dlg";
    /* font */      b.Append( (byte*)(const xwchar*)Name, Name.GetLength()*2+2 );

    // Control ID
    s32 ID = 1;

    // Add buttons
    for( s32 i=0 ; i<5 ; i++ )
    {
        xwstring Name;
        
        // Align to DWORD boundry
        while( (b.GetLength() & 3) != 0 )
            b.Append( (byte)0 );

        // Get name of button
        switch( i )
        {
        case 0: Name = xstring("Break"); break;
        case 1: Name = xstring("Continue"); break;
        case 2: Name = xstring("Ignore"); break;
        case 3: Name = xstring("Ignore All"); break;
        case 4: Name = xstring("Email..."); break;
        }
        /* helpID */    b << (u32)0;
        /* exStyle */   b << (u32)0;
        /* style */     b << (u32)( WS_CHILD | WS_VISIBLE | WS_TABSTOP | ((i==4)?WS_DISABLED:0) | ((i==0)?BS_DEFPUSHBUTTON:0) );
        /* x */         b << (u16)(RTF_DLG_S + (RTF_DLG_BW + RTF_DLG_S)*i);
        /* y */         b << (u16)(RTF_DLG_H - RTF_DLG_S - RTF_DLG_BH);
        /* cx */        b << (u16)RTF_DLG_BW;
        /* cy */        b << (u16)RTF_DLG_BH;
        /* id */        b << (u32)ID++;
                        xwstring ClassName = xstring("BUTTON");
        /* class */     b.Append( (const byte*)(const xwchar*)ClassName, ClassName.GetLength()*2+2 );
        /* class */     //b << (u16)0xffff;
        /* class */     //b << (u16)0x080;
        /* title */     b.Append( (const byte*)(const xwchar*)Name, Name.GetLength()*2+2 );
        /* create */    b << (u16)0;
    }

    // Align to DWORD boundry
    while( (b.GetLength() & 3) != 0 )
        b.Append( (byte)0 );

    /* helpID */    b << (u32)0;
    /* exStyle */   b << (u32)( /*WS_EX_TRANSPARENT |*/ WS_EX_CLIENTEDGE );
    /* style */     b << (u32)( WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | WS_HSCROLL | WS_VSCROLL | ES_READONLY | WS_TABSTOP );
    /* x */         b << (u16)RTF_DLG_S;
    /* y */         b << (u16)RTF_DLG_S;
    /* cx */        b << (u16)(RTF_DLG_W - RTF_DLG_S*2);
    /* cy */        b << (u16)(RTF_DLG_H - RTF_DLG_S*3 - RTF_DLG_BH);
    /* id */        b << (u32)ID++;
                    Name = xstring("RichEdit20A");
    /* class */     b.Append( (const byte*)(const xwchar*)Name, Name.GetLength()*2+2 );
                    Name = xstring("");
    /* title */     b.Append( (const byte*)(const xwchar*)Name, Name.GetLength()*2+2 );
    /* create */    b << (u16)0;

    // Setup title and message strings, picked up in the WM_INITDIALOG message above
    s_pRTF_Title   = pTitle;
    s_pRTF_Message = pMessage;

    // Execute the dialog
    HINSTANCE hInstance = (HINSTANCE)GetModuleHandle( NULL );
    s32 Result = ::DialogBoxIndirect( hInstance, (LPCDLGTEMPLATE)b.GetBuffer(), NULL, RTFDialogProc );

    // Return the dialog result
    return Result;
}

#endif // TARGET_PC

//==============================================================================

xbool xExceptionCatchHandler( const char* pFileName, s32 LineNum, const char* pMessage, xbool& bSkipDialog )
{
#ifdef TARGET_PC

#ifdef X_LOGGING
    log_LOCK();
    log_ERROR( "ExceptionCATCH", 
                (pMessage) ?
                xfs("%s \nFROM THROW:\n%s\n",pMessage,s_ErrorBuffer) :
                xfs("FROM THROW:\n%s\n",s_ErrorBuffer)
              );
    LOG_FLUSH();
#endif

    // If user has chosen to skip notifying about this catch then just return
    if( (bSkipDialog == TRUE) || (g_bSkipAllCatchDialogs == TRUE) || (g_bSkipThrowCatchAssertDialogs==TRUE))
        return FALSE; // Don't break

    char Report[32768];
    Report[0] = 0;
    if( pMessage ) x_strcat(Report,xfs("\n%s\n",pMessage));
    if( pFileName ) x_strcat(Report,xfs("\n%s:(%d)\n",pFileName,LineNum));
    x_strcat(Report,"\n----------------------------------------------------------------------\n");
    x_strcat(Report,xfs("\n%s\n",s_ErrorBuffer));

    switch( RTFDialog( "EXCEPTION CATCH", Report ) )
    {
    case RTF_BREAK:                                         return TRUE; break;
    case RTF_CONTINUE:                                      return FALSE; break;
    case RTF_IGNORE:        bSkipDialog = TRUE;             return FALSE; break;
    case RTF_IGNORE_ALL:    g_bSkipAllCatchDialogs = TRUE;  return FALSE; break;
    }

#else
    (void)pFileName;
    (void)LineNum;
    (void)pMessage;
    (void)bSkipDialog;
#endif

    return FALSE;
}

//==============================================================================

#ifdef TARGET_PC

xbool xExceptionThrowHandler( const char* pFileName, s32 LineNum, const char* pMessage, xbool bConcatenate, xbool& bSkipDialog )
{
    return xExceptionThrowHandler( pFileName, LineNum, pMessage, bConcatenate, 0xBADBEEB, bSkipDialog );
}

#endif

//==============================================================================

#ifdef TARGET_PC

xbool xExceptionThrowHandler( const char* pFileName, s32 LineNum, const char* pMessage, xbool bConcatenate, s32 Code, xbool& bSkipDialog )
{
    // Update state variables
    if( bConcatenate == FALSE )
    {
        s_iErrorLast     = 0;
        s_iErrorBuffer   = 0;
        s_ErrorBuffer[0] = 0;
    }
    else
    {
        s_iErrorLast = s_iErrorBuffer;
    }

    // Do a Divider
    if( bConcatenate )
    {
        static const char* pDivider = "=============================================================\n";
        s32 Length = x_strlen(pDivider);

        if( (s_iErrorBuffer + Length + 8 ) < ERROR_BUFFER_SIZE )
        {
            x_strcpy( &s_ErrorBuffer[ s_iErrorBuffer ], pDivider );
            s_iErrorBuffer += Length;
        }
    }

    // Copy Message
    if( pMessage )
    {
        s32 Length = strlen( pMessage );
        if( (s_iErrorBuffer + Length + 8 ) < ERROR_BUFFER_SIZE )
        {
            s_iErrorBuffer += x_sprintf( &s_ErrorBuffer[ s_iErrorBuffer ], "%s\n\n", pMessage ); 
        }
    }

    // Copy the file name
    if( pFileName )
    {
        s32 Length = strlen( pFileName );
        if( (s_iErrorBuffer + Length + 8 ) < ERROR_BUFFER_SIZE )
        {
            s_iErrorBuffer += x_sprintf( &s_ErrorBuffer[ s_iErrorBuffer ], "%s", pFileName ); 
        }
    }

    // Copy the line Number
    if( (s_iErrorBuffer + 32) < ERROR_BUFFER_SIZE )
    {
        s_iErrorBuffer += x_sprintf( &s_ErrorBuffer[ s_iErrorBuffer ], "(%d)", LineNum ); 
    }

    // Copy the code
    if( (s_iErrorBuffer + 32) < ERROR_BUFFER_SIZE && Code != 0xBADBEEB )
    {
        s_iErrorBuffer += x_sprintf( &s_ErrorBuffer[ s_iErrorBuffer ], ":\nerror C%d: ", Code ); 
    }
    else
    {
        s_iErrorBuffer += x_sprintf( &s_ErrorBuffer[ s_iErrorBuffer ], ":\n" ); 
    }
        

    // Print the message in the output window
    //x_DebugMsg( "*** EXCEPTION THROW ***\n%s\n", s_ErrorBuffer );

    // Log the throw exception
    //log_LOCK();
    //log_ERROR( "ExceptionTHROW", s_ErrorBuffer  );
    //LOG_FLUSH();

#ifdef TARGET_PC

    if( (g_bSkipAllThrowDialogs == FALSE) &&  (g_bSkipThrowCatchAssertDialogs==FALSE) )
    {
        // If user has chosen to skip notifying about this catch then just return
        if( (bSkipDialog == TRUE) || (g_bSkipAllThrowDialogs == TRUE) )
            return FALSE; // Don't break

        switch( RTFDialog( "EXCEPTION THROW", s_ErrorBuffer ) )
        {
        case RTF_BREAK:                                         return TRUE; break;
        case RTF_CONTINUE:                                      return FALSE; break;
        case RTF_IGNORE:        bSkipDialog = TRUE;             return FALSE; break;
        case RTF_IGNORE_ALL:    g_bSkipAllThrowDialogs = TRUE;  return FALSE; break;
        }
    }
#else
    (void)bSkipDialog;
#endif

    return FALSE;
}

#endif

//==============================================================================

#ifndef X_RETAIL
void x_DebugMsgSetFunction( x_debug_msg_fn* CallBack )
{
    s_DebugMsgFunction = CallBack;
}
#endif

#ifndef X_RETAIL
static void sys_dbg_OutputConsole(s32 Channel, const char* pString)
{
    (void)Channel;
    (void)pString;

#if defined(X_DEBUG)
    if (s_DebugMsgFunction)
    {
        s_DebugMsgFunction(pString);
        return;
    }

#ifdef TARGET_PC
    OutputDebugString(pString);
#else
    printf("%s",pString);
#endif

#else

#if defined(TARGET_PC) && !defined(X_RETAIL)
    if( s_DebugMsgFunction )
    {
        s_DebugMsgFunction(pString);
        return;
    }
#endif

#endif
}
#endif
//==============================================================================
#ifndef X_RETAIL
void x_DebugMsg( s32 Channel, const char* pFormatStr, ... )
{
    static xbool s_SuppressTime=FALSE;

    x_va_list   Args;
    x_va_start( Args, pFormatStr );

    xvfs XVFS( pFormatStr, Args );
    xfs XFS("%2.2f: %s",x_GetTimeSec(),(const char*)XVFS);
    if (s_SuppressTime)
    {
        sys_dbg_OutputConsole(Channel, (const char*)XVFS);
    }
    else
    {
        sys_dbg_OutputConsole(Channel, (const char*)XFS);
    }
    const char* pString;

    pString = (const char*)XFS;
    s_SuppressTime = (pString[x_strlen(pString)-1]!='\n');
}
#endif

//==============================================================================
#ifndef X_RETAIL
void x_DebugMsg( const char* pFormatStr, ... )
{
    static xbool s_SuppressTime=FALSE;

    x_va_list   Args;
    x_va_start( Args, pFormatStr );

    xvfs XVFS( pFormatStr, Args );
    xfs XFS("%2.2f: %s",x_GetTimeSec(),(const char*)XVFS);
    if (s_SuppressTime)
    {
        sys_dbg_OutputConsole(0, (const char*)XVFS);
    }
    else
    {
        sys_dbg_OutputConsole(0, (const char*)XFS);
    }
    const char* pString;

    pString = (const char*)XFS;
    s_SuppressTime = (pString[x_strlen(pString)-1]!='\n');
}
#endif
    
//==============================================================================
#ifndef X_RETAIL
void x_DebugLog( const char* pFormatStr, ... )
{
    (void)pFormatStr;

    x_va_list   Args;
    x_va_start( Args, pFormatStr );

    xvfs XVFS( pFormatStr, Args );

    if( s_pRTFHandler )
    {
        s_pLogHandler(XVFS);
    }
    else
    {
        s_DefaultLogHandler(XVFS);
    }

}
#endif
//==============================================================================
#ifndef X_RETAIL
void x_DebugSetVersionString(const char *pString)
{
    x_strncpy(s_DebugVersion,pString,sizeof(s_DebugVersion));
}
#endif

//==============================================================================
#ifndef X_RETAIL
const char *x_DebugGetVersionString(void)
{
    return s_DebugVersion;
}
#endif

//==============================================================================
void        x_DebugSetCrashFunction(x_debug_crash_fn *Callback)
{
    s_DebugCrashFunction = Callback;
}

//==============================================================================
x_debug_crash_fn *x_DebugGetCrashFunction(void)
{
    return s_DebugCrashFunction;
}

//==============================================================================

xbool RTFHandler( const char* pFileName,      
                  s32         LineNumber,     
                  const char* pExprString,    
                  const char* pMessageString )
{
    xbool bResult;
    if( !s_pRTFHandler )
        bResult = TRUE;
    else
    {
        xbool WasAtomic = x_IsAtomic();
        if( WasAtomic )
        {
            x_EndAtomic();
        }
        bResult=s_pRTFHandler(
            pFileName,
            LineNumber,
            pExprString,
            pMessageString
        );
        if( WasAtomic )
        {
            x_BeginAtomic();
        }
    }
    return bResult;
}

//==============================================================================

void x_SetRTFHandler( rtf_fn* pRTFHandler )
{
    if( pRTFHandler == NULL )
        s_pRTFHandler = s_DefaultRTFHandler;
    else
        s_pRTFHandler = pRTFHandler;

}

//==============================================================================

rtf_fn* x_GetRTFHandler( void )
{
    return( s_pRTFHandler );
}

//==============================================================================

void x_SetRTFMailer( rtfmailer_fn* pRTFMailer )
{
    s_pRTFMailer = pRTFMailer;
}

//==============================================================================

rtfmailer_fn* x_GetRTFMailer( void )
{
    return( s_pRTFMailer );
}

//==============================================================================
void x_SetLogHandler(log_fn *pLogHandler)
{
    if (pLogHandler == NULL)
    {
        s_pLogHandler = s_DefaultLogHandler;
    }
    else
    {
        s_pLogHandler = pLogHandler;
    }
}

//==============================================================================
static void s_DefaultLogHandler(const char *pString)
{

    X_FILE *fp;

    fp = x_fopen("debuglog.txt","wat");
    if (fp)
    {
        x_fwrite(pString,x_strlen(pString),1,fp);
        x_fclose(fp);
    }
    else
    {
        x_DebugMsg("Cannot open Log: %s\n",pString);
    }
}

//==============================================================================

//------------------------------------------------------------------------------
#ifdef TARGET_PC
//------------------------------------------------------------------------------

#define DEFAULT_RTF_HANDLER_DEFINED

#include <windows.h>    // OutputDebugString()
#include <stdio.h>      // printf()

struct ignore_entry
{
    const char  *pFilename;
    s32         Line;
};

#define MAX_IGNORE_ENTRIES 256
static ignore_entry     s_IgnoreList[ MAX_IGNORE_ENTRIES ];
static s32              s_iIgnore = 0;

inline void DebugPrintString( const char* pString )
{
    OutputDebugString( pString );
    printf( pString );
}

static
xbool s_DefaultRTFHandler( const char* pFileName,      
                           s32         LineNumber,     
                           const char* pExprString,    
                           const char* pMessageString )
{
    // Watch out for "Oops! I forgot to call x_Init()."
    if( x_GetInitialized() == 0 )
    {
        DebugPrintString( "***\n*** ERROR: x_files not initialized.\n***\n" );
        BREAK;
    }

    // Skip dealing with assert if requested
    if( (g_bSkipAllAsserts == TRUE) || (g_bSkipThrowCatchAssertDialogs==TRUE) )
    {
        s_Asserting = FALSE;
        return FALSE;
    }

    // Check if we have chosen to ignore this assert
    for( s32 i=0; i<s_iIgnore; i++ )
    {
        if( (s_IgnoreList[i].pFilename == pFileName) && 
            (s_IgnoreList[i].Line == LineNumber) )
        {
            s_Asserting = FALSE;
            return FALSE;
        }
    }

    // Watch out for infinite assertion failures.
    if( s_Asserting )
        return FALSE;

    s_Asserting = TRUE;

    DebugPrintString( "***\n*** RUNTIME FAILURE\n" );

    if( pFileName )         DebugPrintString( xfs( "*** File: %s on line %d\n", pFileName, LineNumber ) );
    else                    DebugPrintString( xfs( "*** File: <unknown> on line %d\n", LineNumber ) );
    if( pExprString )       DebugPrintString( xfs( "*** Expr: %s\n", pExprString ) );
    if( pMessageString )    DebugPrintString( xfs( "*** Msg : %s\n", pMessageString ) );
    
    DebugPrintString( "***\n" );

    // Do dialog
    {
        char Report[32768];
        Report[0] = 0;
        if( pFileName      ) x_strcat(Report,xfs( "*** File: %s on line %d\n", pFileName, LineNumber ) );
        if( pExprString    ) x_strcat(Report,xfs( "*** Expr: %s\n", pExprString ) );
        if( pMessageString ) x_strcat(Report,xfs( "*** Msg : %s\n", pMessageString ) );

#ifdef X_LOGGING
        log_LOCK();
        log_ERROR( "ASSERT", Report );
        LOG_FLUSH();
#endif

        switch( RTFDialog( "ASSERT", Report ) )
        {
        case RTF_BREAK:
            s_Asserting = FALSE;
            return TRUE;
            break;

        case RTF_CONTINUE:
            s_Asserting = FALSE;
            return FALSE;
            break;

        case RTF_IGNORE:
            {
                if (s_iIgnore < MAX_IGNORE_ENTRIES)
                {
                    s_IgnoreList[s_iIgnore].pFilename = pFileName;
                    s_IgnoreList[s_iIgnore].Line      = LineNumber;
                    s_iIgnore++;
                }
                s_Asserting = FALSE;
                return FALSE;
            }
            break;

        case RTF_IGNORE_ALL:
            s_Asserting = FALSE;
            g_bSkipAllAsserts = TRUE;
            return FALSE;
            break;
        }
    }

    s_Asserting = FALSE;
    return( TRUE );
}

//------------------------------------------------------------------------------
#endif // TARGET_PC
//------------------------------------------------------------------------------

//==============================================================================

//------------------------------------------------------------------------------
#ifndef DEFAULT_RTF_HANDLER_DEFINED
//------------------------------------------------------------------------------

static
xbool s_DefaultRTFHandler( const char* pFileName,      
                           s32         LineNumber,     
                           const char* pExprString,    
                           const char* pMessageString )
{
    // Watch out for infinite assertion failures.
    if( s_Asserting )
        BREAK;

    s_Asserting = TRUE;

    x_printf( "***\n*** RUNTIME FAILURE\n" );
    if( pFileName )         x_printf( "*** File: %s on line %d\n", pFileName, LineNumber );
    else                    x_printf( "*** File: <unknown> on line %d\n", LineNumber );
    if( pExprString )       x_printf( "*** Expr: %s\n", pExprString );
    if( pMessageString )    x_printf( "*** Msg : %s\n", pMessageString );
    x_printf( "***\n" );

    x_DebugMsg( "***\n*** RUNTIME FAILURE\n" );
    if( pFileName )         x_DebugMsg( "*** File: %s on line %d\n", pFileName, LineNumber );
    else                    x_DebugMsg( "*** File: <unknown> on line %d\n", LineNumber );
    if( pExprString )       x_DebugMsg( "*** Expr: %s\n", pExprString );
    if( pMessageString )    x_DebugMsg( "*** Msg : %s\n", pMessageString );
    x_DebugMsg( "***\n" );

    s_Asserting = FALSE;
    return( TRUE );
}

//------------------------------------------------------------------------------
#endif
//------------------------------------------------------------------------------
//==============================================================================

//------------------------------------------------------------------------------
#ifndef DEFAULT_RTF_HANDLER_DEFINED
//------------------------------------------------------------------------------

static
xbool s_DefaultRTFHandler( const char* pFileName,      
                           s32         LineNumber,     
                           const char* pExprString,    
                           const char* pMessageString )
{
    // Watch out for infinite assertion failures.
    if( s_Asserting )
        BREAK;

    s_Asserting = TRUE;

    x_printf( "***\n*** RUNTIME FAILURE\n" );

    if( pFileName )         x_printf( "*** File: %s on line %d\n", pFileName, LineNumber );
    else                    x_printf( "*** File: <unknown> on line %d\n", LineNumber );
    if( pExprString )       x_printf( "*** Expr: %s\n", pExprString );
    if( pMessageString )    x_printf( "*** Msg : %s\n", pMessageString );
    
    x_printf( "***\n" );

    s_Asserting = FALSE;
    return( TRUE );
}

//------------------------------------------------------------------------------
#endif // !DEFAULT_RTF_HANDLER_DEFINED
//------------------------------------------------------------------------------

//==============================================================================
//  Callstack walking
//==============================================================================

// TODO: Make this function load the dbghelp.dll and find the appropriate functions
//       rather than requiring linkage with dbghelp.lib

//------------------------------------------------------------------------------
#if defined(TARGET_PC) && defined(USE_DBGHELP)
//------------------------------------------------------------------------------

#include <dbghelp.h>

static HANDLE       s_hProcess;
static xbool        s_DbgHelpInitialized    = FALSE;
static char*        s_pCallStackString      = NULL;
PIMAGEHLP_SYMBOL    s_pSymbol               = NULL;

xbool x_DebugGetCallStack( s32& CallStackDepth, u32*& pCallStack )
{
    CallStackDepth  = 0;
    pCallStack      = NULL;
    return FALSE;
}

const char* x_DebugGetCallStackString( void )
{
    // Initialize if not initialized
    if( !s_DbgHelpInitialized )
    {
        // Init library
        SymSetOptions( SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS );
        s_hProcess = GetCurrentProcess();
//        VERIFY( SymInitialize( s_hProcess, "Debug;Release", TRUE ) );
        VERIFY( SymInitialize( s_hProcess, "Debug;Release", TRUE ) ); // TODO: Add path to module with GetModuleFileNameEx

        // Allocate string to build callstack
        s_pCallStackString = (char*)x_malloc( 1*1024*1024 );
        ASSERT( s_pCallStackString );

        // Setup symbol
        s_pSymbol = (PIMAGEHLP_SYMBOL)x_malloc(10000);
        s_pSymbol->MaxNameLength = 10000-sizeof(IMAGEHLP_SYMBOL);
        s_pSymbol->SizeOfStruct  = 10000;

        s_DbgHelpInitialized = TRUE;
    }

    // Get the Thread Context
    HANDLE hThread = GetCurrentThread();
    CONTEXT c;
    x_memset( (void*)&c, 0, sizeof(c) );
    c.ContextFlags = CONTEXT_FULL;
    VERIFY( GetThreadContext( hThread, &c ) );
    
    // Build STACKFRAME
    STACKFRAME s;
    x_memset( (void*)&s, 0, sizeof(s) );
    s.AddrPC.Offset     = c.Eip;
    s.AddrStack.Offset  = c.Esp;
    s.AddrFrame.Offset  = c.Ebp;
    s.AddrPC.Mode       = AddrModeFlat;
    s.AddrStack.Mode    = AddrModeFlat;
    s.AddrFrame.Mode    = AddrModeFlat;

    // Clear CallStack string
    s_pCallStackString[0] = 0;

    // Do the stack walk
    for( s32 iFrame=0 ; ; iFrame++ )
    {
        BOOL Success = StackWalk( IMAGE_FILE_MACHINE_I386,
                                  s_hProcess,
                                  hThread,
                                  &s,
                                  &c,
                                  NULL,
                                  SymFunctionTableAccess,
                                  SymGetModuleBase,
                                  NULL );

        // Exit loop?
        if( !Success || (s.AddrPC.Offset == 0) )
            break;

        // Dump info about current stack level
        if( iFrame > 0 )
        {
            DWORD   Offset;
            char    TempString[4096] = {0};
            if( SymGetSymFromAddr( s_hProcess, s.AddrPC.Offset, &Offset, s_pSymbol ) )
            {
                ASSERT( (x_strlen(s_pCallStackString)+x_strlen(s_pSymbol->Name)+2) < 4096 );

                strcpy( TempString, s_pSymbol->Name );
                if( s_pCallStackString[0] != 0 )
                {
                    strcat( TempString, "\\" );
                    strcat( TempString, s_pCallStackString );
                }
                strcpy( s_pCallStackString, TempString );
            }
            else
            {
                ASSERT( 0 );
                ASSERT( (x_strlen(s_pCallStackString)+12) < 4096 );
                x_strcpy( TempString, xfs("\\0x%08x",s.AddrPC.Offset) );
                if( s_pCallStackString[0] != 0 )
                {
                    x_strcat( TempString, "\\" );
                    x_strcat( TempString, s_pCallStackString );
                }
                x_strcpy( s_pCallStackString, TempString );
            }
        }
    }

    // Strip leading info from the CallStack string, it just takes up screen space and isn't useful
    char* p = strstr( s_pCallStackString, "\\" );
    p = strstr( p, "\\" );
    p = strstr( &p[1], "\\" );
    p = strstr( &p[1], "\\" );
    return &p[1];
}

#else

xbool x_DebugGetCallStack( s32& CallStackDepth, u32*& pCallStack )
{
    CallStackDepth  = 0;
    pCallStack      = NULL;
    return FALSE;
}

const char* x_DebugGetCallStackString( void )
{
    return "<not implemented for this platform>";
}

#endif

//==============================================================================

#if !defined( CONFIG_RETAIL )

void x_DebugSetCause( const char* pCause )
{
    x_strncpy( s_Cause, pCause, CAUSE_BUFFER_SIZE-1 );
    s_Cause[CAUSE_BUFFER_SIZE-1] = 0;
}

const char* x_DebugGetCause( void )
{
    return s_Cause;
}

#endif

//==============================================================================
