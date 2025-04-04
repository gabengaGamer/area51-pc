//==============================================================================
//  
//  x_bitmap_s3tc.cpp
//  
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================
#include "aux_bitmap.hpp"
#include "x_math.hpp"

#ifndef TARGET_PS2
#   ifdef TARGET_XBOX
#       ifdef CONFIG_RETAIL
#           define D3DCOMPILE_PUREDEVICE 1
#       endif
#       include<xtl.h>
#       include<XGraphics.h>
#   endif
#   include<..\3rdParty\DXTLibrary\ImageDXTC.h>
#   if _MSC_VER >= 1300
#       define WIN32_LEAN_AND_MEAN
#       ifdef TARGET_PC
#           include <windows.h>
#           include <D3d8.h>
#           include <XGraphics.h>
#       endif
#   endif
#else
enum DXTCMethod
{
	DC_None,
	DC_DXT1,
	DC_DXT3,
	DC_DXT5,
};
#endif

//==============================================================================
//  DEFINES
//==============================================================================

const f32 ERROR_TOLERANCE = 100.0f;

static auxcompress_fn*     s_pCallback = NULL;      // Progress callback func

//=============================================================================

#ifndef TARGET_PS2
static AlphaType GetAlphaUsage( xbitmap& Source )
{
    if( !Source.HasAlphaBits( ))
        return AT_None;
    if( Source.GetBPP( )<16 )
        return AT_None;

    s32	x,y,i;
    s32	Unique;
    u32	Usage[256];
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
#endif

//=============================================================================

static void PackImage( xbitmap& Dest,const xbitmap& Source,xbool bForceMips,DXTCMethod Method )
{
#ifndef TARGET_PC
    (void)bForceMips;
    (void)Method;
    (void)Source;
    (void)Dest;

#elif _MSC_VER < 1300 // USE NVidia compressor (known issues with DXT1)

    u32 W = Source.GetWidth ( );
    u32 H = Source.GetHeight( );

    // ========================================================================

    xbitmap Temp( Source );
    if( Temp.GetFormat() != xbitmap::FMT_32_BGRA_8888 )
        Temp.ConvertFormat( xbitmap::FMT_32_BGRA_8888 );
    ASSERT( Temp.GetBPP()==32 );
    if( bForceMips && !Temp.GetNMips( ))
        Temp.BuildMips( );

    // ========================================================================

    ImageDXTC Dxtc;

    // ========================================================================

    s32 NMips = Temp.GetNMips( );
    if(!NMips )
    {   //
        //  Create bitmap
        //
        xbitmap::format Format;
        Image32 DxtImage( W,H,( Color* )Temp.GetPixelData( ));
        DxtImage.DiffuseError( 8,5,6,5 );
        switch( Method )
        {
            case DC_None:
                switch( GetAlphaUsage( Temp ))
                {
		            case AT_ConstantBinary:
		            case AT_Constant:
		            case AT_Binary:
		            case AT_None:
                        Dxtc.CompressDXT1( &DxtImage );
                        Format = xbitmap::FMT_DXT1;
                        break;
		            case AT_DualConstant:
		            case AT_Modulated:
                        Dxtc.CompressDXT3( &DxtImage );
                        Format = xbitmap::FMT_DXT3;
                        break;
                }
                break;
            case DC_DXT3:
                Dxtc.CompressDXT3( &DxtImage );
                Format = xbitmap::FMT_DXT3;
                break;
            case DC_DXT1:
                Dxtc.CompressDXT1( &DxtImage );
                Format = xbitmap::FMT_DXT1;
                break;
        }
        //
        //  Create texture
        //
        Dest.Setup( Format,W,H,TRUE,NULL,FALSE,NULL,W,NMips );
        u32 Size = Dest.GetDataSize();
        x_memmove(
            (void*)Dest.GetPixelData(),
            Dxtc.GetBlocks( ),
            Size );
        x_memset(
            &DxtImage,
            0,
            sizeof( Image32 ));
        return;
    }

    // ========================================================================

    for( s32 i=0;i<=NMips;i++ )
    {   //
        //  Compress image
        //
        Image32 DxtImage;
        u32 W = Temp.GetWidth (i);
        u32 H = Temp.GetHeight(i);
        DxtImage.SetSize( W,H );
        x_memmove(
            DxtImage.GetPixels     ( ),
            Temp    .GetPixelData  (i),
            Temp    .GetMipDataSize(i));
        //
        //  Duplicate pixels( assumes RGBA not ARGB)
        //
        DxtImage.DiffuseError( 8,5,6,5 );
    	Dxtc.FromImage32( &DxtImage,Method );
        if( !i )
        {
            Method = Dxtc.GetMethod( );
            xbitmap::format Format;
            switch( Method )
            {
                case DC_DXT1: Format = xbitmap::FMT_DXT1; break;
                case DC_DXT3: Format = xbitmap::FMT_DXT3; break;

                default:
                    ASSERT(0);
                    break;
            }
            Dest.Setup( Format,W,H,TRUE,NULL,FALSE,NULL,W,NMips );
        }
        else
        {
            ASSERT( Method==Dxtc.GetMethod( ));
        }
        //
        //  Load dest
        //
        u32 Size = Dest.GetMipDataSize(i);
        x_memmove(
            (void*)Dest.GetPixelData(i),
            Dxtc.GetBlocks( ),
            Size );
    }

#else // Use Xbox specific compilers

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
            case DC_DXT3: goto Dxt3;
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

          Dxt3: Format = xbitmap::FMT_DXT3;
                DstD3DFormat = D3DFMT_DXT3;
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
        case DC_DXT3: goto dxt3;
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

      dxt3: Format = xbitmap::FMT_DXT3;
            DstD3DFormat = D3DFMT_DXT3;
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

#endif
}

//=============================================================================

void auxbmp_CompressDXTC( xbitmap& Dest, const xbitmap& Bitmap, xbool bForceMips )
{
    if( Bitmap.GetFormat()==xbitmap::FMT_DXT1 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT3 )
        return;
    PackImage( Dest,Bitmap,bForceMips,DC_None );
}

//=============================================================================

void auxbmp_CompressDXTC( xbitmap& Bitmap, xbool bForceMips )
{
    if( Bitmap.GetFormat()==xbitmap::FMT_DXT1 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT3 )
        return;
    xbitmap Temp;
    auxbmp_CompressDXTC( Temp, Bitmap, bForceMips );
    Bitmap = Temp;
}

//=============================================================================

void auxbmp_CompressDXT3( xbitmap& Dest, const xbitmap& Bitmap, xbool bForceMips )
{
    if( Bitmap.GetFormat()==xbitmap::FMT_DXT1 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT3 )
        return;
    PackImage( Dest,Bitmap,bForceMips,DC_DXT3 );
}

//=============================================================================

void auxbmp_CompressDXT3( xbitmap& Bitmap, xbool bForceMips )
{
    if( Bitmap.GetFormat()==xbitmap::FMT_DXT1 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT3 )
        return;
    xbitmap Temp;
    auxbmp_CompressDXT3( Temp, Bitmap, bForceMips );
    Bitmap = Temp;
}

//=============================================================================

void auxbmp_CompressDXT1( xbitmap& Dest, const xbitmap& Bitmap, xbool bForceMips )
{
    if( Bitmap.GetFormat()==xbitmap::FMT_DXT1 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT3 )
        return;
    PackImage( Dest,Bitmap,bForceMips,DC_DXT1 );
}

//=============================================================================

void auxbmp_CompressDXT1( xbitmap& Bitmap,xbool bForceMips )
{
    if( Bitmap.GetFormat()==xbitmap::FMT_DXT1 ||
        Bitmap.GetFormat()==xbitmap::FMT_DXT3 )
        return;
    xbitmap Temp;
    auxbmp_CompressDXT1( Temp, Bitmap, bForceMips );
    Bitmap = Temp;
}

//=============================================================================

static void DecompressDXTC( xbitmap& Dest, const xbitmap& Source, DXTCMethod Method )
{
#ifdef TARGET_PS2
    (void)Method;
    (void)Source;
    (void)Dest;
#else
    s32  H     = Source.GetHeight( );
    s32  W     = Source.GetWidth ( );
    s32  NMips = Source.GetNMips ( );

    ImageDXTC dxtc;
    Image32 Temp;

    if( !NMips )
    {
        dxtc.SetMethod( Method );
        dxtc.SetSize( W,H );
        x_memmove(
            dxtc  .GetBlocks   ( ),
            Source.GetPixelData( ),
            Source.GetDataSize ( ));
        dxtc.ToImage32( &Temp );
        Dest.Setup(
            xbitmap::FMT_32_BGRA_8888,
            W,
            H,
            TRUE,
            NULL );
        x_memmove(
            (void*)Dest.GetPixelData( ),
            Temp.GetPixels( ),
            W*H*4 );
    }
    else
    {
        Dest.Setup( xbitmap::FMT_32_BGRA_8888,W,H,TRUE,NULL,FALSE,NULL,W,NMips );
        for( s32 i=0;i<=NMips;i++ )
        {
            dxtc.SetMethod( Method );
            dxtc.SetSize( W,H );
            x_memmove(
                dxtc  .GetBlocks     ( ),
                Source.GetPixelData  (i),
                Source.GetMipDataSize(i));
            dxtc.ToImage32( &Temp );
            x_memmove(
                (void*)Dest.GetPixelData(i),
                Temp.GetPixels( ),
                W*H*4 );
            W >>= 1;
            H >>= 1;
        }
    }
#endif
}

//=============================================================================

void auxbmp_DecompressDXT1( xbitmap& Dest, const xbitmap& Source )
{
    DecompressDXTC( Dest,Source,DC_DXT1 );
}

//=============================================================================

void auxbmp_DecompressDXT3( xbitmap& Dest, const xbitmap& Source )
{
    DecompressDXTC( Dest,Source,DC_DXT3 );
}

//=============================================================================

void auxbmp_DecompressDXT1( xbitmap& Bitmap )
{
    xbitmap Temp;
    auxbmp_DecompressDXT1( Temp,Bitmap );
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

void auxbmp_SetCompressCallback( auxcompress_fn* pFn )
{
    s_pCallback = pFn;
}

//=============================================================================

xcolor auxbmp_ReadPixelColorDXT1( const xbitmap& Bitmap,s32 X,s32 Y,s32 Mip )
{
#ifdef TARGET_PS2
    (void)Bitmap;
    (void)Mip;
    (void)X;
    (void)Y;
    return xcolor(0,0,0,0);
#else
    xcolor Result;
    {
        s32 y = Y/4;
        s32 x = X/4;
        s32 w = Bitmap.GetWidth(Mip);

        u16* pSrc = (u16*)(Bitmap.GetPixelData(Mip)+(y*(w/4)+x)*8);

        // Optimise linear searches
        static u16* pOld = NULL;
        static Color Cache[16];
        if( pOld!=pSrc )
        {
            PlotDXT1( pSrc,Cache,4 );
            pOld = pSrc;
        }

        // Return color
        s32 I = ((Y%4)*4)+(X%4);
        Result = xcolor(
            Cache[I].r,
            Cache[I].g,
            Cache[I].b,
            Cache[I].a );
    }
    return Result;
#endif
}

//=============================================================================

xcolor auxbmp_ReadPixelColorDXT3( const xbitmap& Bitmap,s32 X,s32 Y,s32 Mip )
{
#ifdef TARGET_PS2
    (void)Bitmap;
    (void)Mip;
    (void)X;
    (void)Y;
    return xcolor(0,0,0,0);
#else
    xcolor Result;
    {
        s32 y = Y/4;
        s32 x = X/4;
        s32 w = Bitmap.GetWidth(Mip);

        // Calculate tile address
        u16* pSrc = (u16*)(Bitmap.GetPixelData(Mip)+(y*(w/4)+x)*16);

        // Optimise linear searches
        static u16* pOld = NULL;
        static Color Cache[16];
        if( pOld!=pSrc )
        {
            PlotDXT1     ( pSrc+4,Cache,4 );
		    PlotDXT3Alpha( pSrc  ,Cache,4 );
            pOld = pSrc;
        }

        // Return color
        s32 I = ((Y%4)*4)+(X%4);
        Result = xcolor(
            Cache[I].r,
            Cache[I].g,
            Cache[I].b,
            Cache[I].a );
    }
    return Result;
#endif
}
