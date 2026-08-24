//=========================================================================
//
//  ShadowMapMgr.hpp
//
//=========================================================================

#ifndef SHADOW_MAP_MGR_HPP
#define SHADOW_MAP_MGR_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_math.hpp"
#include "geom.hpp"
#include "Material.hpp"
#include "ShadowTypes.hpp"

//=========================================================================
//  TYPES
//=========================================================================

enum
{
    POINT_SHADOW_FACE_COUNT      = 6,
    MAX_SHADOW_LIGHTS            = 8,
    MAX_SHADOW_SOURCES           = 64,
    MAX_SHADOW_ATLAS_SIZE        = 8192,
    MAX_SHADOW_MOMENT_ATLAS_SIZE = 2048,
    SHADOW_EVSM_BLUR_RADIUS      = 4,
};

class object;

//=========================================================================

class ShadowMapMgr
{
public:

    enum SourceType
    {
        SHADOW_SOURCE_POINT_FACE = 0,
        SHADOW_SOURCE_SPOT,
    };

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
        s32     AtlasTileX;
        s32     AtlasTileY;
        s32     AtlasTileWidth;
        s32     AtlasTileHeight;
    };

private:

    struct ScratchData;

    ShadowMapMgr( ShadowMapMgr const& );
    ShadowMapMgr& operator=( ShadowMapMgr const& );

    ScratchData& GetScratch               ( void );
    void         ComputePerspectiveSource ( ShadowSource& Destination, s32 SourceType,
                                            matrix4 const& LocalToWorld, radian FieldOfView,
                                            f32 LightRadius, f32 LightFalloff,
                                            s32 ShadowMapResolution ) const;
    void         UpdateAtlasLayout        ( void );

    //-------------------------------------------------------------------------

public:

    ShadowMapMgr( void );
    ~ShadowMapMgr( void );

    // Add shadow sources to the manager, then finalize the atlas layout.
    void  ClearSources        ( void );
    void  FinalizeSources     ( void );
    void  SetEnabled          ( xbool Enabled );
    void  SetShadowFilterType ( ShadowFilterType Type );
    xbool AddPointSource      ( matrix4 const& LocalToWorld, radian FieldOfView,
                                f32 LightRadius, f32 LightFalloff, s32 ShadowMapResolution,
                                s32 ShadowPriority, f32 ShadowScore, s32 DynamicLightIndex );
    xbool AddSpotSource       ( matrix4 const& LocalToWorld, radian FieldOfView,
                                f32 LightRadius, f32 LightFalloff, s32 ShadowMapResolution,
                                s32 ShadowPriority, f32 ShadowScore, s32 DynamicLightIndex );

    // Get finalized shadow source data.
    ShadowSource const& GetSource           ( s32 SourceIndex ) const;
    s32                 GetSourceCount      ( void ) const;
    s32                 GetAtlasSize        ( void ) const;
    s32                 GetAtlasSourceCount ( void ) const;
    ShadowFilterType    GetShadowFilterType ( void ) const;
    xbool               HasActiveSources    ( void ) const;

    // Build all shadow sources and submit shadow casters for the current frame.
    void CreateShadowMap ( void );

    //-------------------------------------------------------------------------

private:

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

//=========================================================================
//  GLOBAL INSTANCE
//=========================================================================

extern ShadowMapMgr g_ShadowMapMgr;

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline ShadowMapMgr::ShadowSource const& ShadowMapMgr::GetSource( s32 SourceIndex ) const
{
    ASSERT( ( SourceIndex >= 0 ) && ( SourceIndex < m_sourceCount ) );
    ASSERT( !m_atlasLayoutDirty );

    return m_sources[SourceIndex];
}

//=========================================================================

inline s32 ShadowMapMgr::GetSourceCount( void ) const
{
    return m_sourceCount;
}

//=========================================================================

inline s32 ShadowMapMgr::GetAtlasSize( void ) const
{
    return ( m_atlasSize > 0 ) ? m_atlasSize : MAX_SHADOW_ATLAS_SIZE;
}

//=========================================================================

inline s32 ShadowMapMgr::GetAtlasSourceCount( void ) const
{
    return m_atlasSourceCount;
}

//=========================================================================

inline ShadowFilterType ShadowMapMgr::GetShadowFilterType( void ) const
{
    return m_shadowFilterType;
}

//=========================================================================

inline xbool ShadowMapMgr::HasActiveSources( void ) const
{
    return m_enabled && ( m_sourceCount > 0 );
}

//=========================================================================
#endif // SHADOW_MAP_MGR_HPP
//=========================================================================
