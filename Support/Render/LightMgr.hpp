//==============================================================================
//
//  LightMgr.hpp
//
//==============================================================================

#ifndef LIGHTMGR_HPP
#define LIGHTMGR_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_color.hpp"
#include "x_math.hpp"
#include "x_array.hpp"
#include "Texture.hpp"

//==============================================================================
//  LIGHTMGR MANAGER CLASS
//==============================================================================

//------------------------------------------------------------------------------
//
// GLOBAL TODO: GS: I'm noticing some serious performance issues,
// the lighting system needs to be optimized because it's a mess right now.
//
//------------------------------------------------------------------------------

class light_mgr
{
  public:
    enum
    {
        MAX_CHAR_LIGHTS = 128
    };
    enum
    {
        MAX_DYNAMIC_LIGHTS = 64
    };
    enum
    {
        MAX_FADING_LIGHTS = 64
    };
    enum
    {
        MAX_COLLECTED_LIGHTS = 16
    };

    enum light_shape
    {
        LIGHT_SHAPE_OMNI = 0,
        LIGHT_SHAPE_SPOT,
    };

    struct collection_stats
    {
        u32 SceneQueries;
        u32 SceneCandidates;
        u32 SceneHits;
        u32 CharQueries;
        u32 CharCandidates;
        u32 CharHits;
    };

    light_mgr( void );
    virtual ~light_mgr( void );

    void OnUpdate( f32 deltaTime );

    // Functions for adding lights to the light manager. You should probably do this once
    // per frame. You clear the light list first, and then for each light that is visible
    // re-add it to the light manager. The exception is a fading light (such as a muzzle
    // flash). These will automatically fade and disappear over time.
    void ClearLights( void ); // does not clear the fading lights
    void AddFadingLight( vector3 const& pos, xcolor const& c, f32 radius, f32 intensity, f32 fadeTime );
    void AddDynamicLight( vector3 const& pos, xcolor const& c, f32 radius, f32 intensity, xbool charOnly,
                          s32 shape = LIGHT_SHAPE_OMNI, xbool castShadows = TRUE, f32 innerRadius = 0.0f,
                          vector3 const& direction = vector3( 0.0f, 0.0f, 1.0f ), f32 falloff = 1.0f,
                          f32 innerAngle = 30.0f, f32 outerAngle = 45.0f, s32 shadowMapResolution = 512,
                          s32 shadowPriority = 1, texture::handle const& cookie = texture::handle() );

    // Here are the functions for getting lights that actually hit an object. You should
    // make sure you are in the begin/end pair before asking for lights. Also, make
    // sure you only call begin/end once per frame, because it can be an expensive
    // operation. (Although worthwhile because it will do some optimizations on the data
    // before you make a bunch of ligh queries.)
    void BeginLightCollection( void );
    void EndLightCollection( void );
    void ResetAfterException( void );
    s32  CollectLights( bbox const& worldBBox, s32 maxLightCount = 3 );
    void GetCollectedLight( s32 index, vector3& pos, f32& radius, xcolor& c );
    void GetCollectedLightInfo( s32 index, vector3& pos, f32& radius, xcolor& c, f32& falloff );
    void GetCollectedLightInfo( s32 index, vector3& pos, f32& radius, xcolor& c, f32& falloff, s32& shape,
                                vector3& direction, f32& innerConeCos, f32& outerConeCos );
    void GetCollectedLightCookie( s32 index, s32& cookieIndex, vector3& cookieU, vector3& cookieV );
    s32  GetCollectedDynamicLightIndex( s32 index ) const;
    s32  CollectCharLights( matrix4 const& l2W, bbox const& b, s32 maxLightCount = 3 );
    s32  CollectCharLightsOnly( matrix4 const& l2W, bbox const& b, s32 maxLightCount = 3 );
    void GetCollectedCharLight( s32 index, vector3& dir, xcolor& c );
    s32  GetNDynamicLights( void ) const;
    collection_stats const& GetCollectionStats( void ) const;
    texture::handle const&  GetLightCookieHandle( s32 index ) const;
    s32                     GetLightCookieCount( void ) const;
    void GetDynamicLight( s32 index, vector3& pos, f32& radius, xcolor& c, s32& shape, xbool& castShadows,
                          f32& innerRadius, vector3& direction, f32& falloff, f32& innerAngle, f32& outerAngle,
                          s32& shadowMapResolution, s32& shadowPriority ) const;
    s32  GetNNonCharLights( void ) const;
    void GetLight( s32 index, vector3& pos, f32& radius, xcolor& c ) const;

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

        s32 PrevLink;
        s32 NextLink;
    };

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

    struct dir_light
    {
        vector3 Dir;
        xcolor  Col;
    };

    struct spad_light
    {

        // WARNING: Make sure position and radius are the first elements
        // of this structure, and the structure itself needs to be
        // 16-byte aligned.
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

    // internal helper routines
    s32        AddLight( void );
    void       RemoveLight( s32 lightIndex );
    void       InsertCollectedCharLight( dir_light const& light, s32 maxLightCount );
    s32        RegisterSpotLightCookie( texture::handle const& cookie );
    xbool      CalcDirLight( dir_light* pDst, matrix4 const& l2W, bbox const& localBox, bbox const& worldBox,
                             vector3 const& worldBoxCenter, vector3 const& pos, f32 radius, f32 intensity, xcolor& c,
                             s32 shape = LIGHT_SHAPE_OMNI, vector3 const& direction = vector3( 0.0f, 0.0f, 1.0f ),
                             f32 outerConeCos = 1.0f, f32 coneCosRangeInv = 0.0f );
    friend s32 SpadLightSortFn( void const* pA, void const* pB );

    // linked-list of fading lights (caused by muzzle flashes, explosions, etc.)
    s32          m_firstLink;
    s32          m_nFadingLights;
    fading_light m_fadingLights[MAX_FADING_LIGHTS];

    // list of dynamic lights (no need for it to be a linked-list, since they
    // should be accessed linearly)
    s32                     m_nDynamicLights;
    dynamic_light           m_dynamicLights[MAX_DYNAMIC_LIGHTS];
    s32                     m_nCharLights;
    dynamic_light           m_charLights[MAX_CHAR_LIGHTS];
    xarray<texture::handle> m_lightCookieFaces;

    // list of potential collectors
    xbool       m_isInCollection;
    s32         m_nSpadLights;
    s32         m_nNonCharLightsInSpad;
    spad_light* m_pSpadLights;

    // collection information for a particular instance
    s32              m_nCollectedLights;
    s32              m_collectedLights[MAX_COLLECTED_LIGHTS];
    dir_light        m_collectedCharLights[MAX_COLLECTED_LIGHTS];
    collection_stats m_collectionStats;
};

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

extern light_mgr g_LightMgr;

//==============================================================================
//  INLINE FUNCTIONS
//==============================================================================

inline void light_mgr::ClearLights( void )
{
    for ( s32 i = 0; i < m_lightCookieFaces.GetCount(); i++ )
    {
        m_lightCookieFaces[i].Destroy();
    }

    m_nDynamicLights = 0;
    m_nCharLights = 0;
    m_lightCookieFaces.SetCount( 0 );
}

//==============================================================================

inline s32 light_mgr::GetNNonCharLights( void ) const
{
    ASSERT( m_isInCollection );

    return m_nNonCharLightsInSpad;
}

//==============================================================================

inline light_mgr::collection_stats const& light_mgr::GetCollectionStats( void ) const
{
    return m_collectionStats;
}

//==============================================================================

inline s32 light_mgr::GetNDynamicLights( void ) const
{
    return m_nDynamicLights;
}

//==============================================================================

inline texture::handle const& light_mgr::GetLightCookieHandle( s32 index ) const
{
    ASSERT( ( index >= 0 ) && ( index < m_lightCookieFaces.GetCount() ) );
    return m_lightCookieFaces[index];
}

//==============================================================================

inline s32 light_mgr::GetLightCookieCount( void ) const
{
    return m_lightCookieFaces.GetCount();
}

//==============================================================================
#endif // LIGHTMGR_HPP
//==============================================================================
