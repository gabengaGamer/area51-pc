//=============================================================================
//
//  PrimitiveBatch.cpp
//
//=============================================================================

#include "PrimitiveBatch.hpp"

#include "e_Engine.hpp"
#include "x_debug.hpp"

//=============================================================================
//  HELPER FUNCTIONS
//=============================================================================

static 
xcolor ColorFromPackedRGBA( u32 color )
{
    return xcolor( static_cast<u8>( color & 0xff ), static_cast<u8>( ( color >> 8 ) & 0xff ),
                   static_cast<u8>( ( color >> 16 ) & 0xff ), static_cast<u8>( ( color >> 24 ) & 0xff ) );
}

//=============================================================================

static 
xbool AppendQuad( render::primitive_vertex* pVertices,
                  s32                       maxVertices,
                  s32&                      vertexCount,
                  u16*                      pIndices,
                  s32                       maxIndices,
                  s32&                      indexCount,
                  vector3 const*            pPositions,
                  vector2 const*            pUVs,
                  xcolor const&             color )
{
    if( !pVertices || !pIndices || !pPositions || !pUVs ||
        ((vertexCount + 4) > maxVertices) || ((indexCount + 6) > maxIndices) )
    {
        return FALSE;
    }

    const u16 baseVertex = (u16)vertexCount;
    for( s32 i = 0; i < 4; ++i )
        pVertices[vertexCount++] = render::primitive_vertex( pPositions[i], pUVs[i], color );

    const u16 quadIndices[6] = { 0, 1, 2, 2, 3, 0 };
    for( s32 i = 0; i < 6; ++i )
        pIndices[indexCount++] = (u16)(baseVertex + quadIndices[i]);

    return TRUE;
}

//=============================================================================
//  IMPLEMENTATION
//=============================================================================

render::primitive_vertex::primitive_vertex( void )
    : Position( 0.0f, 0.0f, 0.0f ), Color( 0xffffffffu ), UV( 0.0f, 0.0f )
{
}

//=============================================================================

render::primitive_vertex::primitive_vertex( vector3 const& position, vector2 const& uV, xcolor const& color )
    : Position( position ), Color( ( static_cast<u32>( color.A ) << 24 ) | ( static_cast<u32>( color.R ) << 16 ) |
                                  ( static_cast<u32>( color.G ) << 8 ) | static_cast<u32>( color.B ) ),
      UV( uV )
{
}

//=============================================================================

render::primitive_draw_desc::primitive_draw_desc( void )
    : pTexture( NULL ), Topology( PRIMITIVE_TOPOLOGY_TRIANGLE_LIST ), Blend( PRIMITIVE_BLEND_OPAQUE ),
      Depth( PRIMITIVE_DEPTH_READ_WRITE ), Raster( PRIMITIVE_RASTER_SOLID ),
      Sampler( PRIMITIVE_SAMPLER_LINEAR_WRAP ), Output( PRIMITIVE_OUTPUT_COLOR ), Layer( PRIMITIVE_LAYER_SURFACE ),
      DistortionScale( 12.0f )
{
}

//=============================================================================

render::primitive_draw_desc::primitive_draw_desc( texture const* pDrawTexture, primitive_topology topology,
                                                  primitive_blend_mode blend, primitive_depth_mode depth,
                                                  primitive_raster_mode raster, primitive_sampler_mode sampler,
                                                  primitive_render_layer renderLayer, primitive_output_mode outputMode,
                                                  f32 distortionScalePixels )
    : pTexture( pDrawTexture ), Topology( topology ), Blend( blend ), Depth( depth ), Raster( raster ),
      Sampler( sampler ), Output( outputMode ), Layer( renderLayer ), DistortionScale( distortionScalePixels )
{
    ASSERT( ( Topology >= PRIMITIVE_TOPOLOGY_TRIANGLE_LIST ) && ( Topology < PRIMITIVE_TOPOLOGY_COUNT ) );
    ASSERT( ( Blend >= PRIMITIVE_BLEND_OPAQUE ) && ( Blend < PRIMITIVE_BLEND_COUNT ) );
    ASSERT( ( Depth >= PRIMITIVE_DEPTH_DISABLED ) && ( Depth < PRIMITIVE_DEPTH_COUNT ) );
    ASSERT( ( Raster >= PRIMITIVE_RASTER_SOLID ) && ( Raster < PRIMITIVE_RASTER_COUNT ) );
    ASSERT( ( Sampler >= PRIMITIVE_SAMPLER_LINEAR_WRAP ) && ( Sampler < PRIMITIVE_SAMPLER_COUNT ) );
    ASSERT( ( Layer >= PRIMITIVE_LAYER_SURFACE ) && ( Layer < PRIMITIVE_LAYER_COUNT ) );
    ASSERT( ( Output >= PRIMITIVE_OUTPUT_COLOR ) && ( Output < PRIMITIVE_OUTPUT_COUNT ) );
    ASSERT( ( Output == PRIMITIVE_OUTPUT_DISTORTION ) == ( Layer == PRIMITIVE_LAYER_DISTORTION ) );
}

//=============================================================================

render::PrimitiveBatch::PrimitiveBatch( primitive_draw_desc const& desc )
    : m_desc( desc ), m_vertices(), m_indices()
{
}

//=============================================================================

void render::PrimitiveBatch::Reserve( s32 nVertices, s32 nIndices )
{
    if ( ( nVertices > m_vertices.GetCapacity() ) && ( nVertices <= MAX_PRIMITIVE_VERTICES ) )
    {
        m_vertices.SetCapacity( nVertices );
    }

    if ( nIndices > m_indices.GetCapacity() )
    {
        m_indices.SetCapacity( nIndices );
    }
}

//=============================================================================

void render::PrimitiveBatch::Clear( void )
{
    m_vertices.SetCount( 0 );
    m_indices.SetCount( 0 );
}

//=============================================================================

xbool render::PrimitiveBatch::CanAppend( s32 nVertices, s32 nIndices ) const
{
    return ( nVertices > 0 ) && ( nIndices > 0 ) &&
           ( ( m_vertices.GetCount() + nVertices ) <= MAX_PRIMITIVE_VERTICES );
}

//=============================================================================

xbool render::PrimitiveBatch::AppendIndexed( primitive_vertex const* pVertices, s32 nVertices, u16 const* pIndices,
                                             s32 nIndices )
{
    if ( !pVertices || !pIndices || !CanAppend( nVertices, nIndices ) )
    {
        return FALSE;
    }

    for ( s32 i = 0; i < nIndices; ++i )
    {
        if ( pIndices[i] >= nVertices )
        {
            return FALSE;
        }
    }

    u16 const baseVertex = static_cast<u16>( m_vertices.GetCount() );
    for ( s32 i = 0; i < nVertices; ++i )
    {
        m_vertices.Append() = pVertices[i];
    }

    for ( s32 i = 0; i < nIndices; ++i )
    {
        m_indices.Append() = static_cast<u16>( baseVertex + pIndices[i] );
    }

    return TRUE;
}

//=============================================================================

xbool render::PrimitiveBatch::AddTriangle( primitive_vertex const& vertex0, primitive_vertex const& vertex1,
                                           primitive_vertex const& vertex2 )
{
    if ( m_desc.Topology != PRIMITIVE_TOPOLOGY_TRIANGLE_LIST )
    {
        return FALSE;
    }

    primitive_vertex const vertices[3] = { vertex0, vertex1, vertex2 };
    u16 const              indices[3] = { 0, 1, 2 };
    return AppendIndexed( vertices, 3, indices, 3 );
}

//=============================================================================

xbool render::PrimitiveBatch::AddLine( primitive_vertex const& vertex0, primitive_vertex const& vertex1 )
{
    if ( m_desc.Topology != PRIMITIVE_TOPOLOGY_LINE_LIST )
    {
        return FALSE;
    }

    primitive_vertex const vertices[2] = { vertex0, vertex1 };
    u16 const              indices[2] = { 0, 1 };
    return AppendIndexed( vertices, 2, indices, 2 );
}

//=============================================================================

xbool render::PrimitiveBatch::AddLine( vector3 const& position0, vector3 const& position1, xcolor const& color0,
                                       xcolor const& color1 )
{
    return AddLine( primitive_vertex( position0, vector2( 0.0f, 0.0f ), color0 ),
                    primitive_vertex( position1, vector2( 0.0f, 0.0f ), color1 ) );
}

//=============================================================================

xbool render::PrimitiveBatch::AddQuad( vector3 const* pPositions, vector2 const* pUVs, xcolor const* pColors )
{
    if ( ( m_desc.Topology != PRIMITIVE_TOPOLOGY_TRIANGLE_LIST ) || !pPositions || !pUVs || !pColors )
    {
        return FALSE;
    }

    primitive_vertex const vertices[4] = {
        primitive_vertex( pPositions[0], pUVs[0], pColors[0] ),
        primitive_vertex( pPositions[1], pUVs[1], pColors[1] ),
        primitive_vertex( pPositions[2], pUVs[2], pColors[2] ),
        primitive_vertex( pPositions[3], pUVs[3], pColors[3] ) };
    u16 const indices[6] = { 0, 1, 2, 2, 3, 0 };
    return AppendIndexed( vertices, 4, indices, 6 );
}

//=============================================================================

xbool render::PrimitiveBatch::AddTriangleStripQuad( vector3 const* pPositions, vector2 const* pUVs,
                                                    xcolor const* pColors )
{
    if ( ( m_desc.Topology != PRIMITIVE_TOPOLOGY_TRIANGLE_LIST ) || !pPositions || !pUVs || !pColors )
    {
        return FALSE;
    }

    primitive_vertex const vertices[4] = {
        primitive_vertex( pPositions[0], pUVs[0], pColors[0] ),
        primitive_vertex( pPositions[1], pUVs[1], pColors[1] ),
        primitive_vertex( pPositions[2], pUVs[2], pColors[2] ),
        primitive_vertex( pPositions[3], pUVs[3], pColors[3] ) };
    u16 const indices[6] = { 0, 1, 2, 2, 1, 3 };
    return AppendIndexed( vertices, 4, indices, 6 );
}

//=============================================================================

xbool render::PrimitiveBatch::AddViewOrientedQuad( vector3 const& position0, vector3 const& position1,
                                                   vector3 const& viewPosition, vector2 const& uV0, vector2 const& uV1,
                                                   xcolor const& color0, xcolor const& color1, f32 radius0,
                                                   f32 radius1 )
{
    vector3 direction = position1 - position0;
    if ( !direction.SafeNormalize() )
    {
        return FALSE;
    }

    vector3 cross = direction.Cross( viewPosition - position0 );
    if ( !cross.SafeNormalize() )
    {
        return FALSE;
    }

    vector3 const cross0 = cross * radius0;
    vector3 const cross1 = cross * radius1;
    vector3 const positions[4] = { position1 + cross1, position1 - cross1, position0 - cross0, position0 + cross0 };
    vector2 const uVs[4] = { vector2( uV1.X, uV1.Y ), vector2( uV1.X, uV0.Y ), vector2( uV0.X, uV0.Y ),
                             vector2( uV0.X, uV1.Y ) };
    xcolor const colors[4] = { color1, color1, color0, color0 };
    return AddQuad( positions, uVs, colors );
}

//=============================================================================

xbool render::PrimitiveBatch::AddViewOrientedStrand( vector3 const* pPositions, s32 nPositions,
                                                     vector3 const& viewPosition, vector2 const& uV0,
                                                     vector2 const& uV1, xcolor const& color0, xcolor const& color1,
                                                     f32 radius )
{
    if ( !pPositions || ( nPositions < 2 ) )
    {
        return FALSE;
    }

    for ( s32 i = 1; i < nPositions; ++i )
    {
        f32 const t0 = static_cast<f32>( i - 1 ) / static_cast<f32>( nPositions - 1 );
        f32 const t1 = static_cast<f32>( i ) / static_cast<f32>( nPositions - 1 );
        vector2 const strandUV0( uV0.X + ( uV1.X - uV0.X ) * t0, uV0.Y );
        vector2 const strandUV1( uV0.X + ( uV1.X - uV0.X ) * t1, uV1.Y );

        if ( !AddViewOrientedQuad( pPositions[i - 1], pPositions[i], viewPosition, strandUV0, strandUV1, color0,
                                   color1, radius, radius ) )
        {
            return FALSE;
        }
    }

    return TRUE;
}

//=============================================================================

xbool render::PrimitiveBatch::AddPlaneAlignedQuad( vector3 const& center, vector3 const& normal, f32 radius,
                                                   xcolor const& color )
{
    vector3 unitNormal = normal;
    if ( !unitNormal.SafeNormalize() || ( radius <= 0.0f ) )
    {
        return FALSE;
    }

    vector3 axisA;
    vector3 axisB;
    if ( x_abs( unitNormal.GetY() ) < 0.001f )
    {
        axisA = unitNormal.Cross( vector3( 0.0f, 1.0f, 0.0f ) );
        axisB = unitNormal.Cross( axisA );
        if ( !axisA.SafeNormalize() || !axisB.SafeNormalize() )
        {
            return FALSE;
        }
    }
    else
    {
        plane orientationPlane( unitNormal, 0.0f );
        orientationPlane.GetOrthoVectors( axisA, axisB );
    }

    axisA *= radius;
    axisB *= radius;
    vector3 const positions[4] = { center + axisA + axisB, center + axisA - axisB, center - axisA - axisB,
                                   center - axisA + axisB };
    vector2 const uVs[4] = { vector2( 0.0f, 0.0f ), vector2( 1.0f, 0.0f ), vector2( 1.0f, 1.0f ),
                             vector2( 0.0f, 1.0f ) };
    xcolor const colors[4] = { color, color, color, color };
    return AddQuad( positions, uVs, colors );
}

//=============================================================================

xbool render::PrimitiveBatch::Submit( matrix4 const& localToWorld ) const
{
    if ( ( m_vertices.GetCount() == 0 ) || ( m_indices.GetCount() == 0 ) )
    {
        return TRUE;
    }

    return SubmitPrimitives( m_desc, localToWorld, m_vertices.GetPtr(), m_vertices.GetCount(), m_indices.GetPtr(),
                             m_indices.GetCount() );
}

//=============================================================================

render::primitive_draw_desc const& render::PrimitiveBatch::GetDesc( void ) const
{
    return m_desc;
}

//=============================================================================

s32 render::PrimitiveBatch::GetVertexCount( void ) const
{
    return m_vertices.GetCount();
}

//=============================================================================

s32 render::PrimitiveBatch::GetIndexCount( void ) const
{
    return m_indices.GetCount();
}

//=============================================================================

xbool render::SubmitPrimitiveSprite( primitive_draw_desc const& desc, vector3 const& position, vector2 const& size,
                                     vector2 const& uV0, vector2 const& uV1, xcolor const& color, radian rotation )
{
    view const* pView = eng_GetView();
    if ( !pView || !position.IsValid() || !size.IsValid() || !uV0.IsValid() || !uV1.IsValid() ||
         !x_isvalid( rotation ) )
    {
        return FALSE;
    }

    // FX scale controllers legitimately cross zero and use negative values to
    // mirror sprites. A zero-sized sprite is a successful, empty submission.
    if ( ( size.X == 0.0f ) || ( size.Y == 0.0f ) )
    {
        return TRUE;
    }

    vector3 center = pView->GetW2V() * position;
    f32 sine;
    f32 cosine;
    x_sincos( -rotation, sine, cosine );

    f32 const     halfWidth = size.X * 0.5f;
    f32 const     halfHeight = size.Y * 0.5f;
    vector3 const v0( cosine * halfWidth - sine * halfHeight, sine * halfWidth + cosine * halfHeight, 0.0f );
    vector3 const v1( cosine * halfWidth + sine * halfHeight, sine * halfWidth - cosine * halfHeight, 0.0f );
    vector3 const positions[4] = { center + v0, center + v1, center - v0, center - v1 };
    vector2 const uVs[4] = { vector2( uV0.X, uV0.Y ), vector2( uV0.X, uV1.Y ), vector2( uV1.X, uV1.Y ),
                             vector2( uV1.X, uV0.Y ) };
    primitive_vertex vertices[4];
    u16 indices[6];
    s32 vertexCount = 0;
    s32 indexCount = 0;
    return (desc.Topology == PRIMITIVE_TOPOLOGY_TRIANGLE_LIST) &&
           AppendQuad( vertices, 4, vertexCount, indices, 6, indexCount, positions, uVs, color ) &&
           SubmitPrimitives( desc, pView->GetV2W(), vertices, vertexCount, indices, indexCount );
}

//=============================================================================

xbool render::SubmitPrimitiveBillboards( primitive_draw_desc const& desc, s32 nSprites, f32 uniformScale,
                                         matrix4 const* pLocalToWorld, vector4 const* pPositions,
                                         vector2 const* pRotScales, u32 const* pColors )
{
    if ( ( nSprites <= 0 ) || !pPositions || !pRotScales || !pColors )
    {
        return FALSE;
    }

    view const* pView = eng_GetView();
    if ( !pView )
    {
        return FALSE;
    }

    matrix4 localToView = pView->GetW2V();
    if ( pLocalToWorld )
    {
        localToView = localToView * ( *pLocalToWorld );
    }

    enum
    {
        MAX_SPRITES_PER_BATCH = 256,
        MAX_VERTICES_PER_BATCH = MAX_SPRITES_PER_BATCH * 4,
        MAX_INDICES_PER_BATCH = MAX_SPRITES_PER_BATCH * 6
    };
    if( desc.Topology != PRIMITIVE_TOPOLOGY_TRIANGLE_LIST )
        return FALSE;

    primitive_vertex vertices[MAX_VERTICES_PER_BATCH];
    u16 indices[MAX_INDICES_PER_BATCH];

    s32 const maxSpritesPerBatch = MAX_SPRITES_PER_BATCH;
    for ( s32 firstSprite = 0; firstSprite < nSprites; firstSprite += maxSpritesPerBatch )
    {
        s32 const endSprite = MIN( firstSprite + maxSpritesPerBatch, nSprites );
        s32 vertexCount = 0;
        s32 indexCount = 0;

        for ( s32 i = firstSprite; i < endSprite; ++i )
        {
            if ( ( pPositions[i].GetIW() & 0x8000 ) == 0x8000 )
            {
                continue;
            }

            vector3 const center =
                localToView * vector3( pPositions[i].GetX(), pPositions[i].GetY(), pPositions[i].GetZ() );
            f32 sine;
            f32 cosine;
            x_sincos( -pRotScales[i].X, sine, cosine );

            f32 const     scale = pRotScales[i].Y * uniformScale;
            vector3 const v0( ( cosine - sine ) * scale, ( sine + cosine ) * scale, 0.0f );
            vector3 const v1( ( cosine + sine ) * scale, ( sine - cosine ) * scale, 0.0f );
            vector3 const positions[4] = { center + v0, center + v1, center - v0, center - v1 };
            vector2 const uVs[4] = { vector2( 0.0f, 0.0f ), vector2( 0.0f, 1.0f ), vector2( 1.0f, 1.0f ),
                                     vector2( 1.0f, 0.0f ) };
            xcolor const color = ColorFromPackedRGBA( pColors[i] );
            if ( !AppendQuad( vertices, MAX_VERTICES_PER_BATCH, vertexCount,
                              indices, MAX_INDICES_PER_BATCH, indexCount,
                              positions, uVs, color ) )
            {
                return FALSE;
            }
        }

        if ( (vertexCount > 0) &&
             !SubmitPrimitives( desc, pView->GetV2W(), vertices, vertexCount, indices, indexCount ) )
        {
            return FALSE;
        }
    }

    return TRUE;
}

//=============================================================================

xbool render::SubmitPrimitiveVelocityBillboards( primitive_draw_desc const& desc, s32 nSprites, f32 uniformScale,
                                                 matrix4 const* pLocalToWorld, matrix4 const* pVelocityMatrix,
                                                 vector4 const* pPositions,
                                                 vector4 const* pVelocities, u32 const* pColors )
{
    if ( ( nSprites <= 0 ) || !x_isvalid( uniformScale ) || !pPositions || !pVelocities || !pColors ||
         ( pLocalToWorld && !pLocalToWorld->IsValid() ) ||
         ( pVelocityMatrix && !pVelocityMatrix->IsValid() ) )
    {
        return FALSE;
    }

    view const* pView = eng_GetView();
    if ( !pView )
    {
        return FALSE;
    }

    matrix4 localToWorld;
    if ( pLocalToWorld )
    {
        localToWorld = *pLocalToWorld;
    }
    else
    {
        localToWorld.Identity();
    }

    matrix4 velocityToWorld = localToWorld;
    velocityToWorld.ClearTranslation();
    if ( pVelocityMatrix )
    {
        velocityToWorld = velocityToWorld * ( *pVelocityMatrix );
    }
    vector3 const viewDirection = pView->GetViewZ();
    matrix4 identity;
    identity.Identity();

    enum
    {
        MAX_SPRITES_PER_BATCH = 256,
        MAX_VERTICES_PER_BATCH = MAX_SPRITES_PER_BATCH * 4,
        MAX_INDICES_PER_BATCH = MAX_SPRITES_PER_BATCH * 6
    };
    if( desc.Topology != PRIMITIVE_TOPOLOGY_TRIANGLE_LIST )
        return FALSE;

    primitive_vertex vertices[MAX_VERTICES_PER_BATCH];
    u16 indices[MAX_INDICES_PER_BATCH];

    s32 const maxSpritesPerBatch = MAX_SPRITES_PER_BATCH;
    for ( s32 firstSprite = 0; firstSprite < nSprites; firstSprite += maxSpritesPerBatch )
    {
        s32 const endSprite = MIN( firstSprite + maxSpritesPerBatch, nSprites );
        s32 vertexCount = 0;
        s32 indexCount = 0;

        for ( s32 i = firstSprite; i < endSprite; ++i )
        {
            if ( ( pPositions[i].GetIW() & 0x8000 ) == 0x8000 )
            {
                continue;
            }

            vector3 const center =
                localToWorld * vector3( pPositions[i].GetX(), pPositions[i].GetY(), pPositions[i].GetZ() );
            vector3 right( pVelocities[i].GetX(), pVelocities[i].GetY(), pVelocities[i].GetZ() );
            right = velocityToWorld * right;
            if ( !right.SafeNormalize() )
            {
                continue;
            }

            vector3 up = viewDirection.Cross( right );
            f32 const scale = pVelocities[i].GetW() * uniformScale;
            right *= scale;
            up *= scale;
            vector3 const positions[4] = { center + right - up, center - right - up, center - right + up,
                                           center + right + up };
            vector2 const uVs[4] = { vector2( 1.0f, 0.0f ), vector2( 0.0f, 0.0f ), vector2( 0.0f, 1.0f ),
                                     vector2( 1.0f, 1.0f ) };
            xcolor const color = ColorFromPackedRGBA( pColors[i] );
            if ( !AppendQuad( vertices, MAX_VERTICES_PER_BATCH, vertexCount,
                              indices, MAX_INDICES_PER_BATCH, indexCount,
                              positions, uVs, color ) )
            {
                return FALSE;
            }
        }

        if ( (vertexCount > 0) &&
             !SubmitPrimitives( desc, identity, vertices, vertexCount, indices, indexCount ) )
        {
            return FALSE;
        }
    }

    return TRUE;
}