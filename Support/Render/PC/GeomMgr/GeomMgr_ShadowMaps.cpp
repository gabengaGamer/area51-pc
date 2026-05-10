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

    ID3D11ShaderResourceView* pNullSRV[2] = { NULL, NULL };
    ID3D11SamplerState*       pNullSamp[2] = { NULL, NULL };
    g_pd3dContext->PSSetShaderResources( PC_POINT_SHADOW_TEX_SLOT, 2, pNullSRV );
    g_pd3dContext->PSSetSamplers( PC_POINT_SHADOW_SAMP_SLOT, 2, pNullSamp );
}

//==============================================================================

xbool geom_mgr::UpdateShadowMaps( const matrix4& L2W,
                                      const bbox&    B )
{
    if( !g_pd3dContext )
        return FALSE;

    cb_shadow_maps cb;
    x_memset( &cb, 0, sizeof(cb) );

    s32 PointLightSlots[MAX_SHADOW_LIGHTS];
    x_memset( PointLightSlots, 0xFF, sizeof(PointLightSlots) );

    s32 nFaceShadows = 0;
    s32 nPointLights = 0;
    const s32 nCollected = g_ShadowMapMgr.CollectSources( L2W, B, MAX_SHADOW_SOURCES );
    for( s32 i = 0; i < nCollected; i++ )
    {
        s32     SourceIndex;
        s32     Type;
        matrix4 ShadowMatrix;
        vector4 LightPosRadius;
        f32     Falloff;
        f32     NearZ;
        f32     FarZ;
        s32     PointLightIndex;
        s32     FaceIndex;

        g_ShadowMapMgr.GetCollectedSource( i,
                                           SourceIndex,
                                           Type,
                                           ShadowMatrix,
                                           LightPosRadius,
                                           Falloff,
                                           NearZ,
                                           FarZ,
                                           PointLightIndex,
                                           FaceIndex );

        (void)FaceIndex;

        if( Type == shadow_map_mgr::SHADOW_SOURCE_POINT_FACE )
        {
            if( ( PointLightIndex < 0 ) || ( PointLightIndex >= MAX_SHADOW_LIGHTS ) )
                continue;

            if( PointLightSlots[PointLightIndex] >= 0 )
                continue;

            const s32 PackedLightIndex = nPointLights;
            if( PackedLightIndex >= MAX_SHADOW_LIGHTS )
                continue;

            PointLightSlots[PointLightIndex] = PackedLightIndex;
            cb.PointShadowLightPosRadius[PackedLightIndex] = LightPosRadius;
            cb.PointShadowLightData[PackedLightIndex].Set( Falloff,
                                                           (f32)PointLightIndex,
                                                           NearZ,
                                                           FarZ );
            nPointLights++;
        }
        else
        {
            if( nFaceShadows >= MAX_SHADOW_SOURCES )
                continue;

            const shadow_map_mgr::shadow_source& Source = g_ShadowMapMgr.GetSource( SourceIndex );
            cb.FaceShadowMatrix[nFaceShadows] = ShadowMatrix;
            cb.FaceShadowLightPosRadius[nFaceShadows] = LightPosRadius;
            cb.FaceShadowLightDirFalloff[nFaceShadows] = Source.FaceLightDirFalloff;
            cb.FaceShadowLightData[nFaceShadows] = Source.FaceLightData;
            nFaceShadows++;
        }
    }

    ID3D11ShaderResourceView* pPointShadowSRV = ( nPointLights > 0 ) ? g_ShadowMgr.GetPointShadowSRV() : NULL;
    ID3D11ShaderResourceView* pFaceShadowSRV  = ( nFaceShadows > 0 ) ? g_ShadowMgr.GetSpotShadowSRV()  : NULL;

    cb.FaceShadowCount      = pFaceShadowSRV  ? nFaceShadows : 0;
    cb.PointShadowLightCount= pPointShadowSRV ? nPointLights : 0;

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

    ID3D11ShaderResourceView* pShadowSRV[2] =
    {
        pPointShadowSRV,
        pFaceShadowSRV
    };

    ID3D11SamplerState* pShadowSampler[2] =
    {
        pPointShadowSRV ? m_pPointShadowSampler : NULL,
        pFaceShadowSRV  ? m_pSpotShadowSampler  : NULL
    };

    g_pd3dContext->PSSetShaderResources( PC_POINT_SHADOW_TEX_SLOT, 2, pShadowSRV );
    g_pd3dContext->PSSetSamplers( PC_POINT_SHADOW_SAMP_SLOT, 2, pShadowSampler );

    return ( cb.FaceShadowCount > 0 ) || ( cb.PointShadowLightCount > 0 );
}
