//=========================================================================
//
//  RigidGeom.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "RigidGeom.hpp"
#include "GeomFile.hpp"
#include "ResourceMgr/ResourceMgr.hpp"

//=========================================================================
//  RESOURCE LOADER
//=========================================================================

static struct rigid_geom_loader : public rsc_loader
{
    rigid_geom_loader( void ) : rsc_loader( "RIGID GEOM", ".rigidgeom" )
    {
    }

    //--------------------------------------------------------------------------

    virtual void* PreLoad( X_FILE* pFile )
    {
        MEMORY_OWNER( "RIGID GEOM DATA" );

        rigid_geom* pGeom = NULL;
        xstring     error;
        if ( !geom_file::LoadRigid( pFile, pGeom, error ) )
        {
            x_DebugMsg( "RIGIDGEOM: load failed: %s\n", (char const*)error );
            x_throw( (char const*)error );
        }

        return ( pGeom );
    }

    //--------------------------------------------------------------------------

    virtual void* Resolve( void* pData )
    {
        return ( pData );
    }

    //--------------------------------------------------------------------------

    virtual void Unload( void* pData )
    {
        rigid_geom* pRigidGeom = static_cast<rigid_geom*>( pData );
        ASSERT( pRigidGeom );

        delete pRigidGeom;
    }

} s_RigidGeomLoader;

//=========================================================================
//  RIGID GEOM
//=========================================================================

rigid_geom::rigid_geom( void )
    : geom(), m_collision(), m_nSections( 0 ), m_pSection( NULL ), m_nIndices( 0 ), m_pIndex( NULL ),
      m_nVertexData( 0 ), m_pVertex( NULL )
{
}

//=========================================================================

rigid_geom::~rigid_geom( void )
{
    delete[] m_collision.pHighCluster;
    delete[] m_collision.pHighIndexToVert0;
    delete[] m_collision.pLowCluster;
    delete[] m_collision.pLowVector;
    delete[] m_collision.pLowQuad;
    delete[] m_pSection;
    delete[] m_pIndex;
    delete[] m_pVertex;
}

//=========================================================================

extern xbool RigidGeom_GetTriangle( rigid_geom const* pRigidGeom, s32 key, vector3& p0, vector3& p1, vector3& p2 );

//=========================================================================

xbool rigid_geom::GetGeoTri( s32 key, vector3& v0, vector3& v1, vector3& v2 ) const
{
    if ( key == -1 )
    {
        return ( FALSE );
    }

    return ( RigidGeom_GetTriangle( this, key, v0, v1, v2 ) );
}
