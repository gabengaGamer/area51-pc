//==============================================================================
//
//  GeomMgr_ShadowMaps.cpp
//
//  Shadow map utilities for the PC geom manager
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

#include "GeomMgr.hpp"

//==============================================================================
//  EXTERNAL VARIABLES
//==============================================================================

extern ID3D11DeviceContext* g_pd3dContext;

//==============================================================================
//  FUNCTIONS
//==============================================================================

xbool geom_mgr::InitShadowMaps( void )
{
    m_pShadowBuffer        = shader_CreateConstantBuffer( sizeof(cb_shadow_maps), CB_TYPE_DYNAMIC );
    m_pPointShadowSampler  = NULL;
    m_pSpotShadowSampler   = NULL;

    if( g_pd3dDevice )
    {
        D3D11_SAMPLER_DESC PointSamplerDesc;
        x_memset( &PointSamplerDesc, 0, sizeof(PointSamplerDesc) );
        PointSamplerDesc.Filter         = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
        PointSamplerDesc.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
        PointSamplerDesc.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
        PointSamplerDesc.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
        PointSamplerDesc.MaxAnisotropy  = 1;
        PointSamplerDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
        PointSamplerDesc.MinLOD         = 0.0f;
        PointSamplerDesc.MaxLOD         = D3D11_FLOAT32_MAX;
        g_pd3dDevice->CreateSamplerState( &PointSamplerDesc, &m_pPointShadowSampler );

        D3D11_SAMPLER_DESC SpotSamplerDesc;
        x_memset( &SpotSamplerDesc, 0, sizeof(SpotSamplerDesc) );
        SpotSamplerDesc.Filter        = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
        SpotSamplerDesc.AddressU      = D3D11_TEXTURE_ADDRESS_CLAMP;
        SpotSamplerDesc.AddressV      = D3D11_TEXTURE_ADDRESS_CLAMP;
        SpotSamplerDesc.AddressW      = D3D11_TEXTURE_ADDRESS_CLAMP;
        SpotSamplerDesc.MaxAnisotropy = 1;
        SpotSamplerDesc.MinLOD        = 0.0f;
        SpotSamplerDesc.MaxLOD        = D3D11_FLOAT32_MAX;
        g_pd3dDevice->CreateSamplerState( &SpotSamplerDesc, &m_pSpotShadowSampler );
    }

    return TRUE;
}

//==============================================================================

void geom_mgr::KillShadowMaps( void )
{
    if( m_pShadowBuffer )
    {
        m_pShadowBuffer->Release();
        m_pShadowBuffer = NULL;
    }

    if( m_pPointShadowSampler )
    {
        m_pPointShadowSampler->Release();
        m_pPointShadowSampler = NULL;
    }

    if( m_pSpotShadowSampler )
    {
        m_pSpotShadowSampler->Release();
        m_pSpotShadowSampler = NULL;
    }
}

//==============================================================================

void geom_mgr::ResetShadowMaps( void )
{
    if( !g_pd3dContext )
        return;

    if( m_pShadowBuffer )
    {
        cb_shadow_maps cb;
        x_memset( &cb, 0, sizeof(cb) );

        D3D11_MAPPED_SUBRESOURCE Mapped;
        if( SUCCEEDED( g_pd3dContext->Map( m_pShadowBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped ) ) )
        {
            x_memcpy( Mapped.pData, &cb, sizeof(cb) );
            g_pd3dContext->Unmap( m_pShadowBuffer, 0 );
        }
    }

    g_pd3dContext->PSSetConstantBuffers( PC_SHADOW_BUFFER_SLOT, 1, &m_pShadowBuffer );

    ID3D11ShaderResourceView* pNullSRV[PC_POINT_SHADOW_TEX_COUNT + 1] = { NULL };
    ID3D11SamplerState*       pNullSamp[2] = { NULL, NULL };
    g_pd3dContext->PSSetShaderResources( PC_POINT_SHADOW_TEX_SLOT, ARRAYSIZE(pNullSRV), pNullSRV );
    g_pd3dContext->PSSetSamplers( PC_POINT_SHADOW_SAMP_SLOT, 2, pNullSamp );
}

//==============================================================================

xbool geom_mgr::UpdateShadowMaps( void )
{
    if( !g_pd3dContext )
        return FALSE;

    cb_shadow_maps cb;
    x_memset( &cb, 0, sizeof(cb) );

    s32 PointLightSlots[MAX_SHADOW_LIGHTS];
    x_memset( PointLightSlots, 0xFF, sizeof(PointLightSlots) );

    s32 nFaceShadows = 0;
    s32 nPointLights = 0;
    const s32 nSources = g_ShadowMapMgr.GetSourceCount();
    for( s32 i = 0; i < nSources; i++ )
    {
        const shadow_map_mgr::shadow_source& Source = g_ShadowMapMgr.GetSource( i );

        if( Source.Type == shadow_map_mgr::SHADOW_SOURCE_POINT_FACE )
        {
            if( ( Source.PointLightIndex < 0 ) || ( Source.PointLightIndex >= MAX_SHADOW_LIGHTS ) )
                continue;

            if( PointLightSlots[Source.PointLightIndex] != -1 )
                continue;

            s32 BucketIndex = -1;
            s32 LocalLightIndex = -1;
            if( !g_ShadowMgr.GetPointShadowBinding( Source.PointLightIndex, BucketIndex, LocalLightIndex ) )
            {
                PointLightSlots[Source.PointLightIndex] = -2;
                continue;
            }

            const s32 PackedLightIndex = nPointLights;
            if( PackedLightIndex >= MAX_SHADOW_LIGHTS )
                continue;

            PointLightSlots[Source.PointLightIndex] = PackedLightIndex;
            cb.PointShadowLightPosRadius[PackedLightIndex] = Source.LightPosRadius;
            cb.PointShadowLightData[PackedLightIndex].Set( Source.LightFalloff,
                                                           Source.NearZ,
                                                           Source.FarZ,
                                                           (f32)LocalLightIndex );
            cb.PointShadowLightParams[PackedLightIndex].Set( (f32)BucketIndex,
                                                             0.0f,
                                                             0.0f,
                                                             0.0f );
            nPointLights++;
        }
        else
        {
            if( nFaceShadows >= MAX_SHADOW_SOURCES )
                continue;

            cb.FaceShadowMatrix[nFaceShadows] = Source.WorldToAtlas;
            cb.FaceShadowLightPosRadius[nFaceShadows] = Source.LightPosRadius;
            cb.FaceShadowLightDirFalloff[nFaceShadows] = Source.FaceLightDirFalloff;
            cb.FaceShadowLightData[nFaceShadows] = Source.FaceLightData;
            nFaceShadows++;
        }
    }

    ID3D11ShaderResourceView* pFaceShadowSRV  = ( nFaceShadows > 0 ) ? g_ShadowMgr.GetSpotShadowSRV()  : NULL;

    cb.FaceShadowCount       = pFaceShadowSRV  ? nFaceShadows : 0;
    cb.PointShadowLightCount = nPointLights;

    cb.ShadowParams.Set( g_ShadowMgr.GetShadowBias(),
                         g_ShadowMgr.GetShadowStrength(),
                         g_ShadowMgr.GetShadowMinVariance(),
                         g_ShadowMgr.GetShadowLightBleedReduction() );

    if( m_pShadowBuffer )
    {
        D3D11_MAPPED_SUBRESOURCE Mapped;
        if( SUCCEEDED( g_pd3dContext->Map( m_pShadowBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped ) ) )
        {
            x_memcpy( Mapped.pData, &cb, sizeof(cb) );
            g_pd3dContext->Unmap( m_pShadowBuffer, 0 );
        }
    }

    g_pd3dContext->PSSetConstantBuffers( PC_SHADOW_BUFFER_SLOT, 1, &m_pShadowBuffer );

    ID3D11ShaderResourceView* pShadowSRV[PC_POINT_SHADOW_TEX_COUNT + 1] = { NULL };
    for( s32 iBucket = 0; iBucket < PC_POINT_SHADOW_TEX_COUNT; iBucket++ )
        pShadowSRV[iBucket] = g_ShadowMgr.GetPointShadowSRV( iBucket );

    pShadowSRV[PC_POINT_SHADOW_TEX_COUNT] = pFaceShadowSRV;

    ID3D11SamplerState* pShadowSampler[2] =
    {
        ( nPointLights > 0 ) ? m_pPointShadowSampler : NULL,
        pFaceShadowSRV  ? m_pSpotShadowSampler  : NULL
    };

    g_pd3dContext->PSSetShaderResources( PC_POINT_SHADOW_TEX_SLOT, ARRAYSIZE(pShadowSRV), pShadowSRV );
    g_pd3dContext->PSSetSamplers( PC_POINT_SHADOW_SAMP_SLOT, 2, pShadowSampler );

    return ( cb.FaceShadowCount > 0 ) || ( cb.PointShadowLightCount > 0 );
}
