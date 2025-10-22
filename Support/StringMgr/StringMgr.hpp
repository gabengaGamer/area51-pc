//==============================================================================
//
//  StringMgr.hpp
//
//==============================================================================

#ifndef STRINGMGR_HPP
#define STRINGMGR_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_files.hpp"

//==============================================================================
//  DEFINES UND STRUCTURES
//==============================================================================

#define STRINGBIN_SIGNATURE          (('V'<<24)|('E'<<16)|('n'<<8)|('I'))
#define STRINGBIN_VERSION            15

//-----------------------------------------------------------------------------

struct stringbin_header
{
    u32     Signature;
    s16     Version;
    s16     Encoding;
    s32     StringCount;
};

//-----------------------------------------------------------------------------

enum stringbin_encoding
{
    STRINGBIN_ENCODING_UTF16LE   = 1,
    STRINGBIN_ENCODING_UTF8      = 2
};

//-----------------------------------------------------------------------------

enum
{
    STRINGBIN_HEADER_BYTES = sizeof(stringbin_header)
};


//==============================================================================
//  string_table
//==============================================================================

class string_table
{
    friend class string_mgr;

protected:
                        string_table    ( void );
                       ~string_table    ( void );

    xbool               Load            ( const char* pTableName, const char* pFileName );
    s32                 GetCount        ( void ) const ;
    const xwchar*       GetAt           ( s32 Index ) const;
    const xwchar*       GetAt               ( const char* lookupString ) const;
    const xwchar*       FindString          ( const char* lookupString ) const;
    const xwchar*       GetSubTitleSpeaker  ( const char* lookupString ) const;
    const xwchar*       GetSoundDescString  ( const char* lookupString ) const;

protected:
    const char*     m_pTableName;
    byte*           m_pData;
    s32             m_nStrings;
    s16             m_version;
    s16             m_encoding;
    s32*            m_pIndex;
    byte*           m_pStrings;
};

//==============================================================================
//  string_mgr
//==============================================================================

class string_mgr
{
public:
    struct char_string
    {
        char    m_String[256];
    };

    struct wchar_string
    {
        xwchar  m_String[256];
    };

public:
                        string_mgr      ( void );
                       ~string_mgr      ( void );

    xbool               LoadTable       ( const char* pTableName, const char* pFileName );
    void                UnloadTable     ( const char* pTableName );
    s32                 GetStringCount  ( const char* pTableName ) ;
    const xwchar*       operator ()     ( const char* pTableName, s32 Index, const xbool bLogNULLString = TRUE ) const;
    const xwchar*       operator ()     ( const char* pTableName, const char* TitleString, const xbool bLogNULLString = TRUE ) const;
    const xwchar*       GetSubTitleSpeaker( const char* pTableName, const char* SpeakerString, const xbool bLogNULLString = TRUE ) const;
    const xwchar*       GetSoundDescString( const char* pTableName, const char* SpeakerString, const xbool bLogNULLString = TRUE ) const;
    const char*         GetLocalizedName( const char* pFileName, char_string& LocalizedName ) const;

protected:
    const string_table* FindTable       ( const char* pTableName ) const;

    static xbool            m_Initialized;
    xarray<string_table*>   m_Tables;
};

//==============================================================================

extern string_mgr g_StringTableMgr;

//==============================================================================
#endif // STRINGMGR_HPP
//==============================================================================
