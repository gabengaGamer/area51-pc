//==============================================================================
//
//  GeomMgr_ShadowMaps.cpp
//
//  Shadow map utilities for the PC geom manager
//
//==============================================================================

//==============================================================================
//  BASE INCLUDES
//==============================================================================

#include "x_types.hpp"

//==============================================================================
//  INCLUDES
//==============================================================================

#include "GeomMgr.hpp"

namespace
{
u32 const kInvalidShadowIndex = 0xFFFFFFFFu;
}

//==============================================================================
//  FUNCTIONS
//==============================================================================

xbool GeomMgr::InitShadowMaps( void )
{
    x_memset( m_faceShadowMatrices, 0, sizeof( m_faceShadowMatrices ) );
    x_memset( &m_shadowMapData, 0, sizeof( m_shadowMapData ) );
    for ( s32 i = 0; i < light_mgr::MAX_DYNAMIC_LIGHTS; ++i )
    {
        m_dynamicLightShadowIndex[i] = kInvalidShadowIndex;
    }

    rbuffer_desc matrixDesc;
    matrixDesc.Size = sizeof( m_faceShadowMatrices );
    matrixDesc.Stride = sizeof( matrix4 );
    matrixDesc.UsageFlags = RBUFFER_USAGE_GRAPHICS_STORAGE_READ;
    matrixDesc.pDebugName = "GeomShadowMatrices";
    if ( !rbuffer_Create( m_shadowMatrixBuffer, matrixDesc ) )
    {
        return FALSE;
    }

    rbuffer_desc dataDesc;
    dataDesc.Size = sizeof( m_shadowMapData );
    dataDesc.Stride = sizeof( vector4 );
    dataDesc.UsageFlags = RBUFFER_USAGE_GRAPHICS_STORAGE_READ;
    dataDesc.pDebugName = "GeomShadowData";
    if ( !rbuffer_Create( m_shadowDataBuffer, dataDesc ) )
    {
        rbuffer_Destroy( m_shadowMatrixBuffer );
        return FALSE;
    }

    m_pCachedFaceShadowResource = NULL;
    m_pCachedFaceShadowDepthResource = NULL;
    m_areShadowMapConstantsValid = FALSE;
    return TRUE;
}

//==============================================================================

void GeomMgr::KillShadowMaps( void )
{
    rbuffer_Destroy( m_shadowDataBuffer );
    rbuffer_Destroy( m_shadowMatrixBuffer );
    m_pCachedFaceShadowResource = NULL;
    m_pCachedFaceShadowDepthResource = NULL;
    m_areShadowMapConstantsValid = FALSE;
}

//==============================================================================

xbool GeomMgr::PrepareSharedShadowData( void )
{
    return m_areShadowMapConstantsValid || BuildShadowMapConstants();
}

//==============================================================================

u32 GeomMgr::GetDynamicLightShadowIndex( s32 dynamicLightIndex ) const
{
    if ( ( dynamicLightIndex < 0 ) || ( dynamicLightIndex >= light_mgr::MAX_DYNAMIC_LIGHTS ) )
    {
        return kInvalidShadowIndex;
    }

    return m_dynamicLightShadowIndex[dynamicLightIndex];
}

//==============================================================================

shader_resource const* GeomMgr::GetFaceShadowResource( void ) const
{
    return m_pCachedFaceShadowResource;
}

//==============================================================================

shader_resource const* GeomMgr::GetFaceShadowDepthResource( void ) const
{
    return m_pCachedFaceShadowDepthResource;
}

//==============================================================================

shader_resource const* GeomMgr::GetShadowMatricesResource( void ) const
{
    return rbuffer_GetResource( m_shadowMatrixBuffer );
}

//==============================================================================

shader_resource const* GeomMgr::GetShadowDataResource( void ) const
{
    return rbuffer_GetResource( m_shadowDataBuffer );
}

//==============================================================================

rstate_sampler_preset GeomMgr::GetFaceShadowSampler( void ) const
{
    return m_faceShadowSamplerPreset;
}

//==============================================================================

xbool GeomMgr::BuildShadowMapConstants( void )
{
    x_memset( m_faceShadowMatrices, 0, sizeof( m_faceShadowMatrices ) );
    x_memset( &m_shadowMapData, 0, sizeof( m_shadowMapData ) );
    for ( s32 i = 0; i < light_mgr::MAX_DYNAMIC_LIGHTS; ++i )
    {
        m_dynamicLightShadowIndex[i] = kInvalidShadowIndex;
    }

    f32 const depthAtlasTexelSize = g_ShadowMgr.GetAtlasTexelSize();
    vector4 const shadowFilterParams = g_ShadowMgr.GetShadowFilterParams();
    f32 const sampleAtlasTexelSize = shadowFilterParams.GetX();
    ShadowFilterType const shadowFilterType = g_ShadowMapMgr.GetShadowFilterType();
    s32       nFaceShadows = 0;
    s32       nPointLights = 0;
    s32 const nSources = g_ShadowMapMgr.GetSourceCount();
    for ( s32 i = 0; i < nSources; i++ )
    {
        ShadowMapMgr::ShadowSource const& source = g_ShadowMapMgr.GetSource( i );

        if ( nFaceShadows >= MAX_SHADOW_SOURCES )
        {
            continue;
        }

        s32 const packedFaceIndex = nFaceShadows;
        m_faceShadowMatrices[packedFaceIndex] = source.WorldToAtlas;
        m_shadowMapData.FaceShadowLightPosRadius[packedFaceIndex] = source.LightPosRadius;
        m_shadowMapData.FaceShadowLightDirFalloff[packedFaceIndex] = source.FaceLightDirFalloff;
        m_shadowMapData.FaceShadowLightData[packedFaceIndex] = source.FaceLightData;
        m_shadowMapData.FaceShadowProjectionData[packedFaceIndex].Set( source.NearZ, source.FarZ, 0.0f, 0.0f );
        m_shadowMapData.FaceShadowAtlasRect[packedFaceIndex].Set(
            static_cast<f32>( source.AtlasX ) * depthAtlasTexelSize + sampleAtlasTexelSize * 0.5f,
            static_cast<f32>( source.AtlasY ) * depthAtlasTexelSize + sampleAtlasTexelSize * 0.5f,
            static_cast<f32>( source.AtlasX + source.AtlasWidth ) * depthAtlasTexelSize -
                sampleAtlasTexelSize * 0.5f,
            static_cast<f32>( source.AtlasY + source.AtlasHeight ) * depthAtlasTexelSize -
                sampleAtlasTexelSize * 0.5f );
        nFaceShadows++;

        xbool const hasDynamicLightIndex =
            ( source.DynamicLightIndex >= 0 ) && ( source.DynamicLightIndex < light_mgr::MAX_DYNAMIC_LIGHTS );
        if ( source.Type != ShadowMapMgr::SHADOW_SOURCE_POINT_FACE )
        {
            if ( hasDynamicLightIndex )
            {
                m_dynamicLightShadowIndex[source.DynamicLightIndex] = packedFaceIndex;
            }
            continue;
        }

        if ( source.FaceIndex != 0 )
        {
            continue;
        }

        if ( ( source.PointLightIndex < 0 ) || ( source.PointLightIndex >= MAX_SHADOW_LIGHTS ) )
        {
            continue;
        }

        if ( nPointLights >= MAX_SHADOW_LIGHTS )
        {
            continue;
        }

        m_shadowMapData.PointShadowLightPosRadius[nPointLights] = source.LightPosRadius;
        m_shadowMapData.PointShadowLightData[nPointLights].Set( source.LightFalloff, source.ReceiveNearZ, source.FarZ,
                                                                0.0f );
        m_shadowMapData.PointShadowLightParams[nPointLights].Set(
            static_cast<f32>( packedFaceIndex ), static_cast<f32>( POINT_SHADOW_FACE_COUNT ), 0.0f, 0.0f );
        if ( hasDynamicLightIndex )
        {
            m_dynamicLightShadowIndex[source.DynamicLightIndex] = nPointLights;
        }
        nPointLights++;
    }

    rtarget const* pFaceShadowTarget =
        ( nFaceShadows > 0 ) ? g_ShadowMgr.GetShadowSampleAtlasTarget() : NULL;
    rtarget const* pFaceShadowDepthTarget =
        ( nFaceShadows > 0 ) ? g_ShadowMgr.GetShadowDepthAtlasTarget() : NULL;
    shader_resource const* pFaceShadowResource =
        pFaceShadowTarget ? rtarget_GetShaderResource( *pFaceShadowTarget ) : NULL;
    shader_resource const* pFaceShadowDepthResource =
        pFaceShadowDepthTarget ? rtarget_GetShaderResource( *pFaceShadowDepthTarget ) : NULL;
    xbool const bBindShadowSampler = ( pFaceShadowResource != NULL ) && ( pFaceShadowDepthResource != NULL );

    m_shadowMapData.FaceShadowCount = bBindShadowSampler ? nFaceShadows : 0;
    m_shadowMapData.PointShadowLightCount = bBindShadowSampler ? nPointLights : 0;

    m_shadowMapData.ShadowParams.Set( depthAtlasTexelSize, g_ShadowMgr.GetShadowNormalBiasTexels(),
                                      g_ShadowMgr.GetShadowSeamBlendTexels(),
                                      shadowFilterType == ShadowFilterType::Evsm ? 1.0f : 0.0f );
    m_shadowMapData.ShadowFilterParams = shadowFilterParams;
    m_shadowMapData.ShadowContactParams.Set(
        static_cast<f32>( SHADOW_EVSM_BLUR_RADIUS ) * SHADOW_EVSM_BLUR_SCALE, 0.0f, 0.0f, 0.0f );

    m_pCachedFaceShadowResource = bBindShadowSampler ? pFaceShadowResource : vram_GetShaderResource( m_whiteTexture );
    m_pCachedFaceShadowDepthResource =
        bBindShadowSampler ? pFaceShadowDepthResource : vram_GetShaderResource( m_whiteTexture );
    m_faceShadowSamplerPreset = ( shadowFilterType == ShadowFilterType::Evsm )
                                    ? RSTATE_SAMPLER_PRESET_LINEAR_CLAMP
                                    : RSTATE_SAMPLER_PRESET_POINT_CLAMP;
    if ( !m_pCachedFaceShadowResource || !m_pCachedFaceShadowDepthResource ||
         !rbuffer_Upload( m_shadowMatrixBuffer, m_faceShadowMatrices, sizeof( m_faceShadowMatrices ), 0, TRUE ) ||
         !rbuffer_Upload( m_shadowDataBuffer, &m_shadowMapData, sizeof( m_shadowMapData ), 0, TRUE ) )
    {
        return FALSE;
    }

    m_areShadowMapConstantsValid = TRUE;
    return TRUE;
}
