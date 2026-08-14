//==============================================================================
//
//  ShadowMapMgr.hpp
//
//  Shadow-map manager interface for PC shadow sources.
//
//==============================================================================

#ifndef SHADOW_MAP_MGR_HPP
#define SHADOW_MAP_MGR_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_math.hpp"

#include "geom.hpp"
#include "Material.hpp"
#include "ShadowTypes.hpp"

//==============================================================================
//  CONSTANTS
//==============================================================================

enum
{
    POINT_SHADOW_FACE_COUNT = 6,
    // NOTE: GS: Keep in mind that they all must be a power of two!
    MAX_SHADOW_LIGHTS = 8,
    MAX_SHADOW_SOURCES = 64,
    MAX_SHADOW_ATLAS_SIZE = 16384,
    MAX_SHADOW_MOMENT_ATLAS_SIZE = 2048,
    SHADOW_EVSM_BLUR_RADIUS = 4,
};

class object;

//==============================================================================
//  SHADOW MAP MANAGER
//==============================================================================

class ShadowMapMgr
{
  public:
    //--------------------------------------------------------------------------
    // Source Types
    //--------------------------------------------------------------------------

    enum SourceType
    {
        SHADOW_SOURCE_POINT_FACE = 0,
        SHADOW_SOURCE_SPOT,
    };

    //--------------------------------------------------------------------------
    // Source Description
    //--------------------------------------------------------------------------

    struct ShadowSource
    {
        s32     Type;
        s32     DynamicLightIndex;
        s32     PointLightIndex;
        s32     FaceIndex;
        s32     RequestedResolution;
        s32     ShadowPriority;
        f32     ShadowScore;
        matrix4 WorldToClip;
        matrix4 WorldToAtlas;
        vector4 LightPosRadius;
        vector4 FaceLightDirFalloff;
        vector4 FaceLightData;
        f32     LightFalloff;
        f32     NearZ;
        f32     ReceiveNearZ;
        f32     FarZ;
        bbox    WorldBBox;
        s32     AtlasX;
        s32     AtlasY;
        s32     AtlasWidth;
        s32     AtlasHeight;
        // Original packed tile retained for EVSM guard replication.
        s32     AtlasTileX;
        s32     AtlasTileY;
        s32     AtlasTileWidth;
        s32     AtlasTileHeight;
    };

    //--------------------------------------------------------------------------
    // Lifetime
    //--------------------------------------------------------------------------

    ShadowMapMgr( void );
    ~ShadowMapMgr( void );

    //--------------------------------------------------------------------------
    // Source Construction
    //--------------------------------------------------------------------------

    void  ClearSources        ( void );
    void  FinalizeSources     ( void );
    void  SetEnabled          ( xbool Enabled );
    void  SetShadowFilterType ( ShadowFilterType Type );
    xbool AddPointSource( matrix4 const& l2W, radian fov, f32 lightRadius, f32 lightFalloff, s32 shadowMapResolution,
                          s32 shadowPriority, f32 shadowScore, s32 dynamicLightIndex );
    xbool AddSpotSource( matrix4 const& l2W, radian fov, f32 lightRadius, f32 lightFalloff, s32 shadowMapResolution,
                         s32 shadowPriority, f32 shadowScore, s32 dynamicLightIndex );

    //--------------------------------------------------------------------------
    // Source Queries
    //--------------------------------------------------------------------------

    ShadowSource const& GetSource( s32 sourceIndex ) const;
    s32                 GetSourceCount( void ) const;
    s32                 GetAtlasSize( void ) const;
    s32                 GetAtlasSourceCount( void ) const;
    ShadowFilterType    GetShadowFilterType( void ) const;
    xbool               HasActiveSources( void ) const;

    //--------------------------------------------------------------------------
    // Shadow Map Build
    //--------------------------------------------------------------------------

    void CreateShadowMap( object* const* pPCasterCandidates, s32 nCasterCandidates );

  private:
    struct ScratchData;

    ShadowMapMgr( ShadowMapMgr const& );
    ShadowMapMgr& operator=( ShadowMapMgr const& );

    //--------------------------------------------------------------------------
    // Internal Helpers
    //--------------------------------------------------------------------------

    void ComputePerspectiveSource( ShadowSource& dest, s32 sourceType, matrix4 const& l2W, radian fov, f32 lightRadius,
                                   f32 lightFalloff, s32 shadowMapResolution ) const;
    ScratchData& GetScratch( void );
    void         UpdateAtlasLayout( void );

  private:
    //--------------------------------------------------------------------------
    // Manager State
    //--------------------------------------------------------------------------

    ScratchData*     m_pScratch;
    s32              m_sourceCount;
    s32              m_pointFaceCount;
    s32              m_pointLightCount;
    s32              m_atlasSourceCount;
    s32              m_atlasSize;
    s32              m_atlasSizeFloor;
    xbool            m_atlasLayoutDirty;
    xbool            m_enabled;
    ShadowFilterType m_shadowFilterType;
    ShadowSource     m_sources[MAX_SHADOW_SOURCES];
};

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

extern ShadowMapMgr g_ShadowMapMgr;

//==============================================================================
#endif // SHADOW_MAP_MGR_HPP
//==============================================================================
