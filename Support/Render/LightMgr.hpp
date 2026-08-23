//=========================================================================
//
//  LightMgr.hpp
//
//=========================================================================

#ifndef LIGHTMGR_HPP
#define LIGHTMGR_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_color.hpp"
#include "x_math.hpp"
#include "x_array.hpp"
#include "Texture.hpp"

//=========================================================================
//  TYPES
//=========================================================================

class light_mgr
{
public:

    enum
    {
        MAX_CHAR_LIGHTS      = 128,
        MAX_DYNAMIC_LIGHTS   = 64,
        MAX_FADING_LIGHTS    = 64,
        MAX_COLLECTED_LIGHTS = 16,
        MAX_SPAD_LIGHTS      = MAX_FADING_LIGHTS + MAX_DYNAMIC_LIGHTS + MAX_CHAR_LIGHTS,
        MAX_LIGHT_BVH_NODES  = ( MAX_SPAD_LIGHTS * 2 ) - 1,
    };

    //-------------------------------------------------------------------------

    enum light_shape
    {
        LIGHT_SHAPE_OMNI = 0,
        LIGHT_SHAPE_SPOT,
    };

    //--------------------------------------------------------------------------

    struct collection_stats
    {
        u32 SceneQueries;
        u32 SceneCandidates;
        u32 SceneHits;
        u32 CharQueries;
        u32 CharCandidates;
        u32 CharHits;
    };

protected:

    struct fading_light
    {
        vector3 Pos;
        f32     Radius;
        xcolor  StartColor;
        xcolor  CurrentColor;
        f32     FadeTime;
        f32     ElapsedTime;
        xbool   Valid;
        f32     InterpolationT;
        f32     Intensity;
        s32     PrevLink;
        s32     NextLink;
    };

    //-------------------------------------------------------------------------

    struct dynamic_light
    {
        vector3 Pos;
        f32     Radius;
        xcolor  Color;
        f32     Intensity;
        vector3 Direction;
        f32     Falloff;
        f32     InnerAngle;
        f32     OuterAngle;
        f32     InnerConeCos;
        f32     OuterConeCos;
        f32     OuterConeSin;
        f32     OuterConeTan;
        f32     ConeCosRangeInv;
        s32     Shape;
        s32     ShadowMapResolution;
        s32     ShadowPriority;
        xbool   CastShadows;
        s32     CookieIndex;
        vector3 CookieU;
        vector3 CookieV;
    };

    //-------------------------------------------------------------------------

    struct dir_light
    {
        vector3 Dir;
        xcolor  Col;
    };

    //-------------------------------------------------------------------------

    struct spad_light
    {
        // Position and radius must remain the first fields.
        // The structure itself must remain 16-byte aligned.
        vector3 Pos;
        f32     Radius;
        xcolor  Color;
        f32     Intensity;
        f32     Falloff;
        f32     Score;
        xbool   CharOnly;
        s32     Shape;
        vector3 Direction;
        f32     InnerConeCos;
        f32     OuterConeCos;
        f32     OuterConeSin;
        f32     OuterConeTan;
        f32     ConeCosRangeInv;
        s32     CookieIndex;
        vector3 CookieU;
        vector3 CookieV;
        s32     DynamicLightIndex;
    };

    //-------------------------------------------------------------------------

    struct light_bvh_node
    {
        bbox Bounds;
        s32  LeftChild;
        s32  RightChild;
        s32  FirstLight;
        s32  LightCount;
    };

    //=========================================================================

    static s32   SpadLightSortFn           ( void const* pA, void const* pB );
    static xbool CalcDirLight              ( dir_light* pDestination, matrix4 const& LocalToWorld,
                                             bbox const& LocalBBox, bbox const& WorldBBox,
                                             vector3 const& WorldBBoxCenter, vector3 const& Position,
                                             f32 Radius, f32 Intensity, xcolor const& Color,
                                             s32 Shape = LIGHT_SHAPE_OMNI,
                                             vector3 const& Direction = vector3( 0.0f, 0.0f, 1.0f ),
                                             f32 OuterConeCos = 1.0f, f32 ConeCosRangeInv = 0.0f );
    s32          AddLight                  ( void );
    void         RemoveLight               ( s32 LightIndex );
    s32          RegisterSpotLightCookie   ( texture::handle const& Cookie );
    void         InsertCollectedSceneLight ( s32 LightIndex, s32 MaxLightCount );
    void         InsertCollectedCharLight  ( dir_light const& Light, s32 MaxLightCount );
    s32          BuildLightBvhNode         ( s32 FirstLight, s32 LightCount );
    void         BuildLightBvh             ( void );

public:

    light_mgr( void );
    virtual ~light_mgr( void );

    // Add lights once per frame. Fading lights persist and expire automatically.
    void ClearLights     ( void );
    void AddFadingLight  ( vector3 const& Position, xcolor const& Color, f32 Radius,
                           f32 Intensity, f32 FadeTime );
    void AddDynamicLight ( vector3 const& Position, xcolor const& Color, f32 Radius,
                           f32 Intensity, xbool CharOnly, s32 Shape = LIGHT_SHAPE_OMNI,
                           xbool CastShadows = TRUE, f32 InnerRadius = 0.0f,
                           vector3 const& Direction = vector3( 0.0f, 0.0f, 1.0f ),
                           f32 Falloff = 1.0f, f32 InnerAngle = 30.0f,
                           f32 OuterAngle = 45.0f, s32 ShadowMapResolution = 512,
                           s32 ShadowPriority = 1,
                           texture::handle const& Cookie = texture::handle() );
    void OnUpdate          ( f32 DeltaTime );
    s32  GetNDynamicLights ( void ) const;
    void GetDynamicLight   ( s32 Index, vector3& Position, f32& Radius, xcolor& Color,
                             s32& Shape, xbool& CastShadows, f32& InnerRadius,
                             vector3& Direction, f32& Falloff, f32& InnerAngle,
                             f32& OuterAngle, s32& ShadowMapResolution,
                             s32& ShadowPriority ) const;

    // Build frame-local collection data once before querying object lights.
    void                    BeginLightCollection ( void );
    void                    EndLightCollection   ( void );
    void                    ResetAfterException  ( void );
    collection_stats const& GetCollectionStats   ( void ) const;

    // Get scene lights that intersect an object.
    s32                    CollectLights                  ( bbox const& WorldBBox,
                                                            s32 MaxLightCount = 3 );
    void                   GetCollectedLight              ( s32 Index, vector3& Position,
                                                            f32& Radius, xcolor& Color );
    void                   GetCollectedLightInfo          ( s32 Index, vector3& Position,
                                                            f32& Radius, xcolor& Color,
                                                            f32& Falloff );
    void                   GetCollectedLightInfo          ( s32 Index, vector3& Position,
                                                            f32& Radius, xcolor& Color,
                                                            f32& Falloff, s32& Shape,
                                                            vector3& Direction,
                                                            f32& InnerConeCos,
                                                            f32& OuterConeCos );
    void                   GetCollectedLightCookie        ( s32 Index, s32& CookieIndex,
                                                            vector3& CookieU, vector3& CookieV );
    s32                    GetCollectedDynamicLightIndex  ( s32 Index ) const;
    s32                    GetNNonCharLights              ( void ) const;
    void                   GetLight                       ( s32 Index, vector3& Position,
                                                            f32& Radius, xcolor& Color ) const;
    s32                    GetLightCookieCount            ( void ) const;
    texture::handle const& GetLightCookieHandle           ( s32 Index ) const;

    // Get directional character lights derived from scene lights.
    s32  CollectCharLights     ( matrix4 const& LocalToWorld, bbox const& LocalBBox,
                                 s32 MaxLightCount = 3 );
    s32  CollectCharLightsOnly ( matrix4 const& LocalToWorld, bbox const& LocalBBox,
                                 s32 MaxLightCount = 3 );
    void GetCollectedCharLight ( s32 Index, vector3& Direction, xcolor& Color );

protected:

    // Fading lights.
    s32          m_firstLink;
    s32          m_nFadingLights;
    fading_light m_fadingLights[MAX_FADING_LIGHTS];

    // Dynamic lights.
    s32                     m_nDynamicLights;
    dynamic_light           m_dynamicLights[MAX_DYNAMIC_LIGHTS];
    s32                     m_nCharLights;
    dynamic_light           m_charLights[MAX_CHAR_LIGHTS];
    xarray<texture::handle> m_lightCookieFaces;

    // Frame-local collection data.
    xbool           m_isInCollection;
    s32             m_nSpadLights;
    s32             m_nNonCharLightsInSpad;
    spad_light*     m_pSpadLights;
    light_bvh_node* m_pLightBvhNodes;
    s32*            m_pLightBvhLightIndices;
    s32             m_nLightBvhNodes;

    // Results for the current object query.
    s32              m_nCollectedLights;
    s32              m_collectedLights[MAX_COLLECTED_LIGHTS];
    dir_light        m_collectedCharLights[MAX_COLLECTED_LIGHTS];
    collection_stats m_collectionStats;
};

//=========================================================================
//  GLOBAL INSTANCE
//=========================================================================

extern light_mgr g_LightMgr;

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline void light_mgr::ClearLights( void )
{
    for( s32 Index = 0; Index < m_lightCookieFaces.GetCount(); Index++ )
    {
        m_lightCookieFaces[Index].Destroy();
    }

    m_nDynamicLights = 0;
    m_nCharLights = 0;
    m_lightCookieFaces.SetCount( 0 );
}

//=========================================================================

inline s32 light_mgr::GetNDynamicLights( void ) const
{
    return m_nDynamicLights;
}

//=========================================================================

inline light_mgr::collection_stats const& light_mgr::GetCollectionStats( void ) const
{
    return m_collectionStats;
}

//=========================================================================

inline s32 light_mgr::GetNNonCharLights( void ) const
{
    ASSERT( m_isInCollection );

    return m_nNonCharLightsInSpad;
}

//=========================================================================

inline s32 light_mgr::GetLightCookieCount( void ) const
{
    return m_lightCookieFaces.GetCount();
}

//=========================================================================

inline texture::handle const& light_mgr::GetLightCookieHandle( s32 Index ) const
{
    ASSERT( ( Index >= 0 ) && ( Index < m_lightCookieFaces.GetCount() ) );
    return m_lightCookieFaces[Index];
}

//=========================================================================
#endif // LIGHTMGR_HPP
//=========================================================================
