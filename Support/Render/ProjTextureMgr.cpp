//=========================================================================
//
//  ProjTextureMgr.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "ProjTextureMgr.hpp"

//=========================================================================
// GLOBAL INSTANCE
//=========================================================================

proj_texture_mgr    g_ProjTextureMgr;

//=========================================================================
// FUNCTIONS
//=========================================================================

proj_texture_mgr::proj_texture_mgr( void ) :
    m_NLightProjections     ( 0 ),
    m_NShadowProjections    ( 0 ),
    m_NCollectedLights      ( 0 ),
    m_NCollectedShadows     ( 0 ),
    m_CurrCollectedLight    ( 0 ),
    m_CurrCollectedShadow   ( 0 )
{
}

//=========================================================================

proj_texture_mgr::~proj_texture_mgr( void )
{
    ClearProjTextures();
}

//=========================================================================

xbool proj_texture_mgr::CanReceiveProjTexture( material_type Type,
                                               u16           MaterialFlags ) const
{
    if ( !IsAlphaMaterial( Type ) )
        return TRUE;

    if ( !(MaterialFlags & geom::material::FLAG_FORCE_ZFILL) )
        return FALSE;

    if ( MaterialFlags & geom::material::FLAG_IS_SUBTRACTIVE )
        return FALSE;

    return TRUE;
}

//=========================================================================

xbool proj_texture_mgr::CanReceiveProjTexture( const geom::material& Mat ) const
{
    return CanReceiveProjTexture( (material_type)Mat.Type, Mat.Flags );
}

//=========================================================================

xbool proj_texture_mgr::CanReceiveProjTexture( const material& Mat ) const
{
    return CanReceiveProjTexture( (material_type)Mat.m_Type, Mat.m_Flags );
}

//=========================================================================

xbool proj_texture_mgr::ProjectionIntersectsBBox( const projection& Proj, const bbox& B )
{
    return ( Proj.ProjView.BBoxInView( B ) != view::VISIBLE_NONE );
}

//=========================================================================

s32 proj_texture_mgr::CollectProjections( const projection* pProjections,
                                          s32               NProjections,
                                          s32*              pCollectedProjections,
                                          s32&              NCollectedProjections,
                                          s32&              CurrCollectedProjection,
                                          const matrix4&    L2W,
                                          const bbox&       B,
                                          s32               MaxProjectionCount )
{
    ASSERT( pProjections );
    ASSERT( pCollectedProjections );
    ASSERT( NProjections >= 0 );
    ASSERT( MaxProjectionCount >= 0 );

    bbox WorldBBox = B;
    WorldBBox.Transform( L2W );

    NCollectedProjections   = 0;
    CurrCollectedProjection = 0;

    if ( MaxProjectionCount <= 0 )
        return 0;

    for ( s32 iProjection = 0;
          (iProjection < NProjections) && (NCollectedProjections < MaxProjectionCount);
          iProjection++ )
    {
        if ( ProjectionIntersectsBBox( pProjections[iProjection], WorldBBox ) )
            pCollectedProjections[NCollectedProjections++] = iProjection;
    }

    return NCollectedProjections;
}

//=========================================================================

void proj_texture_mgr::GetCollectedProjection( const projection* pProjections,
                                               const s32*        pCollectedProjections,
                                               s32               NCollectedProjections,
                                               s32&              CurrCollectedProjection,
                                               matrix4&          ProjMatrix,
                                               xbitmap*&         pBitmap )
{
    ASSERT( pProjections );
    ASSERT( pCollectedProjections );
    ASSERT( NCollectedProjections >= 0 );
    ASSERT( (CurrCollectedProjection >= 0) && (CurrCollectedProjection < NCollectedProjections) );

    const projection& Proj = pProjections[pCollectedProjections[CurrCollectedProjection++]];

    ProjMatrix = Proj.ProjMatrix;

    texture* pTex = Proj.ProjTexture.GetPointer();
    pBitmap = pTex ? &pTex->m_Bitmap : NULL;
}

//=========================================================================

void proj_texture_mgr::AddProjLight( const matrix4&  L2W,
                                     radian          FOV,
                                     f32             Length,
                                     texture::handle Texture )
{
    ASSERT( (m_NLightProjections >= 0) && (m_NLightProjections <= MAX_PROJ_LIGHTS) );
    ASSERT( Texture.GetPointer() );

    if ( Texture.GetPointer() == NULL )
        return;

    if ( m_NLightProjections >= MAX_PROJ_LIGHTS )
        return;

    SetupProjection( m_LightProjections[m_NLightProjections], L2W, FOV, Length, Texture );
    m_NLightProjections++;
}

//=========================================================================

void proj_texture_mgr::AddProjShadow( const matrix4&  L2W,
                                      radian          FOV,
                                      f32             Length,
                                      texture::handle Texture )
{
    ASSERT( (m_NShadowProjections >= 0) && (m_NShadowProjections <= MAX_PROJ_SHADOWS) );
    ASSERT( Texture.GetPointer() );

    if ( Texture.GetPointer() == NULL )
        return;

    if ( m_NShadowProjections >= MAX_PROJ_SHADOWS )
        return;

    SetupProjection( m_ShadowProjections[m_NShadowProjections], L2W, FOV, Length, Texture );
    m_NShadowProjections++;
}

//=========================================================================

s32 proj_texture_mgr::CollectLights( const matrix4& L2W, const bbox& B, s32 MaxLightCount )
{
    ASSERT( (m_NLightProjections >= 0) && (m_NLightProjections <= MAX_PROJ_LIGHTS) );

    if ( MaxLightCount > MAX_PROJ_LIGHTS )
        MaxLightCount = MAX_PROJ_LIGHTS;

    return CollectProjections( m_LightProjections,
                               m_NLightProjections,
                               m_CollectedLights,
                               m_NCollectedLights,
                               m_CurrCollectedLight,
                               L2W,
                               B,
                               MaxLightCount );
}

//=========================================================================

void proj_texture_mgr::GetCollectedLight( matrix4& LightMatrix, xbitmap*& pBitmap )
{
    GetCollectedProjection( m_LightProjections,
                            m_CollectedLights,
                            m_NCollectedLights,
                            m_CurrCollectedLight,
                            LightMatrix,
                            pBitmap );
}

//=========================================================================

s32 proj_texture_mgr::CollectShadows( const matrix4& L2W, const bbox& B, s32 MaxShadowCount )
{
    ASSERT( (m_NShadowProjections >= 0) && (m_NShadowProjections <= MAX_PROJ_SHADOWS) );

    if ( MaxShadowCount > MAX_PROJ_SHADOWS )
        MaxShadowCount = MAX_PROJ_SHADOWS;

    return CollectProjections( m_ShadowProjections,
                               m_NShadowProjections,
                               m_CollectedShadows,
                               m_NCollectedShadows,
                               m_CurrCollectedShadow,
                               L2W,
                               B,
                               MaxShadowCount );
}

//=========================================================================

void proj_texture_mgr::GetCollectedShadow( matrix4& ShadMatrix, xbitmap*& pBitmap )
{
    ASSERT( (m_NCollectedShadows >= 0) && (m_NCollectedShadows <= MAX_PROJ_SHADOWS) );

    GetCollectedProjection( m_ShadowProjections,
                            m_CollectedShadows,
                            m_NCollectedShadows,
                            m_CurrCollectedShadow,
                            ShadMatrix,
                            pBitmap );
}

//=========================================================================

u32 proj_texture_mgr::CollectProjectionFlags( u32 RenderFlags, const bbox& WorldBBox )
{
    ASSERT( (m_NLightProjections  >= 0) && (m_NLightProjections  <= MAX_PROJ_LIGHTS ) );
    ASSERT( (m_NShadowProjections >= 0) && (m_NShadowProjections <= MAX_PROJ_SHADOWS) );

    u32 RetFlags = 0;

    if ( !m_NLightProjections && !m_NShadowProjections )
        return RetFlags;

    if ( !(RenderFlags & render::DISABLE_SPOTLIGHT) )
    {
        for ( s32 i = 0; i < m_NLightProjections; i++ )
        {
            if ( ProjectionIntersectsBBox( m_LightProjections[i], WorldBBox ) )
            {
                RetFlags |= render::INSTFLAG_SPOTLIGHT;
                break;
            }
        }
    }

    if ( !(RenderFlags & render::DISABLE_PROJ_SHADOWS) )
    {
        for ( s32 i = 0; i < m_NShadowProjections; i++ )
        {
            if ( !ProjectionIntersectsBBox( m_ShadowProjections[i], WorldBBox ) )
                continue;

            RetFlags |= render::INSTFLAG_PROJ_SHADOW;
            break;
        }
    }

    return RetFlags;
}

//=========================================================================

void proj_texture_mgr::SetupProjection( projection&     Dest,
                                        const matrix4&  L2W,
                                        radian          FOV,
                                        f32             Length,
                                        texture::handle Texture )
{
    // set up the bitmap
    Dest.ProjTexture = Texture;

    // set up the view
    texture* pProjTexture = Texture.GetPointer();
    ASSERT( pProjTexture );
    xbitmap& ProjBMP = pProjTexture->m_Bitmap;
    Dest.ProjView.SetXFOV( FOV );
    Dest.ProjView.SetZLimits( 1.0f, Length );
    Dest.ProjView.SetViewport( 0, 0, ProjBMP.GetWidth(), ProjBMP.GetHeight() );
    Dest.ProjView.SetV2W( L2W );

    Dest.ProjMatrix = Dest.ProjView.GetV2C();
    Dest.ProjMatrix(2,2) = -1.0f /       (Length - 1.0f);
    Dest.ProjMatrix(3,2) =  1.0f + 1.0f / (Length - 1.0f);
    Dest.ProjMatrix *= Dest.ProjView.GetW2V();
    Dest.ProjMatrix.Scale( vector3( 0.5f, -0.5f, 1.0f ) );
    Dest.ProjMatrix.Translate( vector3( 0.5f, 0.5f, 0.0f ) );
}

//=========================================================================
