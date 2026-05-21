//==========================================================================
//  
//  Soft Vertex Manager for PC
//  
//==========================================================================

//==========================================================================
//  PLATFORM CHECK
//==========================================================================

#include "x_types.hpp"

#if !defined(TARGET_PC)
#error "This is only for the PC target platform. Please check build exclusion rules"
#endif

//=========================================================================
// INCLUDES
//=========================================================================

#include "SoftVertexMgr.hpp"

//=========================================================================
//  GLOBAL INSTANCE
//=========================================================================

soft_vertex_mgr g_SkinVertMgr;

//=========================================================================
// IMPLEMENTATION
//=========================================================================

void soft_vertex_mgr::Init( void )
{
    // Initialize base vertex manager with proper stride
    vertex_mgr::Init( sizeof( skin_geom::vertex_pc ) );
}

//=========================================================================

void soft_vertex_mgr::Kill( void )
{
    // Clear vertex stuff
    vertex_mgr::Kill();

    // Clear all the soft dlists
    for( s32 i=0; i<m_lSoftDList.GetCount(); i++ )
    {
        if( m_lSoftDList[i].pSection ) 
        {
            delete[]m_lSoftDList[i].pSection;
            m_lSoftDList[i].pSection = NULL;
        }
    }

    m_lSoftDList.Clear();
}

//=========================================================================

xhandle soft_vertex_mgr::AddDList( 
    void*                   pVertex, 
    s32                     nVertices, 
    u16*                    pIndex, 
    s32                     nIndices, 
    s32                     nPrims, 
    s32                     nCmds, 
    skin_geom::command_pc*  pCmd )
{
    xhandle     hSoftDList;
    soft_dlist& SoftDList = m_lSoftDList.Add( hSoftDList );
    xarray<soft_section> Sections;

    x_try;

    SoftDList.hDList    = vertex_mgr::AddDList( pVertex, nVertices, pIndex, nIndices, nPrims );
    SoftDList.nSections = 0;
    SoftDList.pSection  = NULL;

    {
        u16 BoneRemap[MAX_SKIN_BONES];

        for( s32 i = 0; i < MAX_SKIN_BONES; ++i )
            BoneRemap[i] = INVALID_BONE_REMAP;

        for( s32 c = 0; c < nCmds; ++c )
        {
            const skin_geom::command_pc& Cmd = pCmd[c];

            switch( Cmd.Cmd )
            {
                case skin_geom::PC_CMD_UPLOAD_MATRIX:
                {
                    const s16 BoneID  = Cmd.Arg1;
                    const s16 CacheID = Cmd.Arg2;

                    ASSERT( BoneID >= 0 );
                    ASSERT( CacheID >= 0 );
                    ASSERT( CacheID < MAX_SKIN_BONES );

                    BoneRemap[CacheID] = (u16)BoneID;
                }
                break;

                case skin_geom::PC_CMD_DRAW_SECTION:
                {
                    soft_section& Section = Sections.Append();
                    Section.Start = Cmd.Arg1;
                    Section.End   = Cmd.Arg2;
                    x_memcpy( Section.BoneRemap, BoneRemap, sizeof(BoneRemap) );
                }
                break;

                case skin_geom::PC_CMD_END:
                    goto sections_done;

                case skin_geom::PC_CMD_NULL:
                default:
                    break;
            }
        }
        sections_done:;
    }

    SoftDList.nSections = Sections.GetCount();
    if( SoftDList.nSections > 0 )
    {
        SoftDList.pSection = new soft_section[ SoftDList.nSections ];
        x_memcpy( SoftDList.pSection,
                  &Sections[0],
                  sizeof(soft_section) * SoftDList.nSections );
    }

    x_catch_begin;

    m_lSoftDList.DeleteByHandle( hSoftDList );
    
    x_catch_end_ret;

    return hSoftDList;
}

//=========================================================================

void soft_vertex_mgr::DelDList( xhandle hDList )
{
    soft_dlist& SoftDList = m_lSoftDList( hDList );

    vertex_mgr::DelDList( SoftDList.hDList );

    if(SoftDList.pSection) 
    {
        delete []SoftDList.pSection;
        SoftDList.pSection = NULL;
    }
 
    m_lSoftDList.DeleteByHandle( hDList );
}

//=========================================================================

void soft_vertex_mgr::InvalidateCache( void )
{
    vertex_mgr::InvalidateCache();
}

//=========================================================================

void soft_vertex_mgr::BeginRender( void )
{
    vertex_mgr::BeginRender();
}

//=========================================================================

void soft_vertex_mgr::DrawDList( xhandle hDList, const matrix4* pBone )
{
    if( !g_pd3dDevice || !g_pd3dContext )
        return;

    soft_dlist& SoftDList = m_lSoftDList( hDList );
    dlist&      DList     = m_lDList    ( SoftDList.hDList );
    node&       DLIndex   = m_lNode     ( DList.hIndexNode );

    ActivateStreams( SoftDList.hDList );

    ID3D11Buffer* pBoneBuffer = g_GeomMgr.GetSkinBoneBuffer();
    if( !pBoneBuffer )
        return;

    for( s32 s = 0; s < SoftDList.nSections; ++s )
    {
        cb_skin_bone BoneCache[MAX_SKIN_BONES];
        const soft_section& Section = SoftDList.pSection[s];

        for( s32 i = 0; i < MAX_SKIN_BONES; ++i )
        {
            BoneCache[i].L2W.Identity();

            const u16 BoneID = Section.BoneRemap[i];
            if( pBone && ( BoneID != INVALID_BONE_REMAP ) )
            {
                BoneCache[i].L2W = pBone[BoneID];
            }
        }

        D3D11_MAPPED_SUBRESOURCE mappedResource;
        HRESULT hr = g_pd3dContext->Map( pBoneBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource );
        if( SUCCEEDED( hr ) )
        {
            x_memcpy( mappedResource.pData, BoneCache, sizeof(BoneCache) );
            g_pd3dContext->Unmap( pBoneBuffer, 0 );
        }
        else
        {
            x_DebugMsg( "SoftVertexMgr: failed to map skin bone buffer (0x%08X)\n", hr );
            break;
        }

        g_pd3dContext->VSSetConstantBuffers( 2, 1, &pBoneBuffer );
        g_pd3dContext->DrawIndexed( ( Section.End - Section.Start ) * 3,
                                    DLIndex.Offset + ( Section.Start * 3 ),
                                    0 );
    }
}

//=========================================================================

void soft_vertex_mgr::DrawDListInstanced( xhandle hDList, s32 nInstances )
{
    if( !g_pd3dDevice || !g_pd3dContext || ( nInstances <= 0 ) )
        return;

    soft_dlist& SoftDList = m_lSoftDList( hDList );
    dlist&      DList     = m_lDList    ( SoftDList.hDList );
    node&       DLIndex   = m_lNode     ( DList.hIndexNode );

    ActivateStreams( SoftDList.hDList );

    for( s32 s = 0; s < SoftDList.nSections; ++s )
    {
        const soft_section& Section = SoftDList.pSection[s];
        if( !g_GeomMgr.SetSkinSectionRemap( Section.BoneRemap ) )
            break;

        g_pd3dContext->DrawIndexedInstanced( ( Section.End - Section.Start ) * 3,
                                             nInstances,
                                             DLIndex.Offset + ( Section.Start * 3 ),
                                             0,
                                             0 );
    }
}
