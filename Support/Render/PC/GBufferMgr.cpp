//==============================================================================
//
//  GBufferMgr.cpp
//
//  Mini G-Buffer Manager for Forward Renderer
//
//==============================================================================

#include "x_types.hpp"

#if !defined(TARGET_PC)
#error "This is only for the PC target platform. Please check build exclusion rules"
#endif

//==============================================================================
//  INCLUDES
//==============================================================================

#include "GBufferMgr.hpp"
#include "Entropy/D3DEngine/d3deng_composite.hpp"

//==============================================================================
//  EXTERNAL VARIABLES
//==============================================================================

extern ID3D11Device*        g_pd3dDevice;
extern ID3D11DeviceContext* g_pd3dContext;

//==============================================================================
//  INTERNAL CONSTANTS
//==============================================================================

#define GBUFFER_FORMAT_FINAL_COLOR   RTARGET_FORMAT_RGBA8
#define GBUFFER_FORMAT_ALBEDO        RTARGET_FORMAT_RGBA8
#define GBUFFER_FORMAT_NORMAL        RTARGET_FORMAT_RGBA16F
#define GBUFFER_FORMAT_LINEAR_DEPTH  RTARGET_FORMAT_RGBA16F
#define GBUFFER_FORMAT_GLOW          RTARGET_FORMAT_RGBA16F
#define GBUFFER_FORMAT_DEPTH         RTARGET_FORMAT_DEPTH24_STENCIL8

//------------------------------------------------------------------------------

enum
{
    MRT_ALBEDO        = 0,
    MRT_NORMAL        = 1,
    MRT_LINEAR_DEPTH  = 2,
    MRT_GLOW          = 3
};

//------------------------------------------------------------------------------

enum
{
    BIND_SLOT_FINAL_COLOR   = 0,
    BIND_SLOT_ALBEDO        = 1,
    BIND_SLOT_NORMAL        = 2,
    BIND_SLOT_LINEAR_DEPTH  = 3,
    BIND_SLOT_GLOW          = 4,
    BIND_SLOT_COUNT
};

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

gbuffer_mgr g_GBufferMgr;

//==============================================================================
//  CONSTRUCTION / DESTRUCTION
//==============================================================================

gbuffer_mgr::gbuffer_mgr( void ) :
    m_bInitialized          ( FALSE ),
    m_bGBufferValid         ( FALSE ),
    m_bGBufferTargetsActive ( FALSE ),
    m_GBufferWidth          ( 0 ),
    m_GBufferHeight         ( 0 ),
    m_pOverrideColor        ( NULL ),
    m_pOverrideDepth        ( NULL )
{
}

//==============================================================================

gbuffer_mgr::~gbuffer_mgr( void )
{
}

//==============================================================================

void gbuffer_mgr::Init( void )
{
    if( m_bInitialized )
        return;

    m_bInitialized = TRUE;
}

//==============================================================================

void gbuffer_mgr::Kill( void )
{
    if( !m_bInitialized )
        return;

    DestroyGBuffer();
    m_bInitialized = FALSE;
}

//==============================================================================
//  G-BUFFER LIFETIME
//==============================================================================

xbool gbuffer_mgr::CreateTarget( rtarget& Target, rtarget_format Format, const char* pErrorMsg )
{
    rtarget_registration reg;
    reg.Policy         = RTARGET_SIZE_RELATIVE_TO_VIEW;
    reg.ScaleX         = 1.0f;
    reg.ScaleY         = 1.0f;
    reg.Format         = Format;
    reg.SampleCount    = 1;
    reg.SampleQuality  = 0;
    reg.bBindAsTexture = TRUE;

    if( rtarget_GetOrCreate( Target, reg ) )
        return TRUE;

    DestroyGBuffer();
    x_throw( pErrorMsg );
    return FALSE;
}

//==============================================================================

xbool gbuffer_mgr::InitGBuffer( u32 Width, u32 Height )
{
    if( !g_pd3dDevice )
        return FALSE;

    m_GBufferWidth  = Width;
    m_GBufferHeight = Height;

    if( !CreateTarget( m_SceneColorTarget,                   GBUFFER_FORMAT_FINAL_COLOR,
                       "Failed to create GBuffer Scene Color target" ) )      return FALSE;

    if( !CreateTarget( m_GBufferTarget[ MRT_ALBEDO ],        GBUFFER_FORMAT_ALBEDO,
                       "Failed to create GBuffer Albedo target" ) )           return FALSE;

    if( !CreateTarget( m_GBufferTarget[ MRT_NORMAL ],        GBUFFER_FORMAT_NORMAL,
                       "Failed to create GBuffer Normal target" ) )           return FALSE;

    if( !CreateTarget( m_GBufferTarget[ MRT_LINEAR_DEPTH ],  GBUFFER_FORMAT_LINEAR_DEPTH,
                       "Failed to create GBuffer Linear Depth target" ) )     return FALSE;

    if( !CreateTarget( m_GBufferTarget[ MRT_GLOW ],          GBUFFER_FORMAT_GLOW,
                       "Failed to create GBuffer Glow target" ) )             return FALSE;

    if( !CreateTarget( m_GBufferDepth,                       GBUFFER_FORMAT_DEPTH,
                       "Failed to create GBuffer Depth-Stencil" ) )           return FALSE;

    m_bGBufferValid = TRUE;
    return TRUE;
}

//==============================================================================

void gbuffer_mgr::DestroyGBuffer( void )
{
    if( m_bGBufferTargetsActive )
    {
        rtarget_SetBackBuffer();
        m_bGBufferTargetsActive = FALSE;
    }

    rtarget_Unregister( m_SceneColorTarget );
    rtarget_Destroy   ( m_SceneColorTarget );

    for( u32 i = 0; i < GBUFFER_MRT_COUNT; ++i )
    {
        rtarget_Unregister( m_GBufferTarget[i] );
        rtarget_Destroy   ( m_GBufferTarget[i] );
    }

    rtarget_Unregister( m_GBufferDepth );
    rtarget_Destroy   ( m_GBufferDepth );

    m_bGBufferValid = FALSE;
    m_GBufferWidth  = 0;
    m_GBufferHeight = 0;
}

//==============================================================================

xbool gbuffer_mgr::ResizeGBuffer( u32 Width, u32 Height )
{
    return InitGBuffer( Width, Height );
}

//==============================================================================

xbool gbuffer_mgr::SetGBufferTargets( void )
{
    if( !m_bGBufferValid )
    {
        x_throw( "SetGBufferTargets called before a valid G-Buffer was created" );
        return FALSE;
    }

    if( !g_pd3dContext )
        return FALSE;

    if( m_bGBufferTargetsActive )
        return TRUE;

    if( !rtarget_GetBackBuffer() )
    {
        x_throw( "Back buffer is not available" );
        return FALSE;
    }

    const rtarget* pSceneColor  = GetActiveSceneColor();
    const rtarget* pDepthTarget = GetActiveDepthTarget();
    if( !pSceneColor || !pDepthTarget )
        return FALSE;

    rtarget BoundTargets[ BIND_SLOT_COUNT ];
    BoundTargets[ BIND_SLOT_FINAL_COLOR  ] = *pSceneColor;
    BoundTargets[ BIND_SLOT_ALBEDO       ] = m_GBufferTarget[ MRT_ALBEDO       ];
    BoundTargets[ BIND_SLOT_NORMAL       ] = m_GBufferTarget[ MRT_NORMAL       ];
    BoundTargets[ BIND_SLOT_LINEAR_DEPTH ] = m_GBufferTarget[ MRT_LINEAR_DEPTH ];
    BoundTargets[ BIND_SLOT_GLOW         ] = m_GBufferTarget[ MRT_GLOW         ];

    if( !rtarget_SetTargets( BoundTargets, BIND_SLOT_COUNT, pDepthTarget ) )
        return FALSE;

    m_bGBufferTargetsActive = TRUE;
    return TRUE;
}

//==============================================================================

void gbuffer_mgr::SetFinalColorTarget( void )
{
    if( !m_bGBufferValid )
        return;

    const rtarget* pSceneColor  = GetActiveSceneColor();
    const rtarget* pDepthTarget = GetActiveDepthTarget();
    if( !pSceneColor || !pDepthTarget )
        return;

    rtarget_SetTargets( pSceneColor, 1, pDepthTarget );
    m_bGBufferTargetsActive = FALSE;
}

//==============================================================================

void gbuffer_mgr::PresentFinalColor( void )
{
    if( !m_bGBufferValid || !m_SceneColorTarget.pShaderResourceView )
        return;

    if( m_pOverrideColor )
        return;

    if( !rtarget_SetBackBuffer() )
        return;

    composite_Blit( m_SceneColorTarget,
                    COMPOSITE_BLEND_COPY,
                    1.0f,
                    NULL,
                    STATE_SAMPLER_POINT_CLAMP );
}

//==============================================================================

void gbuffer_mgr::ClearGBuffer( void )
{
    if( !m_bGBufferValid || !m_bGBufferTargetsActive || !g_pd3dContext )
        return;

    static const f32 Zero    [4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    static const f32 NeutralN[4] = { 0.5f, 0.5f, 1.0f, 0.0f }; // unit +Z encoded as RGBA
    static const f32 FarDepth[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

    rtarget_ClearColor( m_SceneColorTarget,                 Zero     );
    rtarget_ClearColor( m_GBufferTarget[ MRT_ALBEDO ],      Zero     );
    rtarget_ClearColor( m_GBufferTarget[ MRT_NORMAL ],      NeutralN );
    rtarget_ClearColor( m_GBufferTarget[ MRT_LINEAR_DEPTH], FarDepth );
    rtarget_ClearColor( m_GBufferTarget[ MRT_GLOW ],        Zero     );

    rtarget_ClearDepthStencil( m_GBufferDepth,
                               RTARGET_CLEAR_DEPTH | RTARGET_CLEAR_STENCIL,
                               1.0f, 0 );
}

//==============================================================================

const rtarget* gbuffer_mgr::GetGBufferTarget( gbuffer_target Target ) const
{
    if( !m_bGBufferValid )
        return NULL;

    switch( Target )
    {
    case GBUFFER_FINAL_COLOR:  return GetActiveSceneColor();
    case GBUFFER_DEPTH:        return &m_GBufferDepth;
    case GBUFFER_ALBEDO:       return &m_GBufferTarget[ MRT_ALBEDO ];
    case GBUFFER_NORMAL:       return &m_GBufferTarget[ MRT_NORMAL ];
    case GBUFFER_LINEAR_DEPTH: return &m_GBufferTarget[ MRT_LINEAR_DEPTH ];
    case GBUFFER_GLOW:         return &m_GBufferTarget[ MRT_GLOW ];
    default:                   return NULL;
    }
}

//==============================================================================

void gbuffer_mgr::GetGBufferSize( u32& Width, u32& Height ) const
{
    Width  = m_GBufferWidth;
    Height = m_GBufferHeight;
}

//==============================================================================
//  TARGET OVERRIDE
//==============================================================================

void gbuffer_mgr::SetTargetOverride( const rtarget* pColor, const rtarget* pDepth )
{
    m_pOverrideColor = pColor;
    m_pOverrideDepth = pDepth;
}

//==============================================================================

const rtarget* gbuffer_mgr::GetActiveSceneColor( void ) const
{
    return m_pOverrideColor ? m_pOverrideColor : &m_SceneColorTarget;
}

//==============================================================================

const rtarget* gbuffer_mgr::GetActiveDepthTarget( void ) const
{
    return m_pOverrideDepth ? m_pOverrideDepth : &m_GBufferDepth;
}