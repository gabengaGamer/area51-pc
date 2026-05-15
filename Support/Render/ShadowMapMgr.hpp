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

#include "Geom.hpp"
#include "Material.hpp"

//==============================================================================
//  CONSTANTS
//==============================================================================

enum
{
    POINT_SHADOW_FACE_COUNT = 6,
    MAX_SHADOW_LIGHTS       = 8,
    MAX_SHADOW_SOURCES      = 64,
    POINT_SHADOW_FACE_SIZE  = 256,
    SHADOW_ATLAS_SIZE       = 2048,
};

class object;

//==============================================================================
//  SHADOW MAP MANAGER
//==============================================================================

class shadow_map_mgr
{
public:

    //--------------------------------------------------------------------------
    // Source Types
    //--------------------------------------------------------------------------

    enum source_type
    {
        SHADOW_SOURCE_POINT_FACE = 0,
        SHADOW_SOURCE_SPOT,
    };

    //--------------------------------------------------------------------------
    // Source Description
    //--------------------------------------------------------------------------

    struct shadow_source
    {
        s32     Type;
        s32     PointLightIndex;
        s32     FaceIndex;
        s32     RequestedResolution;
        matrix4 WorldToClip;
        matrix4 WorldToAtlas;
        vector4 LightPosRadius;
        vector4 FaceLightDirFalloff;
        vector4 FaceLightData;
        f32     LightFalloff;
        f32     NearZ;
        f32     FarZ;
        bbox    WorldBBox;
        s32     AtlasX;
        s32     AtlasY;
        s32     AtlasWidth;
        s32     AtlasHeight;
    };

    //--------------------------------------------------------------------------
    // Lifetime
    //--------------------------------------------------------------------------

                shadow_map_mgr          ( void );
               ~shadow_map_mgr          ( void );

    //--------------------------------------------------------------------------
    // Source Construction
    //--------------------------------------------------------------------------

    void        ClearSources            ( void );
    void        FinalizeSources         ( void );
    xbool       AddPointSource          ( const matrix4& L2W,
                                          radian         FOV,
                                          f32            LightRadius,
                                          f32            LightFalloff,
                                          s32            ShadowMapResolution );
    xbool       AddSpotSource           ( const matrix4& L2W,
                                          radian         FOV,
                                          f32            LightRadius,
                                          f32            LightFalloff,
                                          s32            ShadowMapResolution );

    //--------------------------------------------------------------------------
    // Source Queries
    //--------------------------------------------------------------------------

    s32         CollectSources          ( const matrix4& L2W,
                                          const bbox&    B,
                                          s32            MaxSourceCount = MAX_SHADOW_SOURCES );
    void        GetCollectedSource      ( s32      CollectedIndex,
                                          s32&     SourceIndex,
                                          s32&     Type,
                                          matrix4& ShadowMatrix,
                                          vector4& LightPosRadius,
                                          f32&     Falloff,
                                          f32&     NearZ,
                                          f32&     FarZ,
                                          s32&     PointLightIndex,
                                          s32&     FaceIndex ) const;

    const shadow_source&
                GetSource               ( s32 SourceIndex ) const;
    s32         GetSourceCount          ( void ) const;
    s32         GetPointLightCount      ( void ) const;
    s32         GetSpotAtlasSize        ( void ) const;
    s32         GetSpotSourceCount      ( void ) const;
    xbool       HasActiveSources        ( void ) const;

    //--------------------------------------------------------------------------
    // Material Queries
    //--------------------------------------------------------------------------

    xbool       CanReceiveShadowMap     ( material_type          Type,
                                          u16                    MaterialFlags ) const;
    xbool       CanReceiveShadowMap     ( const geom::material&  Mat ) const;
    xbool       CanReceiveShadowMap     ( const material&        Mat ) const;

    //--------------------------------------------------------------------------
    // Shadow Map Build
    //--------------------------------------------------------------------------

    void        CreateShadowMap         ( object* const* ppCasterCandidates,
                                          s32            NCasterCandidates );

private:

    //--------------------------------------------------------------------------
    // Internal Helpers
    //--------------------------------------------------------------------------

    void        ComputePerspectiveSource( shadow_source&  Dest,
                                          s32             SourceType,
                                          const matrix4&  L2W,
                                          radian          FOV,
                                          f32             LightRadius,
                                          f32             LightFalloff,
                                          s32             ShadowMapResolution ) const;
    void        UpdateAtlasLayout       ( void );

private:

    //--------------------------------------------------------------------------
    // Manager State
    //--------------------------------------------------------------------------

    s32             m_SourceCount;
    s32             m_PointFaceCount;
    s32             m_PointLightCount;
    s32             m_SpotSourceCount;
    s32             m_SpotAtlasSize;
    xbool           m_AtlasLayoutDirty;
    s32             m_NCollectedSources;
    s32             m_CollectedSources[MAX_SHADOW_SOURCES];
    shadow_source   m_Sources[MAX_SHADOW_SOURCES];
};

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

extern shadow_map_mgr g_ShadowMapMgr;

//==============================================================================
#endif // SHADOW_MAP_MGR_HPP
//==============================================================================
