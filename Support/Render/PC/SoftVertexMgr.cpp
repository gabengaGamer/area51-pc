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

//#define X_BONE_DEBUG

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
        if( m_lSoftDList[i].pCmd ) 
        {
            delete[]m_lSoftDList[i].pCmd;
            m_lSoftDList[i].pCmd = NULL;
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

    x_try;

    SoftDList.hDList    = vertex_mgr::AddDList( pVertex, nVertices, pIndex, nIndices, nPrims );
    SoftDList.nCommands = nCmds;
    SoftDList.pCmd      = new skin_geom::command_pc[ nCmds ];

    x_memcpy( SoftDList.pCmd, pCmd, sizeof(skin_geom::command_pc)*nCmds );

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

    if(SoftDList.pCmd) 
    {
        delete []SoftDList.pCmd;
        SoftDList.pCmd = NULL;
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

void soft_vertex_mgr::DrawDList( xhandle hDList, const matrix4* pBone, const d3d_lighting* pLighting )
{
    if( !g_pd3dDevice || !g_pd3dContext )
        return;

    soft_dlist& SoftDList = m_lSoftDList( hDList );
    dlist&      DList     = m_lDList    ( SoftDList.hDList  );
    node&       DLVert    = m_lNode     ( DList.hVertexNode );
    node&       DLIndex   = m_lNode     ( DList.hIndexNode  );

    (void)pLighting;

    ActivateStreams( SoftDList.hDList );

    ID3D11Buffer* pBoneBuffer = g_MaterialMgr.GetSkinBoneBuffer();
    if( !pBoneBuffer )
        return;

    cb_skin_bone BoneCache[MAX_SKIN_BONES];
    for( s32 i = 0; i < MAX_SKIN_BONES; ++i )
    {
        BoneCache[i].L2W.Identity();
    }

#ifdef X_BONE_DEBUG
    skin_geom::vertex_pc* pVertData = (skin_geom::vertex_pc*)LockDListVerts( SoftDList.hDList );
    xbool BoneLoaded[MAX_SKIN_BONES];
    x_memset( BoneLoaded, 0, sizeof(BoneLoaded) );
#endif

    for( s32 c = 0; c < SoftDList.nCommands; ++c )
    {
        skin_geom::command_pc& Cmd = SoftDList.pCmd[c];

        switch( Cmd.Cmd )
        {
            case skin_geom::PC_CMD_UPLOAD_MATRIX:
            {
                s16 BoneID  = Cmd.Arg1;
                s16 CacheID = Cmd.Arg2;

                ASSERT( BoneID  >= 0 );
                ASSERT( CacheID >= 0 );
                ASSERT( CacheID < MAX_SKIN_BONES );

                BoneCache[CacheID].L2W = pBone[BoneID];

#ifdef X_BONE_DEBUG
                BoneLoaded[CacheID] = TRUE;
#endif
            }
            break;

            case skin_geom::PC_CMD_DRAW_SECTION:
            {
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

                s16 Start = Cmd.Arg1;
                s16 End   = Cmd.Arg2;

#ifdef X_BONE_DEBUG
                if( pVertData )
                {
                    for( s32 v = Start; v < End; v++ )
                    {
                        s32 i1 = (s32)pVertData[v].Position.GetW();
                        s32 i2 = (s32)pVertData[v].Normal.GetW();
                        f32 w1 = pVertData[v].UVWeights.GetZ();
                        f32 w2 = pVertData[v].UVWeights.GetW();

                        if( (i1 < 0) || (i1 >= MAX_SKIN_BONES) ||
                            (i2 < 0) || (i2 >= MAX_SKIN_BONES) ||
                            !BoneLoaded[i1] || !BoneLoaded[i2] )
                        {
                            x_DebugMsg( "SoftVertexMgr: invalid bones v=%d b1=%d b2=%d\n", v, i1, i2 );
                        }

                        if( x_abs( (w1 + w2) - 1.0f ) > 0.01f )
                        {
                            x_DebugMsg( "SoftVertexMgr: weights != 1 v=%d w1=%f w2=%f\n", v, w1, w2 );
                        }
                    }
                }
#endif

                g_pd3dContext->DrawIndexed( (End - Start) * 3,
                                             DLIndex.Offset + (Start*3),
                                             0 );
            }
            break;

            case skin_geom::PC_CMD_NULL:
                break;

            case skin_geom::PC_CMD_END:
                c = SoftDList.nCommands;
                break;

            default:
                break;
        }
    }

#ifdef X_BONE_DEBUG
    if( pVertData )
        UnlockDListVerts( SoftDList.hDList );
#endif
}

//=========================================================================