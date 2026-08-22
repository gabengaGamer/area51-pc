//==============================================================================
//
//  sdleng_state.cpp
//
//==============================================================================

#include "x_target.hpp"

#if defined(TARGET_DESKTOP) && defined(ENTROPY_RENDER_SDL)

//==============================================================================
//  INCLUDES
//==============================================================================

#include "sdleng_private.hpp"

#ifndef X_STDIO_HPP
#include "x_stdio.hpp"
#endif

//==============================================================================
//  LOCAL STORAGE
//==============================================================================

struct sdlstate_pipeline_key
{
    u64                             VertexShaderSerial;
    u64                             PixelShaderSerial;
    SDL_GPUVertexBufferDescription* pVertexBuffers;
    u32                             VertexBufferCount;
    SDL_GPUVertexAttribute*         pVertexAttributes;
    u32                             VertexAttributeCount;
    SDL_GPUPrimitiveType            PrimitiveType;
    SDL_GPURasterizerState          RasterizerState;
    SDL_GPUMultisampleState         MultisampleState;
    SDL_GPUDepthStencilState        DepthStencilState;
    SDL_GPUColorTargetDescription   ColorTargets[RTARGET_MAX_TARGETS];
    u32                             ColorTargetCount;
    SDL_GPUTextureFormat            DepthStencilFormat;
    xbool                           bHasDepthStencilTarget;

    sdlstate_pipeline_key( void ) :
        VertexShaderSerial    ( 0 ),
        PixelShaderSerial     ( 0 ),
        pVertexBuffers        ( NULL ),
        VertexBufferCount     ( 0 ),
        pVertexAttributes     ( NULL ),
        VertexAttributeCount  ( 0 ),
        PrimitiveType         ( SDL_GPU_PRIMITIVETYPE_TRIANGLELIST ),
        RasterizerState       (),
        MultisampleState      (),
        DepthStencilState     (),
        ColorTargetCount      ( 0 ),
        DepthStencilFormat    ( SDL_GPU_TEXTUREFORMAT_INVALID ),
        bHasDepthStencilTarget( FALSE )
    {
        x_memset( &RasterizerState,   0, sizeof(RasterizerState) );
        x_memset( &MultisampleState,  0, sizeof(MultisampleState) );
        x_memset( &DepthStencilState, 0, sizeof(DepthStencilState) );
        x_memset( ColorTargets,       0, sizeof(ColorTargets) );
    }
};

//------------------------------------------------------------------------------

struct render_pipeline_cache_entry
{
    SDL_GPUGraphicsPipeline*       pPipeline;
    sdlstate_pipeline_key          Key;
    u32                            HandleCount;
    render_pipeline_cache_entry*   pNext;

    render_pipeline_cache_entry( void ) :
        pPipeline ( NULL ),
        Key       (),
        HandleCount( 0 ),
        pNext     ( NULL )
    {
    }
};

//------------------------------------------------------------------------------

static rstate_blend_desc   s_BlendPresets  [RSTATE_BLEND_PRESET_COUNT];
static rstate_depth_desc   s_DepthPresets  [RSTATE_DEPTH_PRESET_COUNT];
static rstate_raster_desc  s_RasterPresets [RSTATE_RASTER_PRESET_COUNT];
static rstate_sampler_desc s_SamplerPresets[RSTATE_SAMPLER_PRESET_COUNT];
static xbool               s_bPresetsBuilt = FALSE;

static render_pipeline_backend*     s_pPipelineHandleList  = NULL;
static render_pipeline_cache_entry* s_pPipelineCache       = NULL;
static u32                          s_PipelineHandleCount  = 0;
static u32                          s_PipelineCount        = 0;
static const render_pipeline_cache_entry*
                                    s_pBoundPipeline       = NULL;
static rstate_sampler_backend*  s_pSamplerList  = NULL;
static u32                      s_SamplerCount  = 0;

//==============================================================================
//  HELPERS
//==============================================================================

static
SDL_GPUBlendFactor sdlstate_ToSDLBlendFactor( rstate_blend_factor Factor )
{
    switch( Factor )
    {
        case RSTATE_BLEND_FACTOR_ZERO:             return SDL_GPU_BLENDFACTOR_ZERO;
        case RSTATE_BLEND_FACTOR_ONE:              return SDL_GPU_BLENDFACTOR_ONE;
        case RSTATE_BLEND_FACTOR_SRC_COLOR:        return SDL_GPU_BLENDFACTOR_SRC_COLOR;
        case RSTATE_BLEND_FACTOR_INV_SRC_COLOR:    return SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_COLOR;
        case RSTATE_BLEND_FACTOR_DEST_COLOR:       return SDL_GPU_BLENDFACTOR_DST_COLOR;
        case RSTATE_BLEND_FACTOR_INV_DEST_COLOR:   return SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_COLOR;
        case RSTATE_BLEND_FACTOR_SRC_ALPHA:        return SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        case RSTATE_BLEND_FACTOR_INV_SRC_ALPHA:    return SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        case RSTATE_BLEND_FACTOR_DEST_ALPHA:       return SDL_GPU_BLENDFACTOR_DST_ALPHA;
        case RSTATE_BLEND_FACTOR_INV_DEST_ALPHA:   return SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_ALPHA;
        case RSTATE_BLEND_FACTOR_BLEND_FACTOR:     return SDL_GPU_BLENDFACTOR_CONSTANT_COLOR;
        case RSTATE_BLEND_FACTOR_INV_BLEND_FACTOR: return SDL_GPU_BLENDFACTOR_ONE_MINUS_CONSTANT_COLOR;
        case RSTATE_BLEND_FACTOR_SRC_ALPHA_SAT:    return SDL_GPU_BLENDFACTOR_SRC_ALPHA_SATURATE;
        default:                                   return SDL_GPU_BLENDFACTOR_ONE;
    }
}

//==============================================================================

static
SDL_GPUBlendOp sdlstate_ToSDLBlendOp( rstate_blend_op Op )
{
    switch( Op )
    {
        case RSTATE_BLEND_OP_ADD:          return SDL_GPU_BLENDOP_ADD;
        case RSTATE_BLEND_OP_SUBTRACT:     return SDL_GPU_BLENDOP_SUBTRACT;
        case RSTATE_BLEND_OP_REV_SUBTRACT: return SDL_GPU_BLENDOP_REVERSE_SUBTRACT;
        case RSTATE_BLEND_OP_MIN:          return SDL_GPU_BLENDOP_MIN;
        case RSTATE_BLEND_OP_MAX:          return SDL_GPU_BLENDOP_MAX;
        default:                           return SDL_GPU_BLENDOP_ADD;
    }
}

//==============================================================================

static
SDL_GPUColorComponentFlags sdlstate_ToSDLColorMask( u8 Mask )
{
    SDL_GPUColorComponentFlags SDLMask = 0;

    if( Mask & RSTATE_COLOR_WRITE_RED   ) SDLMask |= SDL_GPU_COLORCOMPONENT_R;
    if( Mask & RSTATE_COLOR_WRITE_GREEN ) SDLMask |= SDL_GPU_COLORCOMPONENT_G;
    if( Mask & RSTATE_COLOR_WRITE_BLUE  ) SDLMask |= SDL_GPU_COLORCOMPONENT_B;
    if( Mask & RSTATE_COLOR_WRITE_ALPHA ) SDLMask |= SDL_GPU_COLORCOMPONENT_A;

    return SDLMask;
}

//==============================================================================

static
SDL_GPUCompareOp sdlstate_ToSDLCompareOp( rstate_compare_func Func )
{
    switch( Func )
    {
        case RSTATE_COMPARE_NEVER:         return SDL_GPU_COMPAREOP_NEVER;
        case RSTATE_COMPARE_LESS:          return SDL_GPU_COMPAREOP_LESS;
        case RSTATE_COMPARE_EQUAL:         return SDL_GPU_COMPAREOP_EQUAL;
        case RSTATE_COMPARE_LESS_EQUAL:    return SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        case RSTATE_COMPARE_GREATER:       return SDL_GPU_COMPAREOP_GREATER;
        case RSTATE_COMPARE_NOT_EQUAL:     return SDL_GPU_COMPAREOP_NOT_EQUAL;
        case RSTATE_COMPARE_GREATER_EQUAL: return SDL_GPU_COMPAREOP_GREATER_OR_EQUAL;
        case RSTATE_COMPARE_ALWAYS:        return SDL_GPU_COMPAREOP_ALWAYS;
        default:                           return SDL_GPU_COMPAREOP_ALWAYS;
    }
}

//==============================================================================

static
SDL_GPUStencilOp sdlstate_ToSDLStencilOp( rstate_stencil_op Op )
{
    switch( Op )
    {
        case RSTATE_STENCIL_KEEP:     return SDL_GPU_STENCILOP_KEEP;
        case RSTATE_STENCIL_ZERO:     return SDL_GPU_STENCILOP_ZERO;
        case RSTATE_STENCIL_REPLACE:  return SDL_GPU_STENCILOP_REPLACE;
        case RSTATE_STENCIL_INCR_SAT: return SDL_GPU_STENCILOP_INCREMENT_AND_CLAMP;
        case RSTATE_STENCIL_DECR_SAT: return SDL_GPU_STENCILOP_DECREMENT_AND_CLAMP;
        case RSTATE_STENCIL_INVERT:   return SDL_GPU_STENCILOP_INVERT;
        case RSTATE_STENCIL_INCR:     return SDL_GPU_STENCILOP_INCREMENT_AND_WRAP;
        case RSTATE_STENCIL_DECR:     return SDL_GPU_STENCILOP_DECREMENT_AND_WRAP;
        default:                      return SDL_GPU_STENCILOP_KEEP;
    }
}

//==============================================================================

static
SDL_GPUFillMode sdlstate_ToSDLFillMode( rstate_fill_mode Mode )
{
    switch( Mode )
    {
        case RSTATE_FILL_SOLID:     return SDL_GPU_FILLMODE_FILL;
        case RSTATE_FILL_WIREFRAME: return SDL_GPU_FILLMODE_LINE;
        default:                    return SDL_GPU_FILLMODE_FILL;
    }
}

//==============================================================================

static
SDL_GPUCullMode sdlstate_ToSDLCullMode( rstate_cull_mode Mode )
{
    switch( Mode )
    {
        case RSTATE_CULL_NONE:  return SDL_GPU_CULLMODE_NONE;
        case RSTATE_CULL_FRONT: return SDL_GPU_CULLMODE_FRONT;
        case RSTATE_CULL_BACK:  return SDL_GPU_CULLMODE_BACK;
        default:                return SDL_GPU_CULLMODE_NONE;
    }
}

//==============================================================================

static
SDL_GPUFrontFace sdlstate_ToSDLFrontFace( rstate_front_face Face )
{
    switch( Face )
    {
        case RSTATE_FRONT_FACE_CCW: return SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        case RSTATE_FRONT_FACE_CW:  return SDL_GPU_FRONTFACE_CLOCKWISE;
        default:                    return SDL_GPU_FRONTFACE_CLOCKWISE;
    }
}

//==============================================================================

static
SDL_GPUPrimitiveType sdlstate_ToSDLPrimitiveType( shader_topology Topology )
{
    switch( Topology )
    {
        case SHADER_TOPOLOGY_POINT_LIST:     return SDL_GPU_PRIMITIVETYPE_POINTLIST;
        case SHADER_TOPOLOGY_LINE_LIST:      return SDL_GPU_PRIMITIVETYPE_LINELIST;
        case SHADER_TOPOLOGY_LINE_STRIP:     return SDL_GPU_PRIMITIVETYPE_LINESTRIP;
        case SHADER_TOPOLOGY_TRIANGLE_LIST:  return SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        case SHADER_TOPOLOGY_TRIANGLE_STRIP: return SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP;
        default:                             return SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    }
}

//==============================================================================

static
SDL_GPUVertexElementFormat sdlstate_ToSDLVertexFormat( shader_vertex_format Format )
{
    switch( Format )
    {
        case SHADER_VERTEX_FORMAT_FLOAT1: return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT;
        case SHADER_VERTEX_FORMAT_FLOAT2: return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        case SHADER_VERTEX_FORMAT_FLOAT3: return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        case SHADER_VERTEX_FORMAT_FLOAT4: return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        case SHADER_VERTEX_FORMAT_UINT1:  return SDL_GPU_VERTEXELEMENTFORMAT_UINT;
        case SHADER_VERTEX_FORMAT_UBYTE4N_BGRA: return SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM;
        default:                          return SDL_GPU_VERTEXELEMENTFORMAT_INVALID;
    }
}

//==============================================================================

static
xbool sdlstate_ToSDLSamplerFilter( rstate_filter Filter,
                                   SDL_GPUFilter& MinFilter,
                                   SDL_GPUFilter& MagFilter,
                                   SDL_GPUSamplerMipmapMode& MipmapMode,
                                   xbool& bAnisotropy,
                                   xbool& bCompare )
{
    bAnisotropy = FALSE;
    bCompare    = FALSE;

    switch( Filter )
    {
        case RSTATE_FILTER_POINT:
            MinFilter  = SDL_GPU_FILTER_NEAREST;
            MagFilter  = SDL_GPU_FILTER_NEAREST;
            MipmapMode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
            return TRUE;

        case RSTATE_FILTER_LINEAR:
            MinFilter  = SDL_GPU_FILTER_LINEAR;
            MagFilter  = SDL_GPU_FILTER_LINEAR;
            MipmapMode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
            return TRUE;

        case RSTATE_FILTER_ANISOTROPIC:
            MinFilter   = SDL_GPU_FILTER_LINEAR;
            MagFilter   = SDL_GPU_FILTER_LINEAR;
            MipmapMode  = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
            bAnisotropy = TRUE;
            return TRUE;

        case RSTATE_FILTER_COMPARISON_POINT:
            MinFilter = SDL_GPU_FILTER_NEAREST;
            MagFilter = SDL_GPU_FILTER_NEAREST;
            MipmapMode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
            bCompare = TRUE;
            return TRUE;

        case RSTATE_FILTER_COMPARISON_LINEAR:
            MinFilter = SDL_GPU_FILTER_LINEAR;
            MagFilter = SDL_GPU_FILTER_LINEAR;
            MipmapMode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
            bCompare = TRUE;
            return TRUE;

        case RSTATE_FILTER_COMPARISON_ANISOTROPIC:
            MinFilter = SDL_GPU_FILTER_LINEAR;
            MagFilter = SDL_GPU_FILTER_LINEAR;
            MipmapMode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
            bAnisotropy = TRUE;
            bCompare = TRUE;
            return TRUE;

        default:
            return FALSE;
    }
}

//==============================================================================

static
xbool sdlstate_ToSDLAddressMode( rstate_texture_address Address, SDL_GPUSamplerAddressMode& SDLAddress )
{
    switch( Address )
    {
        case RSTATE_TEXTURE_ADDRESS_WRAP:
            SDLAddress = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
            return TRUE;

        case RSTATE_TEXTURE_ADDRESS_MIRROR:
            SDLAddress = SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT;
            return TRUE;

        case RSTATE_TEXTURE_ADDRESS_CLAMP:
            SDLAddress = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
            return TRUE;

        case RSTATE_TEXTURE_ADDRESS_BORDER:
            x_DebugMsg( "SDLState: border sampler addressing is not supported by SDL GPU\n" );
            return FALSE;

        default:
            return FALSE;
    }
}

//==============================================================================

static
void sdlstate_FillStencilFace( SDL_GPUStencilOpState& Dst, const rstate_stencil_face_desc& Src )
{
    Dst.fail_op       = sdlstate_ToSDLStencilOp( Src.StencilFailOp );
    Dst.pass_op       = sdlstate_ToSDLStencilOp( Src.StencilPassOp );
    Dst.depth_fail_op = sdlstate_ToSDLStencilOp( Src.StencilDepthFailOp );
    Dst.compare_op    = sdlstate_ToSDLCompareOp( Src.StencilFunc );
}

//==============================================================================

static
void sdlstate_BuildPresets( void )
{
    if( s_bPresetsBuilt )
        return;

    rstate_blend_desc Blend;
    s_BlendPresets[RSTATE_BLEND_PRESET_NONE] = Blend;

    Blend = rstate_blend_desc();
    Blend.bBlendEnable = TRUE;
    Blend.SrcBlend     = RSTATE_BLEND_FACTOR_SRC_ALPHA;
    Blend.DestBlend    = RSTATE_BLEND_FACTOR_INV_SRC_ALPHA;
    s_BlendPresets[RSTATE_BLEND_PRESET_ALPHA] = Blend;

    Blend = rstate_blend_desc();
    Blend.bBlendEnable   = TRUE;
    Blend.SrcBlend       = RSTATE_BLEND_FACTOR_SRC_ALPHA;
    Blend.DestBlend      = RSTATE_BLEND_FACTOR_ONE;
    Blend.SrcBlendAlpha  = RSTATE_BLEND_FACTOR_ONE;
    Blend.DestBlendAlpha = RSTATE_BLEND_FACTOR_ONE;
    s_BlendPresets[RSTATE_BLEND_PRESET_ADD] = Blend;

    Blend = rstate_blend_desc();
    Blend.bBlendEnable   = TRUE;
    Blend.SrcBlend       = RSTATE_BLEND_FACTOR_SRC_ALPHA;
    Blend.DestBlend      = RSTATE_BLEND_FACTOR_ONE;
    Blend.BlendOp        = RSTATE_BLEND_OP_REV_SUBTRACT;
    Blend.SrcBlendAlpha  = RSTATE_BLEND_FACTOR_ONE;
    Blend.DestBlendAlpha = RSTATE_BLEND_FACTOR_ONE;
    Blend.BlendOpAlpha   = RSTATE_BLEND_OP_REV_SUBTRACT;
    s_BlendPresets[RSTATE_BLEND_PRESET_SUB] = Blend;

    Blend = rstate_blend_desc();
    Blend.bBlendEnable   = TRUE;
    Blend.SrcBlend       = RSTATE_BLEND_FACTOR_ONE;
    Blend.DestBlend      = RSTATE_BLEND_FACTOR_INV_SRC_ALPHA;
    Blend.SrcBlendAlpha  = RSTATE_BLEND_FACTOR_ONE;
    Blend.DestBlendAlpha = RSTATE_BLEND_FACTOR_INV_SRC_ALPHA;
    s_BlendPresets[RSTATE_BLEND_PRESET_PREMULT_ALPHA] = Blend;

    Blend = rstate_blend_desc();
    Blend.bBlendEnable   = TRUE;
    Blend.SrcBlend       = RSTATE_BLEND_FACTOR_ONE;
    Blend.DestBlend      = RSTATE_BLEND_FACTOR_ONE;
    Blend.SrcBlendAlpha  = RSTATE_BLEND_FACTOR_ONE;
    Blend.DestBlendAlpha = RSTATE_BLEND_FACTOR_ONE;
    s_BlendPresets[RSTATE_BLEND_PRESET_PREMULT_ADD] = Blend;

    Blend = rstate_blend_desc();
    Blend.bBlendEnable   = TRUE;
    Blend.SrcBlend       = RSTATE_BLEND_FACTOR_ONE;
    Blend.DestBlend      = RSTATE_BLEND_FACTOR_ONE;
    Blend.BlendOp        = RSTATE_BLEND_OP_REV_SUBTRACT;
    Blend.SrcBlendAlpha  = RSTATE_BLEND_FACTOR_ONE;
    Blend.DestBlendAlpha = RSTATE_BLEND_FACTOR_ONE;
    Blend.BlendOpAlpha   = RSTATE_BLEND_OP_REV_SUBTRACT;
    s_BlendPresets[RSTATE_BLEND_PRESET_PREMULT_SUB] = Blend;

    Blend = rstate_blend_desc();
    Blend.bBlendEnable   = TRUE;
    Blend.SrcBlend       = RSTATE_BLEND_FACTOR_DEST_COLOR;
    Blend.DestBlend      = RSTATE_BLEND_FACTOR_ZERO;
    Blend.SrcBlendAlpha  = RSTATE_BLEND_FACTOR_DEST_ALPHA;
    Blend.DestBlendAlpha = RSTATE_BLEND_FACTOR_ZERO;
    s_BlendPresets[RSTATE_BLEND_PRESET_MULTIPLY] = Blend;

    Blend = rstate_blend_desc();
    Blend.bBlendEnable   = TRUE;
    Blend.SrcBlend       = RSTATE_BLEND_FACTOR_DEST_COLOR;
    Blend.DestBlend      = RSTATE_BLEND_FACTOR_SRC_COLOR;
    Blend.SrcBlendAlpha  = RSTATE_BLEND_FACTOR_ZERO;
    Blend.DestBlendAlpha = RSTATE_BLEND_FACTOR_ONE;
    s_BlendPresets[RSTATE_BLEND_PRESET_INTENSITY] = Blend;

    Blend = rstate_blend_desc();
    Blend.ColorWriteMask = RSTATE_COLOR_WRITE_NONE;
    s_BlendPresets[RSTATE_BLEND_PRESET_COLOR_WRITE_DISABLE] = Blend;

    rstate_depth_desc Depth;
    s_DepthPresets[RSTATE_DEPTH_PRESET_NORMAL] = Depth;

    Depth = rstate_depth_desc();
    Depth.bDepthWrite = FALSE;
    s_DepthPresets[RSTATE_DEPTH_PRESET_NO_WRITE] = Depth;

    Depth = rstate_depth_desc();
    Depth.DepthFunc = RSTATE_COMPARE_ALWAYS;
    s_DepthPresets[RSTATE_DEPTH_PRESET_WRITE_ALWAYS] = Depth;

    Depth = rstate_depth_desc();
    Depth.bDepthEnable = FALSE;
    s_DepthPresets[RSTATE_DEPTH_PRESET_DISABLED] = Depth;

    Depth = rstate_depth_desc();
    Depth.bDepthEnable = FALSE;
    Depth.bDepthWrite  = FALSE;
    s_DepthPresets[RSTATE_DEPTH_PRESET_DISABLED_NO_WRITE] = Depth;

    rstate_raster_desc Raster;
    s_RasterPresets[RSTATE_RASTER_PRESET_SOLID] = Raster;

    Raster = rstate_raster_desc();
    Raster.FillMode = RSTATE_FILL_WIREFRAME;
    s_RasterPresets[RSTATE_RASTER_PRESET_WIRE] = Raster;

    Raster = rstate_raster_desc();
    Raster.CullMode = RSTATE_CULL_NONE;
    s_RasterPresets[RSTATE_RASTER_PRESET_SOLID_NO_CULL] = Raster;

    Raster = rstate_raster_desc();
    Raster.FillMode = RSTATE_FILL_WIREFRAME;
    Raster.CullMode = RSTATE_CULL_NONE;
    s_RasterPresets[RSTATE_RASTER_PRESET_WIRE_NO_CULL] = Raster;

    Raster = rstate_raster_desc();
    Raster.CullMode = RSTATE_CULL_NONE;
    Raster.DepthBias = -1.0f;
    Raster.SlopeScaledDepthBias = -1.0f;
    Raster.bDepthBiasEnable = TRUE;
    s_RasterPresets[RSTATE_RASTER_PRESET_COLLISION_BIASED] = Raster;

    rstate_sampler_desc Sampler;
    s_SamplerPresets[RSTATE_SAMPLER_PRESET_LINEAR_WRAP] = Sampler;

    Sampler = rstate_sampler_desc();
    Sampler.AddressU = RSTATE_TEXTURE_ADDRESS_CLAMP;
    Sampler.AddressV = RSTATE_TEXTURE_ADDRESS_CLAMP;
    Sampler.AddressW = RSTATE_TEXTURE_ADDRESS_CLAMP;
    s_SamplerPresets[RSTATE_SAMPLER_PRESET_LINEAR_CLAMP] = Sampler;

    Sampler = rstate_sampler_desc();
    Sampler.AddressV = RSTATE_TEXTURE_ADDRESS_CLAMP;
    s_SamplerPresets[RSTATE_SAMPLER_PRESET_LINEAR_WRAP_U_CLAMP_V] = Sampler;

    Sampler = rstate_sampler_desc();
    Sampler.AddressU = RSTATE_TEXTURE_ADDRESS_CLAMP;
    s_SamplerPresets[RSTATE_SAMPLER_PRESET_LINEAR_CLAMP_U_WRAP_V] = Sampler;

    Sampler = rstate_sampler_desc();
    Sampler.Filter = RSTATE_FILTER_POINT;
    s_SamplerPresets[RSTATE_SAMPLER_PRESET_POINT_WRAP] = Sampler;

    Sampler = rstate_sampler_desc();
    Sampler.Filter   = RSTATE_FILTER_POINT;
    Sampler.AddressU = RSTATE_TEXTURE_ADDRESS_CLAMP;
    Sampler.AddressV = RSTATE_TEXTURE_ADDRESS_CLAMP;
    Sampler.AddressW = RSTATE_TEXTURE_ADDRESS_CLAMP;
    s_SamplerPresets[RSTATE_SAMPLER_PRESET_POINT_CLAMP] = Sampler;

    Sampler = rstate_sampler_desc();
    Sampler.Filter        = RSTATE_FILTER_ANISOTROPIC;
    Sampler.MaxAnisotropy = 16;
    s_SamplerPresets[RSTATE_SAMPLER_PRESET_ANISOTROPIC_WRAP] = Sampler;

    Sampler = rstate_sampler_desc();
    Sampler.Filter        = RSTATE_FILTER_ANISOTROPIC;
    Sampler.MaxAnisotropy = 16;
    Sampler.AddressU      = RSTATE_TEXTURE_ADDRESS_CLAMP;
    Sampler.AddressV      = RSTATE_TEXTURE_ADDRESS_CLAMP;
    Sampler.AddressW      = RSTATE_TEXTURE_ADDRESS_CLAMP;
    s_SamplerPresets[RSTATE_SAMPLER_PRESET_ANISOTROPIC_CLAMP] = Sampler;

    s_bPresetsBuilt = TRUE;
}

//==============================================================================

static
void sdlstate_ReleaseSamplerBackend( rstate_sampler_backend* pBackend )
{
    if( !pBackend )
        return;

    if( pBackend->pSampler && g_pSDLGPUDevice )
        SDL_ReleaseGPUSampler( g_pSDLGPUDevice, pBackend->pSampler );

    pBackend->pSampler = NULL;
}

//==============================================================================

static
void sdlstate_FreePipelineKey( sdlstate_pipeline_key& Key )
{
    if( Key.pVertexBuffers )
        x_free( Key.pVertexBuffers );

    if( Key.pVertexAttributes )
        x_free( Key.pVertexAttributes );

    Key.pVertexBuffers       = NULL;
    Key.VertexBufferCount    = 0;
    Key.pVertexAttributes    = NULL;
    Key.VertexAttributeCount = 0;
}

//==============================================================================

static
xbool sdlstate_PipelineKeysEqual( const sdlstate_pipeline_key& A,
                                  const sdlstate_pipeline_key& B )
{
    if( (A.VertexShaderSerial     != B.VertexShaderSerial)     ||
        (A.PixelShaderSerial      != B.PixelShaderSerial)      ||
        (A.VertexBufferCount      != B.VertexBufferCount)      ||
        (A.VertexAttributeCount   != B.VertexAttributeCount)   ||
        (A.PrimitiveType          != B.PrimitiveType)          ||
        (A.ColorTargetCount       != B.ColorTargetCount)       ||
        (A.DepthStencilFormat     != B.DepthStencilFormat)     ||
        (A.bHasDepthStencilTarget != B.bHasDepthStencilTarget) )
    {
        return FALSE;
    }

    // These SDL state records and their explicit padding are zero-initialized
    // before fields are assigned, so their byte representation is deterministic.
    if( x_memcmp( &A.RasterizerState, &B.RasterizerState, sizeof(A.RasterizerState) ) != 0 ||
        x_memcmp( &A.MultisampleState, &B.MultisampleState, sizeof(A.MultisampleState) ) != 0 ||
        x_memcmp( &A.DepthStencilState, &B.DepthStencilState, sizeof(A.DepthStencilState) ) != 0 )
    {
        return FALSE;
    }

    if( A.VertexBufferCount &&
        (x_memcmp( A.pVertexBuffers,
                   B.pVertexBuffers,
                   sizeof(SDL_GPUVertexBufferDescription) * A.VertexBufferCount ) != 0) )
    {
        return FALSE;
    }

    if( A.VertexAttributeCount &&
        (x_memcmp( A.pVertexAttributes,
                   B.pVertexAttributes,
                   sizeof(SDL_GPUVertexAttribute) * A.VertexAttributeCount ) != 0) )
    {
        return FALSE;
    }

    if( A.ColorTargetCount &&
        (x_memcmp( A.ColorTargets,
                   B.ColorTargets,
                   sizeof(SDL_GPUColorTargetDescription) * A.ColorTargetCount ) != 0) )
    {
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

static
render_pipeline_cache_entry* sdlstate_FindPipeline( const sdlstate_pipeline_key& Key )
{
    for( render_pipeline_cache_entry* pEntry = s_pPipelineCache;
         pEntry;
         pEntry = pEntry->pNext )
    {
        if( sdlstate_PipelineKeysEqual( pEntry->Key, Key ) )
            return pEntry;
    }

    return NULL;
}

//==============================================================================

static
void sdlstate_LinkPipelineCacheEntry( render_pipeline_cache_entry* pEntry )
{
    pEntry->pNext   = s_pPipelineCache;
    s_pPipelineCache = pEntry;
    s_PipelineCount++;
}

//==============================================================================

static
void sdlstate_ReleasePipelineCacheEntry( render_pipeline_cache_entry* pEntry )
{
    if( !pEntry )
        return;

    if( pEntry->pPipeline && g_pSDLGPUDevice )
        SDL_ReleaseGPUGraphicsPipeline( g_pSDLGPUDevice, pEntry->pPipeline );

    pEntry->pPipeline = NULL;
    sdlstate_FreePipelineKey( pEntry->Key );
}

//==============================================================================

static
void sdlstate_DeletePipelineHandle( render_pipeline_backend* pBackend )
{
    if( !pBackend )
        return;

    if( pBackend->pDebugName )
        x_free( pBackend->pDebugName );

    pBackend->pDebugName = NULL;
    delete pBackend;
}

//==============================================================================

static
xbool sdlstate_FillVertexInputState( const shader_pipeline_desc& Desc,
                                     SDL_GPUVertexBufferDescription** ppVertexBuffers,
                                     SDL_GPUVertexAttribute** ppVertexAttributes,
                                     SDL_GPUVertexInputState& State )
{
    *ppVertexBuffers   = NULL;
    *ppVertexAttributes= NULL;

    x_memset( &State, 0, sizeof(State) );

    if( Desc.VertexBufferCount )
    {
        if( !Desc.pVertexBuffers )
            return FALSE;

        SDL_GPUVertexBufferDescription* pBuffers =
            (SDL_GPUVertexBufferDescription*)x_malloc( sizeof(SDL_GPUVertexBufferDescription) * Desc.VertexBufferCount );
        if( !pBuffers )
            return FALSE;

        x_memset( pBuffers, 0, sizeof(SDL_GPUVertexBufferDescription) * Desc.VertexBufferCount );

        for( u32 i = 0; i < Desc.VertexBufferCount; i++ )
        {
            const shader_vertex_buffer_desc& Src = Desc.pVertexBuffers[i];
            if( Src.InstanceStepRate != 0 )
            {
                x_DebugMsg( "SDLState: per-instance step rates other than 0 are not supported by SDL GPU yet\n" );
                x_free( pBuffers );
                return FALSE;
            }

            pBuffers[i].slot               = Src.Slot;
            pBuffers[i].pitch              = Src.Stride;
            pBuffers[i].input_rate         = Src.bPerInstance ? SDL_GPU_VERTEXINPUTRATE_INSTANCE : SDL_GPU_VERTEXINPUTRATE_VERTEX;
            pBuffers[i].instance_step_rate = 0;
        }

        *ppVertexBuffers = pBuffers;
        State.vertex_buffer_descriptions = pBuffers;
        State.num_vertex_buffers         = Desc.VertexBufferCount;
    }

    if( Desc.InputElementCount )
    {
        if( !Desc.pInputElements )
        {
            if( *ppVertexBuffers )
                x_free( *ppVertexBuffers );
            *ppVertexBuffers = NULL;
            return FALSE;
        }

        SDL_GPUVertexAttribute* pAttributes =
            (SDL_GPUVertexAttribute*)x_malloc( sizeof(SDL_GPUVertexAttribute) * Desc.InputElementCount );
        if( !pAttributes )
        {
            if( *ppVertexBuffers )
                x_free( *ppVertexBuffers );
            *ppVertexBuffers = NULL;
            return FALSE;
        }

        x_memset( pAttributes, 0, sizeof(SDL_GPUVertexAttribute) * Desc.InputElementCount );

        for( u32 i = 0; i < Desc.InputElementCount; i++ )
        {
            const shader_vertex_element& Src = Desc.pInputElements[i];
            const SDL_GPUVertexElementFormat Format = sdlstate_ToSDLVertexFormat( Src.Format );
            if( Format == SDL_GPU_VERTEXELEMENTFORMAT_INVALID )
            {
                x_DebugMsg( "SDLState: unsupported vertex element format %d\n", Src.Format );
                x_free( pAttributes );
                if( *ppVertexBuffers )
                    x_free( *ppVertexBuffers );
                *ppVertexBuffers = NULL;
                return FALSE;
            }

            pAttributes[i].location    = Src.Location;
            pAttributes[i].buffer_slot = Src.InputSlot;
            pAttributes[i].format      = Format;
            pAttributes[i].offset      = Src.AlignedByteOffset;
        }

        *ppVertexAttributes = pAttributes;
        State.vertex_attributes      = pAttributes;
        State.num_vertex_attributes  = Desc.InputElementCount;
    }

    return TRUE;
}

//==============================================================================

static
void sdlstate_FreeVertexInputState( SDL_GPUVertexBufferDescription* pVertexBuffers,
                                    SDL_GPUVertexAttribute* pVertexAttributes )
{
    if( pVertexBuffers )
        x_free( pVertexBuffers );

    if( pVertexAttributes )
        x_free( pVertexAttributes );
}

//==============================================================================

static
void sdlstate_FillBlendState( SDL_GPUColorTargetBlendState& Dst, const rstate_blend_desc& Src )
{
    x_memset( &Dst, 0, sizeof(Dst) );

    Dst.src_color_blendfactor   = sdlstate_ToSDLBlendFactor( Src.SrcBlend );
    Dst.dst_color_blendfactor   = sdlstate_ToSDLBlendFactor( Src.DestBlend );
    Dst.color_blend_op          = sdlstate_ToSDLBlendOp( Src.BlendOp );
    Dst.src_alpha_blendfactor   = sdlstate_ToSDLBlendFactor( Src.SrcBlendAlpha );
    Dst.dst_alpha_blendfactor   = sdlstate_ToSDLBlendFactor( Src.DestBlendAlpha );
    Dst.alpha_blend_op          = sdlstate_ToSDLBlendOp( Src.BlendOpAlpha );
    Dst.color_write_mask        = sdlstate_ToSDLColorMask( Src.ColorWriteMask );
    Dst.enable_blend            = Src.bBlendEnable ? true : false;
    Dst.enable_color_write_mask = true;
}

//==============================================================================

static
void sdlstate_FillDepthStencilState( SDL_GPUDepthStencilState& Dst,
                                     const rstate_depth_desc& Src,
                                     xbool bHasDepthStencilTarget )
{
    x_memset( &Dst, 0, sizeof(Dst) );

    Dst.compare_op          = sdlstate_ToSDLCompareOp( Src.DepthFunc );
    Dst.compare_mask        = Src.StencilReadMask;
    Dst.write_mask          = Src.StencilWriteMask;
    Dst.enable_depth_test   = (bHasDepthStencilTarget && Src.bDepthEnable) ? true : false;
    Dst.enable_depth_write  = (Dst.enable_depth_test && Src.bDepthWrite) ? true : false;
    Dst.enable_stencil_test = (bHasDepthStencilTarget && Src.bStencilEnable) ? true : false;

    sdlstate_FillStencilFace( Dst.front_stencil_state, Src.FrontFace );
    sdlstate_FillStencilFace( Dst.back_stencil_state,  Src.BackFace  );
}

//==============================================================================
//  SYSTEM FUNCTIONS
//==============================================================================

void rstate_Init( void )
{
    sdlstate_BuildPresets();
}

//==============================================================================

void rstate_Kill( void )
{
    s_pBoundPipeline = NULL;

    while( s_pSamplerList )
    {
        rstate_sampler_backend* pBackend = s_pSamplerList;
        sdleng_UnlinkBackend( s_pSamplerList, pBackend, s_SamplerCount );
        sdlstate_ReleaseSamplerBackend( pBackend );

        if( pBackend->pOwner )
        {
            pBackend->pOwner->pBackend = NULL;
            pBackend->pOwner           = NULL;
        }

        delete pBackend;
    }

    while( s_pPipelineHandleList )
    {
        render_pipeline_backend* pBackend = s_pPipelineHandleList;
        sdleng_UnlinkBackend( s_pPipelineHandleList, pBackend, s_PipelineHandleCount );
        sdleng_ClearGraphicsPipelineDebug( pBackend );

        if( pBackend->pEntry )
        {
            ASSERT( pBackend->pEntry->HandleCount > 0 );
            pBackend->pEntry->HandleCount--;
            pBackend->pEntry = NULL;
        }

        if( pBackend->pOwner )
        {
            pBackend->pOwner->pBackend = NULL;
            pBackend->pOwner           = NULL;
        }

        sdlstate_DeletePipelineHandle( pBackend );
    }

    while( s_pPipelineCache )
    {
        render_pipeline_cache_entry* pEntry = s_pPipelineCache;
        s_pPipelineCache = pEntry->pNext;
        ASSERT( pEntry->HandleCount == 0 );
        sdlstate_ReleasePipelineCacheEntry( pEntry );
        delete pEntry;
    }

    s_PipelineCount = 0;
}

//==============================================================================
//  PRESET ACCESS
//==============================================================================

const rstate_blend_desc& rstate_GetBlendDesc( rstate_blend_preset Preset )
{
    sdlstate_BuildPresets();

    if( (Preset < 0) || (Preset >= RSTATE_BLEND_PRESET_COUNT) )
    {
        ASSERT( FALSE );
        return s_BlendPresets[RSTATE_BLEND_PRESET_NONE];
    }

    return s_BlendPresets[Preset];
}

//==============================================================================

const rstate_depth_desc& rstate_GetDepthDesc( rstate_depth_preset Preset )
{
    sdlstate_BuildPresets();

    if( (Preset < 0) || (Preset >= RSTATE_DEPTH_PRESET_COUNT) )
    {
        ASSERT( FALSE );
        return s_DepthPresets[RSTATE_DEPTH_PRESET_NORMAL];
    }

    return s_DepthPresets[Preset];
}

//==============================================================================

const rstate_raster_desc& rstate_GetRasterDesc( rstate_raster_preset Preset )
{
    sdlstate_BuildPresets();

    if( (Preset < 0) || (Preset >= RSTATE_RASTER_PRESET_COUNT) )
    {
        ASSERT( FALSE );
        return s_RasterPresets[RSTATE_RASTER_PRESET_SOLID];
    }

    return s_RasterPresets[Preset];
}

//==============================================================================

const rstate_sampler_desc& rstate_GetSamplerDesc( rstate_sampler_preset Preset )
{
    sdlstate_BuildPresets();

    if( (Preset < 0) || (Preset >= RSTATE_SAMPLER_PRESET_COUNT) )
    {
        ASSERT( FALSE );
        return s_SamplerPresets[RSTATE_SAMPLER_PRESET_LINEAR_WRAP];
    }

    return s_SamplerPresets[Preset];
}

//==============================================================================
//  PIPELINE OBJECTS
//==============================================================================

xbool render_CreatePipeline( render_pipeline& Pipeline, const render_pipeline_desc& Desc )
{
    if( !g_pSDLGPUDevice )
        return FALSE;

    if( Desc.ColorCount > RTARGET_MAX_TARGETS )
        return FALSE;

    if( (Desc.ColorCount == 0) && (Desc.DepthFormat == RTARGET_FORMAT_COUNT) )
        return FALSE;

    if( Desc.SampleMask != 0xffffffff )
    {
        x_DebugMsg( "SDLState: custom sample masks are not supported by SDL GPU yet\n" );
        return FALSE;
    }

    SDL_GPUSampleCount SDLSampleCount;
    if( !sdleng_ToSDLSampleCount( Desc.SampleCount, SDLSampleCount ) )
        return FALSE;

    if( !Desc.Shader.pVertexShader || !Desc.Shader.pPixelShader )
        return FALSE;

    if( Desc.Shader.pVertexShader->Stage != SHADER_STAGE_VERTEX ||
        Desc.Shader.pPixelShader->Stage  != SHADER_STAGE_PIXEL )
    {
        return FALSE;
    }

    SDL_GPUShader* pVertexShader = sdleng_GetGPUShader( Desc.Shader.pVertexShader );
    SDL_GPUShader* pPixelShader  = sdleng_GetGPUShader( Desc.Shader.pPixelShader );
    if( !pVertexShader || !pPixelShader )
        return FALSE;

    const shader_backend* pVertexShaderBackend = Desc.Shader.pVertexShader->pBackend;
    const shader_backend* pPixelShaderBackend  = Desc.Shader.pPixelShader->pBackend;
    if( !pVertexShaderBackend || !pPixelShaderBackend )
        return FALSE;

    if( Desc.Shader.Topology == SHADER_TOPOLOGY_UNDEFINED )
        return FALSE;

    SDL_GPUVertexBufferDescription* pVertexBuffers = NULL;
    SDL_GPUVertexAttribute*         pVertexAttrs   = NULL;
    SDL_GPUVertexInputState         VertexInput;
    if( !sdlstate_FillVertexInputState( Desc.Shader, &pVertexBuffers, &pVertexAttrs, VertexInput ) )
        return FALSE;

    SDL_GPUColorTargetDescription ColorTargets[RTARGET_MAX_TARGETS];
    x_memset( ColorTargets, 0, sizeof(ColorTargets) );

    for( u32 i = 0; i < Desc.ColorCount; i++ )
    {
        if( sdleng_IsDepthFormat( Desc.ColorTargets[i].Format ) )
        {
            sdlstate_FreeVertexInputState( pVertexBuffers, pVertexAttrs );
            return FALSE;
        }

        SDL_GPUTextureFormat Format = sdleng_ToSDLTextureFormat( Desc.ColorTargets[i].Format );
        if( Format == SDL_GPU_TEXTUREFORMAT_INVALID )
        {
            sdlstate_FreeVertexInputState( pVertexBuffers, pVertexAttrs );
            return FALSE;
        }

        if( !SDL_GPUTextureSupportsSampleCount( g_pSDLGPUDevice, Format, SDLSampleCount ) )
        {
            x_DebugMsg( "SDLState: sample count %d is not supported for color target format %d\n",
                        Desc.SampleCount,
                        Desc.ColorTargets[i].Format );
            sdlstate_FreeVertexInputState( pVertexBuffers, pVertexAttrs );
            return FALSE;
        }

        if( !SDL_GPUTextureSupportsFormat( g_pSDLGPUDevice,
                                           Format,
                                           SDL_GPU_TEXTURETYPE_2D,
                                           SDL_GPU_TEXTUREUSAGE_COLOR_TARGET ) )
        {
            x_DebugMsg( "SDLState: color target format %d is not supported\n",
                        Desc.ColorTargets[i].Format );
            sdlstate_FreeVertexInputState( pVertexBuffers, pVertexAttrs );
            return FALSE;
        }

        ColorTargets[i].format = Format;
        sdlstate_FillBlendState( ColorTargets[i].blend_state, Desc.ColorTargets[i].Blend );
    }

    const xbool bHasDepth = (Desc.DepthFormat != RTARGET_FORMAT_COUNT);
    SDL_GPUTextureFormat DepthFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
    if( bHasDepth )
    {
        if( !sdleng_IsDepthFormat( Desc.DepthFormat ) )
        {
            sdlstate_FreeVertexInputState( pVertexBuffers, pVertexAttrs );
            return FALSE;
        }

        DepthFormat = sdleng_ToSDLTextureFormat( Desc.DepthFormat );
        if( DepthFormat == SDL_GPU_TEXTUREFORMAT_INVALID )
        {
            sdlstate_FreeVertexInputState( pVertexBuffers, pVertexAttrs );
            return FALSE;
        }

        if( !SDL_GPUTextureSupportsSampleCount( g_pSDLGPUDevice, DepthFormat, SDLSampleCount ) )
        {
            x_DebugMsg( "SDLState: sample count %d is not supported for depth target format %d\n",
                        Desc.SampleCount,
                        Desc.DepthFormat );
            sdlstate_FreeVertexInputState( pVertexBuffers, pVertexAttrs );
            return FALSE;
        }
        if( !SDL_GPUTextureSupportsFormat( g_pSDLGPUDevice,
                                           DepthFormat,
                                           SDL_GPU_TEXTURETYPE_2D,
                                           SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET ) )
        {
            x_DebugMsg( "SDLState: depth target format %d is not supported\n", Desc.DepthFormat );
            sdlstate_FreeVertexInputState( pVertexBuffers, pVertexAttrs );
            return FALSE;
        }

        if( Desc.Depth.bStencilEnable && !sdleng_HasStencil( Desc.DepthFormat ) )
        {
            x_DebugMsg( "SDLState: stencil state requires a stencil-capable depth target\n" );
            sdlstate_FreeVertexInputState( pVertexBuffers, pVertexAttrs );
            return FALSE;
        }
    }

    if( Desc.bAlphaToCoverage &&
        ((Desc.ColorCount == 0) || !sdleng_HasAlpha( Desc.ColorTargets[0].Format )) )
    {
        x_DebugMsg( "SDLState: alpha-to-coverage requires an alpha color target\n" );
        sdlstate_FreeVertexInputState( pVertexBuffers, pVertexAttrs );
        return FALSE;
    }

    SDL_GPUGraphicsPipelineCreateInfo CreateInfo;
    x_memset( &CreateInfo, 0, sizeof(CreateInfo) );
    CreateInfo.vertex_shader   = pVertexShader;
    CreateInfo.fragment_shader = pPixelShader;
    CreateInfo.vertex_input_state = VertexInput;
    CreateInfo.primitive_type = sdlstate_ToSDLPrimitiveType( Desc.Shader.Topology );
    CreateInfo.rasterizer_state.fill_mode = sdlstate_ToSDLFillMode( Desc.Raster.FillMode );
    CreateInfo.rasterizer_state.cull_mode = sdlstate_ToSDLCullMode( Desc.Raster.CullMode );
    CreateInfo.rasterizer_state.front_face = sdlstate_ToSDLFrontFace( Desc.Raster.FrontFace );
    CreateInfo.rasterizer_state.depth_bias_constant_factor = Desc.Raster.DepthBias;
    CreateInfo.rasterizer_state.depth_bias_clamp = Desc.Raster.DepthBiasClamp;
    CreateInfo.rasterizer_state.depth_bias_slope_factor = Desc.Raster.SlopeScaledDepthBias;
    CreateInfo.rasterizer_state.enable_depth_bias = Desc.Raster.bDepthBiasEnable ? true : false;
    CreateInfo.rasterizer_state.enable_depth_clip = Desc.Raster.bDepthClipEnable ? true : false;
    CreateInfo.multisample_state.sample_count = SDLSampleCount;
    CreateInfo.multisample_state.sample_mask = 0;
    CreateInfo.multisample_state.enable_mask = false;
    CreateInfo.multisample_state.enable_alpha_to_coverage = Desc.bAlphaToCoverage ? true : false;
    sdlstate_FillDepthStencilState( CreateInfo.depth_stencil_state, Desc.Depth, bHasDepth );
    CreateInfo.target_info.color_target_descriptions = (Desc.ColorCount > 0) ? ColorTargets : NULL;
    CreateInfo.target_info.num_color_targets = Desc.ColorCount;
    CreateInfo.target_info.depth_stencil_format = DepthFormat;
    CreateInfo.target_info.has_depth_stencil_target = bHasDepth ? true : false;

    sdlstate_pipeline_key Key;
    Key.VertexShaderSerial     = pVertexShaderBackend->Serial;
    Key.PixelShaderSerial      = pPixelShaderBackend->Serial;
    Key.pVertexBuffers         = pVertexBuffers;
    Key.VertexBufferCount      = VertexInput.num_vertex_buffers;
    Key.pVertexAttributes      = pVertexAttrs;
    Key.VertexAttributeCount   = VertexInput.num_vertex_attributes;
    Key.PrimitiveType          = CreateInfo.primitive_type;
    Key.RasterizerState        = CreateInfo.rasterizer_state;
    Key.MultisampleState       = CreateInfo.multisample_state;
    Key.DepthStencilState      = CreateInfo.depth_stencil_state;
    Key.ColorTargetCount       = CreateInfo.target_info.num_color_targets;
    Key.DepthStencilFormat     = CreateInfo.target_info.depth_stencil_format;
    Key.bHasDepthStencilTarget = CreateInfo.target_info.has_depth_stencil_target ? TRUE : FALSE;
    if( Key.ColorTargetCount )
    {
        x_memcpy( Key.ColorTargets,
                  ColorTargets,
                  sizeof(SDL_GPUColorTargetDescription) * Key.ColorTargetCount );
    }

    render_pipeline_cache_entry* pEntry = sdlstate_FindPipeline( Key );
    const xbool bCacheHit = (pEntry != NULL);
    static xprofile_counter PipelineRequestMetric =
        x_GetProfiler().RegisterCounter( "PipelineRequests", "Renderer" );
    static xprofile_counter PipelineCacheHitMetric =
        x_GetProfiler().RegisterCounter( "PipelineCacheHits", "Renderer" );
    PipelineRequestMetric.Add();
    if( bCacheHit )
        PipelineCacheHitMetric.Add();

    render_pipeline_backend* pBackend = new render_pipeline_backend;
    if( !pBackend )
    {
        sdlstate_FreePipelineKey( Key );
        return FALSE;
    }

    if( Desc.pDebugName && Desc.pDebugName[0] )
    {
        pBackend->pDebugName = x_strdup( Desc.pDebugName );
        if( !pBackend->pDebugName )
        {
            sdlstate_DeletePipelineHandle( pBackend );
            sdlstate_FreePipelineKey( Key );
            return FALSE;
        }
    }

    pBackend->VertexResources = pVertexShaderBackend->Resources;
    pBackend->PixelResources  = pPixelShaderBackend->Resources;

    if( !pEntry )
    {
        SDL_PropertiesID Props = 0;
        if( Desc.pDebugName && Desc.pDebugName[0] )
        {
            Props = SDL_CreateProperties();
            if( !Props )
            {
                sdleng_LogError( "SDLState", "SDL_CreateProperties" );
                sdlstate_DeletePipelineHandle( pBackend );
                sdlstate_FreePipelineKey( Key );
                return FALSE;
            }

            if( !SDL_SetStringProperty( Props, SDL_PROP_GPU_GRAPHICSPIPELINE_CREATE_NAME_STRING, Desc.pDebugName ) )
            {
                sdleng_LogError( "SDLState", "SDL_SetStringProperty" );
                SDL_DestroyProperties( Props );
                sdlstate_DeletePipelineHandle( pBackend );
                sdlstate_FreePipelineKey( Key );
                return FALSE;
            }
        }

        CreateInfo.props = Props;
        static xprofile_zone PipelineCreateAPIMetric =
            x_GetProfiler().RegisterZone( "PipelineCreateAPI", "RendererAPI" );
        const xtick PipelineCreateStart = x_GetTime();
        SDL_GPUGraphicsPipeline* pPipeline =
            SDL_CreateGPUGraphicsPipeline( g_pSDLGPUDevice, &CreateInfo );
        const xtick PipelineCreateTicks = x_GetTime() - PipelineCreateStart;
        PipelineCreateAPIMetric.Record( PipelineCreateTicks );

        //const char* pPipelineName = (Desc.pDebugName && Desc.pDebugName[0])
        //                          ? Desc.pDebugName
        //                          : "UnnamedGraphicsPipeline";
        //x_DebugMsg( "SDLState: graphics PSO '%s' %s in %.3f ms "
        //            "[cache=%u, VS=%llu, PS=%llu, topology=%d, colors=%u, color0=%d, "
        //            "depth=%d, samples=%u, blend={on=%d,src=%d,dst=%d,op=%d}, "
        //            "depthState={test=%d,write=%d,cmp=%d}, "
        //            "raster={fill=%d,cull=%d,front=%d}]\n",
        //            pPipelineName,
        //            pPipeline ? "created" : "FAILED",
        //            x_TicksToMs( PipelineCreateTicks ),
        //            pPipeline ? (s_PipelineCount + 1) : s_PipelineCount,
        //            (unsigned long long)Key.VertexShaderSerial,
        //            (unsigned long long)Key.PixelShaderSerial,
        //            (s32)Key.PrimitiveType,
        //            Key.ColorTargetCount,
        //            (Key.ColorTargetCount > 0) ? (s32)Desc.ColorTargets[0].Format : (s32)RTARGET_FORMAT_COUNT,
        //            (s32)Desc.DepthFormat,
        //            Desc.SampleCount,
        //            (Key.ColorTargetCount > 0) ? (s32)Key.ColorTargets[0].blend_state.enable_blend : 0,
        //            (Key.ColorTargetCount > 0) ? (s32)Key.ColorTargets[0].blend_state.src_color_blendfactor : 0,
        //            (Key.ColorTargetCount > 0) ? (s32)Key.ColorTargets[0].blend_state.dst_color_blendfactor : 0,
        //            (Key.ColorTargetCount > 0) ? (s32)Key.ColorTargets[0].blend_state.color_blend_op : 0,
        //            (s32)Key.DepthStencilState.enable_depth_test,
        //            (s32)Key.DepthStencilState.enable_depth_write,
        //            (s32)Key.DepthStencilState.compare_op,
        //            (s32)Key.RasterizerState.fill_mode,
        //            (s32)Key.RasterizerState.cull_mode,
        //            (s32)Key.RasterizerState.front_face );

        if( Props )
            SDL_DestroyProperties( Props );

        if( !pPipeline )
        {
            sdleng_LogError( "SDLState", "SDL_CreateGPUGraphicsPipeline" );
            sdlstate_DeletePipelineHandle( pBackend );
            sdlstate_FreePipelineKey( Key );
            return FALSE;
        }

        pEntry = new render_pipeline_cache_entry;
        if( !pEntry )
        {
            SDL_ReleaseGPUGraphicsPipeline( g_pSDLGPUDevice, pPipeline );
            sdlstate_DeletePipelineHandle( pBackend );
            sdlstate_FreePipelineKey( Key );
            return FALSE;
        }

        pEntry->pPipeline = pPipeline;
        pEntry->Key       = Key;
        Key.pVertexBuffers       = NULL;
        Key.VertexBufferCount    = 0;
        Key.pVertexAttributes    = NULL;
        Key.VertexAttributeCount = 0;

        sdlstate_LinkPipelineCacheEntry( pEntry );
        static xprofile_counter PipelineCreateMetric =
            x_GetProfiler().RegisterCounter( "PipelineCreates", "Renderer" );
        PipelineCreateMetric.Add();
    }

    sdlstate_FreePipelineKey( Key );

    render_DestroyPipeline( Pipeline );

    pBackend->pOwner = &Pipeline;
    pBackend->pEntry = pEntry;
    pEntry->HandleCount++;
    Pipeline.pBackend = pBackend;
    sdleng_LinkBackend( s_pPipelineHandleList, pBackend, s_PipelineHandleCount );
    return TRUE;
}

//==============================================================================

void render_DestroyPipeline( render_pipeline& Pipeline )
{
    render_pipeline_backend* pBackend = Pipeline.pBackend;
    if( !pBackend )
        return;

    if( pBackend->pOwner != &Pipeline )
    {
        ASSERT( FALSE );
        Pipeline.pBackend = NULL;
        return;
    }

    sdleng_ClearGraphicsPipelineDebug( pBackend );
    sdleng_UnlinkBackend( s_pPipelineHandleList, pBackend, s_PipelineHandleCount );

    if( pBackend->pEntry )
    {
        ASSERT( pBackend->pEntry->HandleCount > 0 );
        pBackend->pEntry->HandleCount--;
    }

    pBackend->pEntry = NULL;
    pBackend->pOwner = NULL;
    sdlstate_DeletePipelineHandle( pBackend );
    Pipeline.pBackend = NULL;
}

//==============================================================================

xbool render_BindPipeline( const render_pipeline& Pipeline )
{
    if( !Pipeline.pBackend ||
        (Pipeline.pBackend->pOwner != &Pipeline) ||
        !Pipeline.pBackend->pEntry ||
        !Pipeline.pBackend->pEntry->pPipeline )
    {
        return FALSE;
    }

    SDL_GPURenderPass* pRenderPass = sdleng_GetRenderPass();
    if( !pRenderPass )
    {
        x_DebugMsg( "SDLState: no active render pass for pipeline bind\n" );
        return FALSE;
    }

    const render_pipeline_cache_entry* pEntry = Pipeline.pBackend->pEntry;
    const xbool bIssued = (s_pBoundPipeline != pEntry);
    static xprofile_counter PipelineBindRequestMetric =
        x_GetProfiler().RegisterCounter( "PipelineBindRequests", "Renderer" );
    static xprofile_counter PipelineBindIssuedMetric =
        x_GetProfiler().RegisterCounter( "PipelineBindIssued", "Renderer" );
    PipelineBindRequestMetric.Add();
    if( bIssued )
    {
        PipelineBindIssuedMetric.Add();
        const xbool bFineTiming = x_GetProfiler().IsFineTimingEnabled();
        const xtick Start = bFineTiming ? x_GetTime() : 0;
        SDL_BindGPUGraphicsPipeline( pRenderPass, pEntry->pPipeline );
        const xtick End = bFineTiming ? x_GetTime() : 0;
        if( bFineTiming )
        {
            static xprofile_zone PipelineBindMetric =
                x_GetProfiler().RegisterZone( "PipelineBindAPI", "RendererAPI" );
            PipelineBindMetric.Record( End - Start );
        }
        s_pBoundPipeline = pEntry;
    }

    sdleng_SetGraphicsPipelineDebug( Pipeline.pBackend );
    return TRUE;
}

//==============================================================================

void sdleng_ResetPipelineBinding( void )
{
    s_pBoundPipeline = NULL;
}

//==============================================================================

u32 sdleng_GetPipelineCount( void )
{
    return s_PipelineCount;
}

//==============================================================================

u32 sdleng_GetPipelineHandleCount( void )
{
    return s_PipelineHandleCount;
}

//==============================================================================
//  SAMPLER OBJECTS
//==============================================================================

xbool rstate_CreateSampler( rstate_sampler& Sampler, const rstate_sampler_desc& Desc )
{
    rstate_DestroySampler( Sampler );

    if( !g_pSDLGPUDevice )
        return FALSE;

    SDL_GPUFilter MinFilter;
    SDL_GPUFilter MagFilter;
    SDL_GPUSamplerMipmapMode MipmapMode;
    xbool bAnisotropy;
    xbool bCompare;
    if( !sdlstate_ToSDLSamplerFilter( Desc.Filter, MinFilter, MagFilter, MipmapMode, bAnisotropy, bCompare ) )
        return FALSE;

    SDL_GPUSamplerAddressMode AddressU;
    SDL_GPUSamplerAddressMode AddressV;
    SDL_GPUSamplerAddressMode AddressW;
    if( !sdlstate_ToSDLAddressMode( Desc.AddressU, AddressU ) ||
        !sdlstate_ToSDLAddressMode( Desc.AddressV, AddressV ) ||
        !sdlstate_ToSDLAddressMode( Desc.AddressW, AddressW ) )
    {
        return FALSE;
    }

    SDL_PropertiesID Props = 0;
    if( Desc.pDebugName && Desc.pDebugName[0] )
    {
        Props = SDL_CreateProperties();
        if( !Props )
        {
            sdleng_LogError( "SDLState", "SDL_CreateProperties" );
            return FALSE;
        }

        if( !SDL_SetStringProperty( Props, SDL_PROP_GPU_SAMPLER_CREATE_NAME_STRING, Desc.pDebugName ) )
        {
            sdleng_LogError( "SDLState", "SDL_SetStringProperty" );
            SDL_DestroyProperties( Props );
            return FALSE;
        }
    }

    SDL_GPUSamplerCreateInfo CreateInfo;
    x_memset( &CreateInfo, 0, sizeof(CreateInfo) );
    CreateInfo.min_filter        = MinFilter;
    CreateInfo.mag_filter        = MagFilter;
    CreateInfo.mipmap_mode       = MipmapMode;
    CreateInfo.address_mode_u    = AddressU;
    CreateInfo.address_mode_v    = AddressV;
    CreateInfo.address_mode_w    = AddressW;
    CreateInfo.mip_lod_bias      = Desc.MipLODBias;
    CreateInfo.max_anisotropy    = (Desc.MaxAnisotropy > 0) ? (f32)Desc.MaxAnisotropy : 1.0f;
    CreateInfo.compare_op        = sdlstate_ToSDLCompareOp( Desc.ComparisonFunc );
    CreateInfo.min_lod           = Desc.MinLOD;
    CreateInfo.max_lod           = Desc.MaxLOD;
    CreateInfo.enable_anisotropy = bAnisotropy ? true : false;
    CreateInfo.enable_compare    = bCompare ? true : false;
    CreateInfo.props             = Props;

    SDL_GPUSampler* pSampler = SDL_CreateGPUSampler( g_pSDLGPUDevice, &CreateInfo );

    if( Props )
        SDL_DestroyProperties( Props );

    if( !pSampler )
    {
        sdleng_LogError( "SDLState", "SDL_CreateGPUSampler" );
        return FALSE;
    }

    rstate_sampler_backend* pBackend = new rstate_sampler_backend;
    if( !pBackend )
    {
        SDL_ReleaseGPUSampler( g_pSDLGPUDevice, pSampler );
        return FALSE;
    }

    pBackend->pSampler = pSampler;
    pBackend->pOwner   = &Sampler;
    Sampler.pBackend   = pBackend;

    sdleng_LinkBackend( s_pSamplerList, pBackend, s_SamplerCount );
    return TRUE;
}

//==============================================================================

xbool rstate_CreateSampler( rstate_sampler& Sampler, rstate_sampler_preset Preset, const char* pDebugName )
{
    rstate_sampler_desc Desc = rstate_GetSamplerDesc( Preset );
    Desc.pDebugName = pDebugName;
    return rstate_CreateSampler( Sampler, Desc );
}

//==============================================================================

void rstate_DestroySampler( rstate_sampler& Sampler )
{
    rstate_sampler_backend* pBackend = Sampler.pBackend;
    if( !pBackend )
        return;

    sdleng_UnlinkBackend( s_pSamplerList, pBackend, s_SamplerCount );
    sdlstate_ReleaseSamplerBackend( pBackend );
    delete pBackend;

    Sampler.pBackend = NULL;
}

//==============================================================================
//  DYNAMIC PASS STATE
//==============================================================================

xbool render_SetBlendConstants( const f32 Color[4] )
{
    if( !Color )
        return FALSE;

    SDL_GPURenderPass* pRenderPass = sdleng_GetRenderPass();
    if( !pRenderPass )
        return FALSE;

    SDL_FColor SDLColor;
    SDLColor.r = Color[0];
    SDLColor.g = Color[1];
    SDLColor.b = Color[2];
    SDLColor.a = Color[3];
    static xprofile_counter DynamicStateCountMetric =
        x_GetProfiler().RegisterCounter( "DynamicStateCalls", "Renderer" );
    DynamicStateCountMetric.Add();
    const xbool bFineTiming = x_GetProfiler().IsFineTimingEnabled();
    const xtick Start = bFineTiming ? x_GetTime() : 0;
    SDL_SetGPUBlendConstants( pRenderPass, SDLColor );
    const xtick End = bFineTiming ? x_GetTime() : 0;
    if( bFineTiming )
    {
        static xprofile_zone DynamicStateMetric =
            x_GetProfiler().RegisterZone( "DynamicStateAPI", "RendererAPI" );
        DynamicStateMetric.Record( End - Start );
    }
    return TRUE;
}

//==============================================================================

xbool render_SetStencilRef( u8 StencilRef )
{
    SDL_GPURenderPass* pRenderPass = sdleng_GetRenderPass();
    if( !pRenderPass )
        return FALSE;

    static xprofile_counter DynamicStateCountMetric =
        x_GetProfiler().RegisterCounter( "DynamicStateCalls", "Renderer" );
    DynamicStateCountMetric.Add();
    const xbool bFineTiming = x_GetProfiler().IsFineTimingEnabled();
    const xtick Start = bFineTiming ? x_GetTime() : 0;
    SDL_SetGPUStencilReference( pRenderPass, StencilRef );
    const xtick End = bFineTiming ? x_GetTime() : 0;
    if( bFineTiming )
    {
        static xprofile_zone DynamicStateMetric =
            x_GetProfiler().RegisterZone( "DynamicStateAPI", "RendererAPI" );
        DynamicStateMetric.Record( End - Start );
    }
    return TRUE;
}

//==============================================================================
#endif // defined(TARGET_DESKTOP) && defined(ENTROPY_RENDER_SDL)
//==============================================================================
