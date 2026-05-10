//=========================================================================
//
//  ProjTextureMgr.hpp
//
//=========================================================================

#ifndef PROJTEXTUREMGR_HPP
#define PROJTEXTUREMGR_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_math.hpp"
#include "e_View.hpp"
#include "Texture.hpp"
#include "Render.hpp"

//=========================================================================
//  PROJ TEXTURE MANAGER CLASS
//=========================================================================

class proj_texture_mgr
{
public:
    enum    { MAX_PROJ_LIGHTS  = 4 };
    enum    { MAX_PROJ_SHADOWS = 8 };

             proj_texture_mgr( void );
    virtual ~proj_texture_mgr( void );

    // Functions for adding projective textures to the manager. You should probably do this once
    // per frame. You clear the projection list first, and then for each projective light or shadow
    // that is visible re-add it to the manager.
    void    ClearProjTextures       ( void );
    void    AddProjLight            ( const matrix4&  L2W,
                                      radian          FOV,
                                      f32             Length,
                                      texture::handle Texture );
    void    AddProjShadow           ( const matrix4&  L2W,
                                      radian          FOV,
                                      f32             Length,
                                      texture::handle Texture );

    // Functions for getting projections that actually hit an object.
    s32     CollectLights           ( const matrix4& L2W,
                                      const bbox&    B,
                                      s32            MaxLightCount = MAX_PROJ_LIGHTS );
    void    GetCollectedLight       ( matrix4&       LightMatrix,
                                      xbitmap*&      pBitmap );

    s32     CollectShadows          ( const matrix4& L2W,
                                      const bbox&    B,
                                      s32            MaxShadowCount = MAX_PROJ_SHADOWS );
    void    GetCollectedShadow      ( matrix4&       ShadMatrix,
                                      xbitmap*&      pBitmap );
    u32     CollectProjectionFlags  ( u32            RenderFlags,
                                      const bbox&    WorldBBox );

    xbool   CanReceiveProjTexture   ( material_type  Type,
                                      u16            MaterialFlags ) const;
    xbool   CanReceiveProjTexture   ( const geom::material& Mat ) const;
    xbool   CanReceiveProjTexture   ( const material&       Mat ) const;

protected:
    struct projection
    {
        view            ProjView;
        matrix4         ProjMatrix;
        texture::handle ProjTexture;
    };

    // internal helper routines
    void        SetupProjection     ( projection&     Dest,
                                      const matrix4&  L2W,
                                      radian          FOV,
                                      f32             Length,
                                      texture::handle Texture );

    s32         CollectProjections  ( const projection* pProjections,
                                      s32               NProjections,
                                      s32*              pCollectedProjections,
                                      s32&              NCollectedProjections,
                                      s32&              CurrCollectedProjection,
                                      const matrix4&    L2W,
                                      const bbox&       B,
                                      s32               MaxProjectionCount );
    void        GetCollectedProjection( const projection* pProjections,
                                        const s32*        pCollectedProjections,
                                        s32               NCollectedProjections,
                                        s32&              CurrCollectedProjection,
                                        matrix4&          ProjMatrix,
                                        xbitmap*&         pBitmap );

    xbool       ProjectionIntersectsBBox( const projection& Proj,
                                          const bbox&      B );

    // list of projective lights and shadows
    s32         m_NLightProjections;
    s32         m_NShadowProjections;
    projection  m_LightProjections[MAX_PROJ_LIGHTS];
    projection  m_ShadowProjections[MAX_PROJ_SHADOWS];

    // collected projections for current query
    s32         m_NCollectedLights;
    s32         m_NCollectedShadows;
    s32         m_CurrCollectedLight;
    s32         m_CurrCollectedShadow;
    s32         m_CollectedLights[MAX_PROJ_LIGHTS];
    s32         m_CollectedShadows[MAX_PROJ_SHADOWS];
};

//=========================================================================
//  GLOBAL INSTANCE
//=========================================================================

extern proj_texture_mgr    g_ProjTextureMgr;

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline
void proj_texture_mgr::ClearProjTextures( void )
{
    for ( s32 i = 0; i < m_NLightProjections; i++ )
    {
        m_LightProjections[i].ProjTexture.Destroy();
    }

    for ( s32 i = 0; i < m_NShadowProjections; i++ )
    {
        m_ShadowProjections[i].ProjTexture.Destroy();
    }

    m_NLightProjections    = 0;
    m_NShadowProjections   = 0;
    m_NCollectedLights     = 0;
    m_NCollectedShadows    = 0;
    m_CurrCollectedLight   = 0;
    m_CurrCollectedShadow  = 0;
}

//=========================================================================
#endif // PROJTEXTUREMGR_HPP
//=========================================================================
