//=============================================================================
//
//  XBOX Specific Post-effect Routines
//
//=============================================================================

#include "../platform_Render.hpp"

//=============================================================================
//=============================================================================
// Types and constants specific to the xbox-implementation
//=============================================================================
//=============================================================================

#define ZBUFFER_BITS    (19)
#define MAX_POST_MIPS   (3)
#define POST_MOTIONBLUR_FLAG        0x0001
#define POST_SELFILLUM_FLAG         0x0002
#define POST_MULTSCREEN_FLAG        0x0004
#define POST_RADIALBLUR_FLAG        0x0008
#define POST_ZFOGFILTERFN_FLAG      0x0010
#define POST_ZFOGFILTERCUSTOM_FLAG  0x0020
#define POST_MIPFILTERFN_FLAG       0x0040
#define POST_MIPFILTERCUSTOM_FLAG   0x0080

//=============================================================================
//=============================================================================
// Internal data
//=============================================================================
//=============================================================================

struct post_effect_params
{
    // flags for turning post-effects on and off
    xbool   Override        : 1;    // Set this to play around with values in the debugger
    xbool   DoMotionBlur    : 1;
    xbool   DoSelfIllumGlow : 1;
    xbool   DoMultScreen    : 1;
    xbool   DoRadialBlur    : 1;
    xbool   DoZFogFn        : 1;
    xbool   DoZFogCustom    : 1;
    xbool   DoMipFn         : 1;
    xbool   DoMipCustom     : 1;
    xbool   DoNoise         : 1;
    xbool   DoScreenFade    : 1;

    // post-effect parameters
    f32                         MotionBlurIntensity;
    f32                         GlowMotionBlurIntensity;
    s32                         GlowCutoff;
    xcolor                      MultScreenColor;
    render::post_screen_blend   MultScreenBlend;
    f32                         RadialZoom;
    radian                      RadialAngle;
    f32                         RadialAlphaSub;
    f32                         RadialAlphaScale;

    render::post_falloff_fn     FogFn       [5];
    xcolor                      FogColor    [5];
    f32                         FogParam1   [5];
    f32                         FogParam2   [5];
    s32                         FogPaletteIndex;

    render::post_falloff_fn     MipFn       [4];
    xcolor                      MipColor    [4];
    f32                         MipParam1   [4];
    f32                         MipParam2   [4];
    s32                         MipCount    [4];
    f32                         MipOffset   [4];
    s32                         MipPaletteIndex;

    xcolor                      NoiseColor;
    xcolor                      FadeColor;
};

// misc. data common to all post-effects
static post_effect_params           s_Post;
static xbool                        s_InPost = FALSE;
static s32                          s_PostViewL;
static s32                          s_PostViewT;
static s32                          s_PostViewR;
static s32                          s_PostViewB;
static f32                          s_PostNearZ = 1.0f;
static f32                          s_PostFarZ  = 2.0f;

// motion blur data
static f32                          s_MotionBlurIntensity;

// mult screen data
static xcolor                       s_MultScreenColor;
static render::post_screen_blend    s_MultScreenBlend;

// radial blur data
static f32                          s_RadialZoom;
static radian                       s_RadialAngle;
static f32                          s_RadialAlphaSub;
static f32                          s_RadialAlphaScale;

// fog data
static s32                          s_hFogClutHandle[3] = { -1, -1, -1 };
static xbool                        s_bFogValid[3]      = { FALSE, FALSE, FALSE };
static u8                           s_FogPalette[3][1024];

// mip filter data
static const xbitmap*               s_pMipTexture;
static s32                          s_hMipClutHandle[2] = { -1, -1 };
static u8                           s_MipPalette[2][1024];

// noise filter data
static const s32 kNoiseMapW = 64;
static const s32 kNoiseMapH = 64;
static xbitmap   s_NoiseBitmap;
static u32       s_NoiseMap[kNoiseMapW*kNoiseMapH/8] =
{
//DELETED
};

static u32 s_NoisePalette[16] =
{
//DELETED
};

//=============================================================================
//=============================================================================
// Internal routines
//=============================================================================
//=============================================================================

static
void xbox_MotionBlur( void )
{
}

//=============================================================================

static
void xbox_BuildScreenMips( s32 nMips, xbool UseAlpha )
{
}

//=============================================================================

static
void xbox_ApplySelfIllumGlows( void )
{
    g_pPipeline->PostEffect( );
}

//=============================================================================

static
void xbox_CopyBackBuffer( void )
{
}

//=============================================================================

static
void xbox_MultScreen( void )
{
}

//=============================================================================

static
void xbox_RadialBlur( void )
{
}

//=============================================================================

static
void xbox_CopyRG2BA( void )
{
}

//=============================================================================

static
void xbox_SetFogTexture( void )
{
}

//=============================================================================

static
void xbox_SetFogTexture( const xbitmap* pBitmap )
{
}

//=============================================================================

static
void xbox_RenderFogSprite( void )
{
}

//=============================================================================

static
void xbox_ZFogFilter( void )
{
    CONTEXT( "xbox_ZFogFilter" );

    // upload the fog palette, and turn the alpha channel of the front buffer
    // into a PSMT8H texture
    xbox_SetFogTexture();

    // now just render the fog sprite with normal blending
    xbox_RenderFogSprite();
}

//=============================================================================

static
void xbox_SetMipTexture( void )
{
    g_Texture.Set( 0,g_pPipeline->m_pTexture[pipeline_mgr::kSAMPLE0] );
}

//=============================================================================

static
void xbox_SetMipTexture( const xbitmap* pBitmap )
{
}

//=============================================================================

static
void xbox_RemapAlpha( void )
{
}

//=============================================================================

extern void Blt( f32 dx,f32 dy,f32 dw,f32 dh,f32 sx,f32 sy,f32 sw,f32 sh );

//=============================================================================

static void xbox_RenderMipSprites( void )
{
return;
    rect Rect;
    const view* pView = eng_GetView();
    pView->GetViewport( Rect );

    g_TextureStageState.Set( 0, D3DTSS_ALPHAKILL, D3DTALPHAKILL_ENABLE );
    g_TextureStageState.Set( 0, D3DTSS_ADDRESSU , D3DTADDRESS_CLAMP    );
    g_TextureStageState.Set( 0, D3DTSS_ADDRESSU , D3DTADDRESS_CLAMP    );
    g_TextureStageState.Set( 0, D3DTSS_ADDRESSV , D3DTADDRESS_CLAMP    );
    g_TextureStageState.Set( 0, D3DTSS_COLOROP  , D3DTOP_SELECTARG1    );
    g_TextureStageState.Set( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE        );
    g_TextureStageState.Set( 0, D3DTSS_ALPHAOP  , D3DTOP_SELECTARG1    );
    g_TextureStageState.Set( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE        );
    g_TextureStageState.Set( 1, D3DTSS_COLOROP  , D3DTOP_DISABLE       );
    g_TextureStageState.Set( 1, D3DTSS_ALPHAOP  , D3DTOP_DISABLE       );

    g_RenderState.Set( D3DRS_ZENABLE,FALSE );

    //  ----------------------------------------------------------------------
    //
    //  Calculate coords
    //
    const f32 dL = Rect.Min.X-0.5f;
    const f32 dT = Rect.Min.Y-0.5f;
    const f32 dR = Rect.Max.X;
    const f32 dB = Rect.Min.Y;
    const f32 sL = 0.0f;
    const f32 sT = 0.0f;
    const f32 sR = g_pPipeline->m_MaxW/2.0f;
    const f32 sB = g_pPipeline->m_MaxH/2.0f;

    struct vertex
    {
        f32 x,y,z,w;
        f32 u,v;
    }
    TriList[4] = 
    {
        { dL,dT,0,0,sL,sT },
        { dR,dT,0,0,sR,sT },
        { dR,dB,0,0,sR,sB },
        { dL,dB,0,0,sL,sB }
    };

    //  ----------------------------------------------------------------------
    //
    //  Draw quad
    //
    ASSERT( g_pShaderMgr );
    g_pShaderMgr->m_VSFlags.Mask = 0;
    g_pd3dDevice->SetVertexShader( D3DFVF_XYZRHW|D3DFVF_TEX1 );
    g_pd3dDevice->DrawPrimitiveUP( D3DPT_QUADLIST,1,TriList,sizeof( vertex ) );

    g_TextureStageState.Set( 0,D3DTSS_ADDRESSU,D3DTADDRESS_WRAP );
    g_TextureStageState.Set( 0,D3DTSS_ADDRESSV,D3DTADDRESS_WRAP );

    g_RenderState.Set( D3DRS_ZENABLE,TRUE );
}

//=============================================================================

static
void xbox_MipFilter( void )
{
    CONTEXT( "xbox_MipFilter" );

    ASSERT( s_Post.MipCount[s_Post.MipPaletteIndex] <= MAX_POST_MIPS );

    // upload the mip palette, which is really an alpha re-map palette
    if ( s_Post.MipFn[s_Post.MipPaletteIndex] == render::FALLOFF_CUSTOM )
        xbox_SetMipTexture( s_pMipTexture );
    else
        xbox_SetMipTexture();

    // now remap the alpha's into the back buffer
    xbox_RemapAlpha();

    // now we render the mip multiple times with a dest alpha blend
    xbox_RenderMipSprites();
}

//=============================================================================

static
void xbox_CreateFalloffPalette( u8* pPalette, render::hgeom_inst Fn, xcolor Color, f32 Param1, f32 Param2 )
{
    f32 FarMinusNear = s_PostFarZ-s_PostNearZ;
    for ( s32 i = 0; i < 256; i++ )
    {
        s32 SwizzledIndex = ((i&0x08)<<1) | ((i&0x10)>>1) | ((i&0xE7));
        ASSERT( (SwizzledIndex>=0) && (SwizzledIndex<256) );

        // The z-buffer is 24-bit, but we can only access bits 8..15 of that.
        // Because the z-buffer is 0 at the far plane, this is actually okay, we
        // just make sure that we're far enough in that bits 16..23 are always zero.
        // So to figure out what the fog color should be, we need to convert from the
        // playstation's z-buffer to our units.
        f32 ZDoublePrime = (f32)((i<<8) + 0x80);

        // knowing what we do about the C2S matrix, we can work our way back
        // into the [-1,1] clipping cube
        // Z" = -Z' * (scale/2) + (scale/2)
        // ==> Z' = (-2 * Z" / scale) + 1
        f32 ZScale = (f32)((s32)1<<ZBUFFER_BITS);
        f32 ZPrime = ((-2.0f * ZDoublePrime) / ZScale) + 1;

        // knowing what we do about the V2C matrix, we can work our way back
        // into the original Z in view space
        // Z' = (Z(F+N)/(F-N) - 2*N*F/(F-N))/Z
        // ==> Z = -2*N*F / (Z'(F-N) - F - N)
        f32 Denom = (ZPrime*FarMinusNear) - s_PostFarZ - s_PostNearZ;
        ASSERT( (Denom >= -2.0f*s_PostFarZ) && (Denom <= -2.0f*s_PostNearZ) );
        f32 Z = -2.0f * s_PostNearZ * s_PostFarZ / Denom;
        ASSERT( (Z>=s_PostNearZ) && (Z<=s_PostFarZ) );
        
        // now apply the appropriate function to figure out the fog intensity
        f32 F;
        switch ( Fn )
        {
        default:
            ASSERT( FALSE );
            F = 1.0f;
            break;

        case render::FALLOFF_CONSTANT:
            F = 1.0f - Color.A/128.0f;
            break;

        case render::FALLOFF_LINEAR:
            ASSERT( Param2 > Param1 );
            if ( Z > Param2 )
                Z = Param2;
            F = (Param2-Z)/(Param2-Param1);
            break;

        case render::FALLOFF_EXP:
            {
                f32 D     = (Z-s_PostNearZ)/(FarMinusNear);
                f32 Denom = x_exp(D*Param1);
                ASSERT( Denom > 0.001f );
                F = 1.0f/Denom;
            }
            break;

        case render::FALLOFF_EXP2:
            {
                f32 D     = (Z-s_PostNearZ)/(FarMinusNear);
                D        *= Param1;
                D         = D*D;
                f32 Denom = x_exp(D);
                ASSERT( Denom > 0.001f );
                F = 1.0f/Denom;
            }
            break;
        }

        F = MIN(F, 1.0f);
        F = MAX(F, 0.0f);

        pPalette[SwizzledIndex*4+0] = Color.R;
        pPalette[SwizzledIndex*4+1] = Color.G;
        pPalette[SwizzledIndex*4+2] = Color.B;
        if ( (i == 0) && (Fn!=render::FALLOFF_CONSTANT) )
            pPalette[SwizzledIndex*4+3] = 0;
        else
            pPalette[SwizzledIndex*4+3] = 0x80-(u8)(F*128.0f);
    }
}

//=============================================================================

static
void xbox_CreateFogPalette( render::post_falloff_fn Fn, xcolor Color, f32 Param1, f32 Param2 )
{
    CONTEXT( "xbox_CreateFogPalette" );

    f32 NearZ, FarZ;
    const view* pView = eng_GetView();
    pView->GetZLimits( NearZ, FarZ );

    // force this to be fog palette 2
    s_Post.FogPaletteIndex = 2;
    
    // is it necessary to rebuild the palette?
    if ( (s_Post.FogFn[s_Post.FogPaletteIndex]     == Fn    ) &&
         (s_Post.FogParam1[s_Post.FogPaletteIndex] == Param1) &&
         (s_Post.FogParam2[s_Post.FogPaletteIndex] == Param2) &&
         (s_Post.FogColor[s_Post.FogPaletteIndex]  == Color ) &&
         (s_PostNearZ == NearZ ) &&
         (s_PostFarZ  == FarZ  ) )
    {
        return;
    }

    s_Post.FogFn[s_Post.FogPaletteIndex]     = Fn;
    s_Post.FogParam1[s_Post.FogPaletteIndex] = Param1;
    s_Post.FogParam2[s_Post.FogPaletteIndex] = Param2;
    s_Post.FogColor[s_Post.FogPaletteIndex]  = Color;
    s_PostNearZ = NearZ;
    s_PostFarZ  = FarZ;

    xbox_CreateFalloffPalette( s_FogPalette[s_Post.FogPaletteIndex], Fn, Color, Param1, Param2 );
}

//=============================================================================

static
void xbox_CreateMipPalette( render::post_falloff_fn Fn, xcolor Color, f32 Param1, f32 Param2, s32 PaletteIndex )
{
    CONTEXT( "xbox_CreateMipPalette" );

    f32 NearZ, FarZ;
    const view* pView = eng_GetView();
    pView->GetZLimits( NearZ, FarZ );

    // is it necessary to rebuild the palette?
    if ( (s_Post.MipFn[PaletteIndex]     == Fn    ) &&
         (s_Post.MipParam1[PaletteIndex] == Param1) &&
         (s_Post.MipParam2[PaletteIndex] == Param2) &&
         (s_Post.MipColor[PaletteIndex]  == Color ) &&
         (s_PostNearZ      == NearZ ) &&
         (s_PostFarZ       == FarZ  ) )
    {
        return;
    }

    s_Post.MipFn[PaletteIndex]     = Fn;
    s_Post.MipParam1[PaletteIndex] = Param1;
    s_Post.MipParam2[PaletteIndex] = Param2;
    s_Post.MipColor[PaletteIndex]  = Color;
    s_Post.MipPaletteIndex         = PaletteIndex;
    s_PostNearZ = NearZ;
    s_PostFarZ  = FarZ;

    xbox_CreateFalloffPalette( s_MipPalette[PaletteIndex], Fn, Color, Param1, Param2 );
}

//=============================================================================

static
void xbox_NoiseFilter( void )
{
}

//=============================================================================

static
void xbox_ScreenFade( void )
{
    // render a big transparent quad over the screen
    irect Rect;
    Rect.l = 0;
    Rect.t = 0;
    Rect.r = g_PhysW;
    Rect.b = g_PhysH;
    draw_Rect( Rect, s_Post.FadeColor, FALSE );
}

//=============================================================================
//=============================================================================
// Functions that are exposed to the main render file
//=============================================================================
//=============================================================================

//=============================================================================

static
void platform_SetCustomFogPalette( const texture::handle&    Texture,
                                   xbool                     ImmediateSwitch,
                                   s32                       PaletteIndex )
{
    ASSERT( g_pShaderMgr );
    g_pShaderMgr->SetCustomFogPalette( Texture,ImmediateSwitch,PaletteIndex );
}

//=============================================================================

static
xcolor platform_GetFogValue( const vector3& WorldPos, s32 PaletteIndex )
{
    (void)WorldPos;
    (void)PaletteIndex;
    return xcolor(255,255,255,0);
}

//=============================================================================

static
void platform_InitPostEffects( void )
{
}

//=============================================================================

static
void platform_KillPostEffects( void )
{
}

//=============================================================================

xcolor xbox_GetFogValue( const vector3& Position,s32 PaletteIndex )
{
    const matrix4& W2C = eng_GetView()->GetW2C();

    vector4 ScreenPos( Position );
    ScreenPos.GetW() = 1.0f;
    ScreenPos = W2C * ScreenPos;
    if( x_abs( ScreenPos.GetW() ) < 0.001f )
        return xcolor(255,255,255,0);

    f32 Z  = ScreenPos.GetZ();
    f32 Z2 = Z*Z;
    f32 Z3 = Z2*Z;
    f32 FogIntensity = g_pShaderMgr->m_FogConst[PaletteIndex].GetX()      +
                       g_pShaderMgr->m_FogConst[PaletteIndex].GetY() * Z  +
                       g_pShaderMgr->m_FogConst[PaletteIndex].GetZ() * Z2 +
                       g_pShaderMgr->m_FogConst[PaletteIndex].GetW() * Z3;
    
    FogIntensity = MINMAX( 0.0f, FogIntensity, 1.0f );
    
    return xcolor( 255, 255, 255, u8(FogIntensity*255.0f) );
}

//=============================================================================

static
void platform_BeginPostEffects( void )
{
    ASSERT( !s_InPost );
    s_InPost               = TRUE;
    
    if ( s_Post.Override )
        return;

    s_Post.DoMotionBlur    = FALSE;
    s_Post.DoSelfIllumGlow = FALSE;
    s_Post.DoMultScreen    = FALSE;
    s_Post.DoRadialBlur    = FALSE;
    s_Post.DoZFogFn        = FALSE;
    s_Post.DoZFogCustom    = FALSE;
    s_Post.DoMipFn         = FALSE;
    s_Post.DoMipCustom     = FALSE;
    s_Post.DoNoise         = FALSE;
    s_Post.DoScreenFade    = FALSE;

    s_pMipTexture          = NULL;
}

//=============================================================================

static
void platform_AddScreenWarp( const vector3& WorldPos, f32 Radius, f32 WarpAmount )
{
    (void)WorldPos;
    (void)Radius;
    (void)WarpAmount;
}

//=============================================================================

static
void platform_MotionBlur( f32 Intensity )
{
    if ( s_Post.Override )
        return;

    ASSERT( s_InPost );
    s_Post.DoMotionBlur         = TRUE;
    s_Post.MotionBlurIntensity  = Intensity;
}

//=============================================================================

static
void platform_ApplySelfIllumGlows( f32 MotionBlurIntensity, s32 GlowCutoff )
{
    if ( s_Post.Override )
        return;

    ASSERT( s_InPost );
    s_Post.DoSelfIllumGlow         = TRUE;
    s_Post.GlowMotionBlurIntensity = MotionBlurIntensity;
    s_Post.GlowCutoff              = GlowCutoff;

    g_pPipeline->SetMotionBlurIntensity( MotionBlurIntensity );
    g_pPipeline->SetGlowCutOff( (f32)GlowCutoff );
}

//=============================================================================

static
void platform_MultScreen( xcolor MultColor, render::post_screen_blend FinalBlend )
{
    if ( s_Post.Override )
        return;

    ASSERT( s_InPost );
    s_Post.DoMultScreen    = TRUE;
    s_Post.MultScreenColor = MultColor;
    s_Post.MultScreenBlend = FinalBlend;
}

//=============================================================================

static
void platform_RadialBlur( f32 Zoom, radian Angle, f32 AlphaSub, f32 AlphaScale  )
{
    if ( s_Post.Override )
        return;

    ASSERT( s_InPost );
    s_Post.DoRadialBlur     = TRUE;
    s_Post.RadialZoom       = Zoom;
    s_Post.RadialAngle      = Angle;
    s_Post.RadialAlphaSub   = AlphaSub;
    s_Post.RadialAlphaScale = AlphaScale;
}

//=============================================================================

static
void platform_ZFogFilter( render::post_falloff_fn Fn, xcolor Color, f32 Param1, f32 Param2 )
{
    if ( s_Post.Override )
        return;

    ASSERT( s_InPost );
    ASSERT( Fn != render::FALLOFF_CUSTOM );

    // adjust our fog palette (will also save the fog parameters)
    xbox_CreateFogPalette( Fn, Color, Param1, Param2 );
    s_Post.DoZFogFn        = TRUE;
    s_Post.FogPaletteIndex = 2;
}

//=============================================================================

static
void platform_ZFogFilter( render::post_falloff_fn Fn, s32 PaletteIndex )
{
    if ( s_Post.Override )
        return;

    ASSERT( (PaletteIndex >= 0) && (PaletteIndex <= 4) );
    if ( !s_bFogValid[PaletteIndex] )
        return;

    ASSERT( s_InPost );
    ASSERT( Fn == render::FALLOFF_CUSTOM );
    s_Post.DoZFogCustom        = TRUE;
    s_Post.FogFn[PaletteIndex] = Fn;
    s_Post.FogPaletteIndex     = PaletteIndex;
}

//=============================================================================

static
void platform_MipFilter( s32                        nFilters,
                         f32                        Offset,
                         render::post_falloff_fn    Fn,
                         xcolor                     Color,
                         f32                        Param1,
                         f32                        Param2, 
                         s32                        PaletteIndex )
{
    if ( s_Post.Override )
        return;

    ASSERT( s_InPost );
    ASSERT( Fn != render::FALLOFF_CUSTOM );

    // adjust our mip palette (will also save the mip parameters)
    xbox_CreateMipPalette( Fn, Color, Param1, Param2, PaletteIndex );
    s_Post.DoMipFn                 = TRUE;
    s_Post.MipFn[PaletteIndex]     = Fn;
    s_Post.MipCount[PaletteIndex]  = nFilters;
    s_Post.MipOffset[PaletteIndex] = Offset;
    s_Post.MipPaletteIndex         = PaletteIndex;
}

//=============================================================================

static
void platform_MipFilter( s32                        nFilters,
                         f32                        Offset,
                         render::post_falloff_fn    Fn,
                         const texture::handle&     Texture,
                         s32                        PaletteIndex )
{
    if ( s_Post.Override )
        return;

    ASSERT( s_InPost );
    ASSERT( Fn == render::FALLOFF_CUSTOM );

    // safety check
    if ( Texture.GetPointer() == NULL )
        return;

    s_Post.DoMipCustom             = TRUE;
    s_pMipTexture                  = &Texture.GetPointer()->m_Bitmap;
    s_Post.MipFn[PaletteIndex]     = Fn;
    s_Post.MipCount[PaletteIndex]  = nFilters;
    s_Post.MipOffset[PaletteIndex] = Offset;
    s_Post.MipPaletteIndex         = PaletteIndex;
    ASSERT( s_pMipTexture );
}

//=============================================================================

static
void platform_NoiseFilter( xcolor Color )
{
    if ( s_Post.Override )
        return;

    ASSERT( s_InPost );

    s_Post.DoNoise    = TRUE;
    s_Post.NoiseColor = Color;
}

//=============================================================================

static
void platform_ScreenFade( xcolor Color )
{
    s_Post.FadeColor    = Color;
    s_Post.DoScreenFade = TRUE;
}

//=============================================================================

static
void platform_EndPostEffects( void )
{
    CONTEXT( "platform_EndPostEffects" );

    ASSERT( s_InPost );
    s_InPost = FALSE;

    // get common engine/screen information
    const view* pView = eng_GetView();
    pView->GetViewport( s_PostViewL, s_PostViewT, s_PostViewR, s_PostViewB );

    // handle fog
    if ( s_Post.DoZFogCustom || s_Post.DoZFogFn )
    {
        xbox_ZFogFilter();
    }
    
    // handle the mip filter
    if ( s_Post.DoMipCustom || s_Post.DoMipFn )
        xbox_MipFilter();

    // handle motion-blur. this is a stand-alone blur that will not effect vram
    // or the contents of any alpha or z-buffers.
    if ( s_Post.DoMotionBlur )
        xbox_MotionBlur();

    // handle self-illum
    if ( s_Post.DoSelfIllumGlow )
        xbox_ApplySelfIllumGlows();

    // radial blurs and post-mults will want a copy of the back-buffer to work with
    if ( s_Post.DoMultScreen || s_Post.DoRadialBlur )
        xbox_CopyBackBuffer();

    // handle the multscreen filter
    if ( s_Post.DoMultScreen )
        xbox_MultScreen();

    // handle the radial blur filter
    if ( s_Post.DoRadialBlur )
        xbox_RadialBlur();

    // handle the noise filter
    if ( s_Post.DoNoise )
        xbox_NoiseFilter();

    // handle the fade filter
    if ( s_Post.DoScreenFade )
        xbox_ScreenFade();

    // reset the things we've most likely screwed up along the way
    g_RenderState.Set( D3DRS_SHADEMODE, D3DSHADE_GOURAUD );
    g_RenderState.Set( D3DRS_CULLMODE, D3DCULL_CW );
    g_RenderState.Set( D3DRS_ZENABLE, TRUE );

    g_Texture.Clear( 0 );
    g_Texture.Clear( 1 );
    g_Texture.Clear( 2 );
    g_Texture.Clear( 3 );
}


//=============================================================================

void platform_BeginDistortion( void )
{
    g_pPipeline->BeginDistortion();
}

//=============================================================================

void platform_EndDistortion( void )
{
    g_pPipeline->EndDistortion();
}
