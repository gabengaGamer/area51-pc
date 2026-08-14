//==============================================================================
//
//  PostMgr_Fog.cpp
//
//  Fog post-processing module for the PC platform.
//
//==============================================================================

//==============================================================================
//  BASE INCLUDES
//==============================================================================

#include "x_types.hpp"

//==============================================================================
//  INCLUDES
//==============================================================================

#include "PostMgr.hpp"
#include "../../LeastSquares/LeastSquares.hpp"

//==============================================================================
//  FILE-LOCAL TYPES AND HELPERS
//==============================================================================

namespace
{
// Constants
static s32 const kFogSourcePaletteCount = 64;
static s32 const kFogPaletteCount = 256;
static s32 const kFogGeneratedPaletteIndex = 2;
static f32 const kPS2ZScale = static_cast<f32>( 1 << ZBUFFER_BITS );
static f32 const kPS2ZConst = 0.5f * kPS2ZScale;

// Constant buffer layout
struct PostFogConstants
{
    vector4 FogColor;
    vector4 FogCoeff;
    vector4 FogParams;
};

void WritePaletteColor( u8* pPalette, s32 index, xcolor color )
{
    ASSERT( index >= 0 );
    ASSERT( index < kFogPaletteCount );

    pPalette[index * 4 + 0] = color.R;
    pPalette[index * 4 + 1] = color.G;
    pPalette[index * 4 + 2] = color.B;
    pPalette[index * 4 + 3] = color.A;
}

static xcolor ReadPaletteColor( u8 const* pPalette, s32 index )
{
    ASSERT( pPalette );
    ASSERT( index >= 0 );

    return xcolor( pPalette[index * 4 + 0], pPalette[index * 4 + 1], pPalette[index * 4 + 2], pPalette[index * 4 + 3] );
}

static f32 ComputeFogFalloff( render::post_falloff_fn fn, f32 viewZ, f32 nearZ, f32 farZ, f32 param1, f32 param2 )
{
    f32 const farMinusNear = MAX( farZ - nearZ, 0.001f );

    switch ( fn )
    {
        case render::FALLOFF_LINEAR:
            {
                ASSERT( param2 > param1 );
                f32 const clampedZ = MINMAX( param1, viewZ, param2 );
                return ( param2 - clampedZ ) / MAX( param2 - param1, 0.001f );
            }

        case render::FALLOFF_EXP:
            {
                f32 const d = ( viewZ - nearZ ) / farMinusNear;
                return 1.0f / x_exp( d * param1 );
            }

        case render::FALLOFF_EXP2:
            {
                f32 d = ( viewZ - nearZ ) / farMinusNear;
                d *= param1;
                return 1.0f / x_exp( d * d );
            }

        default:
            {
                break;
            }
    }

    return 1.0f;
}

static s32 GetPaletteIndexFromDepth( f32 linearDepth )
{
    f32 const clampedDepth = MINMAX( 0.0f, linearDepth, 1.0f );
    return static_cast<s32>( clampedDepth * static_cast<f32>( kFogPaletteCount - 1 ) + 0.5f );
}

static void ExpandCustomFogPalette( u8 const* pSourcePalette, u8* pExpandedPalette, f32 nearZ, f32 farZ )
{
    ASSERT( pSourcePalette );
    ASSERT( pExpandedPalette );

    f32 const range = MAX( farZ - nearZ, 0.001f );
    for ( s32 i = 0; i < kFogPaletteCount; ++i )
    {
        f32 const linearDepth = static_cast<f32>( i ) / static_cast<f32>( kFogPaletteCount - 1 );
        f32 const viewZ = nearZ + linearDepth * range;

        f32 pS2ScreenZ = ( viewZ * ( farZ + nearZ ) / range );
        pS2ScreenZ -= ( ( 2.0f * farZ * nearZ ) / range );
        pS2ScreenZ /= viewZ;
        pS2ScreenZ *= -kPS2ZConst;
        pS2ScreenZ += kPS2ZConst;

        u32 upS2Sz = static_cast<u32>( pS2ScreenZ * 16.0f + 0.5f );
        s32 clutIx = 0;
        if ( upS2Sz <= 0xFFFF )
        {
            clutIx = ( ( upS2Sz >> 8 ) & 0xFF ) / 4;
        }

        clutIx = MINMAX( 0, clutIx, kFogSourcePaletteCount - 1 );
        WritePaletteColor( pExpandedPalette, i, ReadPaletteColor( pSourcePalette, clutIx ) );
    }

    pExpandedPalette[3] = 0;
}
} // namespace

//==============================================================================
//  FOG RESOURCE MANAGEMENT
//==============================================================================

PostMgr::FogResources::FogResources()
{
    PaletteTexture = vram_texture();
    CompositePS = shader();
    PolynomialPS = shader();
    PaletteSampler = rstate_sampler();
    ConstantFogColor.Set( 0.0f, 0.0f, 0.0f, 0.0f );
    ConstantFogCoeff.Set( 0.0f, 0.0f, 0.0f, 0.0f );
    ConstantFogParams.Set( 0.0f, 0.0f, 0.0f, 0.0f );
}

//==============================================================================

void PostMgr::FogResources::Initialize( void )
{
    Shutdown();

    shader_LoadFromEcs( CompositePS, "post_fog_ps.ps.ecs" );
    shader_LoadFromEcs( PolynomialPS, "post_fog_polynomial_ps.ps.ecs" );
    rstate_CreateSampler( PaletteSampler, RSTATE_SAMPLER_PRESET_LINEAR_CLAMP, "PostFogPalette" );

    if ( !CompositePS || !PolynomialPS || !PaletteSampler )
    {
        x_DebugMsg( "PostMgr: WARNING - Failed to initialize fog resources\n" );
    }
}

//==============================================================================

void PostMgr::FogResources::Shutdown( void )
{
    vram_DestroyTexture( PaletteTexture );
    rstate_DestroySampler( PaletteSampler );
    shader_Destroy( PolynomialPS );
    shader_Destroy( CompositePS );
}

//==============================================================================

void PostMgr::FogResources::UpdateConstants( vector4 const& fogColor, vector4 const& fogCoeff, f32 fogStart, f32 nearZ,
                                             f32 farZ, xbool bUsePolynomial )
{
    ConstantFogColor = fogColor;
    ConstantFogCoeff = fogCoeff;
    ConstantFogParams.Set( nearZ, farZ, bUsePolynomial ? 1.0f : 0.0f, fogStart );
}

//==============================================================================

xbool PostMgr::FogResources::UpdatePaletteTexture( u8 const* pPalette )
{
    if ( !pPalette )
    {
        return FALSE;
    }

    if ( !vram_IsValid( PaletteTexture ) )
    {
        vram_texture_desc desc;
        desc.Width = kFogPaletteCount;
        desc.Height = 1;
        desc.Format = VRAM_TEXTURE_FORMAT_RGBA8;
        desc.UsageFlags = VRAM_TEXTURE_USAGE_SAMPLED;
        desc.pDebugName = "PostFogPalette";

        if ( !vram_CreateTexture( PaletteTexture, desc ) )
        {
            return FALSE;
        }
    }

    vram_texture_upload_desc upload;
    upload.Region.Width = kFogPaletteCount;
    upload.Region.Height = 1;
    upload.Region.Depth = 1;
    upload.pData = pPalette;
    upload.Size = kFogPaletteCount * 4;
    upload.RowPitch = kFogPaletteCount * 4;
    upload.SlicePitch = kFogPaletteCount * 4;
    upload.bCycle = TRUE;

    return vram_UploadTexture( PaletteTexture, upload );
}

//==============================================================================

xbool PostMgr::FogResources::BindForComposite( shader const& pixelShader, xbool bBindPalette ) const
{
    PostFogConstants constants;
    constants.FogColor = ConstantFogColor;
    constants.FogCoeff = ConstantFogCoeff;
    constants.FogParams = ConstantFogParams;

    u32 fogParamsSlot = 0;
    if ( shader_FindUniformSlot( pixelShader, "FogParams", fogParamsSlot ) )
    {
        if ( !shader_PushUniformData( SHADER_STAGE_PIXEL, fogParamsSlot, &constants, sizeof( constants ) ) )
        {
            return FALSE;
        }
    }
    else if ( !bBindPalette )
    {
        return FALSE;
    }

    if ( bBindPalette )
    {
        if ( !vram_IsValid( PaletteTexture ) || !PaletteSampler )
        {
            return FALSE;
        }

        if ( !shader_BindSampler( pixelShader, SHADER_STAGE_PIXEL, "FogPalette",
                                  vram_GetShaderResource( PaletteTexture ), &PaletteSampler ) )
        {
            return FALSE;
        }
    }

    return TRUE;
}

//==============================================================================
//  FOG PROCESSING
//==============================================================================

void PostMgr::SetCustomFogPalette( texture::handle const& textureHandle, xbool immediateSwitch, s32 paletteIndex )
{
    ASSERT( paletteIndex >= 0 );
    ASSERT( paletteIndex < 5 );

    u8 targetPalette[kFogSourcePaletteCount * 4];
    x_memset( targetPalette, 0, sizeof( targetPalette ) );

    texture* pTexture = textureHandle.GetPointer();
    if ( pTexture )
    {
        xbitmap const& bitmap = pTexture->m_bitmap;
        s32 const      width = bitmap.GetWidth();
        s32 const      height = bitmap.GetHeight();
        s32 const      count = width * height;

        if ( count > 0 )
        {
            for ( s32 i = 0; i < kFogSourcePaletteCount; ++i )
            {
                f32 const sampleT = ( kFogSourcePaletteCount > 1 )
                                        ? ( static_cast<f32>( i ) / static_cast<f32>( kFogSourcePaletteCount - 1 ) )
                                        : 0.0f;
                s32 const sourceIx =
                    MIN( static_cast<s32>( sampleT * static_cast<f32>( count - 1 ) + 0.5f ), count - 1 );
                s32 const    sourceX = sourceIx % width;
                s32 const    sourceY = sourceIx / width;
                xcolor const color = bitmap.GetPixelColor( sourceX, sourceY );

                WritePaletteColor( targetPalette, i, color );
            }
        }
    }

    u8* pCurrentPalette = m_fogSourcePalette[paletteIndex];
    for ( s32 i = 0; i < ( kFogSourcePaletteCount * 4 ); ++i )
    {
        if ( immediateSwitch )
        {
            pCurrentPalette[i] = targetPalette[i];
        }
        else if ( pCurrentPalette[i] < targetPalette[i] )
        {
            pCurrentPalette[i]++;
        }
        else if ( pCurrentPalette[i] > targetPalette[i] )
        {
            pCurrentPalette[i]--;
        }
    }

    view const* pView = eng_GetView();
    if ( pView )
    {
        f32 nearZ, farZ;
        pView->GetZLimits( nearZ, farZ );
        ExpandCustomFogPalette( m_fogSourcePalette[paletteIndex], m_fogPalette[paletteIndex], nearZ, farZ );

        u32 colorTotal[4] = { 0, 0, 0, 0 };
        for ( s32 i = 0; i < kFogSourcePaletteCount * 4; i += 4 )
        {
            colorTotal[0] += m_fogSourcePalette[paletteIndex][i + 0];
            colorTotal[1] += m_fogSourcePalette[paletteIndex][i + 1];
            colorTotal[2] += m_fogSourcePalette[paletteIndex][i + 2];
            colorTotal[3] += m_fogSourcePalette[paletteIndex][i + 3];
        }

        colorTotal[0] /= kFogSourcePaletteCount;
        colorTotal[1] /= kFogSourcePaletteCount;
        colorTotal[2] /= kFogSourcePaletteCount;
        colorTotal[3] /= kFogSourcePaletteCount;

        m_fogColor[paletteIndex].Set(
            static_cast<f32>( colorTotal[0] ) / 255.0f, static_cast<f32>( colorTotal[1] ) / 255.0f,
            static_cast<f32>( colorTotal[2] ) / 255.0f, static_cast<f32>( colorTotal[3] ) / 255.0f );

        f32 const        numer = ( 2.0f * kPS2ZConst * nearZ * farZ ) / ( farZ - nearZ );
        f32 const        denomOffset = -kPS2ZConst + ( kPS2ZConst * ( farZ + nearZ ) ) / ( farZ - nearZ );
        f32 const        depthScaleQ = farZ / ( farZ - nearZ );
        static f32 const scale = 1 / 16.0f;

        f32 const pS2ZValue = static_cast<f32>( 0xffff ) * scale;
        f32 const denom = pS2ZValue + denomOffset;
        ASSERT( x_abs( denom ) > 0.001f );
        f32 const viewZValue = numer / denom;
        f32 const projectedZValue = viewZValue * depthScaleQ - nearZ * depthScaleQ;
        ASSERT( viewZValue > 0.001f );
        m_fogStart[paletteIndex] = projectedZValue;

        LeastSquares alphaApprox;
        alphaApprox.Setup( 3 );

        static f32 const nSamples = 512.0f;
        f32 const        stepSize = ( farZ - nearZ ) / nSamples;
        for ( f32 viewZ = nearZ; viewZ <= farZ; viewZ += stepSize )
        {
            f32 pS2ScreenZ = ( viewZ * ( farZ + nearZ ) / ( farZ - nearZ ) );
            pS2ScreenZ -= ( ( 2.0f * farZ * nearZ ) / ( farZ - nearZ ) );
            pS2ScreenZ /= viewZ;
            pS2ScreenZ *= -kPS2ZConst;
            pS2ScreenZ += kPS2ZConst;

            f32 const projectedScreenZ = ( viewZ * depthScaleQ ) - ( nearZ * depthScaleQ );

            f32       a = 0.0f;
            u32 const upS2Sz = static_cast<u32>( pS2ScreenZ * 16.0f + 0.5f );
            if ( upS2Sz <= 0xFFFF )
            {
                s32 const clutIx =
                    MINMAX( 0, static_cast<s32>( ( ( upS2Sz >> 8 ) & 0xFF ) / 4 ), kFogSourcePaletteCount - 1 );
                a = static_cast<f32>( m_fogSourcePalette[paletteIndex][clutIx * 4 + 3] ) / 255.0f;
            }

            alphaApprox.AddSample( projectedScreenZ, a );
        }

        if ( !alphaApprox.Solve() )
        {
            alphaApprox.SetCoeff( 0, static_cast<f32>( m_fogSourcePalette[paletteIndex][3] ) / 255.0f );
            alphaApprox.SetCoeff( 1, 0.0f );
            alphaApprox.SetCoeff( 2, 0.0f );
            alphaApprox.SetCoeff( 3, 0.0f );
        }

        m_fogConst[paletteIndex].Set( alphaApprox.GetCoeff( 0 ), alphaApprox.GetCoeff( 1 ), alphaApprox.GetCoeff( 2 ),
                                      alphaApprox.GetCoeff( 3 ) );
    }

    m_isFogValid[paletteIndex] = TRUE;
}

//==============================================================================

void PostMgr::BuildFogPalette( render::post_falloff_fn fn, xcolor color, f32 param1, f32 param2 )
{
    m_fogFilter.PaletteIndex = kFogGeneratedPaletteIndex;

    m_fogFilter.Fn[m_fogFilter.PaletteIndex] = fn;

    f32 const nearZ = m_postNearZ;
    f32 const farZ = m_postFarZ;
    f32 const range = MAX( farZ - nearZ, 0.001f );

    for ( s32 i = 0; i < kFogPaletteCount; ++i )
    {
        xcolor entryColor( color.R, color.G, color.B, color.A );

        if ( fn == render::FALLOFF_CONSTANT )
        {
            entryColor.A = color.A;
        }
        else
        {
            f32 const linearDepth = static_cast<f32>( i ) / static_cast<f32>( kFogPaletteCount - 1 );
            f32 const viewZ = nearZ + linearDepth * range;
            f32 const f = MINMAX( 0.0f, ComputeFogFalloff( fn, viewZ, nearZ, farZ, param1, param2 ), 1.0f );
            entryColor.A = static_cast<u8>( MINMAX( 0.0f, 128.0f - ( f * 128.0f ), 255.0f ) );

            if ( i == 0 )
            {
                entryColor.A = 0;
            }
        }

        WritePaletteColor( m_fogPalette[m_fogFilter.PaletteIndex], i, entryColor );
    }

    m_isFogValid[m_fogFilter.PaletteIndex] = TRUE;
}

//==============================================================================

void PostMgr::ExecuteZFogFilter( void )
{
    s32 const paletteIndex = m_fogFilter.PaletteIndex;
    if ( ( paletteIndex < 0 ) || ( paletteIndex >= 5 ) || !m_isFogValid[paletteIndex] )
    {
        return;
    }

    rtarget const* pNormalDepthTarget = g_GBufferMgr.GetGBufferTarget( GBufferTarget::NormalDepth );
    if ( !pNormalDepthTarget || !rtarget_HasShaderResource( *pNormalDepthTarget ) )
    {
        return;
    }

    xbool const   bUsePolynomial = ( m_fogFilter.Fn[paletteIndex] == render::FALLOFF_CUSTOM );
    shader const& pixelShader = bUsePolynomial ? m_fogResources.PolynomialPS : m_fogResources.CompositePS;
    if ( !pixelShader )
    {
        return;
    }

    if ( !bUsePolynomial )
    {
        if ( !m_fogResources.UpdatePaletteTexture( m_fogPalette[paletteIndex] ) )
        {
            return;
        }
    }

    m_fogResources.UpdateConstants( m_fogColor[paletteIndex], m_fogConst[paletteIndex], m_fogStart[paletteIndex],
                                    m_postNearZ, m_postFarZ, bUsePolynomial );

    if ( !m_fogResources.BindForComposite( pixelShader, !bUsePolynomial ) )
    {
        return;
    }

    composite_Blit( *pNormalDepthTarget, COMPOSITE_BLEND_ALPHA, 1.0f, &pixelShader, RSTATE_SAMPLER_PRESET_POINT_CLAMP,
                    "NormalDepthSource" );
}

//==============================================================================

void PostMgr::GetGeometryFogConstants( vector4& color, vector4& coeff, vector4& params ) const
{
    color.Set( 0.0f, 0.0f, 0.0f, 0.0f );
    coeff.Set( 0.0f, 0.0f, 0.0f, 0.0f );
    params.Set( 0.0f, 0.0f, 0.0f, 0.0f );

    if ( m_Flags.Override || !m_Flags.DoZFogCustom )
    {
        return;
    }

    s32 const paletteIndex = m_fogFilter.PaletteIndex;
    if ( ( paletteIndex < 0 ) || ( paletteIndex >= 5 ) || !m_isFogValid[paletteIndex] ||
         ( m_fogFilter.Fn[paletteIndex] != render::FALLOFF_CUSTOM ) )
    {
        return;
    }

    color = m_fogColor[paletteIndex];
    coeff = m_fogConst[paletteIndex];
    params.Set( m_postNearZ, m_postFarZ, m_fogStart[paletteIndex], 1.0f );
}

//==============================================================================

xcolor PostMgr::GetFogValue( vector3 const& worldPos, s32 paletteIndex )
{
    if ( ( paletteIndex < 0 ) || ( paletteIndex >= 5 ) || !m_isFogValid[paletteIndex] )
    {
        return xcolor( 255, 255, 255, 0 );
    }

    view const* pView = eng_GetView();
    if ( !pView )
    {
        return xcolor( 255, 255, 255, 0 );
    }

    if ( m_fogFilter.Fn[paletteIndex] == render::FALLOFF_CUSTOM )
    {
        vector4 screenPos( worldPos );
        screenPos.GetW() = 1.0f;
        screenPos = pView->GetW2C() * screenPos;
        if ( x_abs( screenPos.GetW() ) < 0.001f )
        {
            return xcolor( 255, 255, 255, 0 );
        }

        f32 const z = screenPos.GetZ();
        f32 const z2 = z * z;
        f32 const z3 = z2 * z;
        f32       fogIntensity = m_fogConst[paletteIndex].GetX() + m_fogConst[paletteIndex].GetY() * z +
                           m_fogConst[paletteIndex].GetZ() * z2 + m_fogConst[paletteIndex].GetW() * z3;

        fogIntensity = MINMAX( 0.0f, fogIntensity, 1.0f );
        return xcolor( 255, 255, 255, static_cast<u8>( fogIntensity * 255.0f ) );
    }

    f32 nearZ, farZ;
    pView->GetZLimits( nearZ, farZ );

    vector4 viewPos( worldPos );
    viewPos.GetW() = 1.0f;
    viewPos = pView->GetW2V() * viewPos;

    if ( viewPos.GetZ() <= nearZ )
    {
        return xcolor( 255, 255, 255, m_fogPalette[paletteIndex][3] );
    }

    f32 const linearDepth = ( viewPos.GetZ() - nearZ ) / MAX( farZ - nearZ, 0.001f );
    s32 const paletteIx = GetPaletteIndexFromDepth( linearDepth );
    u8 const  alpha = m_fogPalette[paletteIndex][paletteIx * 4 + 3];

    return xcolor( 255, 255, 255, alpha );
}
