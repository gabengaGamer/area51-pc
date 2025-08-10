//==============================================================================
//  
//  GBufferMgr.cpp
//  
//  Mini G-Buffer Manager for Forward+ Renderer
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

//==============================================================================
//  EXTERNAL VARIABLES
//==============================================================================

extern ID3D11Device*           g_pd3dDevice;
extern ID3D11DeviceContext*    g_pd3dContext;

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

gbuffer_mgr g_GBufferMgr;

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

void gbuffer_mgr::Init( void )
{
    if( m_bInitialized )
        return;

    m_bGBufferValid = FALSE;
    m_GBufferWidth = 0;
    m_GBufferHeight = 0;
    m_bGBufferTargetsActive = FALSE;
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

xbool gbuffer_mgr::InitGBuffer( u32 Width, u32 Height )
{
    if( !g_pd3dDevice )
        return FALSE;
    
    if( m_bGBufferValid && m_GBufferWidth == Width && m_GBufferHeight == Height )
        return TRUE;
        
    DestroyGBuffer();
    
    m_GBufferWidth = Width;
    m_GBufferHeight = Height;
    
    rtarget_desc desc;
    desc.Width = Width;
    desc.Height = Height;
    desc.SampleCount = 1;
    desc.SampleQuality = 0;
    desc.bBindAsTexture = TRUE;
    
    // Create G-Buffer targets
    desc.Format = GBUFFER_FORMAT_ALBEDO;
    if( !rtarget_Create( m_GBufferTargets[0], desc ) )
        return FALSE;
    
    desc.Format = GBUFFER_FORMAT_NORMAL;
    if( !rtarget_Create( m_GBufferTargets[1], desc ) )
    {
        rtarget_Destroy( m_GBufferTargets[0] );
        return FALSE;
    }
    
    desc.Format = GBUFFER_FORMAT_DEPTH_INFO;
    if( !rtarget_Create( m_GBufferTargets[2], desc ) )
    {
        rtarget_Destroy( m_GBufferTargets[0] );
        rtarget_Destroy( m_GBufferTargets[1] );
        return FALSE;
    }
    
    desc.Format = RTARGET_FORMAT_DEPTH24_STENCIL8;
    if( !rtarget_Create( m_GBufferDepth, desc ) )
    {
        DestroyGBuffer();
        return FALSE;
    }
    
    m_bGBufferValid = TRUE;
    return TRUE;
}

//==============================================================================

void gbuffer_mgr::DestroyGBuffer( void )
{
    for( u32 i = 0; i < (GBUFFER_TARGET_COUNT - 1); i++ )
        rtarget_Destroy( m_GBufferTargets[i] );
    
    rtarget_Destroy( m_GBufferDepth );
    
    m_bGBufferValid = FALSE;
    m_GBufferWidth = 0;
    m_GBufferHeight = 0;
    m_bGBufferTargetsActive = FALSE;
}

//==============================================================================

xbool gbuffer_mgr::ResizeGBuffer( u32 Width, u32 Height )
{
    if( !m_bGBufferValid || m_GBufferWidth != Width || m_GBufferHeight != Height )
        return InitGBuffer( Width, Height );

    return TRUE;
}

//==============================================================================

xbool gbuffer_mgr::SetGBufferTargets( void )
{
    if( !m_bGBufferValid || !g_pd3dContext || m_bGBufferTargetsActive )
        return m_bGBufferTargetsActive;
    
    const rtarget* pBackBuffer = rtarget_GetBackBuffer();
    if( !pBackBuffer )
        return FALSE;
    
    rtarget targets[GBUFFER_TARGET_COUNT];
    targets[GBUFFER_FINAL_COLOR] = *pBackBuffer;
    targets[GBUFFER_ALBEDO]      = m_GBufferTargets[0];
    targets[GBUFFER_NORMAL]      = m_GBufferTargets[1];
    targets[GBUFFER_DEPTH_INFO]  = m_GBufferTargets[2];
    
    if( rtarget_SetTargets( targets, GBUFFER_TARGET_COUNT, &m_GBufferDepth ) )
    {
        m_bGBufferTargetsActive = TRUE;
        return TRUE;
    }
    
    return FALSE;
}

//==============================================================================

void gbuffer_mgr::SetBackBufferTarget( void )
{
    if( m_bGBufferTargetsActive )
    {
        rtarget_SetBackBuffer();
        m_bGBufferTargetsActive = FALSE;
    }
}

//==============================================================================

void gbuffer_mgr::ClearGBuffer( void )
{
    if( !m_bGBufferValid || !m_bGBufferTargetsActive )
        return;

    // Albedo
    static const f32 clearAlbedo[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    g_pd3dContext->ClearRenderTargetView(m_GBufferTargets[0].pRenderTargetView, clearAlbedo);

    // Normals
    static const f32 clearNormal[4] = { 0.5f, 0.5f, 1.0f, 0.5f };
    g_pd3dContext->ClearRenderTargetView(m_GBufferTargets[1].pRenderTargetView, clearNormal);

    // Depth info
    static const f32 clearDepthInfo[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
    g_pd3dContext->ClearRenderTargetView(m_GBufferTargets[2].pRenderTargetView, clearDepthInfo);

    // Depth-stencil
    g_pd3dContext->ClearDepthStencilView(m_GBufferDepth.pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}

//==============================================================================

const rtarget* gbuffer_mgr::GetGBufferTarget( gbuffer_target Target ) const
{
    if( !m_bGBufferValid )
        return NULL;
        
    switch( Target )
    {
        case GBUFFER_FINAL_COLOR:
            return rtarget_GetBackBuffer();
            
        case GBUFFER_DEPTH:
            return &m_GBufferDepth;
            
        case GBUFFER_ALBEDO:
        case GBUFFER_NORMAL:
        case GBUFFER_DEPTH_INFO:
            return &m_GBufferTargets[Target - 1];
            
        default:
            return NULL;
    }
}

//==============================================================================

void gbuffer_mgr::GetGBufferSize( u32& Width, u32& Height ) const
{
    Width = m_GBufferWidth;
    Height = m_GBufferHeight;
}