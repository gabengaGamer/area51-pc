//=============================================================================
//
//  platform_Render.hpp
//
//=============================================================================

#ifndef PLATFORM_RENDER_HPP
#define PLATFORM_RENDER_HPP

//=============================================================================
//  includes
//=============================================================================

#include "Render/Render.hpp"
#include "Render/DecalBatch.hpp"
#include "Render/PrimitiveBatch.hpp"
#include "Render/GeometryDraw.hpp"
#include "x_array.hpp"

//=============================================================================

// These functions are for internal rendering use, and you should never call
// them directly. If you need to, then there is a flaw in the system.

//=============================================================================
// init/kill functions
//=============================================================================
static void    platform_Init( void ) X_SECTION( init );
static void    platform_Kill( void ) X_SECTION( init );
static void    platform_BeginSession( u32 nPlayers ) X_SECTION( init );
static void    platform_EndSession( void ) X_SECTION( init );
static xhandle platform_RegisterSkinGeom( skin_geom const& geom ) X_SECTION( init );
static xhandle platform_RegisterRigidGeom( rigid_geom const& geom ) X_SECTION( init );
static void    platform_UnregisterSkinGeom( xhandle hGeom ) X_SECTION( init );
static void    platform_UnregisterRigidGeom( xhandle hGeom ) X_SECTION( init );

//=============================================================================
// Forward primitive submission
//=============================================================================
static xbool platform_SubmitPrimitives( render::primitive_draw_desc const& desc, matrix4 const& localToWorld,
                                        render::primitive_vertex const* pVertices, s32 nVertices,
                                        u16 const* pIndices, s32 nIndices )
    X_SECTION( render_primitives );
static xbool platform_SetDepthRect( irect const& rect, f32 depth ) X_SECTION( render_primitives );
static xbool platform_BeginPrimitiveRender( void ) X_SECTION( render_primitives );
static void  platform_EndPrimitiveRender( void ) X_SECTION( render_primitives );
static void  platform_ExecuteForwardRender( render::forward_render_stage stage ) X_SECTION( render_primitives );
static xbool platform_SubmitDecalBatch( render::decal_draw_desc const& desc, cubemap const* pCubeMap,
                                        render::decal_vertex const* pVertices, s32 nVertices,
                                        u16 const* pIndices, s32 nIndices )
    X_SECTION( render_primitives );
static void platform_ReserveDecalSubmissionCapacity( s32 nVertices, s32 nIndices, s32 nDraws )
    X_SECTION( render_infrequent );
static void  platform_ResetAfterException( void ) X_SECTION( render_infrequent );
//=============================================================================
// runtime lighting functions
//=============================================================================
static void* platform_CalculateRigidLighting( matrix4 const& l2W, bbox const& worldBBox ) X_SECTION( render_add );
static void* platform_CalculateSkinLighting( u32 flags, matrix4 const& l2W, bbox const& bBox, xcolor ambient )
    X_SECTION( render_add );

//=============================================================================
// geometry submission
//=============================================================================
static void platform_SubmitGeometry( xarray<geometry_draw_item> const& draws,
                                     xarray<dynamic_geometry_draw> const& dynamicDraws,
                                     cubemap const* pCubeMap )
    X_SECTION( render_deferred );

//=============================================================================
// dynamic shadow-map sources
//=============================================================================

static void platform_ClearShadowSourceList( void ) X_SECTION( render_infrequent );
static void platform_FinalizeShadowSourceList( void ) X_SECTION( render_infrequent );
static void platform_AddPointShadowMapSource( matrix4 const& l2W, radian fov, f32 lightRadius, f32 lightFalloff,
                                              s32 shadowMapResolution, s32 shadowPriority, f32 shadowScore,
                                              s32 dynamicLightIndex ) X_SECTION( render_add_shadow );
static void platform_AddSpotShadowMapSource( matrix4 const& l2W, radian fov, f32 lightRadius, f32 lightFalloff,
                                             s32 shadowMapResolution, s32 shadowPriority, f32 shadowScore,
                                             s32 dynamicLightIndex ) X_SECTION( render_add_shadow );
static void platform_BeginShadowShaders( void ) X_SECTION( render_infrequent );
static void platform_EndShadowShaders( void ) X_SECTION( render_infrequent );
static void platform_RenderShadowCasters( xarray<geometry_draw_item const*> const& draws,
                                          xarray<dynamic_geometry_shadow_draw> const& dynamicDraws )
    X_SECTION( render_deferred_shadow );

//=============================================================================
// post effects
//=============================================================================
static void platform_SetCustomFogPalette( texture::handle const& texture, xbool immediateSwitch, s32 paletteIndex )
    X_SECTION( render_post );
static xcolor platform_GetFogValue( vector3 const& worldPos, s32 paletteIndex ) X_SECTION( render_infrequent );
static void   platform_InitPostEffects( void ) X_SECTION( init );
static void   platform_KillPostEffects( void ) X_SECTION( init );
static void   platform_UpdatePostEffects( f32 deltaTime ) X_SECTION( update );
static void   platform_BeginPostEffects( void ) X_SECTION( render_post );
static void   platform_AddScreenWarp( vector3 const& worldPos, f32 radius, f32 warpAmount ) X_SECTION( render_post );
static void   platform_MotionBlur( f32 intensity ) X_SECTION( render_post );
static void   platform_ApplySelfIllumGlows( f32 motionBlurIntensity, s32 glowCutoff ) X_SECTION( render_post );
static void   platform_MultScreen( xcolor multColor, render::post_screen_blend finalBlend ) X_SECTION( render_post );
static void   platform_RadialBlur( f32 zoom, radian angle, f32 alphaSub, f32 alphaScale ) X_SECTION( render_post );
static void   platform_ZFogFilter( render::post_falloff_fn fn, xcolor color, f32 param1, f32 param2 )
    X_SECTION( render_post );
static void platform_ZFogFilter( render::post_falloff_fn fn, s32 paletteIndex ) X_SECTION( render_post );
static void platform_MipFilter( s32 nFilters, f32 offset, render::post_falloff_fn fn, xcolor color, f32 param1,
                                f32 param2, s32 paletteIndex ) X_SECTION( render_post );
static void platform_MipFilter( s32 nFilters, f32 offset, render::post_falloff_fn fn, texture::handle const& texture,
                                s32 paletteIndex ) X_SECTION( render_post );
static void platform_NoiseFilter( xcolor color ) X_SECTION( render_post );
static void platform_EndPostEffects( void ) X_SECTION( render_deferred_post );

//=============================================================================

static void platform_BeginNormalRender( void );
static void platform_EndNormalRender( void );

#endif
