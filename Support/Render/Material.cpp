//=============================================================================
//
//  Material.cpp
//
//=============================================================================

//=============================================================================
//  INCLUDES
//=============================================================================

#include "Material.hpp"

//=============================================================================

namespace
{
enum material_render_flags
{
    RENDER_FLAG_ALPHA = ( 1u << 0 ),
    RENDER_FLAG_DISTORTION = ( 1u << 1 ),
    RENDER_FLAG_DISTORTION_PERPOLY_ENV = ( 1u << 2 ),
    RENDER_FLAG_FORCE_Z_FILL = ( 1u << 3 ),
    RENDER_FLAG_PUNCH_THRU = ( 1u << 4 ),
    RENDER_FLAG_ENV_CUBEMAP = ( 1u << 5 ),
    RENDER_FLAG_FORWARD = ( 1u << 6 ),
    RENDER_FLAG_DEPTH_SORTED = ( 1u << 7 ),
    RENDER_FLAG_POST_EFFECT_BLEND = ( 1u << 8 ),
    RENDER_FLAG_RECEIVES_PROJECTION = ( 1u << 9 ),
    RENDER_FLAG_RECEIVES_SHADOW = ( 1u << 10 ),
    RENDER_FLAG_USES_SHADOW_ALPHA = ( 1u << 11 ),
    RENDER_FLAG_DEPTH_WRITE = ( 1u << 12 ),
    RENDER_FLAG_CULL_BACK = ( 1u << 13 ),
    RENDER_FLAG_CLAMP_SAMPLER = ( 1u << 14 ),
};

inline xbool HasRenderFlag( u32 flags, u32 flag )
{
    return ( flags & flag ) != 0;
}

struct material_type_policy
{
    u32 ShaderFeatures;
    u32 RenderFlags;
    u8  RenderOrder;
    u8  SortPriority;
};

static material_type_policy const kMaterialTypePolicy[Material_NumTypes] = {
    // Shader features                           Render flags                                              Render order
    // Priority
    { 0, 0, MATERIAL_RENDER_ORDER_OPAQUE, 1 }, // Material_Not_Used
    { 0, 0, MATERIAL_RENDER_ORDER_OPAQUE, 2 }, // Material_Diff
    { MATERIAL_SHADER_ALPHA_BLEND | MATERIAL_SHADER_ALPHA_TEST,
      RENDER_FLAG_ALPHA | RENDER_FLAG_FORWARD | RENDER_FLAG_USES_SHADOW_ALPHA, MATERIAL_RENDER_ORDER_ALPHA,
      5 },                                                                        // Material_Alpha
    { MATERIAL_SHADER_DIFF_PERPIXEL_ENV, 0, MATERIAL_RENDER_ORDER_OPAQUE, 2 },    // Material_Diff_PerPixelEnv
    { MATERIAL_SHADER_DIFF_PERPIXEL_ILLUM, 0, MATERIAL_RENDER_ORDER_GLOWING, 3 }, // Material_Diff_PerPixelIllum
    { MATERIAL_SHADER_ALPHA_BLEND | MATERIAL_SHADER_ALPHA_PERPOLY_ENV, RENDER_FLAG_ALPHA | RENDER_FLAG_FORWARD,
      MATERIAL_RENDER_ORDER_ALPHA, 5 }, // Material_Alpha_PerPolyEnv
    { MATERIAL_SHADER_ALPHA_BLEND | MATERIAL_SHADER_ALPHA_PERPIXEL_ILLUM, RENDER_FLAG_ALPHA | RENDER_FLAG_FORWARD,
      MATERIAL_RENDER_ORDER_ALPHA_GLOWING, 4 }, // Material_Alpha_PerPixelIllum
    { MATERIAL_SHADER_ALPHA_BLEND | MATERIAL_SHADER_ALPHA_PERPOLY_ILLUM, RENDER_FLAG_ALPHA | RENDER_FLAG_FORWARD,
      MATERIAL_RENDER_ORDER_ALPHA_GLOWING, 4 }, // Material_Alpha_PerPolyIllum
    { MATERIAL_SHADER_ALPHA_BLEND | MATERIAL_SHADER_DISTORTION, RENDER_FLAG_ALPHA | RENDER_FLAG_DISTORTION,
      MATERIAL_RENDER_ORDER_DISTORTION, 6 }, // Material_Distortion
    { MATERIAL_SHADER_ALPHA_BLEND | MATERIAL_SHADER_DISTORTION_PERPOLY_ENV,
      RENDER_FLAG_ALPHA | RENDER_FLAG_DISTORTION | RENDER_FLAG_DISTORTION_PERPOLY_ENV, MATERIAL_RENDER_ORDER_DISTORTION,
      6 }, // Material_Distortion_PerPolyEnv
};

static_assert( ( sizeof( kMaterialTypePolicy ) / sizeof( kMaterialTypePolicy[0] ) ) == Material_NumTypes,
               "Every material type must have a render policy" );
} // namespace

//=============================================================================

material::uvanim::uvanim( void )
    : CurrentFrame( 0.0f ), iKey( 0 ), iFrame( 0 ), Dir( 1 ), Type( 0 ), nFrames( 0 ), FPS( 0 ), StartFrame( 0 )
{
}

//=============================================================================

material::material( void )
    : m_Type( Material_Not_Used ), m_detailScale( 1.0f ), m_fixedAlpha( 0.0f ), m_Flags( 0 ), m_diffuseMap(),
      m_environmentMap(), m_detailMap(), m_uvAnim(), m_refCount( 0 ), m_renderData()
{
    Finalize();
}

//=============================================================================

material::~material( void )
{
}

//=============================================================================

void material::Finalize( void )
{
    ASSERT( ( m_Type >= Material_Not_Used ) && ( m_Type < Material_NumTypes ) );

    s32 const                   typeIndex = ( ( m_Type >= Material_Not_Used ) && ( m_Type < Material_NumTypes ) )
                                                ? static_cast<s32>( m_Type )
                                                : static_cast<s32>( Material_Not_Used );
    material_type_policy const& policy = kMaterialTypePolicy[typeIndex];

    m_renderData.ShaderFeatures = policy.ShaderFeatures;
    m_renderData.Flags = policy.RenderFlags;
    m_renderData.BlendMode = MATERIAL_BLEND_OPAQUE;
    m_renderData.RenderOrder = policy.RenderOrder;
    m_renderData.SortPriority = policy.SortPriority;
    m_renderData.AlphaRef = 0.0f;
    m_renderData.DetailScale = ( m_detailScale > 0.0f ) ? m_detailScale : 1.0f;
    m_renderData.FixedAlpha = m_fixedAlpha;
    m_renderData.CubeMapIntensity = 1.0f;

    xbool const isAlpha = HasRenderFlag( m_renderData.Flags, RENDER_FLAG_ALPHA );
    xbool const isDistortion = HasRenderFlag( m_renderData.Flags, RENDER_FLAG_DISTORTION );
    xbool const forceZFill = !!( m_Flags & MATERIAL_FLAG_FORCE_ZFILL );
    xbool const punchThru = !!( m_Flags & MATERIAL_FLAG_IS_PUNCH_THRU );
    xbool const doubleSided = !!( m_Flags & MATERIAL_FLAG_DOUBLE_SIDED );
    xbool const additive = !!( m_Flags & MATERIAL_FLAG_IS_ADDITIVE );
    xbool const subtractive = !!( m_Flags & MATERIAL_FLAG_IS_SUBTRACTIVE );
    xbool const isPostEffect = additive || subtractive;

    if ( forceZFill )
    {
        m_renderData.Flags |= RENDER_FLAG_FORCE_Z_FILL;
    }
    if ( punchThru )
    {
        m_renderData.Flags |= RENDER_FLAG_PUNCH_THRU | RENDER_FLAG_USES_SHADOW_ALPHA;
    }
    if ( isPostEffect )
    {
        m_renderData.Flags |= RENDER_FLAG_POST_EFFECT_BLEND;
    }

    if ( m_Flags & MATERIAL_FLAG_HAS_DETAIL_MAP )
    {
        m_renderData.ShaderFeatures |= MATERIAL_SHADER_DETAIL;
    }

    // Preserve the current shader contract: the environment path is always
    // enabled and missing resources are bound to the renderer fallback.
    m_renderData.ShaderFeatures |= MATERIAL_SHADER_ENVIRONMENT;

    if ( m_Flags & MATERIAL_FLAG_ENV_CUBE_MAP )
    {
        m_renderData.ShaderFeatures |= MATERIAL_SHADER_ENV_CUBEMAP;
        m_renderData.Flags |= RENDER_FLAG_ENV_CUBEMAP;
    }

    if ( m_Flags & MATERIAL_FLAG_ENV_VIEW_SPACE )
    {
        m_renderData.ShaderFeatures |= MATERIAL_SHADER_ENV_VIEWSPACE;
    }
    if ( m_Flags & MATERIAL_FLAG_ENV_WORLD_SPACE )
    {
        m_renderData.ShaderFeatures |= MATERIAL_SHADER_ENV_WORLDSPACE;
    }
    if ( punchThru )
    {
        m_renderData.ShaderFeatures |= MATERIAL_SHADER_ALPHA_TEST;
    }
    if ( doubleSided )
    {
        m_renderData.ShaderFeatures |= MATERIAL_SHADER_TWO_SIDED;
    }

    if ( additive )
    {
        m_renderData.ShaderFeatures |= MATERIAL_SHADER_ADDITIVE;
    }
    else if ( subtractive )
    {
        m_renderData.ShaderFeatures |= MATERIAL_SHADER_SUBTRACTIVE;
    }

    if ( m_Flags & MATERIAL_FLAG_ILLUM_USES_DIFFUSE )
    {
        m_renderData.ShaderFeatures |= MATERIAL_SHADER_ILLUM_USE_DIFFUSE;
    }

    if ( m_renderData.ShaderFeatures & MATERIAL_SHADER_ALPHA_TEST )
    {
        m_renderData.AlphaRef = punchThru ? 0.5f : ( 4.0f / 255.0f );
    }

    if ( isDistortion )
    {
        m_renderData.BlendMode = MATERIAL_BLEND_DISTORTION;
        m_renderData.Flags |= RENDER_FLAG_CLAMP_SAMPLER;
    }
    else if ( HasRenderFlag( m_renderData.Flags, RENDER_FLAG_FORWARD ) )
    {
        if ( additive )
        {
            m_renderData.BlendMode = MATERIAL_BLEND_ADDITIVE;
        }
        else if ( subtractive )
        {
            m_renderData.BlendMode = MATERIAL_BLEND_SUBTRACTIVE;
        }
        else
        {
            m_renderData.BlendMode = MATERIAL_BLEND_ALPHA;
        }
    }

    if ( HasRenderFlag( m_renderData.Flags, RENDER_FLAG_FORWARD ) && !isPostEffect )
    {
        m_renderData.Flags |= RENDER_FLAG_DEPTH_SORTED;
    }

    if ( !isAlpha || ( forceZFill && !subtractive ) )
    {
        m_renderData.Flags |= RENDER_FLAG_RECEIVES_PROJECTION;
    }

    if ( ( !isAlpha || forceZFill ) && !punchThru )
    {
        m_renderData.Flags |= RENDER_FLAG_RECEIVES_SHADOW;
    }

    xbool const enablesBlend = ( m_renderData.BlendMode == MATERIAL_BLEND_ALPHA ) ||
                               ( m_renderData.BlendMode == MATERIAL_BLEND_ADDITIVE ) ||
                               ( m_renderData.BlendMode == MATERIAL_BLEND_SUBTRACTIVE );
    if ( !( ( isAlpha || enablesBlend ) && !forceZFill && !isDistortion ) )
    {
        m_renderData.Flags |= RENDER_FLAG_DEPTH_WRITE;
    }

    if ( !doubleSided && !( isAlpha && !isDistortion ) )
    {
        m_renderData.Flags |= RENDER_FLAG_CULL_BACK;
    }

    if ( HasRenderFlag( m_renderData.Flags, RENDER_FLAG_ENV_CUBEMAP ) )
    {
        if ( HasRenderFlag( m_renderData.Flags, RENDER_FLAG_DISTORTION_PERPOLY_ENV ) ||
             ( m_Type == Material_Alpha_PerPolyEnv ) )
        {
            m_renderData.CubeMapIntensity = 1.0f;
        }
        else
        {
            f32 const defaultCubeMapIntensity = 0.35f;
            f32       cubeMapIntensity = m_fixedAlpha;
            if ( cubeMapIntensity <= 0.0f )
            {
                cubeMapIntensity = defaultCubeMapIntensity;
            }
            if ( cubeMapIntensity < 0.0f )
            {
                cubeMapIntensity = 0.0f;
            }
            if ( cubeMapIntensity > 1.0f )
            {
                cubeMapIntensity = 1.0f;
            }
            m_renderData.CubeMapIntensity = cubeMapIntensity;
        }
    }
}

//=============================================================================

xbool material::IsAlpha( void ) const
{
    return HasRenderFlag( m_renderData.Flags, RENDER_FLAG_ALPHA );
}

//=============================================================================

xbool material::IsDistortion( void ) const
{
    return HasRenderFlag( m_renderData.Flags, RENDER_FLAG_DISTORTION );
}

//=============================================================================

xbool material::IsDistortionPerPolyEnv( void ) const
{
    return HasRenderFlag( m_renderData.Flags, RENDER_FLAG_DISTORTION_PERPOLY_ENV );
}

//=============================================================================

xbool material::ForcesZFill( void ) const
{
    return HasRenderFlag( m_renderData.Flags, RENDER_FLAG_FORCE_Z_FILL );
}

//=============================================================================

xbool material::IsPunchThrough( void ) const
{
    return HasRenderFlag( m_renderData.Flags, RENDER_FLAG_PUNCH_THRU );
}

//=============================================================================

xbool material::UsesCubeMap( void ) const
{
    return HasRenderFlag( m_renderData.Flags, RENDER_FLAG_ENV_CUBEMAP );
}

//=============================================================================

xbool material::RequiresForwardPass( void ) const
{
    return HasRenderFlag( m_renderData.Flags, RENDER_FLAG_FORWARD );
}

//=============================================================================

xbool material::RequiresDepthSort( void ) const
{
    return HasRenderFlag( m_renderData.Flags, RENDER_FLAG_DEPTH_SORTED );
}

//=============================================================================

xbool material::IsPostEffectBlend( void ) const
{
    return HasRenderFlag( m_renderData.Flags, RENDER_FLAG_POST_EFFECT_BLEND );
}

//=============================================================================

xbool material::ReceivesProjection( void ) const
{
    return HasRenderFlag( m_renderData.Flags, RENDER_FLAG_RECEIVES_PROJECTION );
}

//=============================================================================

xbool material::ReceivesShadow( void ) const
{
    return HasRenderFlag( m_renderData.Flags, RENDER_FLAG_RECEIVES_SHADOW );
}

//=============================================================================

xbool material::UsesShadowAlpha( void ) const
{
    return HasRenderFlag( m_renderData.Flags, RENDER_FLAG_USES_SHADOW_ALPHA );
}

//=============================================================================

xbool material::WritesDepth( void ) const
{
    return HasRenderFlag( m_renderData.Flags, RENDER_FLAG_DEPTH_WRITE );
}

//=============================================================================

xbool material::CullsBackFaces( void ) const
{
    return HasRenderFlag( m_renderData.Flags, RENDER_FLAG_CULL_BACK );
}

//=============================================================================

xbool material::ClampsSampler( void ) const
{
    return HasRenderFlag( m_renderData.Flags, RENDER_FLAG_CLAMP_SAMPLER );
}

//=============================================================================

xbool material::operator==( material const& rhs ) const
{
    if ( m_diffuseMap.GetIndex() != rhs.m_diffuseMap.GetIndex() )
    {
        return FALSE;
    }
    if ( m_environmentMap.GetIndex() != rhs.m_environmentMap.GetIndex() )
    {
        return FALSE;
    }
    if ( m_detailMap.GetIndex() != rhs.m_detailMap.GetIndex() )
    {
        return FALSE;
    }
    if ( m_Type != rhs.m_Type )
    {
        return FALSE;
    }
    if ( m_detailScale != rhs.m_detailScale )
    {
        return FALSE;
    }
    if ( m_fixedAlpha != rhs.m_fixedAlpha )
    {
        return FALSE;
    }
    if ( m_Flags != rhs.m_Flags )
    {
        return FALSE;
    }

    // Mutable animation state is not part of material identity. Otherwise a
    // geometry loaded after animations advance cannot reuse an identical
    // registered material.
    if ( m_uvAnim.iKey != rhs.m_uvAnim.iKey )
    {
        return FALSE;
    }
    if ( m_uvAnim.Type != rhs.m_uvAnim.Type )
    {
        return FALSE;
    }
    if ( m_uvAnim.nFrames != rhs.m_uvAnim.nFrames )
    {
        return FALSE;
    }
    if ( m_uvAnim.FPS != rhs.m_uvAnim.FPS )
    {
        return FALSE;
    }
    if ( m_uvAnim.StartFrame != rhs.m_uvAnim.StartFrame )
    {
        return FALSE;
    }

    return TRUE;
}

//=============================================================================
