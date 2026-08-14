//=============================================================================
//
//  GeomStorage.hpp
//
//  Owns the uploaded geometry used by the render pipeline.
//
//=============================================================================

#ifndef GEOM_STORAGE_HPP
#define GEOM_STORAGE_HPP

//=============================================================================
//  INCLUDES
//=============================================================================

#include "../RigidGeom.hpp"
#include "../SkinGeom.hpp"

//=============================================================================
//  GEOMETRY STORAGE
//=============================================================================

class VertexMgr;
class SoftVertexMgr;

//=============================================================================

class GeomStorage
{
public:
    GeomStorage ( void );

    void Init ( VertexMgr& rigidVertices, SoftVertexMgr& skinVertices );
    void Kill ( void );

    xhandle AddRigid    ( rigid_geom const& geom );
    xhandle AddSkin     ( skin_geom const& geom );
    void    RemoveRigid ( xhandle hGeom );
    void    RemoveSkin  ( xhandle hGeom );

    xhandle GetRigidSurface ( xhandle hGeom, s32 iSurface ) const;
    xhandle GetSkinSurface  ( xhandle hGeom, s32 iSurface ) const;

private:
    struct GeomResource
    {
        xarray<xhandle> m_surfaces;
    };

    xharray<GeomResource> m_rigidGeoms;
    xharray<GeomResource> m_skinGeoms;
    VertexMgr*            m_pRigidVertices;
    SoftVertexMgr*        m_pSkinVertices;
};

//=============================================================================
//  GLOBAL INSTANCE
//=============================================================================

extern GeomStorage g_GeomStorage;

//=============================================================================
#endif // GEOM_STORAGE_HPP
//=============================================================================
