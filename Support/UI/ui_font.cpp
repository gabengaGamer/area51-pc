//=========================================================================
//
//  ui_font.cpp
//
//=========================================================================

#include "Entropy.hpp"
#include "Bitmap/aux_Bitmap.hpp"
#include "ui_font.hpp"
#include "UI/ui_manager.hpp"
#include "UI/ui_renderer.hpp"

//=========================================================================

#define HELP_TEXT_SPACING            8

static s32 ui_GetEmbeddedButtonAdvance( s32 ButtonCode )
{
    return (ButtonCode == INPUT_KBD_RETURN) ? 24 : BUTTON_SPRITE_WIDTH;
}

//=========================================================================
//  Font
//=========================================================================

ui_font::ui_font( void )
{
    m_BmWidth    = 0;
    m_BmHeight   = 0;
    m_AvgWidth   = 0;
    m_Height     = 0;
    m_Characters = NULL;
    m_CMap       = NULL;
    m_CMapSize   = 0;
    m_NumChars   = 0;
    m_pManager   = NULL;
}

//=========================================================================

xbool ui_font::Load( ui_manager* pManager, const char* pPathName )
{

    ASSERT( pManager );
    m_pManager = pManager;

    xstring	FontName;

    // Make file name
    FontName = pPathName;
    FontName = FontName.Left(FontName.Find(".xbmp")) + ".font";

    // load the map

    X_FILE* pFontFile = x_fopen( FontName, "rb" );

    ASSERTS( pFontFile, xfs("ui_font::Load() failed %s", (const char*)FontName));

    if( pFontFile )
    {
        u16 num;

        // read the number of character map entries
        x_fread(&num, sizeof(u16), 1, pFontFile);
        m_CMapSize = num;
        ASSERT( m_CMapSize > 0 );

        // read the number of characters
        x_fread(&num, sizeof(u16), 1, pFontFile);
        m_NumChars = num;
        ASSERT( m_NumChars > 0 );

        // read font line height
        x_fread(&num, sizeof(u16), 1, pFontFile);
        m_Height = num;
        ASSERT(m_Height > 0);

        // allocate memory for the char list and registration data.
        m_CMap = (charmap*)x_malloc(sizeof(charmap) * m_CMapSize);
        ASSERT( m_CMap );
        x_memset( m_CMap, 0, sizeof(charmap) * m_CMapSize );

        m_Characters = (Character*)x_malloc( sizeof(Character) * m_NumChars );
        ASSERT( m_Characters );
        x_memset( m_Characters, 0, sizeof(Character) * m_NumChars );

        // and read it in
        s32 count = x_fread(m_CMap, sizeof(charmap), m_CMapSize, pFontFile);
        ASSERT( count == m_CMapSize );

        count = x_fread(m_Characters, sizeof(Character), m_NumChars, pFontFile);
        ASSERT( count == m_NumChars );

        x_fclose( pFontFile );
    }

    // Load font image
    m_bitmap.SetName( pPathName );
    texture* pTexture = m_bitmap.GetPointer();
    ASSERTS( pTexture, xfs("ui_font::Load() failed %s",pPathName));
    if( !pTexture )
        return FALSE;

    const xbitmap& Bitmap = pTexture->m_bitmap;

    // Get info from bitmap size
    m_BmWidth   = Bitmap.GetWidth();
    m_BmHeight  = Bitmap.GetHeight();

    // Set AvgWidth
    m_AvgWidth = m_Characters[ LookUpCharacter('x') ].W;

    return TRUE;
}

//=========================================================================

void ui_font::Kill( void )
{
    x_free( m_CMap );
    x_free( m_Characters );
    m_bitmap.Destroy(); // Make sure this guy actually unloads
}

//=========================================================================

void ui_font::TextSize( irect& Rect, const char* pString, s32 Count ) const
{
    s32 Height    = m_Height;
    s32 BestWidth = 0;
    s32 Width     = 0;
    ASSERT( pString );

    if( pString )
    {
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
                Width += m_Characters[ LookUpCharacter(c) ].W;
                Width += 1;
            }             

            // Decrease character count
            Count--;
        }

        BestWidth = MAX( BestWidth, Width-1 );
    }

    // We have all we need.
    Rect.Set( 0, 0, BestWidth, Height );
}

//=========================================================================

void ui_font::TextSize( irect& Rect, const xwchar* pString, s32 Count ) const
{
    s32 Height    = m_Height;
    s32 BestWidth = 0;
    s32 Width     = 0;
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
                Height   += m_Height;
            }

            // Check for Control Code
            else if( c == 0xAB ) // '?'
            {
                c = *pString++;

                // If this is a ButtonIcon then Add Sprite Width
                s32 buttonCode = m_pManager->LookUpButtonCode( pString,
                                                               0,
                                                               g_Input.GetCurrentInputDevice(),
                                                               g_Input.GetCurrentInputPlatform() );

                if( buttonCode == CREDIT_TITLE_LINE || buttonCode == NEW_CREDIT_PAGE || buttonCode == CREDIT_END )
                    buttonCode = -1;

                if( buttonCode != -1 )
                {
                    Width += ui_GetEmbeddedButtonAdvance( buttonCode );
                }

                // Loop past control code.
                while( c != 0xBB ) // '?'
                {
                    c = *pString++;
                }
            }
            else
            // Normal character.
            {
                Width += m_Characters[ LookUpCharacter(c) ].W;
                Width += 1;
            }             

            // Decrease character count
            Count--;
        }

        BestWidth = MAX( BestWidth, Width-1 );
    }

    // We have all we need.
    Rect.Set( 0, 0, BestWidth, Height );
}

//=========================================================================

s32 ui_font::TextWidth( const xwchar* pString, s32 Count ) const
{
    s32 BestWidth = 0;
    s32 Width     = 0;
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
            else if( c == 0xAB ) // '?'
            {

                // If this is a ButtonIcon then Add Sprite Width
                s32 ButtonCode = m_pManager->LookUpButtonCode( pString,
                                                               0,
                                                               g_Input.GetCurrentInputDevice(),
                                                               g_Input.GetCurrentInputPlatform() );

                if( ButtonCode == CREDIT_TITLE_LINE || ButtonCode == NEW_CREDIT_PAGE || ButtonCode == CREDIT_END )
                    ButtonCode = -1;

                if( ButtonCode != -1 )
                {
                    Width += ui_GetEmbeddedButtonAdvance( ButtonCode );
                }

                // Loop past control code.
                while( c != 0xBB ) // '?'
                {
                    c = *pString++;
                }
            }

            else
            // Normal character.
            {
                // Add character to width
                Width += m_Characters[ LookUpCharacter(c) ].W;
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

s32 ui_font::TextHeight( const xwchar* pString, s32 Count ) const
{
    s32 Height = 0; 
    s32 ButtonHeight = 22;
    xbool LineHasButton = FALSE;

    ASSERT( pString );

    if( pString )
    {
        // Loop until end of string or end of count.
        while( pString && *pString && (Count != 0) )
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
            // Check for Control Code
            else if( c == 0xAB ) // '?'
            {
                // We have found a button code so we need to make the height of this
                // string the height of the button bmps.
                 LineHasButton = TRUE;

                // Loop past control code.
                while( c != 0xBB ) // '?'
                {
                    c = *pString++;
                }
            }
            else
            // Check for newline.
            if( c == '\n' )
            {
                if( LineHasButton )
                    Height += ButtonHeight;
                else
                    Height += m_Height;

                LineHasButton = FALSE;
            }

            // Decrease character count.
            Count--;
        }
    }

    // Return height
    if( LineHasButton )
        Height += ButtonHeight;
    else
        Height += m_Height;

    return( Height );
}

//=========================================================================

u32 ui_font::LookUpCharacter(u32 c ) const
{
    #define UNDEFINED_CHARACTER (0x7F)

    if( c < 256 )
    {
        // All our fonts should contain a character (a square) to designate an unsupported character.
        // in the event that even this character is not present, display an 'x'.
        // (that character MUST be present, as it's used for 'average width').
        if( !((c < 0x10) || (m_CMap[c].character != 0)) )
        {
            if( m_CMap[UNDEFINED_CHARACTER].character != 0 )
                c = UNDEFINED_CHARACTER;
            else
                c = 'x';
        }

        // c < 256 are direct mapped.
        return m_CMap[c].bitmap;
    }
    else
    {
        //ASSERTS( m_CMapSize > 256, "No extended characters (>256) in font." );
        s32 imax = m_CMapSize;
        s32 imin = 256;
        xbool bFound = FALSE;

        while( imax >= imin )
        {
            s32 i = (imin + imax)/2;

            if( imax == imin + 1 )
            {
                if( m_CMap[i].character == c )
                    bFound = TRUE;
                else  //-- NOT FOUND
                    break;
            }

            if( m_CMap[i].character == c )
                bFound = TRUE;
            else if ( m_CMap[i].character > c )
                imax = i-1;
            else
                imin = i+1;

            if( bFound )
            {
                return( m_CMap[i].bitmap );    
            }
        }

//ASSERTS((0), "could not look up character");

        // return unknown character (see above)
        if( m_CMap[UNDEFINED_CHARACTER].character != 0 )
            return( m_CMap[UNDEFINED_CHARACTER].bitmap );
        else
            return( m_CMap['x'].bitmap );
    }
}

//=========================================================================

const ui_font::Character& ui_font::GetCharacter( s32 Index ) const
{

    return m_Characters[ LookUpCharacter(Index) ];
}

//=========================================================================
//  Return a clipped string with ellipsis that fits in the supplied rect
//=========================================================================

const xwchar* ui_font::ClipEllipsis( const xwchar* pString, const irect& Rect ) const
{
    static xwstring ClippedString;

    // Should we be clipping?
    if( TextWidth( pString ) > Rect.GetWidth() )
    {
        s32     FieldWidth  = Rect.GetWidth() - 15;
        s32     Width       = 0;
        xbool   Clipping    = FALSE;

        // Clear the string
        ClippedString.Clear();

        // Clip the string
        while( *pString )
        {
            xwchar c = *pString++;

            // Check for embedded color code.
            if( (c & 0xFF00) == 0xFF00 )
            {
                // Copy into clipped string
                ClippedString += (xwchar)0xFF00;
                ClippedString += *pString++;
            }
            else
            // Check for newline.
            if( c == '\n' )
            {
                ClippedString  += (xwchar)'\n';
                Width           = 0;
                Clipping        = FALSE;
            }
            else
            // Normal character.
            {
                if( !Clipping )
                {
                    // Add character to width
                    Width += m_Characters[ LookUpCharacter(c) ].W;
                    Width += 1;

                    // Width still in range?
                    if( Width < FieldWidth )
                    {
                        // Add to string
                        ClippedString += c;
                    }
                    else
                    {
                        // Add ellipsis
                        ClippedString += '.';
                        ClippedString += '.';
                        ClippedString += '.';
                        Clipping = TRUE;
                    }
                }
            }
        }

        return (const xwchar*)ClippedString;
    }
    else
    {
        return pString;
    }
}

//=========================================================================
void ui_font::TextWrap( const xwchar* pString, const irect& Rect, xwstring& WrappedString ) 
{
    const xwchar* pStart = pString;

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
                WrappedString += c;
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
            else if( c == 0xAB ) // '?'
            {
                // If this is a ButtonIcon then Add Sprite Width
                s32 ButtonCode = m_pManager->LookUpButtonCode( pString,
                                                               0,
                                                               g_Input.GetCurrentInputDevice(),
                                                               g_Input.GetCurrentInputPlatform() );

                if( ButtonCode == CREDIT_TITLE_LINE || ButtonCode == NEW_CREDIT_PAGE || ButtonCode == CREDIT_END )
                    ButtonCode = -1;

                if( ButtonCode != -1 )
                {
                    Width += ui_GetEmbeddedButtonAdvance( ButtonCode );
                }
                
                WrappedString += c;
                
                // Loop past control code.
                while( c != 0xBB ) // '?'
                {                 
                    c = *pString++;
                    WrappedString += c;
                }
            }
            else
            // Normal character.
            {
                if( !Clipping )
                {
                    // Add character to width
                    Width += m_Characters[ LookUpCharacter(c) ].W;
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
                        c = *--pString;
                        s32 NumChars = 0;

                        while(  (c != ' ') && 
                                (c != '\n') && 
                                (pString > pStart) && 
                                (NumChars < 7)         ) 
                        {
                            c = *--pString;
                            NumChars++;
                        }

                        // Bingo, we backed up to a space!
                        if( c == ' ' )
                        {
                            WrappedString.Delete(WrappedString.GetLength()-NumChars,NumChars);
                            pString++;
                        }

                        // No such luck, just cleave the word in twain.
                        else
                        {
                            WrappedString += '-';
                            pString += NumChars;
                        }

                        Width            = 0;
                        WrappedString   += '\n';
                    }
                }
            }
        }
    }
    else
    {
        // no wrapping required
        WrappedString = pString;
    }
}


//=========================================================================

void ui_font::RenderInputText( const irect&    Rect,
                                    u32       Flags,
                              const xcolor&   aColor,
                              const xwchar*   pString,
                                    input_platform Platform ) const
{
    s32     c;
    s32     sx;
    s32     sy;
    s32     tx       = Rect.l;
    s32     ty       = Rect.t;
    s32     iStart   = 0;
    s32     Width    = 0;
    s32     MaxWidth = 0;
    s32     CurrWidth = 0;
    s32     Height;    
    xcolor  Color    = aColor;
    xbool   bFirstButton = TRUE;
    texture* pFontTexture = m_bitmap.GetPointer();
    if( !pFontTexture )
        return;

    ASSERT( pString );
    vector2 uv0;
    vector2 uv1;   
    vector2 Size( 0, (f32)m_Height );

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


    if (Flags & h_left)
    {
        tx = Rect.l;
    }
    else
    {
        // Determine width of the help text
        while( pString[iStart] )
        {
            c = pString[iStart];

    
            if ((c & 0xff) == 0xAB ) // '?'
            {
                // get the button code
                s32 buttonCode = m_pManager->LookUpButtonCode( pString, iStart, INPUT_DEVICE_GAMEPAD, Platform );

                while( (c & 0xff) != 0xBB ) // '?'
                {
                    iStart++;
                    c = pString[iStart];
                }

                if (CurrWidth > MaxWidth)
                {
                    Width   += CurrWidth;
                }
                else
                {
                    Width   += MaxWidth;
                }

                if (bFirstButton)
                {
                    bFirstButton = FALSE;
                }
                else
                {
                    Width   += HELP_TEXT_SPACING;
                }				
                Width += ui_GetEmbeddedButtonAdvance( buttonCode );
                CurrWidth    = 0;
                MaxWidth     = 0;
            }
            else if ( pString[iStart] == '\n' )
            {
                if (CurrWidth > MaxWidth)
                {
                    MaxWidth = CurrWidth;
                    CurrWidth = 0;
                }
            }
            else
            {
                // add the width of the character
                s32 w  = m_Characters[ LookUpCharacter(c) ].W;
                CurrWidth += w + 1;
            }

            iStart++;
        }
        // add the width of the last word to the total
        if (CurrWidth > MaxWidth)
        {
            Width   += CurrWidth;
        }
        else
        {
            Width   += MaxWidth;
        }
        
        // Adjust lateral position for alignment flags.
        if( Flags & h_center )
        {
            tx = Rect.l + (Rect.GetWidth() - Width) / 2;
        }
        else if( Flags & h_right )
        {
            tx = Rect.l + (Rect.GetWidth() - Width);
        }
    }
   
    
    // initialize block start co-ords
    sx           = tx;
    sy           = ty;

    // reset string controls
    iStart       = 0;
    CurrWidth    = 0;
    MaxWidth     = 0;
    bFirstButton = TRUE;

    // Render text one item block at a time
    while( pString[iStart] )
    {
        c = pString[iStart];

        if( (c & 0xff) == 0xAB ) // '?' 
        {
            // Check for Command Codes
            iStart++;

            // get the button code
            s32 buttonCode = m_pManager->LookUpButtonCode( pString, iStart, INPUT_DEVICE_GAMEPAD, Platform );

            if( buttonCode == CREDIT_TITLE_LINE || buttonCode == NEW_CREDIT_PAGE || buttonCode == CREDIT_END)
                buttonCode = -1;
            
            // If we found a button code then render it.
            if( buttonCode > -1 )
            {
                while( (c & 0xff) != 0xBB ) // '?'
                {
                    iStart++;
                    c = pString[iStart];
                }

                // calculate start pos of icon
                if (CurrWidth > MaxWidth)
                {
                    sx += CurrWidth;
                }
                else
                {
                    sx += MaxWidth;
                }

                // add spacing
                if (bFirstButton)
                {
                    bFirstButton = FALSE;
                }
                else
                {
                    sx += HELP_TEXT_SPACING;
                }

                // reset max block width
                MaxWidth  = 0;
                CurrWidth = 0;

                texture* pButton = m_pManager->GetButtonTexture( buttonCode );
                if( !pButton )
                    continue;

                g_UIRenderer.DrawImage( *pButton,
                                        vector2( (f32)sx + 1.0f, (f32)sy + 1.0f ),
                                        vector2( BUTTON_SPRITE_WIDTH, BUTTON_SPRITE_WIDTH ),
                                        vector2( 0.0f, 0.0f ),
                                        vector2( 1.0f, 1.0f ),
                                        xcolor( 0, 0, 0, 255 ),
                                        0.0f,
                                        UI_BLEND_ALPHA,
                                        UI_SAMPLER_LINEAR_CLAMP );
                g_UIRenderer.DrawImage( *pButton,
                                        vector2( (f32)sx, (f32)sy ),
                                        vector2( BUTTON_SPRITE_WIDTH, BUTTON_SPRITE_WIDTH ),
                                        vector2( 0.0f, 0.0f ),
                                        vector2( 1.0f, 1.0f ),
                                        XCOLOR_WHITE,
                                        0.0f,
                                        UI_BLEND_ALPHA,
                                        UI_SAMPLER_LINEAR_CLAMP );

                sx += ui_GetEmbeddedButtonAdvance( buttonCode );

                tx = sx;
                ty = sy;
            }
        }
        else if ( pString[iStart] == '\n' )
        {
            // found a new line
            tx = sx;
            ty += m_Height;

            if (CurrWidth > MaxWidth)
            {
                MaxWidth = CurrWidth;
                CurrWidth = 0;
            }
        }
        else
        {
            // render the character
            s32 ci = LookUpCharacter(c);
            s32 x  = m_Characters[ ci ].X;
            s32 y  = m_Characters[ ci ].Y;
            s32 w  = m_Characters[ ci ].W;

            const f32 u0 = (f32)x              / m_BmWidth;
            const f32 u1 = (f32)(x + w)        / m_BmWidth;
            const f32 v0 = (f32)y              / m_BmHeight;
            const f32 v1 = (f32)(y + m_Height) / m_BmHeight;

            Size.X = (f32)w;
            Size.Y = (f32)m_Height;

            uv0.Set( u0, v0 );
            uv1.Set( u1, v1 );
            g_UIRenderer.DrawImage( *pFontTexture,
                                    vector2( (f32)tx, (f32)ty ),
                                    Size,
                                    uv0,
                                    uv1,
                                    Color,
                                    0.0f,
                                    UI_BLEND_ALPHA,
                                    UI_SAMPLER_POINT_CLAMP );
        
            tx        += w + 1;
            CurrWidth += w + 1;
        }

        // get new character
        iStart++;
    }
}
//=========================================================================

void ui_font::RenderText( const irect&  Rect, 
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
    s32     buttonCode = -1;
    xcolor  Color1    = aColor;
    xcolor  Color2    = aColor;
    xbool   WasEllipsisClipped = FALSE;

    const s32 MaxButtons = 10;
    s32 NumButtons = 0;
    s32 ButtonCodes [ MaxButtons ];
    f32 Button_X    [ MaxButtons ];
    f32 Button_Y    [ MaxButtons ];


    if( UseGradient )
    {
        if( (aColor.R + aColor.G + aColor.B) / 3.0f > 59.0f )
        {
            Color2.R = 255; //(255 + Color1.R) / 2;
            Color2.G = 255; //(255 + Color1.G) / 2;
            Color2.B = 255; //(255 + Color1.B) / 2;
            Color2.A = aColor.A;
        } 
        else
        {
            Color1.R = 0; //(2
            Color1.G = 0; //(2
            Color1.B = 0; //(2
            Color1.A = aColor.A;
        }


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
    
    const xbool ClipText = (Flags & clip_character) != 0;

    ASSERT( pString );

    const ui_blend_mode FontBlend = (Flags & ui_font::blend_additive)
                                   ? UI_BLEND_ADDITIVE
                                   : UI_BLEND_ALPHA;

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

    // Check for clipping with ellipsis
    if( Flags & clip_ellipsis )
    {
        const xwchar* pNewString = ClipEllipsis( pString, Rect );
        if( pNewString != pString )
        {
            pString = pNewString;
            WasEllipsisClipped = TRUE;
        }
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
        if( (Width > Rect.GetWidth()) || WasEllipsisClipped )
        {
            if( Flags & clip_l_justify ) 
                tx = Rect.l;
            else
            if( Flags & clip_r_justify )
                tx = Rect.r - Width;
        }

        // Skip lines above the visible area and stop once we're past the bottom.
        if( ClipText && (ty + m_Height <= Rect.t) )
        {
            iStart = iEnd;
            if( pString[iStart] == '\n' )
            {
                ty += m_Height;
                iStart++;
            }
            continue;
        }

        if( ClipText && (ty >= Rect.b) )
            break;

        //
        // Render each character.
        //
        for( ; iStart < iEnd; iStart++ )
        {
            c = pString[ iStart ];
            //
            // Button code stuff.

            // Check for Command Codes
            if( c == 0x00AB )
            {
                iStart++;

                if( pString[ iStart ] == 0 ) continue; // shouldn't happen, but much safer.

                // get the button code
                buttonCode = m_pManager->LookUpButtonCode( pString,
                                                           iStart,
                                                           g_Input.GetCurrentInputDevice(),
                                                           g_Input.GetCurrentInputPlatform() );

                if( buttonCode == CREDIT_TITLE_LINE || buttonCode == NEW_CREDIT_PAGE || buttonCode == CREDIT_END )
                    buttonCode = -1;

                while( c && (c != 0x00BB) ) // '?'
                {
                    iStart++;
                    c = pString[iStart];
                }

                
                // If we found a button code then render it.
                if( buttonCode > -1 )
                {
                    while( c && (c != 0x00BB) ) // '?'
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
                    Button_X[ NumButtons ] = (f32)(tx);
                    Button_Y[ NumButtons ] = (f32)(ty);
                    tx += ui_GetEmbeddedButtonAdvance( buttonCode );
                    NumButtons++;
                }
                continue;
            }

            // Look for an embedded color code.
            else if( c && ((c & 0xFF00) == 0xFF00) )
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

            // if we got the the end of the line, bail.
            if( c == 0 ) break;

            //
            // We have a normal character if we've made it this far
            //
            s32 ci = LookUpCharacter(c);
            s32 x  = m_Characters[ ci ].X;
            s32 y  = m_Characters[ ci ].Y;
            s32 w  = m_Characters[ ci ].W;


            const f32 u0 = (f32)x              / m_BmWidth;
            const f32 u1 = (f32)(x + w)        / m_BmWidth;
            const f32 v0 = (f32)y              / m_BmHeight;
            const f32 v1 = (f32)(y + m_Height) / m_BmHeight;
		    
            RenderGlyphQuad( *pFontTexture,
                             MakeGlyphQuad( (f32)tx,
                                            (f32)ty,
                                            (f32)(tx + w),
                                            (f32)(ty + m_Height),
                                            u0,
                                            v0,
                                            u1,
                                            v1 ),
                             Color2,
                             Color1,
                             FontBlend );
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
            texture* pButton = m_pManager->GetButtonTexture( ButtonCodes[i] );
            if( !pButton )
                continue;

            RenderGlyphSprite( *pButton,
                               MakeGlyphQuad( Button_X[ i ] + 1.0f,
                                              Button_Y[ i ] + 1.0f,
                                              Button_X[ i ] + 1.0f + BUTTON_SPRITE_WIDTH,
                                              Button_Y[ i ] + 1.0f + BUTTON_SPRITE_WIDTH,
                                               0.0f,
                                               0.0f,
                                               1.0f,
                                               1.0f ),
                               xcolor(0,0,0,255),
                               UI_BLEND_ALPHA );
            RenderGlyphSprite( *pButton,
                               MakeGlyphQuad( Button_X[ i ],
                                              Button_Y[ i ],
                                              Button_X[ i ] + BUTTON_SPRITE_WIDTH,
                                              Button_Y[ i ] + BUTTON_SPRITE_WIDTH,
                                               0.0f,
                                               0.0f,
                                               1.0f,
                                               1.0f ),
                               xcolor(255,255,255),
                               UI_BLEND_ALPHA );
        }
    }
#endif

    if( ClipText )
        g_UIRenderer.PopClipRect();
}
//=========================================================================

void ui_font::RenderText( const irect&  Rect, 
                                u32     Flags, 
                                s32     Alpha, 
                          const xwchar* pString,
                                xbool   IgnoreEmbeddedColor,
                                xbool   UseGradient, 
                                f32     FlareAmount ) const
{
    xcolor Color = XCOLOR_PURPLE;
    Color.A = Alpha;

    RenderText( Rect, Flags, Color, pString, IgnoreEmbeddedColor, UseGradient, FlareAmount );
}

//=========================================================================

void ui_font::RenderText( const irect&  R, 
                                u32     Flags, 
                          const xcolor& Color, 
                          const char*   pString,
                                xbool   IgnoreEmbeddedColor,
                                xbool   UseGradient,
                                f32     FlareAmount 
                          ) const
{
    xwstring t( pString );

    RenderText( R, Flags, Color, (const xwchar*)t, IgnoreEmbeddedColor, UseGradient, FlareAmount );
}

//=========================================================================

void ui_font::RenderStateControlledText( const irect& Rect, u32 Flags, const xcolor& Color, const xwchar* pString, void* StateData) const
{
    xwchar  c;
    s32     tx       = Rect.l;
    s32     ty       = Rect.t;
    s32     iStart   = 0;
    s32     iEnd     = 0;
    s32     Width;
    s32     Height;    
    s32     buttonCode = -1;
    xcolor  Color1    = Color;
    xbool   WasEllipsisClipped = FALSE;

    texture* pFontTexture = m_bitmap.GetPointer();
    if( !pFontTexture )
        return;

    ASSERT( pString );

    const xbool ClipText = (Flags & clip_character) != 0;

    const ui_blend_mode FontBlend = (Flags & ui_font::blend_additive)
                                   ? UI_BLEND_ADDITIVE
                                   : UI_BLEND_ALPHA;

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

    // Check for clipping with ellipsis
    if( Flags & clip_ellipsis )
    {
        const xwchar* pNewString = ClipEllipsis( pString, Rect );
        if( pNewString != pString )
        {
            pString = pNewString;
            WasEllipsisClipped = TRUE;
        }
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
        if( (Width > Rect.GetWidth()) || WasEllipsisClipped )
        {
            if( Flags & clip_l_justify ) 
                tx = Rect.l;
            else
                if( Flags & clip_r_justify )
                    tx = Rect.r - Width;
        }

        // Skip lines above the visible area and stop once we're past the bottom.
        if( ClipText && (ty + m_Height <= Rect.t) )
        {
            iStart = iEnd;
            if( pString[iStart] == '\n' )
            {
                ty += m_Height;
                iStart++;
            }
            continue;
        }

        if( ClipText && (ty >= Rect.b) )
            break;

        //
        // Render each character.
        //
        for( ; iStart < iEnd; iStart++ )
        {
            c = pString[ iStart ];
            //
            // Button code stuff.

            // Check for Command Codes
            if( c == 0x00AB )
            {
                iStart++;

                if( pString[ iStart ] == 0 ) continue; // shouldn't happen, but much safer.

                // get the button code
                buttonCode = m_pManager->LookUpButtonCode( pString,
                                                           iStart,
                                                           g_Input.GetCurrentInputDevice(),
                                                           g_Input.GetCurrentInputPlatform() );

                if( buttonCode == CREDIT_TITLE_LINE || buttonCode == NEW_CREDIT_PAGE || buttonCode == CREDIT_END )
                    buttonCode = -1;

                while( c && (c != 0x00BB) ) // '?'
                {
                    iStart++;
                    c = pString[iStart];
                }

                // If we found a button code then render it.
                if( buttonCode > -1 )
                {
                    // Buttons not supported
                    ASSERT(0);
                }
                continue;
            }

            // Color Codes get thrown out.
            else if( c && ((c & 0xFF00) == 0xFF00) )
            {                
                iStart++;
                continue;
            } 

            // if we got the the end of the line, bail.
            if( c == 0 ) break;

            //
            // We have a normal character if we've made it this far
            //
            s32 ci = LookUpCharacter(c);
            s32 x  = m_Characters[ ci ].X;
            s32 y  = m_Characters[ ci ].Y;
            s32 w  = m_Characters[ ci ].W;
            s32 dw = w;

            if( (((CustomRenderStruct*)StateData)[iStart]).m_State == s_render )
            {                
                const f32 u0 = (f32)x              / m_BmWidth;
                const f32 u1 = (f32)(x + w)        / m_BmWidth;
                const f32 v0 = (f32)y              / m_BmHeight;
                const f32 v1 = (f32)(y + m_Height) / m_BmHeight;

                xcolor RenderColor = Color1;
                RenderColor.A = (u8)(((CustomRenderStruct*)StateData)[iStart]).m_Value;

                // aharp TODO need to add gradient font
                RenderGlyphSprite( *pFontTexture,
                                   MakeGlyphQuad( (f32)tx,
                                                  (f32)ty,
                                                  (f32)(tx + dw),
                                                  (f32)(ty + m_Height),
                                                  u0,
                                                   v0,
                                                   u1,
                                                   v1 ),
                                   RenderColor,
                                   FontBlend );
            }
            tx += dw + 1;
        }

        // Process newline.
        if( pString[iStart] == '\n' )
        {
            ty += m_Height;
            iStart++;
        }
    }

    if( ClipText )
        g_UIRenderer.PopClipRect();
}

//=========================================================================

ui_font::glyph_quad ui_font::MakeGlyphQuad( f32 X0, f32 Y0, f32 X1, f32 Y1,
                                            f32 U0, f32 V0, f32 U1, f32 V1 )
{
    glyph_quad Quad;

    Quad.X0 = X0;
    Quad.Y0 = Y0;
    Quad.X1 = X1;
    Quad.Y1 = Y1;
    Quad.U0 = U0;
    Quad.V0 = V0;
    Quad.U1 = U1;
    Quad.V1 = V1;

    return Quad;
}

//=========================================================================

void ui_font::RenderGlyphQuad( const texture& Texture,
                               const glyph_quad& Quad,
                               const xcolor& TopColor,
                               const xcolor& BottomColor,
                               ui_blend_mode Blend )
{
    const ui_vertex Vertices[4] =
    {
        ui_vertex( vector2( Quad.X0, Quad.Y0 ), vector2( Quad.U0, Quad.V0 ), TopColor    ),
        ui_vertex( vector2( Quad.X1, Quad.Y0 ), vector2( Quad.U1, Quad.V0 ), TopColor    ),
        ui_vertex( vector2( Quad.X1, Quad.Y1 ), vector2( Quad.U1, Quad.V1 ), BottomColor ),
        ui_vertex( vector2( Quad.X0, Quad.Y1 ), vector2( Quad.U0, Quad.V1 ), BottomColor )
    };
    static const u32 Indices[6] = { 0, 1, 2, 2, 3, 0 };

    g_UIRenderer.GetDrawList().AddTriangles( ui_material( Texture,
                                                          Blend,
                                                          UI_SAMPLER_POINT_CLAMP ),
                                             Vertices,
                                             4,
                                             Indices,
                                             6 );
}

//=========================================================================

void ui_font::RenderGlyphSprite( const texture& Texture,
                                 const glyph_quad& Quad,
                                 const xcolor& Color,
                                 ui_blend_mode Blend )
{
    g_UIRenderer.DrawImage( Texture,
                            vector2( Quad.X0, Quad.Y0 ),
                            vector2( Quad.X1 - Quad.X0, Quad.Y1 - Quad.Y0 ),
                            vector2( Quad.U0, Quad.V0 ),
                            vector2( Quad.U1, Quad.V1 ),
                            Color,
                            0.0f,
                            Blend,
                            UI_SAMPLER_POINT_CLAMP );
}
