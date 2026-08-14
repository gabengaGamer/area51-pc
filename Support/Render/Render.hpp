//=============================================================================
//
//  Render.hpp
//
//=============================================================================

#ifndef RENDER_HPP
#define RENDER_HPP

#include "x_types.hpp"
#include "x_time.hpp"

#include "../ResourceMgr/ResourceMgr.hpp"

#include "Material.hpp"
#include "GeometryDraw.hpp"
#include "PrimitiveBatch.hpp"
#include "PrimitiveDebug.hpp"
#include "RigidGeom.hpp"
#include "SkinGeom.hpp"

//=============================================================================

namespace render
{
// debugging information that can be exposed to the game
#ifndef X_RETAIL
struct debug_options
{
    xbool RenderRigidOnly;
    xbool RenderSkinOnly;
    xbool RenderClippedOnly;
    xbool RenderShadowedOnly;
};
#endif

// startup and shutdown routines
void Init( void ) X_SECTION( init );
void Kill( void ) X_SECTION( init );

// update routines (needed for uv and texture animation)
void Update( f32 deltaTime ) X_SECTION( update );

// Forward primitive submission. The high-level renderer owns the pass.
enum
{
    BLEND_MODE_ADDITIVE = 0,
    BLEND_MODE_SUBTRACTIVE,
    BLEND_MODE_NORMAL,
    BLEND_MODE_INTENSITY
};
enum forward_render_stage
{
    FORWARD_RENDER_PRE_EFFECTS = 0,
    FORWARD_RENDER_POST_EFFECTS,
    FORWARD_RENDER_ALL
};
s32   GetHardwareBufferSize( void ) X_SECTION( render_infrequent );
xbool BeginPrimitiveRender( void ) X_SECTION( render_primitives );
void  EndPrimitiveRender( void ) X_SECTION( render_primitives );
void  ExecuteForwardRender( forward_render_stage stage = FORWARD_RENDER_ALL ) X_SECTION( render_primitives );
xbool SetDepthRect( irect const& rect, f32 depth ) X_SECTION( render_primitives );
// data registration routines
using GeometryInstanceHandle = xhandle;
GeometryInstanceHandle RegisterRigidInstance( rigid_geom& geom ) X_SECTION( init );
void                   UnregisterRigidInstance( GeometryInstanceHandle hInst ) X_SECTION( init );
GeometryInstanceHandle RegisterSkinInstance( skin_geom& geom ) X_SECTION( init );
void                   UnregisterSkinInstance( GeometryInstanceHandle hInst ) X_SECTION( init );
geom const*            GetGeom( GeometryInstanceHandle hInst ) X_SECTION( init );

// send these flags to describe how the object should be rendered.
// most of the time you should just be using 0 and possibly CLIPPED,
// as the other flags are not guaranteed to work on all platforms (they
// are intended for use by the editor)
enum
{
    WIREFRAME = 0x00000001,
    WIREFRAME2 = 0x00000002,
    PULSED = 0x00000004,
    GLOWING = 0x00000010,
    FADING_ALPHA = 0x00000020,
    CLIPPED = 0x00000040,
    FORCE_LAST = 0x01000000,
    DISABLE_SPOTLIGHT = 0x02000000,
    DISABLE_FILTERLIGHT = 0x04000000,
    DISABLE_PROJ_SHADOWS = 0x08000000,
    PERPIXEL_POINTLIGHT = 0x20000000,

    // TODO: Our flags have started clashing. For now, if we make INSTFLAG_CLIPPED
    // the same as CLIPPED we are fine, but these flags really need to be re-thought.
    // Preferably, the render flags should be completely separated from the instance
    // flags, and the render system can internally do whatever it needs to do to make
    // things work.
    // these instance flags are considered private. don't look!

    INSTFLAG_CLIPPED = 0x00000080,      // Does the instance intersect with the frustum?
    INSTFLAG_GLOWING = 0x00000100,      // we have forced something that doesn't normally glow to glow
    INSTFLAG_FILTERLIGHT = 0x00000400,  // modulate vertex lighting (i.e. emergency red light situation)
    INSTFLAG_SPOTLIGHT = 0x00000800,    // we are receiving projected (art) lights
    INSTFLAG_FADING_ALPHA = 0x00001000, // the geometry is fading out
    INSTFLAG_DYNAMICLIGHT = 0x00002000, // dynamic lighting is on (point or directional
    INSTFLAG_DETAIL = 0x00004000,       // detail mapping is on (material still has it, but the object is distant)
    INSTFLAG_PROJ_SHADOW = 0x00010000,  // we are receiving projected (art) shadows
    // INSTFLAG_PROJ_SHADOW_1 = 0x00010000, // legacy per-slot projected shadow flag
    // INSTFLAG_PROJ_SHADOW_2 = 0x00020000, // legacy per-slot projected shadow flag
};

void BeginShadowCreation( void ) X_SECTION( render_infrequent );
void EndShadowCreation( void ) X_SECTION( render_deferred_shadow );
void AddPointShadowMapSource( matrix4 const& l2W, radian fov, f32 lightRadius, f32 lightFalloff,
                              s32 shadowMapResolution, s32 shadowPriority, f32 shadowScore, s32 dynamicLightIndex )
    X_SECTION( render_add_shadow );
void AddSpotShadowMapSource( matrix4 const& l2W, radian fov, f32 lightRadius, f32 lightFalloff, s32 shadowMapResolution,
                             s32 shadowPriority, f32 shadowScore, s32 dynamicLightIndex )
    X_SECTION( render_add_shadow );
void AddRigidCasterSimple( GeometryInstanceHandle hInst,
                           matrix4 const*         pL2W, // will be DMA ref'd to!
                           u64                    shadowSourceMask ) X_SECTION( render_add_shadow );
void AddRigidCaster( GeometryInstanceHandle hInst, matrix4 const* pL2W, u64 mask, u64 shadowSourceMask )
    X_SECTION( render_add_shadow );
void AddSkinCaster( GeometryInstanceHandle hInst, matrix4 const* pBone, s32 nBone, u64 mask, u64 shadowSourceMask )
    X_SECTION( render_add_shadow );
// basic instance-rendering routines
// you should call them in this order:
// BeginNormalRender()
//   for all instances:
//   AddRigidInstanceSimple  OR
//   AddRigidInstance        OR
//   AddSkinInstance         OR
//   AddSkinInstanceDistorted
// EndNormalRender()
// Forward geometry is executed around fog through ExecuteForwardRender().

void BeginNormalRender( void ) X_SECTION( render_infrequent );
void EndNormalRender( void ) X_SECTION( render_deferred );
void ResetAfterException( void ) X_SECTION( render_infrequent );
void AddRigidInstanceSimple( GeometryInstanceHandle hInst, u32 const* pColor,
                             matrix4 const* pL2W, // will be DMA ref'd to!
                             bbox const& worldBBox, u32 flags ) X_SECTION( render_add );
void AddRigidInstance( GeometryInstanceHandle hInst, u32 const* pColor, matrix4 const* pL2W, u64 mask, u32 flags,
                       s32 alpha ) X_SECTION( render_add );
void AddRigidInstance( GeometryInstanceHandle hInst, u32 const* pColor, matrix4 const* pL2W, u64 mask, u32 vTextureMask,
                       u32 flags, s32 alpha ) X_SECTION( render_add );
void AddSkinInstance( GeometryInstanceHandle hInst, matrix4 const* pBone, u64 mask, u32 vTextureMask, u32 flags,
                      xcolor const& ambient ) X_SECTION( render_add );
void AddSkinInstanceDistorted( GeometryInstanceHandle hInst, matrix4 const* pBone, u64 mask, u32 flags,
                               radian3 const& normalRot, xcolor ambient ) X_SECTION( render_add );
void AddDynamicGeometry( dynamic_geometry_draw const& draw ) X_SECTION( render_add );
void AddDynamicShadowCaster( dynamic_geometry_shadow_draw const& draw ) X_SECTION( render_add_shadow );

// material access
material& GetMaterial( GeometryInstanceHandle hInst, s32 iSubMesh ) X_SECTION( render_infrequent );
texture*  GetVTexture( geom const* pGeom, s32 iMaterial, s32 vTextureMask ) X_SECTION( render_infrequent );

// env. map specification routines
void SetAreaCubeMap( cubemap::handle const& cubeMap ) X_SECTION( render_infrequent );

// post-effect rendering routines--the begin and end pair are required and
// allow the platforms to do some optimization by sharing major screen
// buffer work between the post-effects.
enum post_screen_blend
{
    SOURCE_MINUS_DEST = 0,
};

enum post_falloff_fn
{
    FALLOFF_CONSTANT = 0,
    FALLOFF_LINEAR,
    FALLOFF_EXP,
    FALLOFF_EXP2,
    FALLOFF_CUSTOM,
};

// For the post-effects, colors are considered to be 128==1, and 255==2 so that we
// can get oversaturate. The post effects are listed here in the order they will
// occur in. This is done to maximize performance, since some operations can be
// shared between post effects (such as filtering a screen down). If you need
// to do the post-effects in a specific order, you'll need to put them in
// their own Begin/End blocks, but be warned that performance won't be as good.

// If you plan on using the zfog filter, but want to manually fog draw items
// that do not write to the z-buffer or happen outside of the post-effects, use
// these functions.
void SetCustomFogPalette( texture::handle const& texture, xbool immediateSwitch, s32 paletteIndex )
    X_SECTION( render_post );
xcolor GetFogValue( vector3 const& worldPos, s32 paletteIndex ) X_SECTION( render_infrequent );

// Use these functions for doing the actual post-effect
void BeginMidPostEffects( void ) X_SECTION( render_post );        // see comments above render::BeginNormalRender
void EndMidPostEffects( void ) X_SECTION( render_deferred_post ); // see comments above render::BeginNormalRender
void BeginPostEffects( void ) X_SECTION( render_post );
void EndPostEffects( void ) X_SECTION( render_deferred_post );
void AddScreenWarp( vector3 const& worldPos, f32 radius, f32 warpAmount ) X_SECTION( render_post );
void MotionBlur( f32 intensity ) X_SECTION( render_post );
void ApplySelfIllumGlows( f32 motionBlurIntensity = 0.0f, s32 glowCutoff = 255 ) X_SECTION( render_post );
void MultScreen( xcolor multColor, post_screen_blend finalBlend ) X_SECTION( render_post );
void RadialBlur( f32 zoom, radian angle, f32 alphaSub, f32 alphaScale ) X_SECTION( render_post );
void ZFogFilter( post_falloff_fn fn, xcolor color, f32 param1, f32 param2 ) X_SECTION( render_post );
void ZFogFilter( post_falloff_fn fn, s32 paletteIndex ) X_SECTION( render_post );
void MipFilter( s32 nFilters, f32 offset, post_falloff_fn fn, xcolor color, f32 param1, f32 param2, s32 paletteIndex )
    X_SECTION( render_post );
void MipFilter( s32 nFilters, f32 offset, post_falloff_fn fn, texture::handle const& texture, s32 paletteIndex )
    X_SECTION( render_post );
void NoiseFilter( xcolor color ) X_SECTION( render_post );
void ScreenFade( xcolor color ) X_SECTION( render_post );

// Filter lighting functions
void   EnableFilterLight( xbool bEnable ) X_SECTION( render_infrequent );
xbool  IsFilterLightEnabled( void ) X_SECTION( render_infrequent );
void   SetFilterLightColor( xcolor color ) X_SECTION( render_infrequent );
xcolor GetFilterLightColor( void ) X_SECTION( render_infrequent );

// New session methods
void BeginSession( u32 nPlayers ) X_SECTION( init );
void EndSession( void ) X_SECTION( init );
} // namespace render

//=============================================================================

// EXTERNS FOR DEBUGGING
//=============================================================================

#ifndef X_RETAIL
extern render::debug_options g_renderDebug;
#endif

//=============================================================================

#endif // RENDER_HPP
