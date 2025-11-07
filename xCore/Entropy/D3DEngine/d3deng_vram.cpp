//==============================================================================
//  
//  d3deng_vram.cpp
//  
//==============================================================================

//==============================================================================
//  PLATFORM CHECK
//==============================================================================

#include "x_target.hpp"

#ifndef TARGET_PC
#error This file should only be compiled for PC platform. Please check your exclusions on your project spec.
#endif

//=========================================================================
// INCLUDES
//=========================================================================

#include "e_Engine.hpp"

//=========================================================================
// DEFINES
//=========================================================================

#define MAX_TEXTURES 8192

//=========================================================================
// TYPES
//=========================================================================

struct d3d_tex_node
{
    ID3D11Resource*            pResource;
    ID3D11ShaderResourceView*  pSRV;
    D3D11_RESOURCE_DIMENSION   Dimension;
    s32                        iNext;
};

//=========================================================================
// LOCALS
//=========================================================================

static s32          s_LastActiveID = 0;
static d3d_tex_node s_List[ MAX_TEXTURES ];
static s32          s_iEmpty;

//=========================================================================
// HELPER FUNCTIONS
//=========================================================================

static 
DXGI_FORMAT GetDXGIFormat( xbitmap::format Format )
{
    switch( Format )
    {
        case xbitmap::FMT_32_ARGB_8888: return DXGI_FORMAT_B8G8R8A8_UNORM;
        case xbitmap::FMT_32_URGB_8888: return DXGI_FORMAT_B8G8R8X8_UNORM;
        case xbitmap::FMT_16_ARGB_4444: return DXGI_FORMAT_B4G4R4A4_UNORM;
        case xbitmap::FMT_16_ARGB_1555: return DXGI_FORMAT_B5G5R5A1_UNORM;
        case xbitmap::FMT_16_URGB_1555: return DXGI_FORMAT_B5G5R5A1_UNORM;
        case xbitmap::FMT_16_RGB_565:   return DXGI_FORMAT_B5G6R5_UNORM;
        case xbitmap::FMT_DXT1:         return DXGI_FORMAT_BC1_UNORM;
        case xbitmap::FMT_DXT3:         return DXGI_FORMAT_BC2_UNORM;
        case xbitmap::FMT_DXT5:         return DXGI_FORMAT_BC3_UNORM;
        default:                        return DXGI_FORMAT_UNKNOWN;
    }
}

//=============================================================================

static 
xbool IsCompressedFormat( DXGI_FORMAT Format )
{
    switch( Format )
    {
        case DXGI_FORMAT_BC1_UNORM:
        case DXGI_FORMAT_BC2_UNORM:
        case DXGI_FORMAT_BC3_UNORM:
            return TRUE;
        default:
            return FALSE;
    }
}

//=============================================================================

static 
s32 GetBlockSize( DXGI_FORMAT Format )
{
    switch( Format )
    {
        case DXGI_FORMAT_BC1_UNORM: return 8;
        case DXGI_FORMAT_BC2_UNORM: return 16;
        case DXGI_FORMAT_BC3_UNORM: return 16;
        default:                    return 0;
    }
}

//=========================================================================
// FUNCTIONS
//=========================================================================

void vram_Init( void )
{
    //
    // Initialize the empty list
    //
    s32 i;
    for( i=0; i<MAX_TEXTURES-1; i++ )
    {
        s_List[i].iNext       = i+1;
        s_List[i].pResource   = 0;
        s_List[i].pSRV        = 0;
        s_List[i].Dimension   = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    }
    s_List[i].iNext       = 0;    // Zero is the universal NULL
    s_List[i].pResource   = 0;
    s_List[i].pSRV        = 0;
    s_List[i].Dimension   = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    s_iEmpty              = 1;    // Leave empty the node 0
}

//=============================================================================

void vram_Kill( void )
{
    for( s32 i=0; i<MAX_TEXTURES; i++ )
    {
        if( s_List[i].pResource != 0 )
        {
            if( s_List[i].pSRV )      s_List[i].pSRV->Release();
            if( s_List[i].pResource ) s_List[i].pResource->Release();
            s_List[i].pResource = 0;
            s_List[i].pSRV      = 0;
            s_List[i].Dimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
        }
    }
}

//=============================================================================

static s32 AddNode( ID3D11Resource* pResource, ID3D11ShaderResourceView* pSRV, D3D11_RESOURCE_DIMENSION Dimension )
{
    s32 NewID;

    ASSERTS( s_iEmpty != 0, "Out of texture space" );

    NewID    = s_iEmpty;
    s_iEmpty = s_List[ s_iEmpty ].iNext;

    ASSERT( NewID > 0 );
    ASSERT( NewID < MAX_TEXTURES );

    s_List[ NewID ].pResource = pResource;
    s_List[ NewID ].pSRV      = pSRV;
    s_List[ NewID ].Dimension = Dimension;
    s_List[ NewID ].iNext     = 0;

    return NewID;
}

//=============================================================================

s32 vram_LoadTexture( const char* pFileName )
{
    // TODO: GS: Implement this.
    return 0;
}

//=============================================================================

void vram_Activate( void )
{
    ASSERT( eng_InBeginEnd() );

    s_LastActiveID = 0;
    
    if( g_pd3dContext )
    {
        ID3D11ShaderResourceView* nullSRV = NULL;
        g_pd3dContext->PSSetShaderResources( 0, 1, &nullSRV );
    }
}

//=============================================================================

void vram_Activate( s32 VRAM_ID )
{
    // Note: VRAM_ID == 0 means bitmap not registered!
    ASSERT( VRAM_ID > 0 );              
    ASSERT( VRAM_ID < MAX_TEXTURES );
    ASSERT( eng_InBeginEnd() );
    ASSERT( s_List[ VRAM_ID ].pResource != NULL );
    ASSERT( s_List[ VRAM_ID ].pSRV != NULL );

    s_LastActiveID = VRAM_ID;
    
    if( g_pd3dContext )
    {
        g_pd3dContext->PSSetShaderResources( 0, 1, &s_List[ VRAM_ID ].pSRV );
    }
}

//=============================================================================

s32 vram_Register( ID3D11Texture1D* pTexture )
{
    ID3D11ShaderResourceView* pSRV = NULL;
    HRESULT                   Error;
    D3D11_TEXTURE1D_DESC      desc;

    if( !pTexture || !g_pd3dDevice )
        return 0;

    pTexture->GetDesc( &desc );

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    x_memset( &srvDesc, 0, sizeof(srvDesc) );
    u32 mipLevels          = desc.MipLevels ? desc.MipLevels : (u32)-1;
    srvDesc.Format        = desc.Format;
    srvDesc.ViewDimension = (desc.ArraySize > 1) ? D3D11_SRV_DIMENSION_TEXTURE1DARRAY
                                                 : D3D11_SRV_DIMENSION_TEXTURE1D;

    if( desc.ArraySize > 1 )
    {
        srvDesc.Texture1DArray.MipLevels        = mipLevels;
        srvDesc.Texture1DArray.MostDetailedMip  = 0;
        srvDesc.Texture1DArray.FirstArraySlice  = 0;
        srvDesc.Texture1DArray.ArraySize        = desc.ArraySize;
    }
    else
    {
        srvDesc.Texture1D.MipLevels       = mipLevels;
        srvDesc.Texture1D.MostDetailedMip = 0;
    }

    Error = g_pd3dDevice->CreateShaderResourceView( pTexture, &srvDesc, &pSRV );
    if( FAILED( Error ) )
        return 0;

    pTexture->AddRef();

    s32 Index = AddNode( pTexture, pSRV, D3D11_RESOURCE_DIMENSION_TEXTURE1D );
    return Index;
}

//=============================================================================

s32 vram_Register( ID3D11Texture2D* pTexture )
{
    ID3D11ShaderResourceView* pSRV = NULL;
    HRESULT                   Error;
    D3D11_TEXTURE2D_DESC      desc;

    if( !pTexture || !g_pd3dDevice )
        return 0;

    // Get texture description
    pTexture->GetDesc( &desc );

    // Create shader resource view
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    x_memset( &srvDesc, 0, sizeof(srvDesc) );
    u32 mipLevels                     = desc.MipLevels ? desc.MipLevels : (u32)-1;
    srvDesc.Format                    = desc.Format;
    srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels       = mipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;

    Error = g_pd3dDevice->CreateShaderResourceView( pTexture, &srvDesc, &pSRV );
    if( FAILED( Error ) )
        return 0;

    // Add reference to texture since we're storing it
    pTexture->AddRef();

    s32 Index = AddNode( pTexture, pSRV, D3D11_RESOURCE_DIMENSION_TEXTURE2D );
    return Index;
}

//=============================================================================

s32 vram_Register( ID3D11Texture3D* pTexture )
{
    ID3D11ShaderResourceView* pSRV = NULL;
    HRESULT                   Error;
    D3D11_TEXTURE3D_DESC      desc;

    if( !pTexture || !g_pd3dDevice )
        return 0;

    pTexture->GetDesc( &desc );

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    x_memset( &srvDesc, 0, sizeof(srvDesc) );
    u32 mipLevels                     = desc.MipLevels ? desc.MipLevels : (u32)-1;
    srvDesc.Format                    = desc.Format;
    srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE3D;
    srvDesc.Texture3D.MipLevels       = mipLevels;
    srvDesc.Texture3D.MostDetailedMip = 0;

    Error = g_pd3dDevice->CreateShaderResourceView( pTexture, &srvDesc, &pSRV );
    if( FAILED( Error ) )
        return 0;

    pTexture->AddRef();

    s32 Index = AddNode( pTexture, pSRV, D3D11_RESOURCE_DIMENSION_TEXTURE3D );
    return Index;
}

//=============================================================================

s32 vram_Register( const xbitmap& Bitmap )
{
    ID3D11Texture2D*          pTexture = NULL;
    ID3D11ShaderResourceView* pSRV = NULL;
    HRESULT                   Error;
    DXGI_FORMAT               Format;

    ASSERT( Bitmap.GetClutData() == NULL && "Not clut is suported" );

    // Check if device is available
    if( !g_pd3dDevice )
        return 0;

    //
    // Get the DX11 format
    //
    Format = GetDXGIFormat( Bitmap.GetFormat() );
    ASSERT( Format != DXGI_FORMAT_UNKNOWN );

    //
    // Create the texture
    //
    D3D11_TEXTURE2D_DESC desc;
    x_memset( &desc, 0, sizeof(desc) );
    desc.Width              = Bitmap.GetWidth();
    desc.Height             = Bitmap.GetHeight();
    desc.MipLevels          = 0;
    desc.ArraySize          = 1;
    desc.Format             = Format;
    desc.SampleDesc.Count   = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage              = D3D11_USAGE_DEFAULT;
    desc.BindFlags          = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.CPUAccessFlags     = 0;
    desc.MiscFlags          = D3D11_RESOURCE_MISC_GENERATE_MIPS;

    xbool bCompressed = IsCompressedFormat( Format );

    //
    // For compressed formats, don't use mipmaps
    //
    if( bCompressed )
    {
        desc.MipLevels = 1;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.MiscFlags = 0;
    }

    Error = g_pd3dDevice->CreateTexture2D( &desc, NULL, &pTexture );
    if( FAILED( Error ) )
    {
        return 0;
    }

    //
    // Create shader resource view
    //
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    x_memset( &srvDesc, 0, sizeof(srvDesc) );
    srvDesc.Format                    = Format;
    srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels       = bCompressed ? 1 : (u32)-1;
    srvDesc.Texture2D.MostDetailedMip = 0;

    Error = g_pd3dDevice->CreateShaderResourceView( pTexture, &srvDesc, &pSRV );
    if( FAILED( Error ) )
    {
        pTexture->Release();
        return 0;
    }

    //
    // Copy texture data safely
    //
    {
        char* pSrcData;   
        s32   BitmapPitch;

        pSrcData = (char*)Bitmap.GetPixelData();

        if( bCompressed )
        {
            s32 blockSize = GetBlockSize( Format );
            ASSERT( blockSize != 0 );

            s32 rowPitch = MAX( 1, (Bitmap.GetWidth() + 3) / 4 ) * blockSize;

            g_pd3dContext->UpdateSubresource( pTexture, 0, NULL,
                                             pSrcData,
                                             rowPitch,
                                             0 );
        }
        else
        {
            BitmapPitch = (Bitmap.GetPWidth() * Bitmap.GetFormatInfo().BPP)/8;

            g_pd3dContext->UpdateSubresource( pTexture, 0, NULL,
                                             pSrcData,
                                             BitmapPitch,
                                             BitmapPitch * Bitmap.GetHeight() );
        }
    }

    //
    // Create the mipmaps
    //
    if( desc.MiscFlags & D3D11_RESOURCE_MISC_GENERATE_MIPS )
    {
        g_pd3dContext->GenerateMips( pSRV );
    }

    //
    // Add texture in the list
    //
    s32 Index = AddNode( pTexture, pSRV, D3D11_RESOURCE_DIMENSION_TEXTURE2D );
    Bitmap.SetVRAMID( Index );
    return Index;
}

//=============================================================================

s32 vram_Register( const xbitmap* pBitmaps, s32 nBitmaps )
{
    if( !pBitmaps || (nBitmaps != 6) )
    {
        ASSERT( FALSE );
        return 0;
    }

    if( !g_pd3dDevice || !g_pd3dContext )
        return 0;

    const xbitmap& Bitmap0 = pBitmaps[0];

    ASSERT( Bitmap0.GetClutData() == NULL && "Not clut is suported" );

    DXGI_FORMAT Format = GetDXGIFormat( Bitmap0.GetFormat() );
    if( Format == DXGI_FORMAT_UNKNOWN )
    {
        ASSERT( FALSE );
        return 0;
    }

    for( s32 i = 1; i < nBitmaps; ++i )
    {
        const xbitmap& Face = pBitmaps[i];
        if( (Face.GetWidth()  != Bitmap0.GetWidth())  ||
            (Face.GetHeight() != Bitmap0.GetHeight()) ||
            (Face.GetFormat() != Bitmap0.GetFormat()) )
        {
            ASSERT( FALSE );
            return 0;
        }
    }

    xbool bCompressed = IsCompressedFormat( Format );

    D3D11_TEXTURE2D_DESC desc;
    x_memset( &desc, 0, sizeof(desc) );
    desc.Width              = Bitmap0.GetWidth();
    desc.Height             = Bitmap0.GetHeight();
    desc.MipLevels          = 0;
    desc.ArraySize          = 6;
    desc.Format             = Format;
    desc.SampleDesc.Count   = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage              = D3D11_USAGE_DEFAULT;
    desc.BindFlags          = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.CPUAccessFlags     = 0;
    desc.MiscFlags          = D3D11_RESOURCE_MISC_TEXTURECUBE | D3D11_RESOURCE_MISC_GENERATE_MIPS;

    if( bCompressed )
    {
        desc.MipLevels = 1;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
    }

    ID3D11Texture2D* pTexture = NULL;
    HRESULT Error = g_pd3dDevice->CreateTexture2D( &desc, NULL, &pTexture );
    if( FAILED( Error ) )
    {
        return 0;
    }

    D3D11_TEXTURE2D_DESC createdDesc;
    pTexture->GetDesc( &createdDesc );
    ASSERT( createdDesc.MipLevels > 0 );

    for( s32 face = 0; face < nBitmaps; ++face )
    {
        const xbitmap& FaceBitmap = pBitmaps[face];
        const byte*    pSrcData   = (const byte*)FaceBitmap.GetPixelData();

        if( !pSrcData )
        {
            pTexture->Release();
            ASSERT( FALSE );
            return 0;
        }

        u32 rowPitch;
        u32 slicePitch;

        if( bCompressed )
        {
            s32 blockSize = GetBlockSize( Format );
            ASSERT( blockSize != 0 );
            rowPitch  = MAX( 1, (FaceBitmap.GetWidth()  + 3) / 4 ) * blockSize;
            slicePitch = MAX( 1, (FaceBitmap.GetHeight() + 3) / 4 ) * blockSize;
        }
        else
        {
            rowPitch  = (FaceBitmap.GetPWidth() * FaceBitmap.GetFormatInfo().BPP) / 8;
            slicePitch = rowPitch * FaceBitmap.GetHeight();
        }

        UINT subresource = D3D11CalcSubresource( 0, face, createdDesc.MipLevels );
        g_pd3dContext->UpdateSubresource( pTexture,
                                          subresource,
                                          NULL,
                                          pSrcData,
                                          rowPitch,
                                          bCompressed ? 0 : slicePitch );
    }

    ID3D11ShaderResourceView* pSRV = NULL;
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    x_memset( &srvDesc, 0, sizeof(srvDesc) );
    srvDesc.Format                        = Format;
    srvDesc.ViewDimension                 = D3D11_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.TextureCube.MipLevels         = bCompressed ? 1 : (u32)-1;
    srvDesc.TextureCube.MostDetailedMip   = 0;

    Error = g_pd3dDevice->CreateShaderResourceView( pTexture, &srvDesc, &pSRV );
    if( FAILED( Error ) )
    {
        pTexture->Release();
        return 0;
    }

    if( (createdDesc.MiscFlags & D3D11_RESOURCE_MISC_GENERATE_MIPS) && !bCompressed )
    {
        g_pd3dContext->GenerateMips( pSRV );
    }

    s32 Index = AddNode( pTexture, pSRV, D3D11_RESOURCE_DIMENSION_TEXTURE2D );
    return Index;
}

//=============================================================================

s32 GetHeight(const xbitmap& BMP, s32 x, s32 y)
{
    xcolor Col = BMP.GetPixelColor(x,y);
    s32    H   = (Col.R + Col.G + Col.B) / 3;
    return H;
}

//=============================================================================

s32 vram_RegisterDuDv( const xbitmap& Bitmap )
{ 
    ASSERT( FALSE && "vram_RegisterDuDv is deprecated." );
    return 0;
}

//=============================================================================

void vram_Unregister( s32 VRAM_ID )
{
    ASSERT( VRAM_ID >= 0 );
    ASSERT( VRAM_ID < MAX_TEXTURES );

    if( VRAM_ID == s_LastActiveID ) s_LastActiveID = 0;

    if( s_List[ VRAM_ID ].pSRV )      s_List[ VRAM_ID ].pSRV->Release();
    if( s_List[ VRAM_ID ].pResource ) s_List[ VRAM_ID ].pResource->Release();

    s_List[ VRAM_ID ].pResource = NULL;
    s_List[ VRAM_ID ].pSRV      = NULL;
    s_List[ VRAM_ID ].Dimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    s_List[ VRAM_ID ].iNext     = s_iEmpty;
    s_iEmpty                    = VRAM_ID;
}

//=============================================================================

void vram_Unregister( const xbitmap& Bitmap  )
{
    vram_Unregister( Bitmap.GetVRAMID() );

    // Signal bitmap is no longer in VRAM.
    Bitmap.SetVRAMID( 0 );
}

//=============================================================================

void vram_Activate( const xbitmap& Bitmap  )
{
    vram_Activate( Bitmap.GetVRAMID() );
}

//=============================================================================

xbool vram_IsActive( const xbitmap& Bitmap )
{
    return( (s_LastActiveID != 0) && (s_LastActiveID == Bitmap.GetVRAMID()) );
}

//=============================================================================

void vram_Flush( void )
{
}

//=============================================================================

s32 vram_GetNRegistered( void )
{
    s32 nRegister = 0;

    for( s32 i=0; i<MAX_TEXTURES; i++ )
    {
        if( s_List[i].pResource != NULL )
        {
            nRegister++;
        }
    }

    return nRegister;
}

//=============================================================================

s32 vram_GetRegistered( s32 ID )
{
    s32 nRegister = 0;

    for( s32 i=0; i<MAX_TEXTURES; i++ )
    {
        if( s_List[i].pResource != NULL )
        {
            nRegister++;
            if( nRegister == ID ) return i;
        }
    }

    ASSERT( 0 );
    return -1;
}

//=============================================================================

void vram_PrintStats( void )
{
    //TODO: GS: To make this work, need to implement new functionality, I'm too lazy.
    x_printfxy( 0,  6, "Free Vmem:   N/A (DX11)" );
    x_printfxy( 0,  7, "NTextures:   %d", vram_GetNRegistered() );
}

//=============================================================================

void vram_SanityCheck( void )
{
}

//=============================================================================

ID3D11ShaderResourceView* vram_GetSRV( s32 VRAM_ID )
{
    // Note: VRAM_ID == 0 means bitmap not registered!
    ASSERT( VRAM_ID > 0 );
    ASSERT( VRAM_ID < MAX_TEXTURES );
    ASSERT( s_List[ VRAM_ID ].pResource != NULL );

    return s_List[ VRAM_ID ].pSRV;
}

//=============================================================================

ID3D11ShaderResourceView* vram_GetSRV( const xbitmap& Bitmap )
{
    return vram_GetSRV( Bitmap.GetVRAMID() );
}

//=============================================================================

ID3D11Texture1D* vram_GetTexture1D( s32 VRAM_ID )
{
    ASSERT( VRAM_ID > 0 );
    ASSERT( VRAM_ID < MAX_TEXTURES );
    ASSERT( s_List[ VRAM_ID ].pResource != NULL );
    ASSERT( s_List[ VRAM_ID ].Dimension == D3D11_RESOURCE_DIMENSION_TEXTURE1D );

    return static_cast<ID3D11Texture1D*>( s_List[ VRAM_ID ].pResource );
}

//=============================================================================

ID3D11Texture2D* vram_GetTexture2D( s32 VRAM_ID )
{
    // Note: VRAM_ID == 0 means bitmap not registered!
    ASSERT( VRAM_ID > 0 );
    ASSERT( VRAM_ID < MAX_TEXTURES );
    ASSERT( s_List[ VRAM_ID ].pResource != NULL );
    ASSERT( s_List[ VRAM_ID ].Dimension == D3D11_RESOURCE_DIMENSION_TEXTURE2D );

    return static_cast<ID3D11Texture2D*>( s_List[ VRAM_ID ].pResource );
}

//=============================================================================

ID3D11Texture2D* vram_GetTexture2D( const xbitmap& Bitmap )
{
    return vram_GetTexture2D( Bitmap.GetVRAMID() );
}

//=============================================================================

ID3D11Texture3D* vram_GetTexture3D( s32 VRAM_ID )
{
    ASSERT( VRAM_ID > 0 );
    ASSERT( VRAM_ID < MAX_TEXTURES );
    ASSERT( s_List[ VRAM_ID ].pResource != NULL );
    ASSERT( s_List[ VRAM_ID ].Dimension == D3D11_RESOURCE_DIMENSION_TEXTURE3D );

    return static_cast<ID3D11Texture3D*>( s_List[ VRAM_ID ].pResource );
}

//=============================================================================

ID3D11Texture2D* vram_GetTextureCube( s32 VRAM_ID )
{
    ASSERT( VRAM_ID > 0 );
    ASSERT( VRAM_ID < MAX_TEXTURES );
    ASSERT( s_List[ VRAM_ID ].pResource != NULL );
    ASSERT( s_List[ VRAM_ID ].Dimension == D3D11_RESOURCE_DIMENSION_TEXTURE2D );

    ID3D11Texture2D* pTexture = static_cast<ID3D11Texture2D*>( s_List[ VRAM_ID ].pResource );

#if defined( X_DEBUG )
    D3D11_TEXTURE2D_DESC desc;
    pTexture->GetDesc( &desc );
    ASSERT( desc.MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE );
#endif

    return pTexture;
}

//=============================================================================