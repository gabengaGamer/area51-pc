//=============================================================================
//
//  GeometryDraw.hpp
//
//  Frame geometry submitted by the common render layer to the renderer.
//
//=============================================================================

#ifndef GEOMETRY_DRAW_HPP
#define GEOMETRY_DRAW_HPP

//=============================================================================
//  INCLUDES
//=============================================================================

#include "x_math.hpp"
#include "x_types.hpp"

//=============================================================================
//  FORWARD DECLARATIONS
//=============================================================================

class material;
class texture;
struct shader_resource;
struct vram_texture;
struct rigid_geom;
struct skin_geom;

//=============================================================================
//  TYPES
//=============================================================================

enum geometry_draw_type
{
    GEOMETRY_DRAW_RIGID = 0,
    GEOMETRY_DRAW_SKIN
};

//-----------------------------------------------------------------------------

enum geometry_render_pass
{
    GEOMETRY_PASS_GBUFFER = 0,
    GEOMETRY_PASS_TRANSPARENT,
    GEOMETRY_PASS_ADDITIVE,
    GEOMETRY_PASS_FORCE_LAST,
    GEOMETRY_PASS_ZPRIME,
    GEOMETRY_PASS_FADING,
    GEOMETRY_PASS_DISTORTION,
    GEOMETRY_PASS_SHADOW_CAST
};

//-----------------------------------------------------------------------------

struct rigid_geometry_draw
{
    rigid_geom*    pGeom;
    matrix4 const* pL2W;
    u32 const*     pColorInfo;
};

//-----------------------------------------------------------------------------

struct skin_geometry_draw
{
    skin_geom*     pGeom;
    matrix4 const* pBones;
    s32            BoneCount;
};

//-----------------------------------------------------------------------------

struct dynamic_geometry_vertex
{
    vector3 Position;
    vector3 Normal;
    vector2 UV;
    xcolor  Color;
};

//-----------------------------------------------------------------------------

struct dynamic_geometry_draw
{
    dynamic_geometry_vertex const* pVertices;
    u16 const*                     pIndices;
    texture const*                 pDiffuseTexture;
    shader_resource const*         pDamageMask;
    vram_texture*                  pDamageTexture;
    u8 const*                      pDamageUpload;
    xbool*                         pDamageUploadPending;
    s32                            DamageUploadX;
    s32                            DamageUploadY;
    s32                            DamageUploadWidth;
    s32                            DamageUploadHeight;
    s32                            VertexCount;
    s32                            IndexCount;
    bbox                           Bounds;
    void const*                    pLighting;
    u32                            Flags;
    u32                            Sequence;

    dynamic_geometry_draw( void );
};

//-----------------------------------------------------------------------------

struct dynamic_geometry_shadow_draw
{
    dynamic_geometry_vertex const* pVertices;
    u16 const*                     pIndices;
    shader_resource const*         pDiffuse;
    shader_resource const*         pDamageMask;
    vram_texture*                  pDamageTexture;
    u8 const*                      pDamageUpload;
    xbool*                         pDamageUploadPending;
    s32                            DamageUploadX;
    s32                            DamageUploadY;
    s32                            DamageUploadWidth;
    s32                            DamageUploadHeight;
    s32                            VertexCount;
    s32                            IndexCount;
    u64                            ShadowSourceMask;

    dynamic_geometry_shadow_draw( void );
};

//-----------------------------------------------------------------------------

union geometry_draw_data
{
    rigid_geometry_draw Rigid;
    skin_geometry_draw  Skin;
};

//-----------------------------------------------------------------------------

struct geometry_draw_item
{
    geometry_render_pass Pass;
    geometry_draw_type   Type;
    xhandle              hRenderGeom;
    material const*      pMaterial;
    u32                  MaterialOrder;
    s32                  iSurface;
    u32                  Flags;
    void const*          pLighting;
    geometry_draw_data   Data;
    radian3              DistortionNormalRot;
    f32                  SortDepth;
    u32                  Sequence;
    s32                  ShadowSourceIndex;
    u8                   UOffset;
    u8                   VOffset;
    u8                   Alpha;
    u8                   MaterialOverride;

    geometry_draw_item( void );
};

//=============================================================================

inline geometry_draw_item::geometry_draw_item( void )
    : Pass( GEOMETRY_PASS_GBUFFER ), Type( GEOMETRY_DRAW_RIGID ), hRenderGeom(), pMaterial( NULL ), MaterialOrder( 0 ),
      iSurface( -1 ), Flags( 0 ), pLighting( NULL ), Data(), DistortionNormalRot(), SortDepth( 0.0f ), Sequence( 0 ),
      ShadowSourceIndex( -1 ), UOffset( 0 ), VOffset( 0 ), Alpha( 255 ), MaterialOverride( FALSE )
{
    hRenderGeom.Handle = HNULL;
    DistortionNormalRot.Zero();
    x_memset( &Data, 0, sizeof( Data ) );
}

//=============================================================================

inline dynamic_geometry_draw::dynamic_geometry_draw( void )
    : pVertices( NULL ), pIndices( NULL ), pDiffuseTexture( NULL ), pDamageMask( NULL ), pDamageTexture( NULL ),
      pDamageUpload( NULL ), pDamageUploadPending( NULL ), DamageUploadX( 0 ), DamageUploadY( 0 ),
      DamageUploadWidth( 0 ), DamageUploadHeight( 0 ), VertexCount( 0 ), IndexCount( 0 ), Bounds(), pLighting( NULL ),
      Flags( 0 ), Sequence( 0 )
{
}

//=============================================================================

inline dynamic_geometry_shadow_draw::dynamic_geometry_shadow_draw( void )
    : pVertices( NULL ), pIndices( NULL ), pDiffuse( NULL ), pDamageMask( NULL ), pDamageTexture( NULL ),
      pDamageUpload( NULL ), pDamageUploadPending( NULL ), DamageUploadX( 0 ), DamageUploadY( 0 ),
      DamageUploadWidth( 0 ), DamageUploadHeight( 0 ), VertexCount( 0 ), IndexCount( 0 ), ShadowSourceMask( 0 )
{
}

//=============================================================================
#endif // GEOMETRY_DRAW_HPP
//=============================================================================
