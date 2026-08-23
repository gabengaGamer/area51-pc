//=========================================================================
//
//  Material.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "Material.hpp"

//=========================================================================
//  TYPES
//=========================================================================

enum
{
    RENDER_FLAG_ALPHA                   = ( 1u << 0  ),
    RENDER_FLAG_DISTORTION              = ( 1u << 1  ),
    RENDER_FLAG_DISTORTION_PERPOLY_ENV  = ( 1u << 2  ),
    RENDER_FLAG_FORCE_Z_FILL            = ( 1u << 3  ),
    RENDER_FLAG_PUNCH_THRU              = ( 1u << 4  ),
    RENDER_FLAG_ENV_CUBEMAP             = ( 1u << 5  ),
    RENDER_FLAG_FORWARD                 = ( 1u << 6  ),
    RENDER_FLAG_DEPTH_SORTED            = ( 1u << 7  ),
    RENDER_FLAG_POST_EFFECT_BLEND       = ( 1u << 8  ),
    RENDER_FLAG_RECEIVES_PROJECTION     = ( 1u << 9  ),
    RENDER_FLAG_RECEIVES_SHADOW         = ( 1u << 10 ),
    RENDER_FLAG_USES_SHADOW_ALPHA       = ( 1u << 11 ),
    RENDER_FLAG_DEPTH_WRITE             = ( 1u << 12 ),
    RENDER_FLAG_CULL_BACK               = ( 1u << 13 ),
    RENDER_FLAG_CLAMP_SAMPLER           = ( 1u << 14 ),
};

//---------------------------------------------------------------------------

struct MaterialTypePolicy
{
    u32 ShaderFeatures;
    u32 RenderFlags;
    u8  RenderOrder;
    u8  SortPriority;
};

//=========================================================================
//  CONSTANTS
//=========================================================================

static MaterialTypePolicy const MaterialTypePolicies[Material_NumTypes] =
{
    { 0, 0, MATERIAL_RENDER_ORDER_OPAQUE, 1 }, // Material_Not_Used
    { 0, 0, MATERIAL_RENDER_ORDER_OPAQUE, 2 }, // Material_Diff
    {
        MATERIAL_SHADER_ALPHA_BLEND | MATERIAL_SHADER_ALPHA_TEST,
        RENDER_FLAG_ALPHA | RENDER_FLAG_FORWARD | RENDER_FLAG_USES_SHADOW_ALPHA,
        MATERIAL_RENDER_ORDER_ALPHA,
        5
    }, // Material_Alpha
    {
        MATERIAL_SHADER_DIFF_PERPIXEL_ENV,
        0,
        MATERIAL_RENDER_ORDER_OPAQUE,
        2
    }, // Material_Diff_PerPixelEnv
    {
        MATERIAL_SHADER_DIFF_PERPIXEL_ILLUM,
        0,
        MATERIAL_RENDER_ORDER_GLOWING,
        3
    }, // Material_Diff_PerPixelIllum
    {
        MATERIAL_SHADER_ALPHA_BLEND | MATERIAL_SHADER_ALPHA_PERPOLY_ENV,
        RENDER_FLAG_ALPHA | RENDER_FLAG_FORWARD,
        MATERIAL_RENDER_ORDER_ALPHA,
        5
    }, // Material_Alpha_PerPolyEnv
    {
        MATERIAL_SHADER_ALPHA_BLEND | MATERIAL_SHADER_ALPHA_PERPIXEL_ILLUM,
        RENDER_FLAG_ALPHA | RENDER_FLAG_FORWARD,
        MATERIAL_RENDER_ORDER_ALPHA_GLOWING,
        4
    }, // Material_Alpha_PerPixelIllum
    {
        MATERIAL_SHADER_ALPHA_BLEND | MATERIAL_SHADER_ALPHA_PERPOLY_ILLUM,
        RENDER_FLAG_ALPHA | RENDER_FLAG_FORWARD,
        MATERIAL_RENDER_ORDER_ALPHA_GLOWING,
        4
    }, // Material_Alpha_PerPolyIllum
    {
        MATERIAL_SHADER_ALPHA_BLEND | MATERIAL_SHADER_DISTORTION,
        RENDER_FLAG_ALPHA | RENDER_FLAG_DISTORTION,
        MATERIAL_RENDER_ORDER_DISTORTION,
        6
    }, // Material_Distortion
    {
        MATERIAL_SHADER_ALPHA_BLEND | MATERIAL_SHADER_DISTORTION_PERPOLY_ENV,
        RENDER_FLAG_ALPHA | RENDER_FLAG_DISTORTION | RENDER_FLAG_DISTORTION_PERPOLY_ENV,
        MATERIAL_RENDER_ORDER_DISTORTION,
        6
    }, // Material_Distortion_PerPolyEnv
};

static_assert( ( sizeof( MaterialTypePolicies ) / sizeof( MaterialTypePolicies[0] ) ) == Material_NumTypes,
               "Every material type must have a render policy" );

//=========================================================================
//  HELPER FUNCTIONS
//=========================================================================

static 
xbool HasRenderFlag( u32 Flags, u32 Flag )
{
    return ( ( Flags & Flag ) != 0 );
}

//=========================================================================
//  FUNCTIONS
//=========================================================================

material::uvanim::uvanim( void )
    : CurrentFrame( 0.0f )
    , iKey( 0 )
    , iFrame( 0 )
    , Dir( 1 )
    , Type( 0 )
    , nFrames( 0 )
    , FPS( 0 )
    , StartFrame( 0 )
{
}

//=========================================================================

material::material( void )
    : m_Type( Material_Not_Used )
    , m_detailScale( 1.0f )
    , m_fixedAlpha( 0.0f )
    , m_Flags( 0 )
    , m_diffuseMap()
    , m_environmentMap()
    , m_detailMap()
    , m_uvAnim()
    , m_refCount( 0 )
    , m_renderData()
{
    Finalize();
}

//=========================================================================

material::~material( void )
{
}

//=========================================================================

void material::Finalize( void )
{
    ASSERT( ( m_Type >= Material_Not_Used ) && ( m_Type < Material_NumTypes ) );

    s32 const TypeIndex = ( ( m_Type >= Material_Not_Used ) && ( m_Type < Material_NumTypes ) )
                              ? static_cast<s32>( m_Type )
                              : static_cast<s32>( Material_Not_Used );

    MaterialTypePolicy const& Policy = MaterialTypePolicies[TypeIndex];

    m_renderData.ShaderFeatures   = Policy.ShaderFeatures;
    m_renderData.Flags            = Policy.RenderFlags;
    m_renderData.BlendMode        = MATERIAL_BLEND_OPAQUE;
    m_renderData.RenderOrder      = Policy.RenderOrder;
    m_renderData.SortPriority     = Policy.SortPriority;
    m_renderData.AlphaRef         = 0.0f;
    m_renderData.DetailScale      = ( m_detailScale > 0.0f ) ? m_detailScale : 1.0f;
    m_renderData.FixedAlpha       = m_fixedAlpha;
    m_renderData.CubeMapIntensity = 1.0f;

    xbool const IsAlpha      = HasRenderFlag( m_renderData.Flags, RENDER_FLAG_ALPHA );
    xbool const IsDistortion = HasRenderFlag( m_renderData.Flags, RENDER_FLAG_DISTORTION );
    xbool const ForceZFill   = ( ( m_Flags & MATERIAL_FLAG_FORCE_ZFILL ) != 0 );
    xbool const PunchThru    = ( ( m_Flags & MATERIAL_FLAG_IS_PUNCH_THRU ) != 0 );
    xbool const DoubleSided  = ( ( m_Flags & MATERIAL_FLAG_DOUBLE_SIDED ) != 0 );
    xbool const Additive     = ( ( m_Flags & MATERIAL_FLAG_IS_ADDITIVE ) != 0 );
    xbool const Subtractive  = ( ( m_Flags & MATERIAL_FLAG_IS_SUBTRACTIVE ) != 0 );
    xbool const IsPostEffect = Additive || Subtractive;

    if( ForceZFill )
    {
        m_renderData.Flags |= RENDER_FLAG_FORCE_Z_FILL;
    }

    if( PunchThru )
    {
        m_renderData.Flags |= RENDER_FLAG_PUNCH_THRU | RENDER_FLAG_USES_SHADOW_ALPHA;
    }

    if( IsPostEffect )
    {
        m_renderData.Flags |= RENDER_FLAG_POST_EFFECT_BLEND;
    }

    if( m_Flags & MATERIAL_FLAG_HAS_DETAIL_MAP )
    {
        m_renderData.ShaderFeatures |= MATERIAL_SHADER_DETAIL;
    }

    // Preserve the current shader contract. Missing environment resources
    // are bound to the renderer fallback.
    m_renderData.ShaderFeatures |= MATERIAL_SHADER_ENVIRONMENT;

    if( m_Flags & MATERIAL_FLAG_ENV_CUBE_MAP )
    {
        m_renderData.ShaderFeatures |= MATERIAL_SHADER_ENV_CUBEMAP;
        m_renderData.Flags |= RENDER_FLAG_ENV_CUBEMAP;
    }

    if( m_Flags & MATERIAL_FLAG_ENV_VIEW_SPACE )
    {
        m_renderData.ShaderFeatures |= MATERIAL_SHADER_ENV_VIEWSPACE;
    }

    if( m_Flags & MATERIAL_FLAG_ENV_WORLD_SPACE )
    {
        m_renderData.ShaderFeatures |= MATERIAL_SHADER_ENV_WORLDSPACE;
    }

    if( PunchThru )
    {
        m_renderData.ShaderFeatures |= MATERIAL_SHADER_ALPHA_TEST;
    }

    if( DoubleSided )
    {
        m_renderData.ShaderFeatures |= MATERIAL_SHADER_TWO_SIDED;
    }

    if( Additive )
    {
        m_renderData.ShaderFeatures |= MATERIAL_SHADER_ADDITIVE;
    }
    else if( Subtractive )
    {
        m_renderData.ShaderFeatures |= MATERIAL_SHADER_SUBTRACTIVE;
    }

    if( m_Flags & MATERIAL_FLAG_ILLUM_USES_DIFFUSE )
    {
        m_renderData.ShaderFeatures |= MATERIAL_SHADER_ILLUM_USE_DIFFUSE;
    }

    if( m_renderData.ShaderFeatures & MATERIAL_SHADER_ALPHA_TEST )
    {
        m_renderData.AlphaRef = PunchThru ? 0.5f : ( 4.0f / 255.0f );
    }

    if( IsDistortion )
    {
        m_renderData.BlendMode = MATERIAL_BLEND_DISTORTION;
        m_renderData.Flags |= RENDER_FLAG_CLAMP_SAMPLER;
    }
    else if( HasRenderFlag( m_renderData.Flags, RENDER_FLAG_FORWARD ) )
    {
        if( Additive )
        {
            m_renderData.BlendMode = MATERIAL_BLEND_ADDITIVE;
        }
        else if( Subtractive )
        {
            m_renderData.BlendMode = MATERIAL_BLEND_SUBTRACTIVE;
        }
        else
        {
            m_renderData.BlendMode = MATERIAL_BLEND_ALPHA;
        }
    }

    if( HasRenderFlag( m_renderData.Flags, RENDER_FLAG_FORWARD ) && !IsPostEffect )
    {
        m_renderData.Flags |= RENDER_FLAG_DEPTH_SORTED;
    }

    if( !IsAlpha || ( ForceZFill && !Subtractive ) )
    {
        m_renderData.Flags |= RENDER_FLAG_RECEIVES_PROJECTION;
    }

    if( ( !IsAlpha || ForceZFill ) && !PunchThru )
    {
        m_renderData.Flags |= RENDER_FLAG_RECEIVES_SHADOW;
    }

    xbool const EnablesBlend = ( m_renderData.BlendMode == MATERIAL_BLEND_ALPHA ) ||
                               ( m_renderData.BlendMode == MATERIAL_BLEND_ADDITIVE ) ||
                               ( m_renderData.BlendMode == MATERIAL_BLEND_SUBTRACTIVE );

    if( !( ( IsAlpha || EnablesBlend ) && !ForceZFill && !IsDistortion ) )
    {
        m_renderData.Flags |= RENDER_FLAG_DEPTH_WRITE;
    }

    if( !DoubleSided && !( IsAlpha && !IsDistortion ) )
    {
        m_renderData.Flags |= RENDER_FLAG_CULL_BACK;
    }

    if( HasRenderFlag( m_renderData.Flags, RENDER_FLAG_ENV_CUBEMAP ) )
    {
        if( HasRenderFlag( m_renderData.Flags, RENDER_FLAG_DISTORTION_PERPOLY_ENV ) ||
            ( m_Type == Material_Alpha_PerPolyEnv ) )
        {
            m_renderData.CubeMapIntensity = 1.0f;
        }
        else
        {
            f32 const DefaultCubeMapIntensity = 0.35f;
            f32       CubeMapIntensity        = m_fixedAlpha;

            if( CubeMapIntensity <= 0.0f )
            {
                CubeMapIntensity = DefaultCubeMapIntensity;
            }

            if( CubeMapIntensity < 0.0f )
            {
                CubeMapIntensity = 0.0f;
            }

            if( CubeMapIntensity > 1.0f )
            {
                CubeMapIntensity = 1.0f;
            }

            m_renderData.CubeMapIntensity = CubeMapIntensity;
        }
    }
}

//=========================================================================

xbool material::operator==( material const& Rhs ) const
{
    if( m_diffuseMap.GetIndex() != Rhs.m_diffuseMap.GetIndex() )
    {
        return FALSE;
    }

    if( m_environmentMap.GetIndex() != Rhs.m_environmentMap.GetIndex() )
    {
        return FALSE;
    }

    if( m_detailMap.GetIndex() != Rhs.m_detailMap.GetIndex() )
    {
        return FALSE;
    }

    if( m_Type != Rhs.m_Type )
    {
        return FALSE;
    }

    if( m_detailScale != Rhs.m_detailScale )
    {
        return FALSE;
    }

    if( m_fixedAlpha != Rhs.m_fixedAlpha )
    {
        return FALSE;
    }

    if( m_Flags != Rhs.m_Flags )
    {
        return FALSE;
    }

    // Mutable animation state is not part of material identity.
    if( m_uvAnim.iKey != Rhs.m_uvAnim.iKey )
    {
        return FALSE;
    }

    if( m_uvAnim.Type != Rhs.m_uvAnim.Type )
    {
        return FALSE;
    }

    if( m_uvAnim.nFrames != Rhs.m_uvAnim.nFrames )
    {
        return FALSE;
    }

    if( m_uvAnim.FPS != Rhs.m_uvAnim.FPS )
    {
        return FALSE;
    }

    if( m_uvAnim.StartFrame != Rhs.m_uvAnim.StartFrame )
    {
        return FALSE;
    }

    return TRUE;
}

//=========================================================================

xbool material::IsAlpha( void ) const
{
    return HasRenderFlag( m_renderData.Flags, RENDER_FLAG_ALPHA );
}

//=========================================================================

xbool material::IsDistortion( void ) const
{
    return HasRenderFlag( m_renderData.Flags, RENDER_FLAG_DISTORTION );
}

//=========================================================================

xbool material::IsDistortionPerPolyEnv( void ) const
{
    return HasRenderFlag( m_renderData.Flags, RENDER_FLAG_DISTORTION_PERPOLY_ENV );
}

//=========================================================================

xbool material::ForcesZFill( void ) const
{
    return HasRenderFlag( m_renderData.Flags, RENDER_FLAG_FORCE_Z_FILL );
}

//=========================================================================

xbool material::IsPunchThrough( void ) const
{
    return HasRenderFlag( m_renderData.Flags, RENDER_FLAG_PUNCH_THRU );
}

//=========================================================================

xbool material::UsesCubeMap( void ) const
{
    return HasRenderFlag( m_renderData.Flags, RENDER_FLAG_ENV_CUBEMAP );
}

//=========================================================================

xbool material::RequiresForwardPass( void ) const
{
    return HasRenderFlag( m_renderData.Flags, RENDER_FLAG_FORWARD );
}

//=========================================================================

xbool material::RequiresDepthSort( void ) const
{
    return HasRenderFlag( m_renderData.Flags, RENDER_FLAG_DEPTH_SORTED );
}

//=========================================================================

xbool material::IsPostEffectBlend( void ) const
{
    return HasRenderFlag( m_renderData.Flags, RENDER_FLAG_POST_EFFECT_BLEND );
}

//=========================================================================

xbool material::ReceivesProjection( void ) const
{
    return HasRenderFlag( m_renderData.Flags, RENDER_FLAG_RECEIVES_PROJECTION );
}

//=========================================================================

xbool material::ReceivesShadow( void ) const
{
    return HasRenderFlag( m_renderData.Flags, RENDER_FLAG_RECEIVES_SHADOW );
}

//=========================================================================

xbool material::UsesShadowAlpha( void ) const
{
    return HasRenderFlag( m_renderData.Flags, RENDER_FLAG_USES_SHADOW_ALPHA );
}

//=========================================================================

xbool material::WritesDepth( void ) const
{
    return HasRenderFlag( m_renderData.Flags, RENDER_FLAG_DEPTH_WRITE );
}

//=========================================================================

xbool material::CullsBackFaces( void ) const
{
    return HasRenderFlag( m_renderData.Flags, RENDER_FLAG_CULL_BACK );
}

//=========================================================================

xbool material::ClampsSampler( void ) const
{
    return HasRenderFlag( m_renderData.Flags, RENDER_FLAG_CLAMP_SAMPLER );
}