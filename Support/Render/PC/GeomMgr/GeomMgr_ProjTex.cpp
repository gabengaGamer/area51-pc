//==============================================================================
//
//  GeomMgr_ProjTex.cpp
//
//  Projected texture utilities for the PC geom manager
//
//==============================================================================

//==============================================================================
//  PLATFORM CHECK
//==============================================================================

#include "x_types.hpp"

#if !defined(TARGET_PC)
#error "This is only for the PC target platform. Please check build exclusion rules"
#endif

//==============================================================================
//  INCLUDES
//==============================================================================

#include "GeomMgr.hpp"

//==============================================================================
//  EXTERNAL VARIABLES
//==============================================================================

extern ID3D11DeviceContext* g_pd3dContext;

//==============================================================================
//  FUNCTIONS
//==============================================================================

xbool geom_mgr::InitProjTextures( void )
{
    x_DebugMsg( "GeomMgr: Initializing projection texture resources\n" );

    // Initialize member variables
    m_pProjTextureBuffer    = NULL;
    m_pProjSampler          = NULL;
    m_LastProjLightCount    = 0;
    m_LastProjShadowCount   = 0;
    m_bProjTexturesDirty    = TRUE;
    m_bProjTexturesBound    = FALSE;

    m_pProjTextureBuffer = shader_CreateConstantBuffer( sizeof(cb_proj_textures), CB_TYPE_DYNAMIC );

    if( g_pd3dDevice )
    {
        D3D11_SAMPLER_DESC sd;
        x_memset( &sd, 0, sizeof(sd) );
        sd.Filter   = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.MipLODBias = 0.0f;
        sd.MaxAnisotropy = 1;
        sd.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        sd.MinLOD = 0;
        sd.MaxLOD = D3D11_FLOAT32_MAX;
        g_pd3dDevice->CreateSamplerState( &sd, &m_pProjSampler );
    }

    x_DebugMsg( "GeomMgr: Projection texture resources initialized\n" );
    return TRUE;
}

//==============================================================================

void geom_mgr::KillProjTextures( void )
{
    if( m_pProjTextureBuffer )
    {
        m_pProjTextureBuffer->Release();
        m_pProjTextureBuffer = NULL;
    }

    if( m_pProjSampler )
    {
        m_pProjSampler->Release();
        m_pProjSampler = NULL;
    }

    x_DebugMsg( "GeomMgr: Projection texture resources released\n" );
}

//==============================================================================

void geom_mgr::ResetProjTextures( void )
{
    if( !g_pd3dContext )
        return;

    if( !m_bProjTexturesBound &&
        !m_LastProjLightCount &&
        !m_LastProjShadowCount )
    {
        return;
    }

    if( m_LastProjLightCount )
    {
        ID3D11ShaderResourceView* nullSRV[proj_texture_mgr::MAX_PROJ_LIGHTS] = { NULL };
        ID3D11SamplerState*       nullSamp[proj_texture_mgr::MAX_PROJ_LIGHTS] = { NULL };
        g_pd3dContext->PSSetShaderResources( PC_PROJ_LIGHT_TEX_SLOT, m_LastProjLightCount, nullSRV );
        g_pd3dContext->PSSetSamplers( PC_PROJ_LIGHT_TEX_SLOT, m_LastProjLightCount, nullSamp );
        m_LastProjLightCount = 0;
    }

    if( m_LastProjShadowCount )
    {
        ID3D11ShaderResourceView* nullSRV[proj_texture_mgr::MAX_PROJ_SHADOWS] = { NULL };
        ID3D11SamplerState*       nullSamp[proj_texture_mgr::MAX_PROJ_SHADOWS] = { NULL };
        g_pd3dContext->PSSetShaderResources( PC_PROJ_SHADOW_TEX_SLOT, m_LastProjShadowCount, nullSRV );
        g_pd3dContext->PSSetSamplers( PC_PROJ_SHADOW_TEX_SLOT, m_LastProjShadowCount, nullSamp );
        m_LastProjShadowCount = 0;
    }

    m_bProjTexturesDirty = TRUE;
    m_bProjTexturesBound = FALSE;
}

//==============================================================================

xbool geom_mgr::UpdateProjTextures( u32 Slot )
{
    if( !m_pProjTextureBuffer || !g_pd3dContext )
        return FALSE;

    if( !m_bProjTexturesDirty && m_bProjTexturesBound )
        return TRUE;

    cb_proj_textures cb;
    x_memset( &cb, 0, sizeof(cb) );
    ID3D11ShaderResourceView* lightSRV[proj_texture_mgr::MAX_PROJ_LIGHTS] = { NULL };
    ID3D11ShaderResourceView* shadSRV [proj_texture_mgr::MAX_PROJ_SHADOWS] = { NULL };

    s32 nAppliedLights = 0;
    const s32 nProjLights = MIN( g_ProjTextureMgr.GetProjLightCount(), proj_texture_mgr::MAX_PROJ_LIGHTS );
    for( s32 i = 0; i < nProjLights; i++ )
    {
        matrix4  ProjMtx;
        xbitmap* pBMP = NULL;
        g_ProjTextureMgr.GetProjLight( i, ProjMtx, pBMP );
        if( !pBMP )
            continue;

        ID3D11ShaderResourceView* pSRV = vram_GetSRV( *pBMP );
        if( !pSRV )
            continue;

        cb.ProjLightMatrix[nAppliedLights] = ProjMtx;
        lightSRV[nAppliedLights] = pSRV;
        nAppliedLights++;
    }

    s32 nAppliedShadows = 0;
    const s32 nProjShadows = MIN( g_ProjTextureMgr.GetProjShadowCount(), proj_texture_mgr::MAX_PROJ_SHADOWS );
    for( s32 i = 0; i < nProjShadows; i++ )
    {
        matrix4  ShadMtx;
        xbitmap* pBMP = NULL;
        g_ProjTextureMgr.GetProjShadow( i, ShadMtx, pBMP );
        if( !pBMP )
            continue;

        ID3D11ShaderResourceView* pSRV = vram_GetSRV( *pBMP );
        if( !pSRV )
            continue;

        cb.ProjShadowMatrix[nAppliedShadows] = ShadMtx;
        shadSRV[nAppliedShadows] = pSRV;
        nAppliedShadows++;
    }

    cb.ProjLightCount  = nAppliedLights;
    cb.ProjShadowCount = nAppliedShadows;
    cb.EdgeSize        = 0.05f;

    D3D11_MAPPED_SUBRESOURCE Mapped;
    if( SUCCEEDED( g_pd3dContext->Map( m_pProjTextureBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped ) ) )
    {
        x_memcpy( Mapped.pData, &cb, sizeof(cb_proj_textures) );
        g_pd3dContext->Unmap( m_pProjTextureBuffer, 0 );
        g_pd3dContext->VSSetConstantBuffers( Slot, 1, &m_pProjTextureBuffer );
        g_pd3dContext->PSSetConstantBuffers( Slot, 1, &m_pProjTextureBuffer );
    }

    if( nAppliedLights )
    {
        g_pd3dContext->PSSetShaderResources( PC_PROJ_LIGHT_TEX_SLOT, nAppliedLights, lightSRV );
        if( m_pProjSampler )
        {
            ID3D11SamplerState* samp[proj_texture_mgr::MAX_PROJ_LIGHTS] = { m_pProjSampler };
            g_pd3dContext->PSSetSamplers( PC_PROJ_LIGHT_TEX_SLOT, nAppliedLights, samp );
        }
    }
    else if( m_LastProjLightCount )
    {
        ID3D11ShaderResourceView* nullSRV[proj_texture_mgr::MAX_PROJ_LIGHTS] = { NULL };
        ID3D11SamplerState*       nullSamp[proj_texture_mgr::MAX_PROJ_LIGHTS] = { NULL };
        g_pd3dContext->PSSetShaderResources( PC_PROJ_LIGHT_TEX_SLOT, m_LastProjLightCount, nullSRV );
        g_pd3dContext->PSSetSamplers( PC_PROJ_LIGHT_TEX_SLOT, m_LastProjLightCount, nullSamp );
    }

    if( nAppliedLights &&
        (m_LastProjLightCount > (u32)nAppliedLights) )
    {
        const u32 nUnusedLights = m_LastProjLightCount - nAppliedLights;
        ID3D11ShaderResourceView* nullSRV[proj_texture_mgr::MAX_PROJ_LIGHTS] = { NULL };
        ID3D11SamplerState*       nullSamp[proj_texture_mgr::MAX_PROJ_LIGHTS] = { NULL };
        g_pd3dContext->PSSetShaderResources( PC_PROJ_LIGHT_TEX_SLOT + nAppliedLights, nUnusedLights, nullSRV );
        g_pd3dContext->PSSetSamplers( PC_PROJ_LIGHT_TEX_SLOT + nAppliedLights, nUnusedLights, nullSamp );
    }

    if( nAppliedShadows )
    {
        g_pd3dContext->PSSetShaderResources( PC_PROJ_SHADOW_TEX_SLOT, nAppliedShadows, shadSRV );
        if( m_pProjSampler )
        {
            ID3D11SamplerState* samp[proj_texture_mgr::MAX_PROJ_SHADOWS] = { m_pProjSampler };
            for( s32 i = 0; i < nAppliedShadows; i++ )
                samp[i] = m_pProjSampler;
            g_pd3dContext->PSSetSamplers( PC_PROJ_SHADOW_TEX_SLOT, nAppliedShadows, samp );
        }
    }
    else if( m_LastProjShadowCount )
    {
        ID3D11ShaderResourceView* nullSRV[proj_texture_mgr::MAX_PROJ_SHADOWS] = { NULL };
        ID3D11SamplerState*       nullSamp[proj_texture_mgr::MAX_PROJ_SHADOWS] = { NULL };
        g_pd3dContext->PSSetShaderResources( PC_PROJ_SHADOW_TEX_SLOT, m_LastProjShadowCount, nullSRV );
        g_pd3dContext->PSSetSamplers( PC_PROJ_SHADOW_TEX_SLOT, m_LastProjShadowCount, nullSamp );
    }

    if( nAppliedShadows &&
        (m_LastProjShadowCount > (u32)nAppliedShadows) )
    {
        const u32 nUnusedShadows = m_LastProjShadowCount - nAppliedShadows;
        ID3D11ShaderResourceView* nullSRV[proj_texture_mgr::MAX_PROJ_SHADOWS] = { NULL };
        ID3D11SamplerState*       nullSamp[proj_texture_mgr::MAX_PROJ_SHADOWS] = { NULL };
        g_pd3dContext->PSSetShaderResources( PC_PROJ_SHADOW_TEX_SLOT + nAppliedShadows, nUnusedShadows, nullSRV );
        g_pd3dContext->PSSetSamplers( PC_PROJ_SHADOW_TEX_SLOT + nAppliedShadows, nUnusedShadows, nullSamp );
    }

    m_LastProjLightCount  = nAppliedLights;
    m_LastProjShadowCount = nAppliedShadows;
    m_bProjTexturesDirty  = FALSE;
    m_bProjTexturesBound  = TRUE;
    return TRUE;
}
