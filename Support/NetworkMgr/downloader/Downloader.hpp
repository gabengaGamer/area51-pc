//==============================================================================
//
//  Downloader.hpp
//
//  Copyright (c) 2002-2003 Inevitable Entertainment Inc.  All rights reserved.
//
//==============================================================================

#if !defined(DOWNLOADER_HPP)
#define DOWNLOADER_HPP

//==============================================================================
//  TYPES
//==============================================================================

enum download_status
{
    DL_STAT_OK,
    DL_STAT_DONE,
    DL_STAT_BUSY,
    DL_STAT_NOT_FOUND,
    DL_STAT_ERROR,
};

//==============================================================================
//  FUNCTIONS
//==============================================================================

class downloader
{
public:
        xbool           Init                ( const char* URL );
        void            Kill                ( void );
        void            Update              ( f32 DeltaTime );
        download_status GetStatus           ( void )                { return m_Status;          };
        s32             GetFileLength       ( void )                { return m_Length;          };
        void*           GetFileData         ( void )                { return m_pData;           };
        void            DownloadComplete    ( download_status Status, void* pData=NULL, s32 Length=0 );
        f32             GetProgress         ( void )                { return m_Progress;        };
        void            SetProgress         ( f32 Progress )        { m_Progress = Progress;    };
private:
        byte*           m_pData;
        s32             m_Length;
        download_status m_Status;
        f32             m_Progress;
        
};

#endif