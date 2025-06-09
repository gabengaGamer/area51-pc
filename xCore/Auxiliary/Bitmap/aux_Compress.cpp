//==============================================================================
//  
//  aux_Compress.cpp
//  
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================
#include "aux_bitmap.hpp"
#include "x_math.hpp"

//==============================================================================
//  EXTERNAL FUNCTIONS FROM x_bitmap_dxtc.cpp
//==============================================================================

extern xcolor ReadPixelColorDXT1( const xbitmap* pBMP, s32 X, s32 Y, s32 Mip );
extern xcolor ReadPixelColorDXT2( const xbitmap* pBMP, s32 X, s32 Y, s32 Mip );
extern xcolor ReadPixelColorDXT3( const xbitmap* pBMP, s32 X, s32 Y, s32 Mip );
extern xcolor ReadPixelColorDXT4( const xbitmap* pBMP, s32 X, s32 Y, s32 Mip );
extern xcolor ReadPixelColorDXT5( const xbitmap* pBMP, s32 X, s32 Y, s32 Mip );
extern xbitmap UnpackImage( const xbitmap& Source );

//==============================================================================
//  DEFINES
//==============================================================================

const f32 ERROR_TOLERANCE = 100.0f;

static auxcompress_fn*     s_pCallback = NULL;      // Progress callback func

//=============================================================================

static AlphaType GetAlphaUsage( xbitmap& Source )
{
    if( !Source.HasAlphaBits( ))
        return AT_None;
    if( Source.GetBPP( )<16 )
        return AT_None;

    s32    x,y,i;
    s32    Unique;
    u32    Usage[256];
    x_memset( Usage,0,sizeof( Usage ));
    //
    //  Count all the different values used
    //
    for( i=0;i<=Source.GetNMips ( );i++ )
    for( y=0;y< Source.GetHeight(i);y++ )
    for( x=0;x< Source.GetWidth (i);x++ )
    {
        xcolor Color = Source.GetPixelColor( x,y,i );
        if( 0 )
            Usage[Color.A]++;
        else
        {
            u32 Ave = (Color.A+Color.R+Color.G+Color.B)/4;
            Usage[Ave]++;
        }
    }
    //
    //  Count the number of unique alpha values
    //
    Unique = 0;
    for( x=0;x<256;x++ )
        Unique += ( Usage[x]!=0 );
    //
    //  Based on the unique alphas, classify the image
    //
    switch(Unique)
    {
        case 0:
            return AT_None;
        case 1:
            if( Usage[0xFF] ) return AT_None;
            if( Usage[0x00] )
            {
                for( i=0;i<=Source.GetNMips ( );i++ )
                for( y=0;y< Source.GetHeight(i);y++ )
                for( x=0;x< Source.GetWidth (i);x++ )
                {
                    xcolor Color = Source.GetPixelColor( x,y,i ); Color.A = 0xFF;
                    Source.SetPixelColor( Color,x,y,i );
                }
            }
            return AT_Constant;
        case 2:
            if(Usage[0] && Usage[0xff])
                return AT_Binary;
            if(Usage[0])
                return AT_ConstantBinary;
            return AT_DualConstant;

        default:
            return AT_Modulated;
    }
}

//=============================================================================

static void PackImage( xbitmap& Dest,const xbitmap& Source,xbool bForceMips,DXTCMethod Method )
{
    u32 W = Source.GetWidth ( );
    u32 H = Source.GetHeight( );

    // ========================================================================

    xbitmap Temp( Source );
    D3DFORMAT SrcD3DFormat;

    switch( Source.GetFormat( ))
    {
        case xbitmap::FMT_32_ARGB_8888 : SrcD3DFormat=D3DFMT_LIN_A8R8G8B8; break;
        case xbitmap::FMT_32_URGB_8888 : SrcD3DFormat=D3DFMT_LIN_X8R8G8B8; break;
        case xbitmap::FMT_16_RGB_565   : SrcD3DFormat=D3DFMT_LIN_R5G6B5  ; break;
        case xbitmap::FMT_16_ARGB_1555 : SrcD3DFormat=D3DFMT_LIN_A1R5G5B5; break;
        case xbitmap::FMT_16_URGB_1555 : SrcD3DFormat=D3DFMT_LIN_X1R5G5B5; break;
        case xbitmap::FMT_16_ARGB_4444 : SrcD3DFormat=D3DFMT_LIN_A4R4G4B4; break;
        case xbitmap::FMT_32_ABGR_8888 : SrcD3DFormat=D3DFMT_LIN_A8B8G8R8; break;
        case xbitmap::FMT_32_UBGR_8888 : SrcD3DFormat=D3DFMT_LIN_B8G8R8A8; break;
        case xbitmap::FMT_16_RGBA_4444 : SrcD3DFormat=D3DFMT_LIN_R4G4B4A4; break;
        case xbitmap::FMT_16_RGBA_5551 : SrcD3DFormat=D3DFMT_LIN_R5G5B5A1; break;
        case xbitmap::FMT_32_RGBA_8888 : SrcD3DFormat=D3DFMT_LIN_R8G8B8A8; break;

        default:
            Temp.ConvertFormat( xbitmap::FMT_32_BGRA_8888 );
            SrcD3DFormat=D3DFMT_LIN_B8G8R8A8;
            break;
    }

    // ========================================================================

    if( bForceMips && !Temp.GetNMips( ))
        Temp.BuildMips( );

    // ========================================================================

    D3DFORMAT DstD3DFormat;
    u32 Stride = 0;
    u32 Flags  = 0;
    f32 fMed = 0.0f;

    s32 NMips = Temp.GetNMips( );
    if(!NMips )
    {   //
        //  Determine compression style
        //
        xbitmap::format Format;
        switch( Method )
        {
            case DC_DXT1: goto Dxt1;
            case DC_DXT2: goto Dxt2;
            case DC_DXT3: goto Dxt3;
            case DC_DXT4: goto Dxt4;
            case DC_DXT5: goto Dxt5;
            case DC_None:
                switch( GetAlphaUsage( Temp ))
                {
                    case AT_ConstantBinary:
                    case AT_Constant:
                    case AT_Binary:
                    case AT_None:
                        goto Dxt1;
                    case AT_DualConstant:
                    case AT_Modulated:
                        goto Dxt3;
                }

          Dxt1: Format = xbitmap::FMT_DXT1;
                DstD3DFormat = D3DFMT_DXT1;
                Stride = (W/4)*8;
                break;

          Dxt2: Format = xbitmap::FMT_DXT2;
                DstD3DFormat = D3DFMT_DXT2;
                Stride = (W/4)*16;
                break;

          Dxt3: Format = xbitmap::FMT_DXT3;
                DstD3DFormat = D3DFMT_DXT3;
                Stride = (W/4)*16;
                break;

          Dxt4: Format = xbitmap::FMT_DXT4;
                DstD3DFormat = D3DFMT_DXT4;
                Stride = (W/4)*16;
                break;

          Dxt5: Format = xbitmap::FMT_DXT5;
                DstD3DFormat = D3DFMT_DXT5;
                Stride = (W/4)*16;
                break;

            default:
                ASSERT(0);
                break;
        }
        Dest.Setup( Format,W,H,TRUE,NULL,FALSE,NULL,W,NMips );
        //
        //  Compress non-swizzled formats
        //
        ASSERT( !XGIsSwizzledFormat( DstD3DFormat ));
        HRESULT hr = XGCompressRect(
            (LPVOID)Dest.GetPixelData(),
            DstD3DFormat,
            Stride,
            W,
            H,
            (LPVOID)Temp.GetPixelData(),
            SrcD3DFormat,
            (Temp.GetBPP()*W)>>3,
            fMed,
            0 );
        ASSERT( !hr );
        return;
    }

    // ========================================================================

    //
    //  Determine compression style
    //
    xbitmap::format Format;
    switch( Method )
    {
        case DC_DXT1: goto dxt1;
        case DC_DXT2: goto dxt2;
        case DC_DXT3: goto dxt3;
        case DC_DXT4: goto dxt4;
        case DC_DXT5: goto dxt5;
        case DC_None:
            switch( GetAlphaUsage( Temp ))
            {
                case AT_ConstantBinary:
                case AT_Constant:
                case AT_Binary:
                case AT_None:
                    goto dxt1;
                case AT_DualConstant:
                case AT_Modulated:
                    goto dxt3;
            }

      dxt1: Format = xbitmap::FMT_DXT1;
            DstD3DFormat = D3DFMT_DXT1;
            Stride = (W/4)*8;
            break;

      dxt2: Format = xbitmap::FMT_DXT2;
            DstD3DFormat = D3DFMT_DXT2;
            Stride = (W/4)*16;
            break;

      dxt3: Format = xbitmap::FMT_DXT3;
            DstD3DFormat = D3DFMT_DXT3;
            Stride = (W/4)*16;
            break;

      dxt4: Format = xbitmap::FMT_DXT4;
            DstD3DFormat = D3DFMT_DXT4;
            Stride = (W/4)*16;
            break;

      dxt5: Format = xbitmap::FMT_DXT5;
            DstD3DFormat = D3DFMT_DXT5;
            Stride = (W/4)*16;
            break;

        default:
            ASSERT(0);
            break;
    }
    Dest.Setup( Format,
                W,
                H,
                TRUE,
                NULL,
                FALSE,
                NULL,
                W,
                NMips );

    // ========================================================================

    for( s32 iMip=0;iMip<=NMips;iMip++ )
    {   //
        //  Compress non-swizzled formats
        //
        ASSERT( !XGIsSwizzledFormat( DstD3DFormat ));
        HRESULT hr = XGCompressRect(
            (LPVOID)Dest.GetPixelData(iMip),
            DstD3DFormat,
            Stride,
            W,
            H,
            (LPVOID)Temp.GetPixelData(iMip),
            SrcD3DFormat,
            (Temp.GetBPP()*W)>>3,
            fMed,
            0 );
        ASSERT( !hr );
        //
        //  Next mip
        //
        Stride >>= 1;
        W      >>= 1;
        H      >>= 1;
    }
}

//=============================================================================

void auxbmp_CompressDXTC( xbitmap& Dest, const xbitmap& Bitmap, xbool bForceMips )
{
    if( Bitmap.GetFormat()==xbitmap::FMT_DXT1 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT2 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT3 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT4 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT5 )
        return;
    PackImage( Dest,Bitmap,bForceMips,DC_None );
}

//=============================================================================

void auxbmp_CompressDXTC( xbitmap& Bitmap, xbool bForceMips )
{
    if( Bitmap.GetFormat()==xbitmap::FMT_DXT1 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT2 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT3 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT4 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT5 )
        return;
    xbitmap Temp;
    auxbmp_CompressDXTC( Temp, Bitmap, bForceMips );
    Bitmap = Temp;
}

//=============================================================================

void auxbmp_CompressDXT1( xbitmap& Dest, const xbitmap& Bitmap, xbool bForceMips )
{
    if( Bitmap.GetFormat()==xbitmap::FMT_DXT1 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT2 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT3 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT4 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT5 )
        return;
    PackImage( Dest,Bitmap,bForceMips,DC_DXT1 );
}

//=============================================================================

void auxbmp_CompressDXT1( xbitmap& Bitmap,xbool bForceMips )
{
    if( Bitmap.GetFormat()==xbitmap::FMT_DXT1 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT2 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT3 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT4 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT5 )
        return;
    xbitmap Temp;
    auxbmp_CompressDXT1( Temp, Bitmap, bForceMips );
    Bitmap = Temp;
}

//=============================================================================

void auxbmp_CompressDXT2( xbitmap& Dest, const xbitmap& Bitmap, xbool bForceMips )
{
    if( Bitmap.GetFormat()==xbitmap::FMT_DXT1 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT2 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT3 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT4 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT5 )
        return;
    PackImage( Dest,Bitmap,bForceMips,DC_DXT2 );
}

//=============================================================================

void auxbmp_CompressDXT2( xbitmap& Bitmap,xbool bForceMips )
{
    if( Bitmap.GetFormat()==xbitmap::FMT_DXT1 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT2 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT3 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT4 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT5 )
        return;
    xbitmap Temp;
    auxbmp_CompressDXT2( Temp, Bitmap, bForceMips );
    Bitmap = Temp;
}

//=============================================================================

void auxbmp_CompressDXT3( xbitmap& Dest, const xbitmap& Bitmap, xbool bForceMips )
{
    if( Bitmap.GetFormat()==xbitmap::FMT_DXT1 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT2 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT3 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT4 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT5 )
        return;
    PackImage( Dest,Bitmap,bForceMips,DC_DXT3 );
}

//=============================================================================

void auxbmp_CompressDXT3( xbitmap& Bitmap, xbool bForceMips )
{
    if( Bitmap.GetFormat()==xbitmap::FMT_DXT1 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT2 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT3 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT4 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT5 )
        return;
    xbitmap Temp;
    auxbmp_CompressDXT3( Temp, Bitmap, bForceMips );
    Bitmap = Temp;
}

//=============================================================================

void auxbmp_CompressDXT4( xbitmap& Dest, const xbitmap& Bitmap, xbool bForceMips )
{
    if( Bitmap.GetFormat()==xbitmap::FMT_DXT1 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT2 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT3 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT4 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT5 )
        return;
    PackImage( Dest,Bitmap,bForceMips,DC_DXT4 );
}

//=============================================================================

void auxbmp_CompressDXT4( xbitmap& Bitmap,xbool bForceMips )
{
    if( Bitmap.GetFormat()==xbitmap::FMT_DXT1 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT2 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT3 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT4 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT5 )
        return;
    xbitmap Temp;
    auxbmp_CompressDXT4( Temp, Bitmap, bForceMips );
    Bitmap = Temp;
}

//=============================================================================

void auxbmp_CompressDXT5( xbitmap& Dest, const xbitmap& Bitmap, xbool bForceMips )
{
    if( Bitmap.GetFormat()==xbitmap::FMT_DXT1 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT2 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT3 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT4 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT5 )
        return;
    PackImage( Dest,Bitmap,bForceMips,DC_DXT5 );
}

//=============================================================================

void auxbmp_CompressDXT5( xbitmap& Bitmap,xbool bForceMips )
{
    if( Bitmap.GetFormat()==xbitmap::FMT_DXT1 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT2 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT3 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT4 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT5 )
        return;
    xbitmap Temp;
    auxbmp_CompressDXT5( Temp, Bitmap, bForceMips );
    Bitmap = Temp;
}

//=============================================================================

void auxbmp_DecompressDXT1( xbitmap& Dest, const xbitmap& Source )
{
    Dest = UnpackImage( Source );
}

//=============================================================================

void auxbmp_DecompressDXT2( xbitmap& Dest, const xbitmap& Source )
{
    Dest = UnpackImage( Source );
}

//=============================================================================

void auxbmp_DecompressDXT3( xbitmap& Dest, const xbitmap& Source )
{
    Dest = UnpackImage( Source );
}

//=============================================================================

void auxbmp_DecompressDXT4( xbitmap& Dest, const xbitmap& Source )
{
    Dest = UnpackImage( Source );
}

//=============================================================================

void auxbmp_DecompressDXT5( xbitmap& Dest, const xbitmap& Source )
{
    Dest = UnpackImage( Source );
}

//=============================================================================

void auxbmp_DecompressDXT1( xbitmap& Bitmap )
{
    xbitmap Temp;
    auxbmp_DecompressDXT1( Temp,Bitmap );
    Bitmap = Temp;
}

//=============================================================================

void auxbmp_DecompressDXT2( xbitmap& Bitmap )
{
    xbitmap Temp;
    auxbmp_DecompressDXT2( Temp,Bitmap );
    Bitmap = Temp;
}

//=============================================================================

void auxbmp_DecompressDXT3( xbitmap& Bitmap )
{
    xbitmap Temp;
    auxbmp_DecompressDXT3( Temp,Bitmap );
    Bitmap = Temp;
}

//=============================================================================

void auxbmp_DecompressDXT4( xbitmap& Bitmap )
{
    xbitmap Temp;
    auxbmp_DecompressDXT4( Temp,Bitmap );
    Bitmap = Temp;
}

//=============================================================================

void auxbmp_DecompressDXT5( xbitmap& Bitmap )
{
    xbitmap Temp;
    auxbmp_DecompressDXT5( Temp,Bitmap );
    Bitmap = Temp;
}

//=============================================================================

void auxbmp_SetCompressCallback( auxcompress_fn* pFn )
{
    s_pCallback = pFn;
}

//=============================================================================

xcolor auxbmp_ReadPixelColorDXT1( const xbitmap& Bitmap,s32 X,s32 Y,s32 Mip )
{
    return ReadPixelColorDXT1( &Bitmap, X, Y, Mip );
}

//=============================================================================

xcolor auxbmp_ReadPixelColorDXT2( const xbitmap& Bitmap,s32 X,s32 Y,s32 Mip )
{
    return ReadPixelColorDXT2( &Bitmap, X, Y, Mip );
}

//=============================================================================

xcolor auxbmp_ReadPixelColorDXT3( const xbitmap& Bitmap,s32 X,s32 Y,s32 Mip )
{
    return ReadPixelColorDXT3( &Bitmap, X, Y, Mip );
}

//=============================================================================

xcolor auxbmp_ReadPixelColorDXT4( const xbitmap& Bitmap,s32 X,s32 Y,s32 Mip )
{
    return ReadPixelColorDXT4( &Bitmap, X, Y, Mip );
}

//=============================================================================

xcolor auxbmp_ReadPixelColorDXT5( const xbitmap& Bitmap,s32 X,s32 Y,s32 Mip )
{
    return ReadPixelColorDXT5( &Bitmap, X, Y, Mip );
}