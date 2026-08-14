//=========================================================================
//
//  ProjTextureMgr.hpp
//
//=========================================================================

#ifndef PROJ_TEXTURE_MGR_HPP
#define PROJ_TEXTURE_MGR_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "x_math.hpp"
#include "e_View.hpp"
#include "Texture.hpp"
#include "Render.hpp"

//=========================================================================
//  TYPES
//=========================================================================

class ProjTextureMgr
{
public:

    static constexpr s32 MaxLightProjectionCount  = 4;
    static constexpr s32 MaxShadowProjectionCount = 8;

protected:

    struct Projection
    {
        view            m_View;
        matrix4         m_Matrix;
        texture::handle m_Texture;
    };

    static void  ClearProjections            ( Projection* pProjections, s32& ProjectionCount );
    static void  AddProjection               ( Projection* pProjections, s32& ProjectionCount,
                                               s32 MaxProjectionCount, matrix4 const& LocalToWorld,
                                               radian FieldOfView, f32 Length, texture::handle Texture );
    static s32   CollectProjections          ( Projection const* pProjections, s32 ProjectionCount,
                                               s32* pCollectedIndices, s32& CollectedCount,
                                               s32& CurrentCollectedIndex, matrix4 const& LocalToWorld,
                                               bbox const& LocalBBox, s32 MaxCollectedCount );
    static void  GetCollectedProjection      ( Projection const* pProjections,
                                               s32 const* pCollectedIndices, s32 CollectedCount,
                                               s32& CurrentCollectedIndex, matrix4& ProjectionMatrix,
                                               texture const*& pTexture );
    static xbool ProjectionIntersectsBBox    ( Projection const& ProjectionData,
                                               bbox const& WorldBBox );
    static xbool AnyProjectionIntersectsBBox ( Projection const* pProjections, s32 ProjectionCount,
                                               bbox const& WorldBBox );
    static void  SetupProjection             ( Projection& Destination, matrix4 const& LocalToWorld,
                                               radian FieldOfView, f32 Length,
                                               texture::handle Texture );

    //-------------------------------------------------------------------------

public:

    ProjTextureMgr( void );
    virtual ~ProjTextureMgr( void );

    // Add projective textures to the manager once per frame.
    // Clear the projection list first, then re-add each visible projective light or shadow.
    void ClearProjTextures ( void );
    void AddProjLight      ( matrix4 const& LocalToWorld, radian FieldOfView, f32 Length,
                             texture::handle Texture );
    void AddProjShadow     ( matrix4 const& LocalToWorld, radian FieldOfView, f32 Length,
                             texture::handle Texture );

    // Get projections that intersect an object.
    s32                    CollectLights          ( matrix4 const& LocalToWorld, bbox const& LocalBBox,
                                                    s32 MaxLightCount = MaxLightProjectionCount );
    void                   GetCollectedLight      ( matrix4& LightMatrix, texture const*& pTexture );
    s32                    GetProjLightCount      ( void ) const;
    void                   GetProjLight           ( s32 Index, matrix4& LightMatrix,
                                                    texture const*& pTexture ) const;
    texture::handle const& GetProjLightTexture    ( s32 Index ) const;
    s32                    CollectShadows         ( matrix4 const& LocalToWorld, bbox const& LocalBBox,
                                                    s32 MaxShadowCount = MaxShadowProjectionCount );
    void                   GetCollectedShadow     ( matrix4& ShadowMatrix, texture const*& pTexture );
    s32                    GetProjShadowCount     ( void ) const;
    void                   GetProjShadow          ( s32 Index, matrix4& ShadowMatrix,
                                                    texture const*& pTexture ) const;
    texture::handle const& GetProjShadowTexture   ( s32 Index ) const;
    u32                    CollectProjectionFlags ( u32 RenderFlags, bbox const& WorldBBox );

    //-------------------------------------------------------------------------

protected:

    Projection m_LightProjections[MaxLightProjectionCount];
    Projection m_ShadowProjections[MaxShadowProjectionCount];
    s32        m_LightProjectionCount;
    s32        m_ShadowProjectionCount;
    s32        m_CollectedLightCount;
    s32        m_CollectedShadowCount;
    s32        m_CurrentCollectedLightIndex;
    s32        m_CurrentCollectedShadowIndex;
    s32        m_CollectedLightIndices[MaxLightProjectionCount];
    s32        m_CollectedShadowIndices[MaxShadowProjectionCount];
};

//=========================================================================
//  GLOBAL INSTANCE
//=========================================================================

extern ProjTextureMgr g_ProjTextureMgr;

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline void ProjTextureMgr::ClearProjTextures( void )
{
    ClearProjections( m_LightProjections, m_LightProjectionCount );
    ClearProjections( m_ShadowProjections, m_ShadowProjectionCount );

    m_CollectedLightCount          = 0;
    m_CollectedShadowCount         = 0;
    m_CurrentCollectedLightIndex   = 0;
    m_CurrentCollectedShadowIndex  = 0;
}

//=========================================================================

inline s32 ProjTextureMgr::GetProjLightCount( void ) const
{
    return m_LightProjectionCount;
}

//=========================================================================

inline void ProjTextureMgr::GetProjLight( s32 Index, matrix4& LightMatrix, texture const*& pTexture ) const
{
    ASSERT( ( Index >= 0 ) && ( Index < m_LightProjectionCount ) );

    Projection const& ProjectionData = m_LightProjections[Index];
    LightMatrix = ProjectionData.m_Matrix;
    pTexture = ProjectionData.m_Texture.GetPointer();
}

//=========================================================================

inline texture::handle const& ProjTextureMgr::GetProjLightTexture( s32 Index ) const
{
    ASSERT( ( Index >= 0 ) && ( Index < m_LightProjectionCount ) );
    return m_LightProjections[Index].m_Texture;
}

//=========================================================================

inline s32 ProjTextureMgr::GetProjShadowCount( void ) const
{
    return m_ShadowProjectionCount;
}

//=========================================================================

inline void ProjTextureMgr::GetProjShadow( s32 Index, matrix4& ShadowMatrix, texture const*& pTexture ) const
{
    ASSERT( ( Index >= 0 ) && ( Index < m_ShadowProjectionCount ) );

    Projection const& ProjectionData = m_ShadowProjections[Index];
    ShadowMatrix = ProjectionData.m_Matrix;
    pTexture = ProjectionData.m_Texture.GetPointer();
}

//=========================================================================

inline texture::handle const& ProjTextureMgr::GetProjShadowTexture( s32 Index ) const
{
    ASSERT( ( Index >= 0 ) && ( Index < m_ShadowProjectionCount ) );
    return m_ShadowProjections[Index].m_Texture;
}

//=========================================================================
#endif // PROJ_TEXTURE_MGR_HPP
//=========================================================================
