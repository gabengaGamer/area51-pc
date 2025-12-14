//=========================================================================
//
// d3deng_font.cpp
//
//=========================================================================

#include "e_Engine.hpp"
#include "d3deng_font.hpp"
#include "d3deng_private.hpp"

//=========================================================================
// LOCALS
//=========================================================================

static 
struct font_locals
{
    //LPD3DXFONT  pFont;
	void*       pFont;
    xcolor      TextColor;
    xbool       bFontInitialized;

    font_locals() : pFont(NULL), TextColor(0xffffffff), bFontInitialized(FALSE) {}

} s_FontMgr;

//=========================================================================
// FUNCTIONS
//=========================================================================

static 
void CreateFont( void )
{
/*
    if( !g_pd3dDevice )
        return;

    // Get height in appropriate units
    HDC hDC = GetDC( NULL );
    int nHeight = -( MulDiv( ENG_FONT_SIZEY, GetDeviceCaps(hDC, LOGPIXELSY), 72 ) );
    ReleaseDC( NULL, hDC );

    HRESULT Error = D3DXCreateFont( g_pd3dDevice, 
                                   nHeight * 3 / 4, 0, FW_NORMAL, 0, FALSE, 
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, 
                                   DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, 
                                   TEXT("Lucida Console"), 
                                   &s_FontMgr.pFont );
    ASSERT( Error == 0 );
*/
}

//=========================================================================

static 
void DestroyFont( void )
{
/*
    if( s_FontMgr.pFont )
    {
        try
        {
            s_FontMgr.pFont->Release();
        }
        catch(...)
        {
            // Ignore release errors during device loss
        }
        s_FontMgr.pFont = NULL;
    }
*/
}

//=========================================================================

void font_Init( void )
{
/*
    if( s_FontMgr.bFontInitialized )
        return;

    // Set font initialized
    s_FontMgr.bFontInitialized = TRUE;
	// Set default text color
    s_FontMgr.TextColor = xcolor(255, 255, 255, 255);

    CreateFont();
*/
}

//=========================================================================

void font_Kill( void )
{
/*
	// Free the font if it was created
    if( !s_FontMgr.bFontInitialized )
        return;

    DestroyFont();
    s_FontMgr.bFontInitialized = FALSE;
*/
}

//=========================================================================

void font_OnDeviceLost( void )
{
/*
    // Font will be automatically lost with device
    // Just clear our pointer
    s_FontMgr.pFont = NULL;
*/
}

//=========================================================================

void font_OnDeviceReset( void )
{
/*
    // Recreate font after device reset
    if( s_FontMgr.bFontInitialized )
    {
        CreateFont();
    }
*/
}

//=========================================================================

void font_SetColor( const xcolor& Color )
{
/*
    s_FontMgr.TextColor = Color;
*/
}

//=========================================================================

#if !defined( X_RETAIL ) || defined( X_QA )

void text_BeginRender( void )
{
/*
    // Make sure that the direct 3d has started
    if( !eng_InBeginEnd() )
    {
        if( eng_Begin() )
        {
            eng_End();
        }
    }
*/
}

//=========================================================================

void text_RenderStr( char* pStr, s32 NChars, xcolor Color, s32 PixelX, s32 PixelY )
{
/*
    if( !g_pd3dDevice || !s_FontMgr.pFont )
        return;

    RECT Rect;
    Rect.left   = PixelX;
    Rect.top    = PixelY;
    Rect.bottom = PixelY + ENG_FONT_SIZEY;
    Rect.right  = Rect.left + (NChars * ENG_FONT_SIZEX);
                    
    HRESULT Error = s_FontMgr.pFont->DrawText( NULL, pStr, NChars, &Rect, 
                                              DT_NOCLIP, Color );
    ASSERT( SUCCEEDED( Error ) );
*/
}

//=========================================================================

void text_EndRender( void )
{
    // Nothing to do for now
}

#endif // X_RETAIL