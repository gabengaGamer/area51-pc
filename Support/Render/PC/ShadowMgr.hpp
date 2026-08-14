//==============================================================================
//
//  ShadowMgr.hpp
//
//  Shadow-map manager for the PC platform.
//
//==============================================================================

#ifndef PC_SHADOW_MGR_HPP
#define PC_SHADOW_MGR_HPP

//==============================================================================
//  BASE INCLUDES
//==============================================================================

#include "x_types.hpp"
#include "x_array.hpp"

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_math.hpp"

#include "../ShadowMapMgr.hpp"
#include "../RenderPipelineCache.hpp"

#include "e_Engine.hpp"

class material;
class texture;
struct geometry_draw_item;
struct dynamic_geometry_shadow_draw;

//==============================================================================
//  CONSTANT BUFFER LAYOUTS
//==============================================================================

struct cb_shadow_cast
{
    matrix4 ShadowViewProjection;
    u32     Padding[4];
};

static_assert( sizeof( cb_shadow_cast ) == 80, "cbShadowCast layout must match HLSL" );

//------------------------------------------------------------------------------

struct cb_shadow_alpha
{
    f32 AlphaRef;
    f32 Padding;
    f32 UVOffset[2];
};

static_assert( sizeof( cb_shadow_alpha ) == 16, "cbShadowAlpha layout must match HLSL" );

//------------------------------------------------------------------------------

struct cb_shadow_filter
{
    vector4 TextureParams;          // xy = input texel size, zw = output texel size
    vector4 SourceClampRect;        // xy = minimum source texel center, zw = maximum source texel center
    vector4 FilterParams;           // x/y = EVSM exponents, z = EVSM blur scale
    vector4 DepthParams;            // x/y = projection near/far, z = light radius, w = point-face flag
    vector4 SourceProjectionParams; // xy = source UV center, z = UV-to-NDC scale, w = corner coverage cosine
};

static_assert( sizeof( cb_shadow_filter ) == 80, "cbShadowFilter layout must match HLSL" );

//------------------------------------------------------------------------------

struct cb_shadow_map_data
{
    vector4 FaceShadowLightPosRadius[MAX_SHADOW_SOURCES];
    vector4 FaceShadowLightDirFalloff[MAX_SHADOW_SOURCES];
    vector4 FaceShadowLightData[MAX_SHADOW_SOURCES]; // x = cone/coverage cosine, y = receive near z, z = far z, w = 0
                                                     // spot / 1 point face
    vector4 FaceShadowProjectionData[MAX_SHADOW_SOURCES]; // x/y = projection near/far z
    vector4 FaceShadowAtlasRect[MAX_SHADOW_SOURCES]; // xy/zw = moment-texel inset minimum/maximum
    vector4 PointShadowLightPosRadius[MAX_SHADOW_LIGHTS];
    vector4 PointShadowLightData[MAX_SHADOW_LIGHTS];   // x = falloff, y = receive near z, z = far z
    vector4 PointShadowLightParams[MAX_SHADOW_LIGHTS]; // x = first face source, y = face count
    u32     FaceShadowCount;
    u32     PointShadowLightCount;
    f32     Padding[2];
    vector4 ShadowParams;       // x = depth-atlas texel size, y = normal bias, z = seam blend width, w = filter type
    vector4 ShadowFilterParams; // x = sampled-atlas texel size, y/z = EVSM exponents, w = light-bleed reduction
    vector4 ShadowContactParams; // x = EVSM filter radius in moment texels
};

static_assert( sizeof( cb_shadow_map_data ) == 5568, "ShadowMapData layout must match HLSL" );

//==============================================================================
//  SHADOW MANAGER
//==============================================================================

class shadow_mgr
{
public:
    struct caster_stats
    {
        u32 InputDrawCount;
        u32 PreparedDrawCount;
        u32 PacketCount;
        u32 GpuDrawCount;
        u32 RigidInstanceCount;
        u32 SkinInstanceCount;
        u32 RigidPacketCount;
        u32 SkinPacketCount;
        u32 AlphaPacketCount;
        u32 SourceStateChangeCount;
        u32 SkinPaletteCount;
        u32 SkinPaletteMatrixCount;
        u32 MaxPacketInstanceCount;
        u32 RigidIndirectRunCount;
        u32 RigidIndirectCommandCount;
        u32 SkinIndirectRunCount;
        u32 SkinIndirectCommandCount;
        u32 BufferReallocationCount;
        u64 UploadBytes;
        u64 SubmittedIndexCount;

        caster_stats( void )
            : InputDrawCount( 0 ), PreparedDrawCount( 0 ), PacketCount( 0 ), GpuDrawCount( 0 ), RigidInstanceCount( 0 ),
              SkinInstanceCount( 0 ), RigidPacketCount( 0 ), SkinPacketCount( 0 ), AlphaPacketCount( 0 ),
              SourceStateChangeCount( 0 ), SkinPaletteCount( 0 ), SkinPaletteMatrixCount( 0 ),
              MaxPacketInstanceCount( 0 ), RigidIndirectRunCount( 0 ), RigidIndirectCommandCount( 0 ),
              SkinIndirectRunCount( 0 ), SkinIndirectCommandCount( 0 ), BufferReallocationCount( 0 ), UploadBytes( 0 ),
              SubmittedIndexCount( 0 )
        {
        }
    };

    //--------------------------------------------------------------------------
    // Lifetime
    //--------------------------------------------------------------------------

    shadow_mgr  ( void );
    ~shadow_mgr ( void );

    void Init ( void );
    void Kill ( void );

    //--------------------------------------------------------------------------
    // Shadow Caster Pipeline
    //--------------------------------------------------------------------------

    void BeginShadowShaders ( void );
    void EndShadowShaders   ( void );
    void BeginCastPass      ( void );
    void EndCastPass        ( void );
    void RenderCasters      ( xarray<geometry_draw_item const*> const& draws,
                              xarray<dynamic_geometry_shadow_draw> const& dynamicDraws );

    //--------------------------------------------------------------------------
    // Runtime Queries
    //--------------------------------------------------------------------------

    rtarget const*      GetShadowSampleAtlasTarget ( void ) const;
    rtarget const*      GetShadowDepthAtlasTarget  ( void ) const;
    f32                 GetShadowNormalBiasTexels  ( void ) const;
    f32                 GetShadowSeamBlendTexels   ( void ) const;
    f32                 GetAtlasTexelSize          ( void ) const;
    vector4             GetShadowFilterParams      ( void ) const;
    caster_stats const& GetLastCasterStats         ( void ) const;

private:
    //--------------------------------------------------------------------------
    // Internal Helpers
    //--------------------------------------------------------------------------

    void  EnsureAtlas             ( void );
    xbool FilterShadowAtlas       ( void );
    xbool RunEVSMBlurPass         ( rtarget const& source, rtarget const& destination, xbool horizontal );
    xbool SetShadowCastConstants  ( u32 uniformSlot, matrix4 const& shadowViewProjection );
    xbool SetShadowAlphaConstants ( texture const& diffuseTexture, xbool bPunchThru, u8 uOffset, u8 vOffset );
    void  ApplySource             ( s32 sourceIndex, s32 casterShader, xbool bAlphaTest, xbool bTwoSided );
    void  ApplyDynamicSource      ( s32 sourceIndex );

private:
    //--------------------------------------------------------------------------
    // Runtime State
    //--------------------------------------------------------------------------

    xbool        m_isInitialized;
    xbool        m_isCastPassActive;
    s32          m_currentSource;
    s32          m_currentCasterVariant;
    s32          m_shadowAtlasSize;
    s32          m_shadowMomentAtlasSize;
    xbool        m_isShadowSampleAtlasReady;
    caster_stats m_lastCasterStats;

    //--------------------------------------------------------------------------
    // GPU Resources
    //--------------------------------------------------------------------------

    rtarget             m_shadowAtlas;
    rtarget             m_shadowMomentAtlas;
    rtarget             m_shadowMomentTempAtlas;
    shader              m_rigidVertexShader;
    shader              m_skinVertexShader;
    shader              m_dynamicVertexShader;
    shader              m_dynamicPixelShader;
    shader              m_opaqueDepthPixelShader;
    shader              m_alphaDepthPixelShader;
    shader              m_evsmVertexShader;
    shader              m_evsmConvertPixelShader;
    shader              m_evsmBlurHorizontalPixelShader;
    shader              m_evsmBlurVerticalPixelShader;
    RenderPipelineCache m_casterPipelines;
    RenderPipelineCache m_evsmPipelines;
    rstate_sampler      m_diffuseSampler;
    rstate_sampler      m_evsmPointSampler;
    rstate_sampler      m_evsmLinearSampler;
    rstate_raster_desc  m_shadowSingleSidedRasterizerDesc;
    rstate_raster_desc  m_shadowTwoSidedRasterizerDesc;
    rbuffer             m_rigidInstanceBuffer;
    rbuffer             m_rigidInstanceIndexBuffer;
    rbuffer             m_rigidIndirectBuffer;
    rbuffer             m_skinBoneBuffer;
    rbuffer             m_skinBoneBaseBuffer;
    rbuffer             m_skinIndirectBuffer;
    rbuffer             m_dynamicVertexBuffer;
    rbuffer             m_dynamicIndexBuffer;
    u32                 m_rigidInstanceCapacity;
    u32                 m_rigidInstanceIndexCapacity;
    u32                 m_rigidIndirectCapacity;
    u32                 m_skinBoneCapacity;
    u32                 m_skinBoneBaseCapacity;
    u32                 m_skinIndirectCapacity;
    u32                 m_dynamicVertexCapacity;
    u32                 m_dynamicIndexCapacity;

    //--------------------------------------------------------------------------
    // Reflected shader slots
    //--------------------------------------------------------------------------

    u32 m_rigidCastConstantsSlot;
    u32 m_rigidInstanceSlot;
    u32 m_skinCastConstantsSlot;
    u32 m_skinBoneSlot;
    u32 m_dynamicCastConstantsSlot;
    u32 m_dynamicDiffuseTextureSlot;
    u32 m_dynamicDamageTextureSlot;
    u32 m_alphaConstantsSlot;
    u32 m_alphaTextureSlot;
    u32 m_evsmConvertConstantsSlot;
    u32 m_evsmConvertTextureSlot;
    u32 m_evsmBlurHorizontalConstantsSlot;
    u32 m_evsmBlurHorizontalTextureSlot;
    u32 m_evsmBlurVerticalConstantsSlot;
    u32 m_evsmBlurVerticalTextureSlot;

    //--------------------------------------------------------------------------
    // Shadow Tuning
    //--------------------------------------------------------------------------

    f32 m_shadowNormalBiasTexels;
    f32 m_shadowSeamBlendTexels;
};

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

extern shadow_mgr g_ShadowMgr;

//==============================================================================
//  END
//==============================================================================

#endif
