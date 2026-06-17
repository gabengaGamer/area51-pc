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
    const s32 MaxConstantBufferSize = D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16;
    ASSERT( sizeof(cb_shadow_maps) <= (size_t)MaxConstantBufferSize );
    if( sizeof(cb_shadow_maps) > (size_t)MaxConstantBufferSize )
    {
        x_DebugMsg( "GeomMgr: cb_shadow_maps size %d exceeds D3D11 constant buffer limit %d\n",
                    (s32)sizeof(cb_shadow_maps),
                    MaxConstantBufferSize );
        return FALSE;
    }

    m_pShadowBuffer       = shader_CreateConstantBuffer( sizeof(cb_shadow_maps), CB_TYPE_DYNAMIC );
    m_pShadowAtlasSampler = NULL;
    m_bShadowMapsDirty    = TRUE;
    m_bShadowMapsBound    = FALSE;

    if( !m_pShadowBuffer )
        return FALSE;

    if( !g_pd3dDevice )
    {
        m_pShadowBuffer->Release();
        m_pShadowBuffer = NULL;
        return FALSE;
    }

    D3D11_SAMPLER_DESC ShadowSamplerDesc;
    x_memset( &ShadowSamplerDesc, 0, sizeof(ShadowSamplerDesc) );
    ShadowSamplerDesc.Filter        = D3D11_FILTER_MIN_MAG_MIP_POINT;
    ShadowSamplerDesc.AddressU      = D3D11_TEXTURE_ADDRESS_CLAMP;
    ShadowSamplerDesc.AddressV      = D3D11_TEXTURE_ADDRESS_CLAMP;
    ShadowSamplerDesc.AddressW      = D3D11_TEXTURE_ADDRESS_CLAMP;
    ShadowSamplerDesc.MaxAnisotropy = 1;
    ShadowSamplerDesc.MinLOD        = 0.0f;
    ShadowSamplerDesc.MaxLOD        = D3D11_FLOAT32_MAX;
    if( FAILED( g_pd3dDevice->CreateSamplerState( &ShadowSamplerDesc, &m_pShadowAtlasSampler ) ) ||
        !m_pShadowAtlasSampler )
    {
        m_pShadowBuffer->Release();
        m_pShadowBuffer = NULL;
        return FALSE;
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

    if( m_pShadowAtlasSampler )
    {
        m_pShadowAtlasSampler->Release();
        m_pShadowAtlasSampler = NULL;
    }
}

//==============================================================================

void geom_mgr::ResetShadowMaps( void )
{
    if( !g_pd3dContext )
        return;

    if( !m_bShadowMapsBound )
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

    ID3D11ShaderResourceView* pNullSRV = NULL;
    ID3D11SamplerState*       pNullSamp = NULL;
    g_pd3dContext->PSSetShaderResources( PC_SHADOW_ATLAS_TEX_SLOT, 1, &pNullSRV );
    g_pd3dContext->PSSetSamplers( PC_SHADOW_ATLAS_SAMP_SLOT, 1, &pNullSamp );

    m_bShadowMapsDirty = TRUE;
    m_bShadowMapsBound = FALSE;
}

//==============================================================================

xbool geom_mgr::UpdateShadowMaps( void )
{
    if( !g_pd3dContext || !m_pShadowBuffer )
        return FALSE;

    if( !m_bShadowMapsDirty && m_bShadowMapsBound )
        return TRUE;

    cb_shadow_maps cb;
    x_memset( &cb, 0, sizeof(cb) );

    s32 nFaceShadows = 0;
    s32 nPointLights = 0;
    const s32 nSources = g_ShadowMapMgr.GetSourceCount();
    for( s32 i = 0; i < nSources; i++ )
    {
        const shadow_map_mgr::shadow_source& Source = g_ShadowMapMgr.GetSource( i );

        if( nFaceShadows >= MAX_SHADOW_SOURCES )
            continue;

        const s32 PackedFaceIndex = nFaceShadows;
        cb.FaceShadowMatrix[PackedFaceIndex] = Source.WorldToAtlas;
        cb.FaceShadowLightPosRadius[PackedFaceIndex] = Source.LightPosRadius;
        cb.FaceShadowLightDirFalloff[PackedFaceIndex] = Source.FaceLightDirFalloff;
        cb.FaceShadowLightData[PackedFaceIndex] = Source.FaceLightData;
        nFaceShadows++;

        if( Source.Type != shadow_map_mgr::SHADOW_SOURCE_POINT_FACE )
            continue;

        if( Source.FaceIndex != 0 )
            continue;

        if( ( Source.PointLightIndex < 0 ) || ( Source.PointLightIndex >= MAX_SHADOW_LIGHTS ) )
            continue;

        if( nPointLights >= MAX_SHADOW_LIGHTS )
            continue;

        cb.PointShadowLightPosRadius[nPointLights] = Source.LightPosRadius;
        cb.PointShadowLightData[nPointLights].Set( Source.LightFalloff,
                                                   Source.ReceiveNearZ,
                                                   Source.FarZ,
                                                   0.0f );
        cb.PointShadowLightParams[nPointLights].Set( (f32)PackedFaceIndex,
                                                     (f32)POINT_SHADOW_FACE_COUNT,
                                                     0.0f,
                                                     0.0f );
        nPointLights++;
    }

    ID3D11ShaderResourceView* pFaceShadowSRV  = ( nFaceShadows > 0 ) ? g_ShadowMgr.GetShadowAtlasSRV() : NULL;
    ID3D11SamplerState*       pShadowSampler  = ( pFaceShadowSRV && m_pShadowAtlasSampler ) ? m_pShadowAtlasSampler : NULL;
    if( !pShadowSampler )
        pFaceShadowSRV = NULL;

    cb.FaceShadowCount       = pShadowSampler ? nFaceShadows : 0;
    cb.PointShadowLightCount = pShadowSampler ? nPointLights : 0;

    cb.ShadowParams.Set( 0.0f,
                         0.0f,
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

    g_pd3dContext->PSSetShaderResources( PC_SHADOW_ATLAS_TEX_SLOT, 1, &pFaceShadowSRV );

    g_pd3dContext->PSSetSamplers( PC_SHADOW_ATLAS_SAMP_SLOT, 1, &pShadowSampler );

    m_bShadowMapsDirty = FALSE;
    m_bShadowMapsBound = TRUE;

    return ( cb.FaceShadowCount > 0 ) || ( cb.PointShadowLightCount > 0 );
}
