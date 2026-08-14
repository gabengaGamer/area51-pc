//=============================================================================
//
//  DecalRenderer.cpp
//
//=============================================================================

#include "DecalRenderer.hpp"

#include "../LightMgr.hpp"
#include "../ProjTextureMgr.hpp"
#include "ProjectionAtlas.hpp"

#include "x_debug.hpp"

//=============================================================================

DecalRenderer g_DecalRenderer;

//=============================================================================
//  HELPERS
//=============================================================================

namespace
{
f32 constexpr kDecalNearZBias = 0.01f;

//=============================================================================

xbool CreateBlackCubeTexture( vram_texture& texture )
{
    vram_texture_desc desc;
    desc.Type = VRAM_TEXTURE_TYPE_CUBE;
    desc.Width = 1;
    desc.Height = 1;
    desc.LayerCount = 6;
    desc.MipCount = 1;
    desc.Format = VRAM_TEXTURE_FORMAT_RGBA8;
    desc.UsageFlags = VRAM_TEXTURE_USAGE_SAMPLED;
    desc.pDebugName = "DecalBlackCube";

    if ( !vram_CreateTexture( texture, desc ) )
    {
        return FALSE;
    }

    u32 const black = 0xff000000u;
    for ( u32 layer = 0; layer < desc.LayerCount; ++layer )
    {
        vram_texture_upload_desc upload;
        upload.Region.Layer = layer;
        upload.Region.Width = 1;
        upload.Region.Height = 1;
        upload.pData = &black;
        upload.Size = sizeof( black );
        upload.RowPitch = sizeof( black );
        upload.SlicePitch = sizeof( black );
        if ( !vram_UploadTexture( texture, upload ) )
        {
            vram_DestroyTexture( texture );
            return FALSE;
        }
    }

    return TRUE;
}

//=============================================================================

xbool EnsureLightingBuffer( rbuffer& buffer, u32& capacity, u32 requiredCount )
{
    u32 const desiredCapacity = MAX( requiredCount, 1u );
    if ( buffer && ( capacity >= desiredCapacity ) )
    {
        return TRUE;
    }

    rbuffer_Destroy( buffer );

    rbuffer_desc desc;
    desc.Size = desiredCapacity * sizeof( GeomLightingConstants );
    desc.Stride = sizeof( GeomLightingConstants );
    desc.UsageFlags = RBUFFER_USAGE_GRAPHICS_STORAGE_READ;
    desc.pDebugName = "DecalLighting";
    if ( !rbuffer_Create( buffer, desc ) )
    {
        capacity = 0;
        return FALSE;
    }

    capacity = desiredCapacity;
    return TRUE;
}
} // namespace

//=============================================================================
//  TYPES
//=============================================================================

DecalRenderer::DrawConstants::DrawConstants( void )
    : WorldToClip(), CameraPosition( 0.0f, 0.0f, 0.0f, 1.0f ), GeometricNormal( 0.0f, 1.0f, 0.0f, 0.0f ),
      EnvParams( 0.0f, 0.0f, 0.0f, 1.0f ), LightingIndex( 0 ),
      Flags( render::DECAL_DRAW_FLAG_NONE ), BlendMode( render::DECAL_BLEND_ALPHA ), OutputMode( 0 )
{
    WorldToClip.Identity();
}

//=============================================================================

DecalRenderer::ShaderBindings::ShaderBindings( void )
    : DrawConstantsVertex( 0xFFFFFFFFu ), DrawConstantsPixel( 0xFFFFFFFFu ),
      ProjectionConstantsPixel( 0xFFFFFFFFu ), DecalTexturePixel( 0xFFFFFFFFu ),
      EnvironmentCubeTexturePixel( 0xFFFFFFFFu ), FaceShadowTexturePixel( 0xFFFFFFFFu ),
      FaceShadowDepthTexturePixel( 0xFFFFFFFFu ), ProjectionAtlasTexturePixel( 0xFFFFFFFFu ),
      LightingDataPixel( 0xFFFFFFFFu ), ShadowMatricesPixel( 0xFFFFFFFFu ),
      ShadowDataPixel( 0xFFFFFFFFu )
{
}

//=============================================================================

DecalRenderer::QueuedDraw::QueuedDraw( void )
    : Constants(), SourceLighting(), pTexture( NULL ), pEnvironmentCube( NULL ), Blend( render::DECAL_BLEND_ALPHA ),
      Flags( render::DECAL_DRAW_FLAG_NONE ), StartIndex( 0 ), BaseVertex( 0 ), IndexCount( 0 )
{
    x_memset( &SourceLighting, 0, sizeof( SourceLighting ) );
}

//=============================================================================
//  IMPLEMENTATION
//=============================================================================

DecalRenderer::DecalRenderer( void )
    : m_vertexMgr(), m_vertexShader(), m_pixelShader(), m_bindings(), m_pipelines(), m_blackCubeTexture(),
      m_lightingBuffer(), m_lightingCapacity( 0 ), m_projectionConstants(), m_frameLighting(), m_queuedVertices(),
      m_queuedIndices(), m_queuedDraws(), m_requestedVertices( 0 ), m_requestedIndices( 0 ), m_requestedDraws( 0 ),
      m_areQueuedDrawsUploaded( FALSE ), m_isInitialized( FALSE )
{
    x_memset( &m_projectionConstants, 0, sizeof( m_projectionConstants ) );
}

//=============================================================================

void DecalRenderer::Init( void )
{
    if ( m_isInitialized )
    {
        return;
    }

    if ( !m_vertexMgr.Init( sizeof( Vertex ) ) || !LoadShaders() || !ResolveBindings() || !CreateSamplers() ||
         !CreateBlackCube() )
    {
        Kill();
        return;
    }

    m_isInitialized = TRUE;
    if ( !Reserve( MAX( m_requestedVertices, 8192 ), MAX( m_requestedIndices, 24576 ),
                   MAX( m_requestedDraws, 2048 ) ) ||
         !PrewarmPipelines() )
    {
        Kill();
    }
}

//=============================================================================

void DecalRenderer::Kill( void )
{
    DiscardQueuedDraws();
    m_frameLighting.Clear();
    m_queuedVertices.Clear();
    m_queuedIndices.Clear();
    m_queuedDraws.Clear();

    rbuffer_Destroy( m_lightingBuffer );
    m_lightingCapacity = 0;
    vram_DestroyTexture( m_blackCubeTexture );
    DestroyPipelines();
    DestroySamplers();
    shader_Destroy( m_pixelShader );
    shader_Destroy( m_vertexShader );
    m_vertexMgr.Kill();

    m_bindings = ShaderBindings();
    m_requestedVertices = 0;
    m_requestedIndices = 0;
    m_requestedDraws = 0;
    m_isInitialized = FALSE;
}

//=============================================================================

void DecalRenderer::BeginRender( void )
{
    ASSERTS( m_queuedDraws.GetCount() == 0, "Decal draw queue survived the previous frame" );
    ASSERTS( !m_areQueuedDrawsUploaded, "Uploaded decal queue survived the previous frame" );
    DiscardQueuedDraws();
}

//=============================================================================

xbool DecalRenderer::Reserve( s32 nVertices, s32 nIndices, s32 nDraws )
{
    if ( ( nVertices <= 0 ) || ( nIndices <= 0 ) || ( nDraws <= 0 ) )
    {
        return FALSE;
    }

    m_requestedVertices = MAX( m_requestedVertices, nVertices );
    m_requestedIndices = MAX( m_requestedIndices, nIndices );
    m_requestedDraws = MAX( m_requestedDraws, nDraws );

    if ( m_queuedVertices.GetCapacity() < nVertices )
    {
        m_queuedVertices.SetCapacity( nVertices );
    }
    if ( m_queuedIndices.GetCapacity() < nIndices )
    {
        m_queuedIndices.SetCapacity( nIndices );
    }
    if ( m_queuedDraws.GetCapacity() < nDraws )
    {
        m_queuedDraws.SetCapacity( nDraws );
    }
    if ( m_frameLighting.GetCapacity() < ( nDraws + 1 ) )
    {
        m_frameLighting.SetCapacity( nDraws + 1 );
    }

    if ( !m_isInitialized )
    {
        return TRUE;
    }

    return m_vertexMgr.Reserve( nVertices, nIndices ) &&
           EnsureLightingBuffer( m_lightingBuffer, m_lightingCapacity, static_cast<u32>( nDraws + 1 ) );
}

//=============================================================================

xbool DecalRenderer::LoadShaders( void )
{
    shader_LoadFromEcs( m_vertexShader, "decal_vs.vs.ecs" );
    shader_LoadFromEcs( m_pixelShader, "decal_ps.ps.ecs" );
    if ( !m_vertexShader || !m_pixelShader )
    {
        x_DebugMsg( "DecalRenderer: failed to load decal shaders\n" );
        return FALSE;
    }

    return TRUE;
}

//=============================================================================

xbool DecalRenderer::ResolveBindings( void )
{
    if ( !shader_FindUniformSlot( m_vertexShader, "cbDecalDraw", m_bindings.DrawConstantsVertex ) ||
         !shader_FindUniformSlot( m_pixelShader, "cbDecalDraw", m_bindings.DrawConstantsPixel ) ||
         !shader_FindUniformSlot( m_pixelShader, "cbProjTextures", m_bindings.ProjectionConstantsPixel ) ||
         !shader_FindSampledTextureSlot( m_pixelShader, "txDecal", m_bindings.DecalTexturePixel ) ||
         !shader_FindSampledTextureSlot( m_pixelShader, "txEnvironmentCube",
                                         m_bindings.EnvironmentCubeTexturePixel ) ||
         !shader_FindSampledTextureSlot( m_pixelShader, "txFaceShadowAtlas", m_bindings.FaceShadowTexturePixel ) ||
         !shader_FindSampledTextureSlot( m_pixelShader, "txFaceShadowDepthAtlas",
                                         m_bindings.FaceShadowDepthTexturePixel ) ||
         !shader_FindSampledTextureSlot( m_pixelShader, "txProjectionAtlas",
                                         m_bindings.ProjectionAtlasTexturePixel ) ||
         !shader_FindStorageBufferSlot( m_pixelShader, "GeomLighting", m_bindings.LightingDataPixel ) ||
         !shader_FindStorageBufferSlot( m_pixelShader, "FaceShadowMatrices", m_bindings.ShadowMatricesPixel ) ||
         !shader_FindStorageBufferSlot( m_pixelShader, "ShadowMapData", m_bindings.ShadowDataPixel ) )
    {
        x_DebugMsg( "DecalRenderer: failed to resolve shader bindings\n" );
        return FALSE;
    }

    return TRUE;
}

//=============================================================================

xbool DecalRenderer::CreateSamplers( void )
{
    for ( s32 i = 0; i < RSTATE_SAMPLER_PRESET_COUNT; ++i )
    {
        if ( !rstate_CreateSampler( m_samplers[i], static_cast<rstate_sampler_preset>( i ), "DecalSampler" ) )
        {
            DestroySamplers();
            return FALSE;
        }
    }

    return TRUE;
}

//=============================================================================

void DecalRenderer::DestroySamplers( void )
{
    for ( s32 i = 0; i < RSTATE_SAMPLER_PRESET_COUNT; ++i )
    {
        rstate_DestroySampler( m_samplers[i] );
    }
}

//=============================================================================

xbool DecalRenderer::CreateBlackCube( void )
{
    return CreateBlackCubeTexture( m_blackCubeTexture );
}

//=============================================================================

void DecalRenderer::DestroyPipelines( void )
{
    m_pipelines.Reset();
}

//=============================================================================

xbool DecalRenderer::PrewarmPipelines( void )
{
    rstate_blend_preset const blendPresets[] = { RSTATE_BLEND_PRESET_ALPHA, RSTATE_BLEND_PRESET_ADD,
                                                 RSTATE_BLEND_PRESET_SUB, RSTATE_BLEND_PRESET_INTENSITY };
    rtarget_format const colorFormats[] = { RTARGET_FORMAT_RGBA8, RTARGET_FORMAT_RGBA16F };

    PipelineDesc desc;
    desc.Pass.SampleCount = 1;
    desc.Raster = RSTATE_RASTER_PRESET_SOLID_NO_CULL;
    desc.pDebugName = "ForwardDecalPipeline";

    for ( u32 colorIndex = 0; colorIndex < ARRAYSIZE( colorFormats ); ++colorIndex )
    {
        desc.Pass.ColorFormat = colorFormats[colorIndex];
        for ( u32 blendIndex = 0; blendIndex < ARRAYSIZE( blendPresets ); ++blendIndex )
        {
            desc.Blend = blendPresets[blendIndex];
            desc.Pass.DepthFormat = RTARGET_FORMAT_DEPTH24_STENCIL8;
            desc.Depth = RSTATE_DEPTH_PRESET_NO_WRITE;
            if ( !GetOrCreatePipeline( desc, TRUE ) )
            {
                return FALSE;
            }

            desc.Pass.DepthFormat = RTARGET_FORMAT_COUNT;
            desc.Depth = RSTATE_DEPTH_PRESET_DISABLED_NO_WRITE;
            if ( !GetOrCreatePipeline( desc, TRUE ) )
            {
                return FALSE;
            }
        }
    }

    return TRUE;
}

//=============================================================================

xbool DecalRenderer::BlendPresetFromMode( render::decal_blend_mode blend, rstate_blend_preset& preset )
{
    switch ( blend )
    {
        case render::DECAL_BLEND_ALPHA:
        {
            preset = RSTATE_BLEND_PRESET_ALPHA;
            return TRUE;
        }
        break;

        case render::DECAL_BLEND_ADDITIVE:
        {
            preset = RSTATE_BLEND_PRESET_ADD;
            return TRUE;
        }
        break;

        case render::DECAL_BLEND_SUBTRACTIVE:
        {
            preset = RSTATE_BLEND_PRESET_SUB;
            return TRUE;
        }
        break;

        case render::DECAL_BLEND_INTENSITY:
        {
            preset = RSTATE_BLEND_PRESET_INTENSITY;
            return TRUE;
        }
        break;

        default:
        {
            return FALSE;
        }
    }
}

//=============================================================================

xbool DecalRenderer::BuildSceneLighting( cb_geom_lighting& lighting, bbox const& bounds, xcolor ambient )
{
    static xprofile_counter lightingQueries =
        x_GetProfiler().RegisterCounter( "DecalLightingQueries", "RenderCounter" );
    static xprofile_counter collectedLights =
        x_GetProfiler().RegisterCounter( "DecalLights", "RenderCounter" );

    x_memset( &lighting, 0, sizeof( lighting ) );
    lighting.AmbCol.Set( static_cast<f32>( ambient.R ) / 255.0f, static_cast<f32>( ambient.G ) / 255.0f,
                         static_cast<f32>( ambient.B ) / 255.0f, 1.0f );

    s32 const lightCount = g_LightMgr.CollectLights( bounds, MAX_GEOM_LIGHTS );
    lightingQueries.Add( 1 );
    collectedLights.Add( lightCount );
    lighting.LightCount = lightCount;
    for ( s32 i = 0; i < lightCount; ++i )
    {
        vector3 position;
        f32     radius;
        xcolor  color;
        f32     falloff;
        s32     shape;
        vector3 direction;
        f32     innerConeCos;
        f32     outerConeCos;
        s32     cookieIndex;
        vector3 cookieU;
        vector3 cookieV;

        g_LightMgr.GetCollectedLightInfo( i, position, radius, color, falloff, shape, direction, innerConeCos,
                                          outerConeCos );
        g_LightMgr.GetCollectedLightCookie( i, cookieIndex, cookieU, cookieV );

        lighting.LightVec[i].Set( position.GetX(), position.GetY(), position.GetZ(), radius );
        lighting.LightCol[i].Set( static_cast<f32>( color.R ) / 255.0f, static_cast<f32>( color.G ) / 255.0f,
                                  static_cast<f32>( color.B ) / 255.0f, falloff );
        lighting.LightDir[i].Set( direction.GetX(), direction.GetY(), direction.GetZ(),
                                  ( shape == light_mgr::LIGHT_SHAPE_SPOT ) ? 1.0f : 0.0f );
        lighting.LightCone[i].Set( innerConeCos, outerConeCos, 0.0f, 0.0f );
        lighting.LightCookieU[i].Set( cookieU.GetX(), cookieU.GetY(), cookieU.GetZ(),
                                      ( cookieIndex >= 0 ) ? static_cast<f32>( cookieIndex + 1 ) : 0.0f );
        lighting.LightCookieV[i].Set( cookieV.GetX(), cookieV.GetY(), cookieV.GetZ(), 0.0f );
        lighting.DynamicLightIndex[i] = g_LightMgr.GetCollectedDynamicLightIndex( i );
    }

    return TRUE;
}

//=============================================================================

xbool DecalRenderer::Submit( render::decal_draw_desc const& desc, cubemap const* pCubeMap, Vertex const* pVertices,
                             s32 nVertices, u16 const* pIndices, s32 nIndices )
{
    if ( !m_isInitialized || m_areQueuedDrawsUploaded || !pVertices || !pIndices || !desc.pTexture ||
         ( desc.Blend < render::DECAL_BLEND_ALPHA ) || ( desc.Blend >= render::DECAL_BLEND_COUNT ) ||
         ( nVertices < 3 ) || ( nIndices < 3 ) || ( ( nIndices % 3 ) != 0 ) )
    {
        return FALSE;
    }

    if ( ( m_queuedVertices.GetCount() > ( m_queuedVertices.GetCapacity() - nVertices ) ) ||
         ( m_queuedIndices.GetCount() > ( m_queuedIndices.GetCapacity() - nIndices ) ) ||
         ( m_queuedDraws.GetCount() >= m_queuedDraws.GetCapacity() ) )
    {
        x_DebugMsg( "DecalRenderer: reserved submission capacity exhausted\n" );
        return FALSE;
    }

    for ( s32 i = 0; i < nIndices; ++i )
    {
        if ( pIndices[i] >= nVertices )
        {
            return FALSE;
        }
    }

    shader_resource const* pTexture = desc.pTexture->GetShaderResource();
    if ( !pTexture || !*pTexture )
    {
        return FALSE;
    }

    shader_resource const* pEnvironmentCube = NULL;
    if ( ( desc.Flags & render::DECAL_DRAW_FLAG_ENV_MAPPED ) && pCubeMap )
    {
        pEnvironmentCube = pCubeMap->GetShaderResource();
        if ( !pEnvironmentCube || !*pEnvironmentCube )
        {
            pEnvironmentCube = NULL;
        }
    }

    view const* pView = eng_GetView();
    if ( !pView )
    {
        return FALSE;
    }

    vector3 normal = desc.GeometricNormal;
    if ( !normal.SafeNormalize() )
    {
        return FALSE;
    }

    QueuedDraw& draw = m_queuedDraws.Append();
    draw.pTexture = pTexture;
    draw.pEnvironmentCube = pEnvironmentCube;
    draw.Blend = desc.Blend;
    draw.Flags = desc.Flags;
    draw.StartIndex = m_queuedIndices.GetCount();
    draw.BaseVertex = m_queuedVertices.GetCount();
    draw.IndexCount = nIndices;

    // Generated decal vertices lie directly on their receiver geometry. Keep
    // the legacy decal projection shift local to this draw instead of changing
    // the engine view for every later submission.
    view decalView = *pView;
    f32 nearZ;
    f32 farZ;
    decalView.GetZLimits( nearZ, farZ );
    decalView.SetZLimits( nearZ + nearZ * kDecalNearZBias, farZ );
    draw.Constants.WorldToClip = decalView.GetW2C();

    vector3 const cameraPosition = pView->GetPosition();
    draw.Constants.CameraPosition.Set( cameraPosition.GetX(), cameraPosition.GetY(), cameraPosition.GetZ(), 1.0f );
    draw.Constants.GeometricNormal.Set( normal.GetX(), normal.GetY(), normal.GetZ(), 0.0f );
    draw.Constants.Flags = desc.Flags;
    draw.Constants.BlendMode = desc.Blend;
    if ( !BuildSceneLighting( draw.SourceLighting, desc.Bounds, desc.Ambient ) )
    {
        m_queuedDraws.SetCount( m_queuedDraws.GetCount() - 1 );
        return FALSE;
    }

    for ( s32 i = 0; i < nVertices; ++i )
    {
        m_queuedVertices.Append() = pVertices[i];
    }
    for ( s32 i = 0; i < nIndices; ++i )
    {
        m_queuedIndices.Append() = pIndices[i];
    }

    return TRUE;
}

//=============================================================================

xbool DecalRenderer::RequestProjectionTextures( void ) const
{
    for ( s32 i = 0; i < m_queuedDraws.GetCount(); ++i )
    {
        cb_geom_lighting const& lighting = m_queuedDraws[i].SourceLighting;
        s32 const lightCount = MIN( lighting.LightCount, MAX_GEOM_LIGHTS );
        for ( s32 j = 0; j < lightCount; ++j )
        {
            s32 const cookieIndex = static_cast<s32>( lighting.LightCookieU[j].GetW() ) - 1;
            if ( cookieIndex < 0 )
            {
                continue;
            }
            if ( cookieIndex >= g_LightMgr.GetLightCookieCount() )
            {
                return FALSE;
            }

            texture::handle const& cookie = g_LightMgr.GetLightCookieHandle( cookieIndex );
            if ( !cookie.GetPointer() ||
                 !g_ProjectionAtlas.Request( cookie, ProjectionAtlasEncoding::LuminanceAlpha ) )
            {
                return FALSE;
            }
        }
    }

    return TRUE;
}

//=============================================================================

xbool DecalRenderer::CopyLightingData( GeomLightingConstants& destination, cb_geom_lighting const& source,
                                       GeomMgr const& geometry ) const
{
    x_memset( &destination, 0, sizeof( destination ) );
    for ( s32 i = 0; i < MAX_GEOM_LIGHTS; ++i )
    {
        destination.LightShadowIndex[i] = 0xFFFFFFFFu;
    }

    x_memcpy( destination.LightVec, source.LightVec, sizeof( destination.LightVec ) );
    x_memcpy( destination.LightCol, source.LightCol, sizeof( destination.LightCol ) );
    x_memcpy( destination.LightDir, source.LightDir, sizeof( destination.LightDir ) );
    x_memcpy( destination.LightCone, source.LightCone, sizeof( destination.LightCone ) );
    x_memcpy( destination.LightCookieU, source.LightCookieU, sizeof( destination.LightCookieU ) );
    x_memcpy( destination.LightCookieV, source.LightCookieV, sizeof( destination.LightCookieV ) );

    s32 const lightCount = MIN( source.LightCount, MAX_GEOM_LIGHTS );
    for ( s32 i = 0; i < lightCount; ++i )
    {
        destination.LightShadowIndex[i] = geometry.GetDynamicLightShadowIndex( source.DynamicLightIndex[i] );

        s32 const cookieIndex = static_cast<s32>( source.LightCookieU[i].GetW() ) - 1;
        destination.LightCookieU[i].GetW() = 0.0f;
        destination.LightCookieV[i].GetW() = 0.0f;
        if ( cookieIndex < 0 )
        {
            continue;
        }
        if ( cookieIndex >= g_LightMgr.GetLightCookieCount() )
        {
            return FALSE;
        }

        ProjectionAtlasRegion region;
        if ( !g_ProjectionAtlas.GetRegion( g_LightMgr.GetLightCookieHandle( cookieIndex ),
                                           ProjectionAtlasEncoding::LuminanceAlpha, region ) )
        {
            return FALSE;
        }

        destination.LightCookieAtlas[i] = region.m_uvScaleBias;
        destination.LightCookieLayer[i] = region.m_layer + 1;
        destination.LightCookieMaxMip[i] = region.m_maxMip;
    }

    destination.LightAmbCol = source.AmbCol;
    destination.LightCount = lightCount;
    return TRUE;
}

//=============================================================================

xbool DecalRenderer::BuildProjectionData( void )
{
    x_memset( &m_projectionConstants, 0, sizeof( m_projectionConstants ) );

    s32 const lightCount = MIN( g_ProjTextureMgr.GetProjLightCount(), ProjTextureMgr::MaxLightProjectionCount );
    for ( s32 i = 0; i < lightCount; ++i )
    {
        matrix4        projection;
        texture const* pTexture = NULL;
        g_ProjTextureMgr.GetProjLight( i, projection, pTexture );
        if ( !pTexture )
        {
            return FALSE;
        }

        ProjectionAtlasRegion region;
        if ( !g_ProjectionAtlas.GetRegion( g_ProjTextureMgr.GetProjLightTexture( i ),
                                           ProjectionAtlasEncoding::BlueMask, region ) )
        {
            return FALSE;
        }

        s32 const destination = m_projectionConstants.ProjLightCount++;
        m_projectionConstants.ProjLightMatrix[destination] = projection;
        m_projectionConstants.ProjLightAtlas[destination] = region.m_uvScaleBias;
        m_projectionConstants.ProjLightInfo[destination].Set( static_cast<f32>( region.m_layer ), region.m_maxMip,
                                                              0.0f, 0.0f );
    }

    s32 const shadowCount = MIN( g_ProjTextureMgr.GetProjShadowCount(), ProjTextureMgr::MaxShadowProjectionCount );
    for ( s32 i = 0; i < shadowCount; ++i )
    {
        matrix4        projection;
        texture const* pTexture = NULL;
        g_ProjTextureMgr.GetProjShadow( i, projection, pTexture );
        if ( !pTexture )
        {
            return FALSE;
        }

        ProjectionAtlasRegion region;
        if ( !g_ProjectionAtlas.GetRegion( g_ProjTextureMgr.GetProjShadowTexture( i ),
                                           ProjectionAtlasEncoding::BlueMask, region ) )
        {
            return FALSE;
        }

        s32 const destination = m_projectionConstants.ProjShadowCount++;
        m_projectionConstants.ProjShadowMatrix[destination] = projection;
        m_projectionConstants.ProjShadowAtlas[destination] = region.m_uvScaleBias;
        m_projectionConstants.ProjShadowInfo[destination].Set( static_cast<f32>( region.m_layer ), region.m_maxMip,
                                                               0.0f, 0.0f );
    }

    return TRUE;
}

//=============================================================================

xbool DecalRenderer::UploadQueuedDraws( GeomMgr const& geometry )
{
    if ( m_areQueuedDrawsUploaded )
    {
        x_DebugMsg( "DecalRenderer: queued draws were uploaded more than once\n" );
        return FALSE;
    }
    if ( m_queuedDraws.GetCount() == 0 )
    {
        return TRUE;
    }
    if ( ( m_frameLighting.GetCapacity() < ( m_queuedDraws.GetCount() + 1 ) ) ||
         ( m_lightingCapacity < static_cast<u32>( m_queuedDraws.GetCount() + 1 ) ) )
    {
        x_DebugMsg( "DecalRenderer: insufficient lighting capacity draws=%d cpu=%d gpu=%u\n",
                    m_queuedDraws.GetCount(), m_frameLighting.GetCapacity(), m_lightingCapacity );
        return FALSE;
    }

    m_frameLighting.SetCount( 0 );
    GeomLightingConstants& emptyLighting = m_frameLighting.Append();
    x_memset( &emptyLighting, 0, sizeof( emptyLighting ) );

    for ( s32 i = 0; i < m_queuedDraws.GetCount(); ++i )
    {
        QueuedDraw& draw = m_queuedDraws[i];
        GeomLightingConstants& lighting = m_frameLighting.Append();
        if ( !CopyLightingData( lighting, draw.SourceLighting, geometry ) )
        {
            x_DebugMsg( "DecalRenderer: failed to copy lighting data packet=%d ambient=%.3f,%.3f,%.3f\n", i,
                        draw.SourceLighting.AmbCol.GetX(), draw.SourceLighting.AmbCol.GetY(),
                        draw.SourceLighting.AmbCol.GetZ() );
            return FALSE;
        }
        draw.Constants.LightingIndex = static_cast<u32>( i + 1 );
    }

    RuntimeVertexMgr::primitive primitive;
    primitive.pVertex = m_queuedVertices.GetPtr();
    primitive.pIndex = m_queuedIndices.GetPtr();
    primitive.nVertices = m_queuedVertices.GetCount();
    primitive.nIndices = m_queuedIndices.GetCount();

    u32 const lightingBytes = static_cast<u32>( sizeof( GeomLightingConstants ) * m_frameLighting.GetCount() );
    if ( !BuildProjectionData() )
    {
        x_DebugMsg( "DecalRenderer: failed to build projection data\n" );
        return FALSE;
    }
    if ( !rbuffer_Upload( m_lightingBuffer, m_frameLighting.GetPtr(), lightingBytes, 0, TRUE ) )
    {
        x_DebugMsg( "DecalRenderer: failed to upload lighting buffer records=%d bytes=%u\n",
                    m_frameLighting.GetCount(), lightingBytes );
        return FALSE;
    }
    if ( !m_vertexMgr.UploadPrimitive( primitive ) )
    {
        x_DebugMsg( "DecalRenderer: failed to upload vertices=%d indices=%d\n",
                    primitive.nVertices, primitive.nIndices );
        return FALSE;
    }

    static xprofile_counter decalDraws =
        x_GetProfiler().RegisterCounter( "DecalDraws", "RenderCounter" );
    static xprofile_counter decalSpans =
        x_GetProfiler().RegisterCounter( "DecalSpans", "RenderCounter" );
    static xprofile_counter decalUploadBytes =
        x_GetProfiler().RegisterCounter( "DecalUploadBytes", "RenderCounter" );
    decalDraws.Add( m_queuedDraws.GetCount() );
    decalSpans.Add( m_queuedDraws.GetCount() );
    decalUploadBytes.Add( lightingBytes +
                          static_cast<u32>( m_queuedVertices.GetCount() * sizeof( Vertex ) ) +
                          static_cast<u32>( m_queuedIndices.GetCount() * sizeof( u16 ) ) );

    m_areQueuedDrawsUploaded = TRUE;
    return TRUE;
}

//=============================================================================

xbool DecalRenderer::ValidatePass( PipelineDesc const& desc )
{
    if ( ( static_cast<u32>( desc.Pass.ColorFormat ) >= static_cast<u32>( RTARGET_FORMAT_COUNT ) ) ||
         ( static_cast<u32>( desc.Pass.DepthFormat ) > static_cast<u32>( RTARGET_FORMAT_COUNT ) ) ||
         ( ( desc.Pass.SampleCount != 1 ) && ( desc.Pass.SampleCount != 2 ) && ( desc.Pass.SampleCount != 4 ) &&
           ( desc.Pass.SampleCount != 8 ) ) ||
         ( desc.Blend < 0 ) || ( desc.Blend >= RSTATE_BLEND_PRESET_COUNT ) ||
         ( desc.Depth < 0 ) || ( desc.Depth >= RSTATE_DEPTH_PRESET_COUNT ) ||
         ( desc.Raster < 0 ) || ( desc.Raster >= RSTATE_RASTER_PRESET_COUNT ) )
    {
        return FALSE;
    }

    if ( ( desc.Pass.DepthFormat == RTARGET_FORMAT_COUNT ) && ( desc.Depth == RSTATE_DEPTH_PRESET_NO_WRITE ) )
    {
        return FALSE;
    }

    return TRUE;
}

//=============================================================================

u64 DecalRenderer::MakePipelineKey( PipelineDesc const& desc )
{
    return static_cast<u64>( static_cast<u8>( desc.Pass.ColorFormat ) ) |
           ( static_cast<u64>( static_cast<u8>( desc.Pass.DepthFormat ) ) << 8 ) |
           ( static_cast<u64>( static_cast<u8>( desc.Pass.SampleCount ) ) << 16 ) |
           ( static_cast<u64>( static_cast<u8>( desc.Blend ) ) << 24 ) |
           ( static_cast<u64>( static_cast<u8>( desc.Depth ) ) << 32 ) |
           ( static_cast<u64>( static_cast<u8>( desc.Raster ) ) << 40 );
}

//=============================================================================

xbool DecalRenderer::BuildPipelineDesc( render_pipeline_desc& out, PipelineDesc const& desc ) const
{
    if ( !ValidatePass( desc ) )
    {
        return FALSE;
    }

    shader_vertex_buffer_desc vertexBuffer;
    vertexBuffer.Slot = 0;
    vertexBuffer.Stride = sizeof( Vertex );

    shader_vertex_element const layout[] = {
        shader_vertex_element( 0, 0, SHADER_VERTEX_FORMAT_FLOAT3, offsetof( Vertex, Position ) ),
        shader_vertex_element( 1, 0, SHADER_VERTEX_FORMAT_UBYTE4N_BGRA, offsetof( Vertex, Color ) ),
        shader_vertex_element( 2, 0, SHADER_VERTEX_FORMAT_FLOAT2, offsetof( Vertex, UV ) ) };

    out = render_pipeline_desc();
    out.Shader.pVertexShader = &m_vertexShader;
    out.Shader.pPixelShader = &m_pixelShader;
    out.Shader.pVertexBuffers = &vertexBuffer;
    out.Shader.VertexBufferCount = 1;
    out.Shader.pInputElements = layout;
    out.Shader.InputElementCount = ARRAYSIZE( layout );
    out.Shader.Topology = SHADER_TOPOLOGY_TRIANGLE_LIST;
    out.Depth = rstate_GetDepthDesc( desc.Depth );
    out.Raster = rstate_GetRasterDesc( desc.Raster );
    out.ColorCount = 1;
    out.DepthFormat = desc.Pass.DepthFormat;
    out.SampleCount = desc.Pass.SampleCount;
    out.pDebugName = desc.pDebugName ? desc.pDebugName : "DecalPipeline";
    out.ColorTargets[0].Format = desc.Pass.ColorFormat;
    out.ColorTargets[0].Blend = rstate_GetBlendDesc( desc.Blend );
    return TRUE;
}

//=============================================================================

render_pipeline* DecalRenderer::GetOrCreatePipeline( PipelineDesc const& desc, xbool isPrewarm )
{
    render_pipeline_desc pipelineDesc;
    if ( !BuildPipelineDesc( pipelineDesc, desc ) )
    {
        return NULL;
    }

    u64 const key = MakePipelineKey( desc );
    return isPrewarm ? m_pipelines.Prewarm( key, pipelineDesc ) : m_pipelines.GetOrCreate( key, pipelineDesc );
}

//=============================================================================

xbool DecalRenderer::BindPipeline( PipelineDesc const& desc )
{
    render_pipeline* pPipeline = GetOrCreatePipeline( desc );
    return pPipeline && render_BindPipeline( *pPipeline );
}

//=============================================================================

xbool DecalRenderer::BindResources( QueuedDraw const& draw, s32 drawIndex, xbool isGlowPass )
{
    if ( ( draw.Constants.LightingIndex == 0 ) ||
         ( draw.Constants.LightingIndex >= static_cast<u32>( m_frameLighting.GetCount() ) ) )
    {
        x_DebugMsg( "DecalRenderer: invalid lighting record packet=%d index=%u count=%d pass=%s\n",
                    drawIndex, draw.Constants.LightingIndex, m_frameLighting.GetCount(),
                    isGlowPass ? "glow" : "scene" );
        return FALSE;
    }

    shader_resource const* pEnvironmentCube = draw.pEnvironmentCube;
    if ( !pEnvironmentCube )
    {
        pEnvironmentCube = vram_GetShaderResource( m_blackCubeTexture );
    }

    shader_resource const* pProjectionAtlas = g_ProjectionAtlas.GetShaderResource();
    shader_resource const* pLighting = rbuffer_GetResource( m_lightingBuffer );
    shader_resource const* pShadowMatrices = g_GeomMgr.GetShadowMatricesResource();
    shader_resource const* pShadowData = g_GeomMgr.GetShadowDataResource();
    shader_resource const* pFaceShadow =
        isGlowPass ? draw.pTexture : g_GeomMgr.GetFaceShadowResource();
    shader_resource const* pFaceShadowDepth =
        isGlowPass ? draw.pTexture : g_GeomMgr.GetFaceShadowDepthResource();
    rstate_sampler_preset const faceShadowSampler =
        isGlowPass ? RSTATE_SAMPLER_PRESET_LINEAR_CLAMP : g_GeomMgr.GetFaceShadowSampler();
    if ( !draw.pTexture )
    {
        x_DebugMsg( "DecalRenderer: missing decal texture packet=%d pass=%s\n",
                    drawIndex, ( isGlowPass ? "glow" : "scene" ) );
        return FALSE;
    }
    if ( !pEnvironmentCube )
    {
        x_DebugMsg( "DecalRenderer: missing environment cube packet=%d pass=%s\n",
                    drawIndex, ( isGlowPass ? "glow" : "scene" ) );
        return FALSE;
    }
    if ( !pProjectionAtlas )
    {
        x_DebugMsg( "DecalRenderer: missing projection atlas packet=%d pass=%s\n",
                    drawIndex, ( isGlowPass ? "glow" : "scene" ) );
        return FALSE;
    }
    if ( !pLighting )
    {
        x_DebugMsg( "DecalRenderer: missing lighting buffer packet=%d pass=%s\n",
                    drawIndex, ( isGlowPass ? "glow" : "scene" ) );
        return FALSE;
    }
    if ( !pShadowMatrices )
    {
        x_DebugMsg( "DecalRenderer: missing shadow matrices packet=%d pass=%s\n",
                    drawIndex, ( isGlowPass ? "glow" : "scene" ) );
        return FALSE;
    }
    if ( !pShadowData )
    {
        x_DebugMsg( "DecalRenderer: missing shadow data packet=%d pass=%s\n",
                    drawIndex, ( isGlowPass ? "glow" : "scene" ) );
        return FALSE;
    }
    if ( !pFaceShadow )
    {
        x_DebugMsg( "DecalRenderer: missing face shadow atlas packet=%d pass=%s\n",
                    drawIndex, ( isGlowPass ? "glow" : "scene" ) );
        return FALSE;
    }
    if ( !pFaceShadowDepth )
    {
        x_DebugMsg( "DecalRenderer: missing face shadow depth atlas packet=%d pass=%s\n",
                    drawIndex, ( isGlowPass ? "glow" : "scene" ) );
        return FALSE;
    }
    if ( ( faceShadowSampler < 0 ) || ( faceShadowSampler >= RSTATE_SAMPLER_PRESET_COUNT ) )
    {
        x_DebugMsg( "DecalRenderer: invalid face shadow sampler packet=%d sampler=%d pass=%s\n",
                    drawIndex, static_cast<s32>( faceShadowSampler ), ( isGlowPass ? "glow" : "scene" ) );
        return FALSE;
    }

    DrawConstants constants = draw.Constants;
    constants.OutputMode = isGlowPass ? 1u : 0u;
    if ( !shader_PushUniformData( SHADER_STAGE_VERTEX, m_bindings.DrawConstantsVertex, &constants,
                                  sizeof( constants ) ) )
    {
        x_DebugMsg( "DecalRenderer: failed to push vertex constants packet=%d pass=%s\n",
                    drawIndex, ( isGlowPass ? "glow" : "scene" ) );
        return FALSE;
    }
    if ( !shader_PushUniformData( SHADER_STAGE_PIXEL, m_bindings.DrawConstantsPixel, &constants,
                                  sizeof( constants ) ) )
    {
        x_DebugMsg( "DecalRenderer: failed to push pixel constants packet=%d pass=%s\n",
                    drawIndex, ( isGlowPass ? "glow" : "scene" ) );
        return FALSE;
    }
    if ( !shader_PushUniformData( SHADER_STAGE_PIXEL, m_bindings.ProjectionConstantsPixel, &m_projectionConstants,
                                  sizeof( m_projectionConstants ) ) )
    {
        x_DebugMsg( "DecalRenderer: failed to push projection constants packet=%d pass=%s\n",
                    drawIndex, ( isGlowPass ? "glow" : "scene" ) );
        return FALSE;
    }

    shader_sampler_binding const samplers[] = {
        shader_sampler_binding( SHADER_STAGE_PIXEL, m_bindings.DecalTexturePixel, draw.pTexture,
                                &m_samplers[RSTATE_SAMPLER_PRESET_LINEAR_CLAMP] ),
        shader_sampler_binding( SHADER_STAGE_PIXEL, m_bindings.EnvironmentCubeTexturePixel, pEnvironmentCube,
                                &m_samplers[RSTATE_SAMPLER_PRESET_LINEAR_CLAMP] ),
        shader_sampler_binding( SHADER_STAGE_PIXEL, m_bindings.FaceShadowTexturePixel, pFaceShadow,
                                &m_samplers[faceShadowSampler] ),
        shader_sampler_binding( SHADER_STAGE_PIXEL, m_bindings.FaceShadowDepthTexturePixel, pFaceShadowDepth,
                                &m_samplers[RSTATE_SAMPLER_PRESET_POINT_CLAMP] ),
        shader_sampler_binding( SHADER_STAGE_PIXEL, m_bindings.ProjectionAtlasTexturePixel, pProjectionAtlas,
                                &m_samplers[RSTATE_SAMPLER_PRESET_LINEAR_CLAMP] ) };
    char const* const samplerNames[] = {
        "decal", "environment", "face-shadow", "face-shadow-depth", "projection-atlas"
    };
    for ( u32 i = 0; i < ARRAYSIZE( samplers ); ++i )
    {
        if ( !shader_BindSampler( samplers[i] ) )
        {
            x_DebugMsg( "DecalRenderer: failed to bind %s sampler packet=%d slot=%u pass=%s\n",
                        samplerNames[i], drawIndex, samplers[i].Slot, ( isGlowPass ? "glow" : "scene" ) );
            return FALSE;
        }
    }

    shader_storage_buffer_binding const storage[] = {
        shader_storage_buffer_binding( SHADER_STAGE_PIXEL, m_bindings.LightingDataPixel, pLighting ),
        shader_storage_buffer_binding( SHADER_STAGE_PIXEL, m_bindings.ShadowMatricesPixel, pShadowMatrices ),
        shader_storage_buffer_binding( SHADER_STAGE_PIXEL, m_bindings.ShadowDataPixel, pShadowData ) };
    char const* const storageNames[] = { "lighting", "shadow-matrices", "shadow-data" };
    for ( u32 i = 0; i < ARRAYSIZE( storage ); ++i )
    {
        if ( !shader_BindStorageBuffer( storage[i] ) )
        {
            x_DebugMsg( "DecalRenderer: failed to bind %s buffer packet=%d slot=%u pass=%s\n",
                        storageNames[i], drawIndex, storage[i].Slot, ( isGlowPass ? "glow" : "scene" ) );
            return FALSE;
        }
    }

    return TRUE;
}

//=============================================================================

xbool DecalRenderer::DrawQueuedDraw( s32 drawIndex, PassDesc const& pass )
{
    if ( !m_areQueuedDrawsUploaded || ( drawIndex < 0 ) || ( drawIndex >= m_queuedDraws.GetCount() ) )
    {
        x_DebugMsg( "DecalRenderer: invalid queued draw packet=%d count=%d uploaded=%d pass=%s\n",
                    drawIndex, m_queuedDraws.GetCount(), m_areQueuedDrawsUploaded,
                    ( pass.IsGlowPass ? "glow" : "scene" ) );
        return FALSE;
    }

    QueuedDraw const& draw = m_queuedDraws[drawIndex];
    PipelineDesc pipeline;
    pipeline.Pass = pass;
    pipeline.Depth = ( pass.DepthFormat == RTARGET_FORMAT_COUNT )
                         ? RSTATE_DEPTH_PRESET_DISABLED_NO_WRITE
                         : RSTATE_DEPTH_PRESET_NO_WRITE;
    pipeline.Raster = RSTATE_RASTER_PRESET_SOLID_NO_CULL;
    pipeline.pDebugName = "ForwardDecalPipeline";
    if ( pass.IsGlowPass )
    {
        pipeline.Blend = RSTATE_BLEND_PRESET_ADD;
    }
    else if ( !BlendPresetFromMode( draw.Blend, pipeline.Blend ) )
    {
        x_DebugMsg( "DecalRenderer: invalid blend mode packet=%d blend=%d pass=%s\n",
                    drawIndex, static_cast<s32>( draw.Blend ), ( pass.IsGlowPass ? "glow" : "scene" ) );
        return FALSE;
    }

    if ( !m_vertexMgr.BindBuffers() )
    {
        x_DebugMsg( "DecalRenderer: failed to bind vertex/index buffers packet=%d pass=%s\n",
                    drawIndex, ( pass.IsGlowPass ? "glow" : "scene" ) );
        return FALSE;
    }
    if ( !BindPipeline( pipeline ) )
    {
        x_DebugMsg(
            "DecalRenderer: failed to bind pipeline packet=%d color=%d depth=%d samples=%u blend=%d pass=%s\n",
            drawIndex, static_cast<s32>( pipeline.Pass.ColorFormat ), static_cast<s32>( pipeline.Pass.DepthFormat ),
            pipeline.Pass.SampleCount, static_cast<s32>( pipeline.Blend ), ( pass.IsGlowPass ? "glow" : "scene" ) );
        return FALSE;
    }
    if ( !BindResources( draw, drawIndex, pass.IsGlowPass ) )
    {
        return FALSE;
    }
    if ( !m_vertexMgr.DrawIndexed( draw.IndexCount, draw.StartIndex, draw.BaseVertex ) )
    {
        x_DebugMsg( "DecalRenderer: indexed draw failed packet=%d indices=%d start=%d base=%d pass=%s\n",
                    drawIndex, draw.IndexCount, draw.StartIndex, draw.BaseVertex,
                    ( pass.IsGlowPass ? "glow" : "scene" ) );
        return FALSE;
    }

    return TRUE;
}

//=============================================================================

void DecalRenderer::DiscardQueuedDraws( void )
{
    m_frameLighting.SetCount( 0 );
    m_queuedVertices.SetCount( 0 );
    m_queuedIndices.SetCount( 0 );
    m_queuedDraws.SetCount( 0 );
    m_areQueuedDrawsUploaded = FALSE;
    x_memset( &m_projectionConstants, 0, sizeof( m_projectionConstants ) );
}

//=============================================================================

s32 DecalRenderer::GetQueuedDrawCount( void ) const
{
    return m_queuedDraws.GetCount();
}

//=============================================================================

xbool DecalRenderer::IsQueuedDrawGlow( s32 drawIndex ) const
{
    ASSERT( ( drawIndex >= 0 ) && ( drawIndex < m_queuedDraws.GetCount() ) );
    return ( m_queuedDraws[drawIndex].Flags & render::DECAL_DRAW_FLAG_GLOW ) != 0;
}

//=============================================================================
