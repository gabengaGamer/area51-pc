//=========================================================================
//
//  Material.hpp
//
//=========================================================================

#ifndef MATERIAL_HPP
#define MATERIAL_HPP

//=========================================================================
//  INCLUDES
//=========================================================================

#include "Texture.hpp"
#include "Material_Prefs.hpp"

//=========================================================================
//  TYPES
//=========================================================================

enum material_render_order
{
    MATERIAL_RENDER_ORDER_OPAQUE         = 0,
    MATERIAL_RENDER_ORDER_GLOWING        = 1,
    MATERIAL_RENDER_ORDER_ALPHA_GLOWING  = 2,
    MATERIAL_RENDER_ORDER_ALPHA          = 3,
    MATERIAL_RENDER_ORDER_DISTORTION     = 7,
};

//--------------------------------------------------------------------------

// Shader feature bits consumed by geometry shaders.
enum material_shader_features
{
    MATERIAL_SHADER_ALPHA_TEST              = ( 1u << 0  ),
    MATERIAL_SHADER_ADDITIVE                = ( 1u << 1  ),
    MATERIAL_SHADER_SUBTRACTIVE             = ( 1u << 2  ),
    MATERIAL_SHADER_VERTEX_COLOR            = ( 1u << 3  ),
    MATERIAL_SHADER_TWO_SIDED               = ( 1u << 4  ),
    MATERIAL_SHADER_ENVIRONMENT             = ( 1u << 5  ),
    MATERIAL_SHADER_DISTORTION              = ( 1u << 6  ),
    MATERIAL_SHADER_DISTORTION_PERPOLY_ENV  = ( 1u << 7  ),
    MATERIAL_SHADER_DIFF_PERPIXEL_ILLUM     = ( 1u << 8  ),
    MATERIAL_SHADER_ALPHA_PERPIXEL_ILLUM    = ( 1u << 9  ),
    MATERIAL_SHADER_ALPHA_PERPOLY_ILLUM     = ( 1u << 10 ),
    MATERIAL_SHADER_DIFF_PERPIXEL_ENV       = ( 1u << 11 ),
    MATERIAL_SHADER_ALPHA_PERPOLY_ENV       = ( 1u << 12 ),
    MATERIAL_SHADER_DETAIL                  = ( 1u << 13 ),
    MATERIAL_SHADER_ENV_CUBEMAP             = ( 1u << 14 ),
    MATERIAL_SHADER_ENV_VIEWSPACE           = ( 1u << 15 ),
    MATERIAL_SHADER_ENV_WORLDSPACE          = ( 1u << 16 ),
    MATERIAL_SHADER_ALPHA_BLEND             = ( 1u << 17 ),
    MATERIAL_SHADER_ILLUM_USE_DIFFUSE       = ( 1u << 18 ),
};

//---------------------------------------------------------------------------

enum material_blend_mode
{
    MATERIAL_BLEND_OPAQUE = 0,
    MATERIAL_BLEND_ALPHA,
    MATERIAL_BLEND_ADDITIVE,
    MATERIAL_BLEND_SUBTRACTIVE,
    MATERIAL_BLEND_DISTORTION,
};

//=========================================================================

class material
{
public:

    struct uvanim
    {
        f32 CurrentFrame;
        s16 iKey;
        s8  iFrame;
        s8  Dir;
        s8  Type;
        s8  nFrames;
        s8  FPS;
        s8  StartFrame;

        uvanim( void );
    };

    material( void );
    ~material( void );

    void  Finalize   ( void );
    xbool operator== ( material const& Rhs ) const;

    xbool               HasUVAnimation         ( void ) const;
    u32                 GetShaderFeatures      ( void ) const;
    material_blend_mode GetBlendMode           ( void ) const;
    u8                  GetRenderOrder         ( void ) const;
    u8                  GetSortPriority        ( void ) const;
    f32                 GetAlphaRef            ( void ) const;
    f32                 GetDetailScale         ( void ) const;
    f32                 GetFixedAlpha          ( void ) const;
    f32                 GetCubeMapIntensity    ( void ) const;
    xbool               IsAlpha                ( void ) const;
    xbool               IsDistortion           ( void ) const;
    xbool               IsDistortionPerPolyEnv ( void ) const;
    xbool               ForcesZFill            ( void ) const;
    xbool               IsPunchThrough         ( void ) const;
    xbool               UsesCubeMap            ( void ) const;
    xbool               RequiresForwardPass    ( void ) const;
    xbool               RequiresDepthSort      ( void ) const;
    xbool               IsPostEffectBlend      ( void ) const;
    xbool               ReceivesProjection     ( void ) const;
    xbool               ReceivesShadow         ( void ) const;
    xbool               UsesShadowAlpha        ( void ) const;
    xbool               WritesDepth            ( void ) const;
    xbool               CullsBackFaces         ( void ) const;
    xbool               ClampsSampler          ( void ) const;
    s32                 AddRef                 ( void );
    s32                 Release                ( void );
    s32                 GetRefCount            ( void ) const;

public:

    s8  m_Type;
    f32 m_detailScale;
    f32 m_fixedAlpha;
    u16 m_Flags;

    texture::handle m_diffuseMap;
    texture::handle m_environmentMap;
    texture::handle m_detailMap;
    uvanim          m_uvAnim;
    s32             m_refCount;

private:

    struct render_data
    {
        u32                 ShaderFeatures;
        u32                 Flags;
        material_blend_mode BlendMode;
        u8                  RenderOrder;
        u8                  SortPriority;
        f32                 AlphaRef;
        f32                 DetailScale;
        f32                 FixedAlpha;
        f32                 CubeMapIntensity;
    };

    render_data m_renderData;
};

//=========================================================================
//  INLINE FUNCTIONS
//=========================================================================

inline xbool material::HasUVAnimation( void ) const
{
    return ( m_uvAnim.nFrames > 0 );
}

//=========================================================================

inline u32 material::GetShaderFeatures( void ) const
{
    return m_renderData.ShaderFeatures;
}

//=========================================================================

inline material_blend_mode material::GetBlendMode( void ) const
{
    return m_renderData.BlendMode;
}

//=========================================================================

inline u8 material::GetRenderOrder( void ) const
{
    return m_renderData.RenderOrder;
}

//=========================================================================

inline u8 material::GetSortPriority( void ) const
{
    return m_renderData.SortPriority;
}

//=========================================================================

inline f32 material::GetAlphaRef( void ) const
{
    return m_renderData.AlphaRef;
}

//=========================================================================

inline f32 material::GetDetailScale( void ) const
{
    return m_renderData.DetailScale;
}

//=========================================================================

inline f32 material::GetFixedAlpha( void ) const
{
    return m_renderData.FixedAlpha;
}

//=========================================================================

inline f32 material::GetCubeMapIntensity( void ) const
{
    return m_renderData.CubeMapIntensity;
}

//=========================================================================

inline s32 material::AddRef( void )
{
    return ++m_refCount;
}

//=========================================================================

inline s32 material::Release( void )
{
    return --m_refCount;
}

//=========================================================================

inline s32 material::GetRefCount( void ) const
{
    return m_refCount;
}

//=========================================================================
#endif // MATERIAL_HPP
//=========================================================================
