//=========================================================================
//
// d3deng_font.cpp
//
//=========================================================================

#include "e_Engine.hpp"
#include "d3deng_font.hpp"
#include "d3deng_private.hpp"

#define ENG_FONT_SIZEX      7
#define ENG_FONT_SIZEY      12

struct font_locals
{
    font_locals( void ) { memset( this, 0, sizeof(font_locals) ); }

    LPD3DXFONT      pFont;
    xcolor          TextColor;
    xbool           bFontInitialized;
};

static font_locals s_Font;

//=========================================================================

void font_Init( void )
{
    if( g_pd3dDevice )
    {
        // Get height in appropriate units
        HDC hDC = GetDC( NULL );
        int nHeight = -( MulDiv( ENG_FONT_SIZEY, GetDeviceCaps(hDC, LOGPIXELSY), 72 ) );
        ReleaseDC( NULL, hDC );

        HRESULT Error = D3DXCreateFont( g_pd3dDevice, nHeight * 3 / 4, 0, FW_NORMAL, 0, FALSE, 
                                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, 
                                    DEFAULT_PITCH | FF_DONTCARE, TEXT("Lucida Console"), 
                                    &s_Font.pFont );

        ASSERT( Error == 0 );
        
        // Set default text color
        s_Font.TextColor = 0xffffffff; // White color
        //GS: In the future I think it would be useful to add a check to other parts of the code for font initialization.
        s_Font.bFontInitialized = TRUE;
    }
}

//=========================================================================

void font_Kill( void )
{
    // Free the font if it was created
    if( s_Font.pFont )
    {
        s_Font.pFont->Release();
        s_Font.pFont = NULL;
    }
}

//=========================================================================

void font_SetColor( const xcolor& Color )
{
    s_Font.TextColor = Color;
}

//=========================================================================

#if !defined( X_RETAIL ) || defined( X_QA )

void text_BeginRender( void )
{
    //
    // Make sure that the direct 3d has started
    //
    if( !eng_InBeginEnd() )
    {
        if( eng_Begin() )
        {
            eng_End();
        }
    }
}

//=========================================================================

void text_RenderStr( char* pStr, s32 NChars, xcolor Color, s32 PixelX, s32 PixelY )
{
    dxerr                   Error;
    RECT                    Rect;

    if( !g_pd3dDevice || !s_Font.pFont )
        return;

    Rect.left   = PixelX;
    Rect.top    = PixelY;
    Rect.bottom = PixelY + ENG_FONT_SIZEY;
    Rect.right  = Rect.left + (NChars*ENG_FONT_SIZEX);
                    
    Error = s_Font.pFont->DrawText( NULL, pStr, NChars, &Rect, DT_NOCLIP, Color );
    ASSERT( Error == D3D_OK );
}

//=========================================================================

void text_EndRender( void )
{
    // Nothing to do for now
}

#endif // !defined( X_RETAIL ) || defined( X_QA )

//=========================================================================

void font_OnDeviceLost( void )
{
    if( s_Font.pFont )
    {
        try
        {
            s_Font.pFont->OnLostDevice();
        }
        catch(...)
        {
            // Ignore release errors - device is already lost
        }
    }
}

//=========================================================================

void font_OnDeviceReset( void )
{
    if( s_Font.pFont )
    {
        HRESULT hr = s_Font.pFont->OnResetDevice();
        if( FAILED(hr) )
        {
            // Failed to reset the font, recreate it
            s_Font.pFont->Release();
            s_Font.pFont = NULL;
            font_Init();
        }
    }
    else if( g_pd3dDevice )
    {
        // Font was released, recreate it
        font_Init();
    }
}