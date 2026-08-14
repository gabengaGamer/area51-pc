//=========================================================================
//
//  font.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "font.hpp"

#include "Bitmap/aux_Bitmap.hpp"

#include "UI/ui_manager.hpp"
#include "UI/ui_renderer.hpp"

//=========================================================================
//  GLOBAL INSTANCE
//=========================================================================

font g_Font;

//=========================================================================
//  DEFINES
//=========================================================================

#define CHAR_WIDTH  13
#define CHAR_HEIGHT 18
#define XBORDER      8
#define YBORDER      8

//=========================================================================
//  HELPER FUNCTIONS
//=========================================================================

static 
void DrawFontGlyph( const texture& Texture,
                    const vector2& Position,
                    const vector2& Size,
                    const vector2& UV0,
                    const vector2& UV1,
                    const xcolor& TopColor,
                    const xcolor& BottomColor )
{
    const ui_vertex Vertices[4] =
    {
        ui_vertex( Position,                                      vector2( UV0.X, UV0.Y ), TopColor    ),
        ui_vertex( vector2( Position.X + Size.X, Position.Y ),     vector2( UV1.X, UV0.Y ), TopColor    ),
        ui_vertex( Position + Size,                               vector2( UV1.X, UV1.Y ), BottomColor ),
        ui_vertex( vector2( Position.X, Position.Y + Size.Y ),     vector2( UV0.X, UV1.Y ), BottomColor )
    };
    static const u32 Indices[6] = { 0, 1, 2, 2, 3, 0 };

    g_UIRenderer.GetDrawList().AddTriangles( ui_material( Texture,
                                                          UI_BLEND_ALPHA,
                                                          UI_SAMPLER_POINT_CLAMP ),
                                             Vertices,
                                             4,
                                             Indices,
                                             6 );
}

//=========================================================================
//  IMPLEMENTATION
//=========================================================================

xbool font::Load( const char* pPathName )
{
    m_bitmap.SetName( pPathName );
    texture* pTexture = m_bitmap.GetPointer();
    if( !pTexture )
        return FALSE;

    const xbitmap* pBitmap = &pTexture->m_bitmap;


    // Setup info
    m_Height = pBitmap->GetHeight()-1;
#ifdef JAPANESE_VERSION
    x_memset( &m_Characters, 0, sizeof(Character)*1024 );
#else
    x_memset( &m_Characters, 0, sizeof(Character)*256 );
#endif

    // Get info from bitmap size
    m_BmWidth   = pBitmap->GetWidth();
    m_BmHeight  = pBitmap->GetHeight();

    // Clear Data
    m_MaxWidth  = 0;
    m_AvgWidth  = 0;

    // Scan through font building character map
    s32 y = 0;
    xbool Done = FALSE;
    for( s32 Row=0 ; (Row<(7+8)) && !Done ; Row++ )
    {
        // Initialize for character row
        s32 x1 = 0;
        for( s32 Col=0 ; Col<16 ; Col++ )
        {
            // Scan registration marks for character
            s32 x2 = x1+1;
            while( (x2 < m_BmWidth) && !(pBitmap->GetPixelColor( x2, y ).R < 32) )
                x2++;

            // Skip out if nothing on the row
#if !defined(X_EDITOR)
            ASSERT( x2 < m_BmWidth );
#endif

            // Add character
            if( x1 == 0 )
            {
			    m_Characters[16+Row*16+Col].X = 1;		
                m_Characters[16+Row*16+Col].Y = y+1;	
                m_Characters[16+Row*16+Col].W = x2-1;  
            }
            else
            {
			    m_Characters[16+Row*16+Col].X = x1;		
                m_Characters[16+Row*16+Col].Y = y+1;	
                m_Characters[16+Row*16+Col].W = x2-x1;  
            }

            // Update MaxWidth
            if( (x2-x1) > m_MaxWidth )
                m_MaxWidth = (x2-x1);

            // Set start of next character
            x1 = x2+1;
        }

        // Scan down to next row
        if( Row < (6+8) )
        {
            s32 yStart = y;
            y++;
            
            while( (y < m_BmHeight) && !(pBitmap->GetPixelColor( 0, y ).R < 32) )
                y++;

			// Skip out if not found
			if( (y >= m_BmHeight) || ((y-yStart) == 1) )
            {
                Done = TRUE;
				break;
            }

            m_RowHeight = y - yStart;
            m_Height    = m_RowHeight - 1;
        }
    }

    // Set AvgWidth
    m_AvgWidth = m_Characters['x'].W;

    // Return success
    return TRUE;
}


//=========================================================================

void font::Kill( void )
{
    m_bitmap.Destroy();
}

//=========================================================================

void font::TextSize( irect& Rect, const char* pString, s32 Count ) const
{
    s32 Height    = m_Height;
    s32 BestWidth = 0;
    s32 Width     = 0;

    // Loop until end of string or end of count.
    while( *pString && (Count != 0) )
    {
        s32 c = *pString++;

        // Check for newline.
        if( c == '\n' )
        {
            BestWidth = MAX( BestWidth, Width-1 );
            Width     = 0;
            Height   += m_Height;
        }
        else
        // Normal character.
        {
            // Add character to width.
            Width += m_Characters[c].W + 1;
        }             

        // Decrease character count
        Count--;
    }

    BestWidth = MAX( BestWidth, Width-1 );

    // We have all we need.
    Rect.Set( 0, 0, BestWidth, Height );
}

//=========================================================================

void font::TextSize( irect& Rect, const xwchar* pString, s32 Count ) const
{
    s32 Height    = m_Height;
    s32 BestWidth = 0;
    s32 Width     = 0;

    // Loop until end of string or end of count.
    while( *pString && (Count != 0) )
    {
        s32 c = *pString++;

        // Check for embedded color code.
        if( (c & 0xFF00) == 0xFF00 )
        {
            // Skip 2nd character in embedded color code.
            pString++;

            // Decrease character count one extra for 2nd char in code.
            Count--;
        }
        else
        // Check for newline.
        if( c == '\n' )
        {
            BestWidth = MAX( BestWidth, Width-1 );
            Width     = 0;
            Height   += m_Height;
        }
        else
        // Normal character.
        {
            // Add character to width.
            Width += m_Characters[c].W + 1;
        }             

        // Decrease character count
        Count--;
    }

    BestWidth = MAX( BestWidth, Width-1 );

    // We have all we need.
    Rect.Set( 0, 0, BestWidth, Height );
}

//=========================================================================

s32 font::TextWidth( const xwchar* pString, s32 Count ) const
{
    s32 BestWidth = 0;
    s32 Width     = 0;
    //f32 ScaleX=1;
    //f32 ScaleY=1;

    /*
    if( ScaleText )
    {
        s32 XRes, YRes;
        eng_GetRes( XRes, YRes );
        ScaleX = (f32)XRes / 512.0f;
        ScaleY = (f32)YRes / 448.0f;
    } */

    ASSERT( pString );

    if( pString )
    {
        // Loop until end of string or end of count.
        while( *pString && (Count != 0) )
        {
            s32 c = *pString++;

            // Check for embedded color code.
            if( (c & 0xFF00) == 0xFF00 )
            {
                // Skip 2nd character in embedded color code.
                pString++;

                // Decrease character count one extra for 2nd char in code.
                Count--;
            }
            else
            // Check for newline.
            if( c == '\n' )
            {
                BestWidth = MAX( BestWidth, Width-1 );
                Width     = 0;
            }

            // Check for Control Code
            else if( c == 0xAB ) // '«'
            {

#if !defined(APP_EDITOR)
                // If this is a ButtonIcon then Add Sprite Width
                if( g_UiMgr->LookUpButtonCode( pString,
                                               0,
                                               g_Input.GetCurrentInputDevice(),
                                               g_Input.GetCurrentInputPlatform() ) != -1 )
                {
                    Width += BUTTON_SPRITE_WIDTH; break;
                }
#endif

                // Loop past control code.
                while( c != 0xBB ) // '»'
                {
                    c = *pString++;
                }
            }

            else
            // Normal character.
            {

                Width += m_Characters[c].W;
                Width += 1;
            }             

            // Decrease character count.
            Count--;
        }

        BestWidth = MAX( BestWidth, Width-1 );
    }

    // Return best width.
    return( BestWidth );
}

//=========================================================================

const xwchar* font::TextWrap( const xwchar* pString, const irect& Rect ) const
{
    static xwstring WrappedString;
    //f32 ScaleX=1;
    //f32 ScaleY=1;

    // Should we be clipping?
    if( TextWidth( pString ) > Rect.GetWidth() )
    {
        s32     FieldWidth  = Rect.GetWidth() - 15;
        s32     Width       = 0;
        xbool   Clipping    = FALSE;

        // Clear the string
        WrappedString.Clear();

        // Wrap the string
        while( *pString )
        {
            xwchar c = *pString++;

            // Check for embedded color code.
            if( (c & 0xFF00) == 0xFF00 )
            {
                // Copy into Wrap string
                WrappedString += (xwchar)0xFF00;
                WrappedString += *pString++;
            }
            else
            // Check for newline.
            if( c == '\n' )
            {
                WrappedString  += (xwchar)'\n';
                Width           = 0;
                Clipping        = FALSE;
            }
            else
            // Normal character.
            {
                if( !Clipping )
                {

                    Width += m_Characters[c].W;
                    Width += 1;

                    // Width still in range?
                    if( Width < FieldWidth )
                    {
                        // Add to string
                        WrappedString += c;
                    }
                    else
                    {
                        // Over Size.. backup to a space and add a newLine.
                        c = *pString--;

                        while( c != ' ')
                        {
                            c = *pString--;
                            WrappedString.Delete(WrappedString.GetLength()-1,1);
                        }
                        Width           = 0;
                     //   Clipping = TRUE;
                    }
                }
            }
        }

        return (const xwchar*)WrappedString;
    }
    else
    {
        return pString;
    }
}

//=========================================================================

s32 font::TextHeight( const xwchar* pString, s32 Count ) const
{
    s32 Height = m_Height;

    // Loop until end of string or end of count.
    while( *pString && (Count != 0) )
    {
        s32 c = *pString++;

        // Check for embedded color code.
        if( (c & 0xFF00) == 0xFF00 )
        {
            // Skip 2nd character in embedded color code.
            pString++;

            // Decrease character count one extra for 2nd char in code.
            Count--;
        }
        else if( c == 0x00AB )
        {
            Height = 19; // why isn't there a constant for that?
        }
        else
        // Check for newline.
        if( c == '\n' )
        {
            Height += m_Height;
        }

        // Decrease character count.
        Count--;
    }

    // Return height
    return( Height );

}

//=========================================================================

const font::Character& font::GetCharacter( s32 Index ) const
{
    ASSERT( (Index >= 0) && (Index < 256) );

    return m_Characters[Index];
}

//=========================================================================

void font::RenderText( const irect&  Rect, 
                                u32     Flags, 
                          const xcolor& aColor, 
                          const xwchar* pString, 
                                xbool   IgnoreEmbeddedColor,
                                xbool   UseGradient,
                                f32     FlareAmount ) const
{
    xwchar  c;
    s32     tx       = Rect.l;
    s32     ty       = Rect.t;
    s32     iStart   = 0;
    s32     iEnd     = 0;
    s32     Width;
    s32     Height;    
    xcolor  Color1    = aColor;
    xcolor  Color2    = aColor;

    const s32 MaxButtons = 10;
    s32 NumButtons = 0;
    s32 ButtonCodes [ MaxButtons ];
    f32 Button_X    [ MaxButtons ];
    f32 Button_Y    [ MaxButtons ];


    if( UseGradient )
    {
        Color2.R = 200; //(255 + Color1.R) / 2;
        Color2.G = 200; //(255 + Color1.G) / 2;
        Color2.B = 200; //(255 + Color1.B) / 2;
        Color2.A = aColor.A;
    }

    // Do the flare thing
    if( FlareAmount > 0.0f )
    {
        s32 BrightnessDelta = (s32)(FlareAmount * 75);

        Color1.R = (Color1.R + BrightnessDelta) > 255 ? 255 : Color1.R + BrightnessDelta;
        Color1.G = (Color1.G + BrightnessDelta) > 255 ? 255 : Color1.G + BrightnessDelta;
        Color1.B = (Color1.B + BrightnessDelta) > 255 ? 255 : Color1.B + BrightnessDelta;

        Color2.R = (Color2.R + BrightnessDelta) > 255 ? 255 : Color2.R + BrightnessDelta;
        Color2.G = (Color2.G + BrightnessDelta) > 255 ? 255 : Color2.G + BrightnessDelta;
        Color2.B = (Color2.B + BrightnessDelta) > 255 ? 255 : Color2.B + BrightnessDelta;
    }
    texture* pFontTexture = m_bitmap.GetPointer();
    if( !pFontTexture )
        return;

    vector2 uv0;
    vector2 uv1;
    vector2 Size( 0, (f32)m_Height );
    const xbool ClipText = (Flags & clip_character) != 0;
    if( ClipText )
        g_UIRenderer.PushClipRect( Rect );

    // Get size for vertical positioning.
    Height = TextHeight( pString );

    // Position start vertically.
    if( Flags & v_center )
    {
        ty += (Rect.GetHeight() - Height + 4) / 2;
    }
    else if( Flags & v_bottom )
    {
        ty += (Rect.GetHeight() - Height);
    }

    // Render strips of text on same line.
    while( pString[iStart] )
    {
        if( pString[iStart] == '\n' )
        {
            iEnd = iStart;
        }
        else
        {
            // Find end of line.
            iEnd = iStart+1;
            while( pString[iEnd] && (pString[iEnd] != '\n') )
                iEnd++;
        }

        // Determine width of line.
        Width = TextWidth( &pString[iStart], iEnd-iStart );

        // Adjust lateral position for alignment flags.
        if( Flags & h_center )
        {
            tx = Rect.l + (Rect.GetWidth() - Width) / 2;
        }
        else if( Flags & h_right )
        {
            tx = Rect.l + (Rect.GetWidth() - Width);
        }
        else
        {
            tx = Rect.l;
        }

        // Check for justification when clipping.
        if( Width > Rect.GetWidth() )
        {
            if( Flags & clip_l_justify ) 
                tx = Rect.l;
            else
            if( Flags & clip_r_justify ) 
                tx = Rect.r - Width;
        }

        //
        // Render each character.
        //
        for( ; iStart < iEnd; iStart++ )
        {
            c = pString[ iStart ];            
            //
            // Button code stuff.
            //
            if( (0xFFFF & c) == 0x00AB )
            {
                iStart++;
#if !defined(APP_EDITOR)
                // get the button code
                s32 buttonCode = g_UiMgr->LookUpButtonCode( pString,
                                                            iStart,
                                                            g_Input.GetCurrentInputDevice(),
                                                            g_Input.GetCurrentInputPlatform() );
#else
                s32 buttonCode = -1;
#endif

                while( (0xFFFF & c) != 0x00BB ) // '»'
                {
                    iStart++;
                    c = pString[iStart];
                }

                
                // If we found a button code then render it.
                if( buttonCode > -1 )
                {
                    while( (c & 0xFFFF) != 0x00BB ) // '»'
                    {
                        iStart++;
                        c = pString[iStart];
                    }

                    if( NumButtons >= MaxButtons )
                    {
                        ASSERTS( FALSE, "Too many buttons in this string!" );
                        continue;
                    }
                    ButtonCodes[ NumButtons ] = buttonCode;
                    Button_X[ NumButtons ] = (f32)tx;
	        	Button_Y[ NumButtons ] = (f32)ty;
                    tx += BUTTON_SPRITE_WIDTH;
                    
                    NumButtons++;
                }
                continue;
            }

            // Look for an embedded color code.
            else if( (c & 0xFF00) == 0xFF00 )
            {
                if( IgnoreEmbeddedColor )
                {
                    iStart++;
                }
                else
                {
                    Color1.R = (c & 0x00FF);
                    iStart++;
                    c = pString[iStart];
                    Color1.G = (c & 0xFF00) >> 8;
                    Color1.B = (c & 0x00FF);
                }
                continue;
            } 


            //
            // We have a normal character if we've made it this far
            //
            s32 x  = m_Characters[c].X;
            s32 y  = m_Characters[c].Y;
            s32 w  = m_Characters[c].W;

            //Color.A = 255;

            const f32 u0 = (f32)x                / m_BmWidth;
            const f32 u1 = (f32)(x + w)          / m_BmWidth;
            const f32 v0 = (f32)y                / m_BmHeight;
            const f32 v1 = (f32)(y + m_Height)   / m_BmHeight;

            Size.X = (f32)w;
            uv0.Set( u0, v0 );
            uv1.Set( u1, v1 );
            DrawFontGlyph( *pFontTexture,
                           vector2( (f32)tx, (f32)ty ),
                           Size,
                           uv0,
                           uv1,
                           Color2,
                           Color1 );

            tx += w + 1;
        }

        // Process newline.
        if( pString[iStart] == '\n' )
        {
            ty += m_Height;
            iStart++;
        }
    }

#if !defined(APP_EDITOR)
    if( NumButtons > 0 && UseGradient )
    {
        s32 i;
        for( i = 0; i < NumButtons; i++ )
        {
            texture* pButton = g_UiMgr->GetButtonTexture( ButtonCodes[i] );
            if( !pButton )
                continue;

            g_UIRenderer.DrawImage( *pButton,
                                    vector2( Button_X[i] + 1.0f, Button_Y[i] + 1.0f ),
                                    vector2( BUTTON_SPRITE_WIDTH, BUTTON_SPRITE_WIDTH ),
                                    vector2( 0.0f, 0.0f ),
                                    vector2( 1.0f, 1.0f ),
                                    xcolor( 0, 0, 0, 255 ),
                                    0.0f,
                                    UI_BLEND_ALPHA,
                                    UI_SAMPLER_LINEAR_CLAMP );
            g_UIRenderer.DrawImage( *pButton,
                                    vector2( Button_X[i], Button_Y[i] ),
                                    vector2( BUTTON_SPRITE_WIDTH, BUTTON_SPRITE_WIDTH ),
                                    vector2( 0.0f, 0.0f ),
                                    vector2( 1.0f, 1.0f ),
                                    XCOLOR_WHITE,
                                    0.0f,
                                    UI_BLEND_ALPHA,
                                    UI_SAMPLER_LINEAR_CLAMP );
            tx += BUTTON_SPRITE_WIDTH;              
        }
    }
#endif

    if( ClipText )
        g_UIRenderer.PopClipRect();
}

//=========================================================================

void font::RenderText( const irect&  Rect, 
                                u32     Flags, 
                                s32     Alpha, 
                          const xwchar* pString ) const
{
    xcolor Color = XCOLOR_PURPLE;
    Color.A = Alpha;
    RenderText( Rect, Flags, Color, pString, FALSE );
}

//=========================================================================

void font::RenderText( const irect&  R, 
                                u32     Flags, 
                          const xcolor& Color, 
                          const char*   pString ) const
{
    xwstring t( pString );
    RenderText( R, Flags, Color, (const xwchar*)t );
}