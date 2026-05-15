//==============================================================================
//
//  PostMgr_Fog.cpp
//
//  Fog post-processing module for the PC platform.
//
//==============================================================================

//==============================================================================
//  PLATFORM CHECK
//==============================================================================

#include "x_types.hpp"

#if !defined(TARGET_PC)
#error "This is only for the PC target platform. Please check build exclusion rules"
#endif

//==============================================================================
//  INCLUDES
//==============================================================================

#include "PostMgr.hpp"
#include "../../LeastSquares/LeastSquares.hpp"

//==============================================================================
//  EXTERNAL VARIABLES
//==============================================================================

extern ID3D11Device*           g_pd3dDevice;
extern ID3D11DeviceContext*    g_pd3dContext;

//==============================================================================
//  FILE-LOCAL TYPES AND HELPERS
//==============================================================================

namespace
{
    // Constants
    static const s32 kFogSourcePaletteCount = 64;
    static const s32 kFogPaletteCount = 256;
    static const s32 kFogGeneratedPaletteIndex = 2;
    static const f32 kPS2ZScale = (f32)(1 << ZBUFFER_BITS);
    static const f32 kPS2ZConst = 0.5f * kPS2ZScale;

    // Constant buffer layout
    struct cb_post_fog
    {
        vector4 FogColor;
        vector4 FogCoeff;
        vector4 FogParams;
    };

    // Helper functions
    static
    void ReleaseFogTexture( ID3D11Texture2D*& pTexture, ID3D11ShaderResourceView*& pSRV )
    {
        if( pSRV )
        {
            pSRV->Release();
            pSRV = NULL;
        }

        if( pTexture )
        {
            pTexture->Release();
            pTexture = NULL;
        }
    }

    static
    void WritePaletteColor( u8* pPalette, s32 Index, xcolor Color )
    {
        ASSERT( Index >= 0 );
        ASSERT( Index < kFogPaletteCount );

        pPalette[Index * 4 + 0] = Color.R;
        pPalette[Index * 4 + 1] = Color.G;
        pPalette[Index * 4 + 2] = Color.B;
        pPalette[Index * 4 + 3] = Color.A;
    }

    static
    xcolor ReadPaletteColor( const u8* pPalette, s32 Index )
    {
        ASSERT( pPalette );
        ASSERT( Index >= 0 );

        return xcolor( pPalette[Index * 4 + 0],
                       pPalette[Index * 4 + 1],
                       pPalette[Index * 4 + 2],
                       pPalette[Index * 4 + 3] );
    }

    static
    f32 ComputeFogFalloff( render::post_falloff_fn Fn, f32 ViewZ, f32 NearZ, f32 FarZ, f32 Param1, f32 Param2 )
    {
        const f32 FarMinusNear = MAX( FarZ - NearZ, 0.001f );

        switch( Fn )
        {
            case render::FALLOFF_LINEAR:
            {
                ASSERT( Param2 > Param1 );
                const f32 ClampedZ = MINMAX( Param1, ViewZ, Param2 );
                return (Param2 - ClampedZ) / MAX( Param2 - Param1, 0.001f );
            }

            case render::FALLOFF_EXP:
            {
                const f32 D = (ViewZ - NearZ) / FarMinusNear;
                return 1.0f / x_exp( D * Param1 );
            }

            case render::FALLOFF_EXP2:
            {
                f32 D = (ViewZ - NearZ) / FarMinusNear;
                D *= Param1;
                return 1.0f / x_exp( D * D );
            }

            default:
                break;
        }

        return 1.0f;
    }

    static
    s32 GetPaletteIndexFromDepth( f32 LinearDepth )
    {
        const f32 ClampedDepth = MINMAX( 0.0f, LinearDepth, 1.0f );
        return (s32)(ClampedDepth * (f32)(kFogPaletteCount - 1) + 0.5f);
    }

    static
    void ExpandCustomFogPalette( const u8* pSourcePalette, u8* pExpandedPalette, f32 NearZ, f32 FarZ )
    {
        ASSERT( pSourcePalette );
        ASSERT( pExpandedPalette );

        const f32 Range = MAX( FarZ - NearZ, 0.001f );
        for( s32 i = 0; i < kFogPaletteCount; ++i )
        {
            const f32 LinearDepth = (f32)i / (f32)(kFogPaletteCount - 1);
            const f32 ViewZ       = NearZ + LinearDepth * Range;

            f32 PS2ScreenZ = (ViewZ * (FarZ + NearZ) / Range);
            PS2ScreenZ    -= ((2.0f * FarZ * NearZ) / Range);
            PS2ScreenZ    /= ViewZ;
            PS2ScreenZ    *= -kPS2ZConst;
            PS2ScreenZ    +=  kPS2ZConst;

            u32 UPS2SZ = (u32)(PS2ScreenZ * 16.0f + 0.5f);
            s32 ClutIX = 0;
            if( UPS2SZ <= 0xFFFF )
                ClutIX = ((UPS2SZ >> 8) & 0xFF) / 4;

            ClutIX = MINMAX( 0, ClutIX, kFogSourcePaletteCount - 1 );
            WritePaletteColor( pExpandedPalette, i, ReadPaletteColor( pSourcePalette, ClutIX ) );
        }

        pExpandedPalette[3] = 0;
    }
}

//==============================================================================
//  FOG RESOURCE MANAGEMENT
//==============================================================================

post_mgr::fog_resources::fog_resources()
{
    pPaletteTexture = NULL;
    pPaletteSRV     = NULL;
    pCompositePS    = NULL;
    pConstantBuffer = NULL;
}

//==============================================================================

void post_mgr::fog_resources::Initialize( void )
{
    Shutdown();

    if( !g_pd3dDevice )
        return;

    char shaderPath[256];
    x_sprintf( shaderPath, "a51_post_fog.hlsl" );

    char* pSource = shader_LoadSourceFromFile( shaderPath );
    if( !pSource )
        return;

    pCompositePS = shader_CompilePixel( pSource, "PSMain", "ps_5_0", shaderPath );
    pConstantBuffer = shader_CreateConstantBuffer( sizeof(cb_post_fog), CB_TYPE_DYNAMIC );
    x_free( pSource );
}

//==============================================================================

void post_mgr::fog_resources::Shutdown( void )
{
    ReleaseFogTexture( pPaletteTexture, pPaletteSRV );

    if( pCompositePS )
    {
        pCompositePS->Release();
        pCompositePS = NULL;
    }

    if( pConstantBuffer )
    {
        pConstantBuffer->Release();
        pConstantBuffer = NULL;
    }
}

//==============================================================================

void post_mgr::fog_resources::UpdateConstants( const vector4& FogColor, const vector4& FogCoeff, f32 FogStart, f32 NearZ, f32 FarZ, xbool bUsePolynomial )
{
    if( !pConstantBuffer || !g_pd3dContext )
        return;

    cb_post_fog cbData;
    cbData.FogColor  = FogColor;
    cbData.FogCoeff  = FogCoeff;
    cbData.FogParams.Set( NearZ, FarZ, bUsePolynomial ? 1.0f : 0.0f, FogStart );

    shader_UpdateConstantBuffer( pConstantBuffer, &cbData, sizeof(cb_post_fog) );
    g_pd3dContext->PSSetConstantBuffers( 4, 1, &pConstantBuffer );
}

//==============================================================================

xbool post_mgr::fog_resources::UpdatePaletteTexture( const u8* pPalette )
{
    if( !g_pd3dDevice || !g_pd3dContext || !pPalette )
        return FALSE;

    if( !pPaletteTexture || !pPaletteSRV )
    {
        ReleaseFogTexture( pPaletteTexture, pPaletteSRV );

        D3D11_TEXTURE2D_DESC desc;
        x_memset( &desc, 0, sizeof(desc) );
        desc.Width              = kFogPaletteCount;
        desc.Height             = 1;
        desc.MipLevels          = 1;
        desc.ArraySize          = 1;
        desc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count   = 1;
        desc.Usage              = D3D11_USAGE_DEFAULT;
        desc.BindFlags          = D3D11_BIND_SHADER_RESOURCE;

        HRESULT hr = g_pd3dDevice->CreateTexture2D( &desc, NULL, &pPaletteTexture );
        if( FAILED(hr) || !pPaletteTexture )
        {
            ReleaseFogTexture( pPaletteTexture, pPaletteSRV );
            return FALSE;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
        x_memset( &srvDesc, 0, sizeof(srvDesc) );
        srvDesc.Format                    = desc.Format;
        srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels       = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;

        hr = g_pd3dDevice->CreateShaderResourceView( pPaletteTexture, &srvDesc, &pPaletteSRV );
        if( FAILED(hr) || !pPaletteSRV )
        {
            ReleaseFogTexture( pPaletteTexture, pPaletteSRV );
            return FALSE;
        }
    }

    g_pd3dContext->UpdateSubresource( pPaletteTexture, 0, NULL, pPalette, kFogPaletteCount * 4, 0 );
    return TRUE;
}

//==============================================================================
//  FOG PROCESSING
//==============================================================================

void post_mgr::SetCustomFogPalette( const texture::handle& Texture, xbool ImmediateSwitch, s32 PaletteIndex )
{
    ASSERT( PaletteIndex >= 0 );
    ASSERT( PaletteIndex < 5 );

    u8 TargetPalette[kFogSourcePaletteCount * 4];
    x_memset( TargetPalette, 0, sizeof(TargetPalette) );

    texture* pTexture = Texture.GetPointer();
    if( pTexture )
    {
        const xbitmap& Bitmap = pTexture->m_Bitmap;
        const s32 Width  = Bitmap.GetWidth();
        const s32 Height = Bitmap.GetHeight();
        const s32 Count  = Width * Height;

        if( Count > 0 )
        {
            for( s32 i = 0; i < kFogSourcePaletteCount; ++i )
            {
                const f32 SampleT    = (kFogSourcePaletteCount > 1) ? ((f32)i / (f32)(kFogSourcePaletteCount - 1)) : 0.0f;
                const s32 SourceIX   = MIN( (s32)(SampleT * (f32)(Count - 1) + 0.5f), Count - 1 );
                const s32 SourceX    = SourceIX % Width;
                const s32 SourceY    = SourceIX / Width;
                const xcolor Color   = Bitmap.GetPixelColor( SourceX, SourceY );

                WritePaletteColor( TargetPalette, i, Color );
            }
        }
    }

    u8* pCurrentPalette = m_FogSourcePalette[PaletteIndex];
    for( s32 i = 0; i < (kFogSourcePaletteCount * 4); ++i )
    {
        if( ImmediateSwitch )
        {
            pCurrentPalette[i] = TargetPalette[i];
        }
        else if( pCurrentPalette[i] < TargetPalette[i] )
        {
            pCurrentPalette[i]++;
        }
        else if( pCurrentPalette[i] > TargetPalette[i] )
        {
            pCurrentPalette[i]--;
        }
    }

    const view* pView = eng_GetView();
    if( pView )
    {
        f32 NearZ, FarZ;
        pView->GetZLimits( NearZ, FarZ );
        ExpandCustomFogPalette( m_FogSourcePalette[PaletteIndex], m_FogPalette[PaletteIndex], NearZ, FarZ );

        u32 ColorTotal[4] = { 0, 0, 0, 0 };
        for( s32 i = 0; i < kFogSourcePaletteCount * 4; i += 4 )
        {
            ColorTotal[0] += m_FogSourcePalette[PaletteIndex][i + 0];
            ColorTotal[1] += m_FogSourcePalette[PaletteIndex][i + 1];
            ColorTotal[2] += m_FogSourcePalette[PaletteIndex][i + 2];
            ColorTotal[3] += m_FogSourcePalette[PaletteIndex][i + 3];
        }

        ColorTotal[0] /= kFogSourcePaletteCount;
        ColorTotal[1] /= kFogSourcePaletteCount;
        ColorTotal[2] /= kFogSourcePaletteCount;
        ColorTotal[3] /= kFogSourcePaletteCount;

        m_FogColor[PaletteIndex].Set(
            (f32)ColorTotal[0] / 255.0f,
            (f32)ColorTotal[1] / 255.0f,
            (f32)ColorTotal[2] / 255.0f,
            (f32)ColorTotal[3] / 255.0f );

        const f32 Numer        = (2.0f * kPS2ZConst * NearZ * FarZ) / (FarZ - NearZ);
        const f32 DenomOffset  = -kPS2ZConst + (kPS2ZConst * (FarZ + NearZ)) / (FarZ - NearZ);
        const f32 DepthScaleQ  = FarZ / (FarZ - NearZ);
        static const f32 Scale = 1 / 16.0f;

        const f32 PS2ZValue      = (f32)(0xffff) * Scale;
        const f32 Denom          = PS2ZValue + DenomOffset;
        ASSERT( x_abs( Denom ) > 0.001f );
        const f32 ViewZValue     = Numer / Denom;
        const f32 ProjectedZValue = ViewZValue * DepthScaleQ - NearZ * DepthScaleQ;
        ASSERT( ViewZValue > 0.001f );
        m_FogStart[PaletteIndex] = ProjectedZValue;

        least_squares AlphaApprox;
        AlphaApprox.Setup(3);

        static const f32 NSamples = 512.0f;
        const f32 StepSize = (FarZ - NearZ) / NSamples;
        for( f32 ViewZ = NearZ; ViewZ <= FarZ; ViewZ += StepSize )
        {
            f32 PS2ScreenZ = (ViewZ * (FarZ + NearZ) / (FarZ - NearZ));
            PS2ScreenZ    -= ((2.0f * FarZ * NearZ) / (FarZ - NearZ));
            PS2ScreenZ    /= ViewZ;
            PS2ScreenZ    *= -kPS2ZConst;
            PS2ScreenZ    +=  kPS2ZConst;

            const f32 ProjectedScreenZ = (ViewZ * DepthScaleQ) - (NearZ * DepthScaleQ);

            f32 A = 0.0f;
            const u32 UPS2SZ = (u32)(PS2ScreenZ * 16.0f + 0.5f);
            if( UPS2SZ <= 0xFFFF )
            {
                const s32 ClutIX = MINMAX( 0, (s32)(((UPS2SZ >> 8) & 0xFF) / 4), kFogSourcePaletteCount - 1 );
                A = (f32)m_FogSourcePalette[PaletteIndex][ClutIX * 4 + 3] / 255.0f;
            }

            AlphaApprox.AddSample( ProjectedScreenZ, A );
        }

        if( !AlphaApprox.Solve() )
        {
            AlphaApprox.SetCoeff( 0, (f32)m_FogSourcePalette[PaletteIndex][3] / 255.0f );
            AlphaApprox.SetCoeff( 1, 0.0f );
            AlphaApprox.SetCoeff( 2, 0.0f );
            AlphaApprox.SetCoeff( 3, 0.0f );
        }

        m_FogConst[PaletteIndex].Set( AlphaApprox.GetCoeff(0),
                                      AlphaApprox.GetCoeff(1),
                                      AlphaApprox.GetCoeff(2),
                                      AlphaApprox.GetCoeff(3) );
    }

    m_bFogValid[PaletteIndex] = TRUE;
}

//==============================================================================

void post_mgr::BuildFogPalette( render::post_falloff_fn Fn, xcolor Color, f32 Param1, f32 Param2 )
{
    m_FogFilter.PaletteIndex = kFogGeneratedPaletteIndex;

    m_FogFilter.Fn[m_FogFilter.PaletteIndex] = Fn;

    const f32 NearZ = m_PostNearZ;
    const f32 FarZ  = m_PostFarZ;
    const f32 Range = MAX( FarZ - NearZ, 0.001f );

    for( s32 i = 0; i < kFogPaletteCount; ++i )
    {
        xcolor EntryColor( Color.R, Color.G, Color.B, Color.A );

        if( Fn == render::FALLOFF_CONSTANT )
        {
            EntryColor.A = Color.A;
        }
        else
        {
            const f32 LinearDepth = (f32)i / (f32)(kFogPaletteCount - 1);
            const f32 ViewZ       = NearZ + LinearDepth * Range;
            const f32 F           = MINMAX( 0.0f, ComputeFogFalloff( Fn, ViewZ, NearZ, FarZ, Param1, Param2 ), 1.0f );
            EntryColor.A          = (u8)MINMAX( 0.0f, 128.0f - (F * 128.0f), 255.0f );

            if( i == 0 )
                EntryColor.A = 0;
        }

        WritePaletteColor( m_FogPalette[m_FogFilter.PaletteIndex], i, EntryColor );
    }

    m_bFogValid[m_FogFilter.PaletteIndex] = TRUE;
}

//==============================================================================

void post_mgr::ExecuteZFogFilter( void )
{
    if( !g_pd3dContext || !m_FogResources.pCompositePS )
        return;

    const s32 PaletteIndex = m_FogFilter.PaletteIndex;
    if( (PaletteIndex < 0) || (PaletteIndex >= 5) || !m_bFogValid[PaletteIndex] )
        return;

    const rtarget* pLinearDepthTarget = g_GBufferMgr.GetGBufferTarget( GBUFFER_LINEAR_DEPTH );
    if( !pLinearDepthTarget || !pLinearDepthTarget->pShaderResourceView )
        return;

    const xbool bUsePolynomial = (m_FogFilter.Fn[PaletteIndex] == render::FALLOFF_CUSTOM);
    if( !bUsePolynomial )
    {
        if( !m_FogResources.UpdatePaletteTexture( m_FogPalette[PaletteIndex] ) )
            return;
    }

    PrepareFullscreenQuad();
    m_FogResources.UpdateConstants( m_FogColor[PaletteIndex], m_FogConst[PaletteIndex], m_FogStart[PaletteIndex], m_PostNearZ, m_PostFarZ, bUsePolynomial );

    ID3D11ShaderResourceView* pResources[2] = { NULL, NULL };
    pResources[0] = pLinearDepthTarget->pShaderResourceView;
    if( !bUsePolynomial )
        pResources[1] = m_FogResources.pPaletteSRV;
    g_pd3dContext->PSSetShaderResources( 1, 2, pResources );

    composite_Blit( *pLinearDepthTarget, COMPOSITE_BLEND_ALPHA, 1.0f, m_FogResources.pCompositePS, STATE_SAMPLER_LINEAR_CLAMP );

    ID3D11ShaderResourceView* pNullResources[2] = { NULL, NULL };
    g_pd3dContext->PSSetShaderResources( 1, 2, pNullResources );
}

//==============================================================================

xcolor post_mgr::GetFogValue( const vector3& WorldPos, s32 PaletteIndex )
{
    if( (PaletteIndex < 0) || (PaletteIndex >= 5) || !m_bFogValid[PaletteIndex] )
        return xcolor(255, 255, 255, 0);

    const view* pView = eng_GetView();
    if( !pView )
        return xcolor(255, 255, 255, 0);

    if( m_FogFilter.Fn[PaletteIndex] == render::FALLOFF_CUSTOM )
    {
        vector4 ScreenPos( WorldPos );
        ScreenPos.GetW() = 1.0f;
        ScreenPos = pView->GetW2C() * ScreenPos;
        if( x_abs( ScreenPos.GetW() ) < 0.001f )
            return xcolor(255,255,255,0);

        const f32 Z  = ScreenPos.GetZ();
        const f32 Z2 = Z * Z;
        const f32 Z3 = Z2 * Z;
        f32 FogIntensity = m_FogConst[PaletteIndex].GetX() +
                           m_FogConst[PaletteIndex].GetY() * Z +
                           m_FogConst[PaletteIndex].GetZ() * Z2 +
                           m_FogConst[PaletteIndex].GetW() * Z3;

        FogIntensity = MINMAX( 0.0f, FogIntensity, 1.0f );
        return xcolor( 255, 255, 255, (u8)(FogIntensity * 255.0f) );
    }

    f32 NearZ, FarZ;
    pView->GetZLimits( NearZ, FarZ );

    vector4 ViewPos( WorldPos );
    ViewPos.GetW() = 1.0f;
    ViewPos = pView->GetW2V() * ViewPos;

    if( ViewPos.GetZ() <= NearZ )
        return xcolor(255, 255, 255, m_FogPalette[PaletteIndex][3] );

    const f32 LinearDepth = (ViewPos.GetZ() - NearZ) / MAX( FarZ - NearZ, 0.001f );
    const s32 PaletteIX   = GetPaletteIndexFromDepth( LinearDepth );
    const u8  Alpha       = m_FogPalette[PaletteIndex][PaletteIX * 4 + 3];

    return xcolor( 255, 255, 255, Alpha );
}
