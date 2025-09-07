//==============================================================================
//  
//  MaterialMgr.cpp
//  
//  Material Manager for PC platform
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

#include "MaterialMgr.hpp"
#include "VertexMgr.hpp"
#include "SoftVertexMgr.hpp"
#include "../Render.hpp"

static const s32 PROJ_LIGHT_TEX_SLOT  = 3;
static const s32 PROJ_SHADOW_TEX_SLOT = PROJ_LIGHT_TEX_SLOT + proj_texture_mgr::MAX_PROJ_LIGHTS;

//==============================================================================
//  EXTERNAL VARIABLES
//==============================================================================

extern ID3D11Device*           g_pd3dDevice;
extern ID3D11DeviceContext*    g_pd3dContext;

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

material_mgr g_MaterialMgr;

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

void material_mgr::Init( void )
{
    if( m_bInitialized )
        return;

    x_DebugMsg( "MaterialMgr: Initializing material manager\n" );

    // Initialize member variables
    m_pRigidVertexShader    = NULL;
    m_pRigidPixelShader     = NULL;
    m_pRigidInputLayout     = NULL;
    m_pRigidConstantBuffer  = NULL;
    m_pRigidLightBuffer     = NULL;

    m_pProjTextureBuffer    = NULL;
    m_pProjSampler          = NULL;

    m_pSkinVertexShader     = NULL;
    m_pSkinPixelShader      = NULL;
    m_pSkinInputLayout      = NULL;
    m_pSkinVSConstBuffer    = NULL;
    m_pSkinLightBuffer      = NULL;
    m_pSkinBoneBuffer       = NULL;

    m_pCurrentTexture       = NULL;
    m_pCurrentDetailTexture = NULL;
    
    m_bRigidMatricesDirty   = TRUE;
    m_bRigidLightingDirty   = TRUE;
    m_bSkinMatricesDirty    = TRUE;
    m_bSkinLightingDirty    = TRUE;
    
    m_LastProjLightCount    = 0;
    m_LastProjShadowCount   = 0;

    // Initialize shaders and resources
    InitRigidShaders();
    InitSkinShaders();
    
    m_pProjTextureBuffer = shader_CreateConstantBuffer( sizeof(cb_proj_textures) );

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

    m_bInitialized = TRUE;
    x_DebugMsg( "MaterialMgr: Material manager initialized successfully\n" );
}

//==============================================================================

void material_mgr::Kill( void )
{
    if( !m_bInitialized )
        return;

    x_DebugMsg( "MaterialMgr: Shutting down material manager\n" );

    KillShaders();
    InvalidateCache();

    m_bInitialized = FALSE;
    x_DebugMsg( "MaterialMgr: Material manager shutdown complete\n" );
}

//==============================================================================

xbool material_mgr::InitRigidShaders( void )
{
    x_DebugMsg( "MaterialMgr: Initializing rigid shaders\n" );

    // Input layout for rigid vertices
    D3D11_INPUT_ELEMENT_DESC rigidLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_B8G8R8A8_UNORM,  0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    // Load shaders from file
    char shaderPath[256];
    x_sprintf( shaderPath, "%sa51_rigid_simple.hlsl", SHADER_PATH );
    
    m_pRigidVertexShader = shader_CompileVertexFromFileWithLayout( shaderPath,
                                                                  &m_pRigidInputLayout,
                                                                  rigidLayout,
                                                                  ARRAYSIZE(rigidLayout),
                                                                  "VSMain",
                                                                  "vs_5_0" );

    m_pRigidPixelShader = shader_CompilePixelFromFile( shaderPath, "PSMain", "ps_5_0" );
    m_pRigidConstantBuffer = shader_CreateConstantBuffer( sizeof(cb_rigid_matrices) );
    m_pRigidLightBuffer    = shader_CreateConstantBuffer( sizeof(cb_lighting) );

    x_DebugMsg( "MaterialMgr: Rigid shaders initialized successfully\n" );
    return TRUE;
}

//==============================================================================

xbool material_mgr::InitSkinShaders( void )
{
    x_DebugMsg( "MaterialMgr: Initializing skin shaders\n" );

    // Input layout for skinned vertices
    D3D11_INPUT_ELEMENT_DESC skinLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    // Load shaders from file  
    char shaderPath[256];
    x_sprintf( shaderPath, "%sa51_skin_simple.hlsl", SHADER_PATH );

    m_pSkinVertexShader = shader_CompileVertexFromFileWithLayout( shaderPath,
                                                                 &m_pSkinInputLayout,
                                                                 skinLayout,
                                                                 ARRAYSIZE(skinLayout),
                                                                 "VSMain",
                                                                 "vs_5_0" );

    m_pSkinPixelShader = shader_CompilePixelFromFile( shaderPath, "PSMain", "ps_5_0" );
    m_pSkinVSConstBuffer = shader_CreateConstantBuffer( sizeof(cb_skin_matrices) );
    m_pSkinLightBuffer   = shader_CreateConstantBuffer( sizeof(cb_lighting) );
    m_pSkinBoneBuffer    = shader_CreateConstantBuffer( sizeof(cb_skin_bone) * MAX_SKIN_BONES );

    x_DebugMsg( "MaterialMgr: Skin shaders initialized successfully\n" );
    return TRUE;
}

//==============================================================================

void material_mgr::KillShaders( void )
{
    // Release rigid resources
    if( m_pRigidVertexShader )
    {
        m_pRigidVertexShader->Release();
        m_pRigidVertexShader = NULL;
    }

    if( m_pRigidPixelShader )
    {
        m_pRigidPixelShader->Release();
        m_pRigidPixelShader = NULL;
    }

    if( m_pRigidInputLayout )
    {
        m_pRigidInputLayout->Release();
        m_pRigidInputLayout = NULL;
    }

    if( m_pRigidConstantBuffer )
    {
        m_pRigidConstantBuffer->Release();
        m_pRigidConstantBuffer = NULL;
    }

    if( m_pRigidLightBuffer )
    {
        m_pRigidLightBuffer->Release();
        m_pRigidLightBuffer = NULL;
    }

    // Release skin resources
    if( m_pSkinVertexShader )
    {
        m_pSkinVertexShader->Release();
        m_pSkinVertexShader = NULL;
    }

    if( m_pSkinPixelShader )
    {
        m_pSkinPixelShader->Release();
        m_pSkinPixelShader = NULL;
    }

    if( m_pSkinInputLayout )
    {
        m_pSkinInputLayout->Release();
        m_pSkinInputLayout = NULL;
    }

    if( m_pSkinVSConstBuffer )
    {
        m_pSkinVSConstBuffer->Release();
        m_pSkinVSConstBuffer = NULL;
    }

    if( m_pSkinLightBuffer )
    {
        m_pSkinLightBuffer->Release();
        m_pSkinLightBuffer = NULL;
    }

    if( m_pSkinBoneBuffer )
    {
        m_pSkinBoneBuffer->Release();
        m_pSkinBoneBuffer = NULL;
    }

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

    x_DebugMsg( "MaterialMgr: All shaders released\n" );
}

//==============================================================================

void material_mgr::SetRigidMaterial( const matrix4*      pL2W,
                                     const bbox*         pBBox,
                                     const d3d_lighting* pLighting,
                                     const material*     pMaterial,
                                     u32                 RenderFlags )
{
    if( !g_pd3dDevice || !g_pd3dContext )
        return;

    // Set rigid shader pipeline
    g_pd3dContext->IASetInputLayout( m_pRigidInputLayout );
    g_pd3dContext->VSSetShader( m_pRigidVertexShader, NULL, 0 );
    g_pd3dContext->PSSetShader( m_pRigidPixelShader, NULL, 0 );

    // Set default render states
    state_SetState( STATE_TYPE_BLEND, STATE_BLEND_NONE );
    state_SetState( STATE_TYPE_DEPTH, STATE_DEPTH_NORMAL );
    state_SetState( STATE_TYPE_RASTERIZER, STATE_RASTER_SOLID );
    state_SetState( STATE_TYPE_SAMPLER, STATE_SAMPLER_LINEAR_WRAP );
    g_pd3dContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
    
    if( !UpdateRigidConstants( pL2W,  pMaterial, RenderFlags, pLighting ) )
    {
        x_DebugMsg( "MaterialMgr: Failed to update rigid constants\n" );
        return;
    }

    if( pL2W && pBBox )
        UpdateProjTextures( *pL2W, *pBBox, 1, RenderFlags );
}

//==============================================================================

void material_mgr::SetSkinMaterial( const matrix4*      pL2W,
                                    const bbox*         pBBox,
                                    const d3d_lighting* pLighting,
                                    const material*     pMaterial,
                                    u32                 RenderFlags )
{
    if( !g_pd3dDevice || !g_pd3dContext )
        return;

    // Set skin shader pipeline  
    g_pd3dContext->IASetInputLayout( m_pSkinInputLayout );
    g_pd3dContext->VSSetShader( m_pSkinVertexShader, NULL, 0 );
    g_pd3dContext->PSSetShader( m_pSkinPixelShader, NULL, 0 );

    // Set default render states
    state_SetState( STATE_TYPE_BLEND, STATE_BLEND_NONE );
    state_SetState( STATE_TYPE_DEPTH, STATE_DEPTH_NORMAL );
    state_SetState( STATE_TYPE_RASTERIZER, STATE_RASTER_SOLID );
    state_SetState( STATE_TYPE_SAMPLER, STATE_SAMPLER_LINEAR_WRAP );
    g_pd3dContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
    
    if( !UpdateSkinConstants( pLighting, pMaterial, RenderFlags ) )
    {
        x_DebugMsg( "MaterialMgr: Failed to update skin constants\n" );
        return;
    }

    if( pL2W && pBBox )
        UpdateProjTextures( *pL2W, *pBBox, 1, RenderFlags );
}

//==============================================================================

void material_mgr::ResetProjTextures( void )
{
    if( !g_pd3dContext )
        return;

    if( m_LastProjLightCount )
    {
        ID3D11ShaderResourceView* nullSRV[proj_texture_mgr::MAX_PROJ_LIGHTS] = { NULL };
        ID3D11SamplerState*      nullSamp[proj_texture_mgr::MAX_PROJ_LIGHTS] = { NULL };
        g_pd3dContext->PSSetShaderResources( PROJ_LIGHT_TEX_SLOT, m_LastProjLightCount, nullSRV );
        g_pd3dContext->PSSetSamplers( PROJ_LIGHT_TEX_SLOT, m_LastProjLightCount, nullSamp );
        m_LastProjLightCount = 0;
    }

    if( m_LastProjShadowCount )
    {
        ID3D11ShaderResourceView* nullSRV[proj_texture_mgr::MAX_PROJ_SHADOWS] = { NULL };
        ID3D11SamplerState*      nullSamp[proj_texture_mgr::MAX_PROJ_SHADOWS] = { NULL };
        g_pd3dContext->PSSetShaderResources( PROJ_SHADOW_TEX_SLOT, m_LastProjShadowCount, nullSRV );
        g_pd3dContext->PSSetSamplers( PROJ_SHADOW_TEX_SLOT, m_LastProjShadowCount, nullSamp );
        m_LastProjShadowCount = 0;
    }
}

//==============================================================================

xbool material_mgr::UpdateProjTextures( const matrix4& L2W,
                                        const bbox&    B,
                                        u32            Slot,
                                        u32            RenderFlags )
{
    if( !m_pProjTextureBuffer || !g_pd3dContext )
        return FALSE;

    cb_proj_textures cb;
    x_memset( &cb, 0, sizeof(cb) );
    ID3D11ShaderResourceView* lightSRV[proj_texture_mgr::MAX_PROJ_LIGHTS] = { NULL };
    ID3D11ShaderResourceView* shadSRV [proj_texture_mgr::MAX_PROJ_SHADOWS] = { NULL };

    s32 nProjLights = 0;
    if( !(RenderFlags & render::DISABLE_SPOTLIGHT) )
        nProjLights = g_ProjTextureMgr.CollectLights( L2W, B );
    
    s32 nAppliedLights = 0;
    for( s32 i = 0; i < nProjLights; i++ )
    {
        matrix4  ProjMtx;
        xbitmap* pBMP = NULL;
        g_ProjTextureMgr.GetCollectedLight( ProjMtx, pBMP );
        if( pBMP )
        {
            ID3D11ShaderResourceView* pSRV = vram_GetSRV( *pBMP );
            if( pSRV )
            {
                cb.ProjLightMatrix[nAppliedLights] = ProjMtx;
                lightSRV[nAppliedLights] = pSRV;
                nAppliedLights++;
            }
        }
    }

    s32 nProjShadows = 0;
    if( !(RenderFlags & render::DISABLE_PROJ_SHADOWS) )
        nProjShadows = g_ProjTextureMgr.CollectShadows( L2W, B, 2 );
    
    s32 nAppliedShadows = 0;
    for( s32 i = 0; i < nProjShadows; i++ )
    {
        matrix4  ShadMtx;
        xbitmap* pBMP = NULL;
        g_ProjTextureMgr.GetCollectedShadow( ShadMtx, pBMP );
        if( pBMP )
        {
            ID3D11ShaderResourceView* pSRV = vram_GetSRV( *pBMP );
            if( pSRV )
            {
                cb.ProjShadowMatrix[nAppliedShadows] = ShadMtx;
                shadSRV[nAppliedShadows] = pSRV;
                nAppliedShadows++;
            }
        }
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
        g_pd3dContext->PSSetShaderResources( PROJ_LIGHT_TEX_SLOT, nAppliedLights, lightSRV );
        if( m_pProjSampler )
        {
            ID3D11SamplerState* samp[proj_texture_mgr::MAX_PROJ_LIGHTS];
            for( s32 i = 0; i < nAppliedLights; i++ )
                samp[i] = m_pProjSampler;
            g_pd3dContext->PSSetSamplers( PROJ_LIGHT_TEX_SLOT, nAppliedLights, samp );
        }
    }
    if( nAppliedShadows )
    {
        g_pd3dContext->PSSetShaderResources( PROJ_SHADOW_TEX_SLOT, nAppliedShadows, shadSRV );
        if( m_pProjSampler )
        {
            ID3D11SamplerState* samp[proj_texture_mgr::MAX_PROJ_SHADOWS];
            for( s32 i = 0; i < nAppliedShadows; i++ )
                samp[i] = m_pProjSampler;
            g_pd3dContext->PSSetSamplers( PROJ_SHADOW_TEX_SLOT, nAppliedShadows, samp );
        }
    }

    m_LastProjLightCount  = nAppliedLights;
    m_LastProjShadowCount = nAppliedShadows;
    return TRUE;
}

//==============================================================================

xbool material_mgr::UpdateRigidConstants( const matrix4*      pL2W,
                                          const material*     pMaterial,
                                          u32                 RenderFlags,
                                          const d3d_lighting* pLighting )
{
    if( !m_pRigidConstantBuffer || !pLighting || !g_pd3dDevice || !g_pd3dContext )
        return FALSE;

    const view* pView = eng_GetView();
    if( !pView )
        return FALSE;

    cb_rigid_matrices rigidMatrices;

    if( pL2W )
        rigidMatrices.World = *pL2W;
    else
        rigidMatrices.World.Identity();
    
    rigidMatrices.View           = pView->GetW2V();
    rigidMatrices.Projection     = pView->GetV2C();
    rigidMatrices.CameraPosition = pView->GetPosition();
    
    // Analyze material and set flags
    rigidMatrices.MaterialFlags = 0;
    rigidMatrices.AlphaRef      = 0.0f;
    rigidMatrices.Padding[0]    = 0.0f;
    rigidMatrices.Padding[1]    = 0.0f;

    if( pMaterial )
    {
        switch( pMaterial->m_Type )
        {
            case Material_Diff:
                break;
            case Material_Alpha:
                rigidMatrices.MaterialFlags |= MATERIAL_FLAG_ALPHA_TEST;
                break;
            case Material_Diff_PerPixelIllum:
                rigidMatrices.MaterialFlags |= MATERIAL_FLAG_PERPIXEL_ILLUM;
                break;
            case Material_Alpha_PerPixelIllum:
                rigidMatrices.MaterialFlags |= MATERIAL_FLAG_ALPHA_TEST | MATERIAL_FLAG_PERPIXEL_ILLUM;
                break;
            case Material_Alpha_PerPolyIllum:
                rigidMatrices.MaterialFlags |= MATERIAL_FLAG_ALPHA_TEST | MATERIAL_FLAG_PERPOLY_ILLUM;
                break;
            case Material_Diff_PerPixelEnv:
                rigidMatrices.MaterialFlags |= MATERIAL_FLAG_PERPIXEL_ENV;
                break;               
            case Material_Alpha_PerPolyEnv:
                rigidMatrices.MaterialFlags |= MATERIAL_FLAG_ALPHA_TEST | MATERIAL_FLAG_PERPOLY_ENV;
                break;				
            case Material_Distortion:
                rigidMatrices.MaterialFlags |= MATERIAL_FLAG_DISTORTION;
                break;
            case Material_Distortion_PerPolyEnv:
                rigidMatrices.MaterialFlags |= MATERIAL_FLAG_DISTORTION | MATERIAL_FLAG_PERPOLY_ENV;
                break;
        }                      
        
        // Check for detail map
        if( pMaterial->m_DetailMap.GetPointer() )
        {
            rigidMatrices.MaterialFlags |= MATERIAL_FLAG_HAS_DETAIL;
        }
        
        // Check for ENV map
        if( pMaterial->m_EnvironmentMap.GetPointer() )
        {
            rigidMatrices.MaterialFlags |= MATERIAL_FLAG_HAS_ENVIRONMENT;
        }
        
        // Check blend flags
        if( pMaterial->m_Flags & geom::material::FLAG_IS_ADDITIVE )
            rigidMatrices.MaterialFlags |= MATERIAL_FLAG_ADDITIVE;
        else if( pMaterial->m_Flags & geom::material::FLAG_IS_SUBTRACTIVE )
            rigidMatrices.MaterialFlags |= MATERIAL_FLAG_SUBTRACTIVE;
        
        // Always use vertex color for now
        rigidMatrices.MaterialFlags |= MATERIAL_FLAG_VERTEX_COLOR;
        
        // Check proj flags
        if( !(RenderFlags & render::DISABLE_SPOTLIGHT) )
            rigidMatrices.MaterialFlags |= MATERIAL_FLAG_PROJ_LIGHT;
        if( !(RenderFlags & render::DISABLE_PROJ_SHADOWS) )
            rigidMatrices.MaterialFlags |= MATERIAL_FLAG_PROJ_SHADOW;
    }

    // Only update if data changed
    if( m_bRigidMatricesDirty || x_memcmp( &m_CachedRigidMatrices, &rigidMatrices, sizeof(cb_rigid_matrices) ) != 0 )
    {
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        HRESULT hr = g_pd3dContext->Map( m_pRigidConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource );
        if( FAILED(hr) )
        {
            x_DebugMsg( "MaterialMgr: Failed to map rigid constant buffer, HRESULT 0x%08X\n", hr );
            return FALSE;
        }
        
        x_memcpy( mappedResource.pData, &rigidMatrices, sizeof(cb_rigid_matrices) );
        g_pd3dContext->Unmap( m_pRigidConstantBuffer, 0 );
        
        // Cache the data
        m_CachedRigidMatrices = rigidMatrices;
        m_bRigidMatricesDirty = FALSE;
        
        g_pd3dContext->VSSetConstantBuffers( 0, 1, &m_pRigidConstantBuffer );
        g_pd3dContext->PSSetConstantBuffers( 0, 1, &m_pRigidConstantBuffer );
    }
    
    if( m_pRigidLightBuffer )
    {
        cb_lighting lightMatrices;
        if( pLighting )
        {
            for( s32 i = 0; i < MAX_GEOM_LIGHTS; i++ )
            {
                if( i < pLighting->LightCount )
                {
                    lightMatrices.LightVec[i] = pLighting->LightVec[i];
                    lightMatrices.LightCol[i] = pLighting->LightCol[i];
                }
                else
                {
                    lightMatrices.LightVec[i].Set( 0.0f, 0.0f, 0.0f, 0.0f );
                    lightMatrices.LightCol[i].Set( 0.0f, 0.0f, 0.0f, 0.0f );
                }
            }
            lightMatrices.LightAmbCol = pLighting->AmbCol;
            lightMatrices.LightCount  = pLighting->LightCount;
        }
        else
        {
            lightMatrices.LightAmbCol.Set( 0.0f, 0.0f, 0.0f, 1.0f );
            lightMatrices.LightCount = 0;
            for( s32 i = 0; i < MAX_GEOM_LIGHTS; i++ )
            {
                lightMatrices.LightVec[i].Set( 0.0f, 0.0f, 0.0f, 0.0f );
                lightMatrices.LightCol[i].Set( 0.0f, 0.0f, 0.0f, 0.0f );
            }
        }
        lightMatrices.Padding[0] = lightMatrices.Padding[1] = lightMatrices.Padding[2] = 0.0f;

        if( m_bRigidLightingDirty || x_memcmp( &m_CachedRigidLighting, &lightMatrices, sizeof(cb_lighting) ) != 0 )
        {
            D3D11_MAPPED_SUBRESOURCE mappedResource;
            HRESULT hr = g_pd3dContext->Map( m_pRigidLightBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource );
            if( FAILED(hr) )
            {
                x_DebugMsg( "MaterialMgr: Failed to map rigid light buffer, HRESULT 0x%08X\n", hr );
                return FALSE;
            }

            x_memcpy( mappedResource.pData, &lightMatrices, sizeof(cb_lighting) );
            g_pd3dContext->Unmap( m_pRigidLightBuffer, 0 );

            m_CachedRigidLighting = lightMatrices;
            m_bRigidLightingDirty = FALSE;

            g_pd3dContext->PSSetConstantBuffers( 2, 1, &m_pRigidLightBuffer );
        }
    }

    return TRUE;
}

//==============================================================================

xbool material_mgr::UpdateSkinConstants( const d3d_lighting* pLighting,
                                         const material*     pMaterial,
                                         u32                 RenderFlags )
{
    if( !m_pSkinVSConstBuffer || !m_pSkinLightBuffer || !pLighting || !g_pd3dDevice || !g_pd3dContext )
        return FALSE;

    const view* pView = eng_GetView();
    if( !pView )
        return FALSE;

    cb_skin_matrices skinMatrices;
    skinMatrices.View       = pView->GetW2V();
    skinMatrices.Projection = pView->GetV2C();
    
    // Analyze material and set flags
    skinMatrices.MaterialFlags = 0;
    skinMatrices.AlphaRef      = 0.0f;    
    skinMatrices.Padding[0]    = 0.0f;
    skinMatrices.Padding[1]    = 0.0f;
    
    if( pMaterial )
    {
        switch( pMaterial->m_Type )
        {
            case Material_Diff:
                break;
            case Material_Alpha:
                skinMatrices.MaterialFlags |= MATERIAL_FLAG_ALPHA_TEST;
                break;
            case Material_Diff_PerPixelIllum:
                skinMatrices.MaterialFlags |= MATERIAL_FLAG_PERPIXEL_ILLUM;
                break;
            case Material_Alpha_PerPixelIllum:
                skinMatrices.MaterialFlags |= MATERIAL_FLAG_ALPHA_TEST | MATERIAL_FLAG_PERPIXEL_ILLUM;
                break;
            case Material_Alpha_PerPolyIllum:
                skinMatrices.MaterialFlags |= MATERIAL_FLAG_ALPHA_TEST | MATERIAL_FLAG_PERPOLY_ILLUM;
                break;
            case Material_Diff_PerPixelEnv:
                skinMatrices.MaterialFlags |= MATERIAL_FLAG_PERPIXEL_ENV;
                break; 	
            case Material_Alpha_PerPolyEnv:
                skinMatrices.MaterialFlags |= MATERIAL_FLAG_ALPHA_TEST | MATERIAL_FLAG_PERPOLY_ENV;
                break;				
            case Material_Distortion:
                skinMatrices.MaterialFlags |= MATERIAL_FLAG_DISTORTION;
                break;
            case Material_Distortion_PerPolyEnv:
                skinMatrices.MaterialFlags |= MATERIAL_FLAG_DISTORTION | MATERIAL_FLAG_PERPOLY_ENV;
                break;
        }                      
        
        // Check for ENV map
        if( pMaterial->m_EnvironmentMap.GetPointer() )
        {
            skinMatrices.MaterialFlags |= MATERIAL_FLAG_HAS_ENVIRONMENT;
        }
        
        // Check blend flags
        if( pMaterial->m_Flags & geom::material::FLAG_IS_ADDITIVE )
            skinMatrices.MaterialFlags |= MATERIAL_FLAG_ADDITIVE;
        else if( pMaterial->m_Flags & geom::material::FLAG_IS_SUBTRACTIVE )
            skinMatrices.MaterialFlags |= MATERIAL_FLAG_SUBTRACTIVE;
        
        // Check proj flags
        if( !(RenderFlags & render::DISABLE_SPOTLIGHT) )
            skinMatrices.MaterialFlags |= MATERIAL_FLAG_PROJ_LIGHT;
        if( !(RenderFlags & render::DISABLE_PROJ_SHADOWS) )
            skinMatrices.MaterialFlags |= MATERIAL_FLAG_PROJ_SHADOW;
    }    
    
    cb_lighting lightMatrices;
    
    for( s32 i = 0; i < MAX_GEOM_LIGHTS; i++ )
    {
        if( i < pLighting->LightCount )
        {
            lightMatrices.LightVec[i] = pLighting->LightVec[i];
            lightMatrices.LightCol[i] = pLighting->LightCol[i];
        }
        else
        {
            lightMatrices.LightVec[i].Set( 0.0f, 0.0f, 0.0f, 0.0f );
            lightMatrices.LightCol[i].Set( 0.0f, 0.0f, 0.0f, 0.0f );
        }
    }

    vector4 BaseBrightness( 0.05f, 0.05f, 0.05f, 0.0f ); // Prevent fully black surfaces, render of this game sucks.
    lightMatrices.LightAmbCol = pLighting->AmbCol + BaseBrightness;
    lightMatrices.LightCount  = pLighting->LightCount;
    lightMatrices.Padding[0]  = 0.0f;
    lightMatrices.Padding[1]  = 0.0f;
    lightMatrices.Padding[2]  = 0.0f;

    // Only update if data changed
    if( m_bSkinMatricesDirty || x_memcmp( &m_CachedSkinMatrices, &skinMatrices, sizeof(cb_skin_matrices) ) != 0 )
    {
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        HRESULT hr = g_pd3dContext->Map( m_pSkinVSConstBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource );
        if( FAILED(hr) )
        {
            x_DebugMsg( "MaterialMgr: Failed to map skin VS constant buffer, HRESULT 0x%08X\n", hr );
            return FALSE;
        }

        x_memcpy( mappedResource.pData, &skinMatrices, sizeof(cb_skin_matrices) );
        g_pd3dContext->Unmap( m_pSkinVSConstBuffer, 0 );
        
        // Cache the data
        m_CachedSkinMatrices = skinMatrices;
        m_bSkinMatricesDirty = FALSE;

        g_pd3dContext->VSSetConstantBuffers( 0, 1, &m_pSkinVSConstBuffer );
        g_pd3dContext->VSSetConstantBuffers( 2, 1, &m_pSkinBoneBuffer );
        g_pd3dContext->PSSetConstantBuffers( 0, 1, &m_pSkinVSConstBuffer );
    }
    
    if( m_pSkinLightBuffer )
    {
        if( m_bSkinLightingDirty || x_memcmp( &m_CachedSkinLighting, &lightMatrices, sizeof(cb_lighting) ) != 0 )
        {
            D3D11_MAPPED_SUBRESOURCE mappedResource;
            HRESULT hr = g_pd3dContext->Map( m_pSkinLightBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource );
            if( FAILED(hr) )
            {
                x_DebugMsg( "MaterialMgr: Failed to map skin light buffer, HRESULT 0x%08X\n", hr );
                return FALSE;
            }

            x_memcpy( mappedResource.pData, &lightMatrices, sizeof(cb_lighting) );
            g_pd3dContext->Unmap( m_pSkinLightBuffer, 0 );

            m_CachedSkinLighting = lightMatrices;
            m_bSkinLightingDirty = FALSE;

            g_pd3dContext->PSSetConstantBuffers( 2, 1, &m_pSkinLightBuffer );
        }
    }    
    
    return TRUE;
}

//==============================================================================

void material_mgr::SetBitmap( const xbitmap* pBitmap, texture_slot slot )
{
    if( !g_pd3dContext )
        return;

    // Cache check to avoid redundant texture changes
    if( slot == TEXTURE_SLOT_DIFFUSE && pBitmap == m_pCurrentTexture )
        return;
    if( slot == TEXTURE_SLOT_DETAIL && pBitmap == m_pCurrentDetailTexture )
        return;

    if( pBitmap )
    {
        ID3D11ShaderResourceView* pSRV = vram_GetSRV( *pBitmap );
        
        if( pSRV )
        {
            g_pd3dContext->PSSetShaderResources( slot, 1, &pSRV );
            
            // Update cache
            if( slot == TEXTURE_SLOT_DIFFUSE )
                m_pCurrentTexture = pBitmap;
            else if( slot == TEXTURE_SLOT_DETAIL )
                m_pCurrentDetailTexture = pBitmap;
        }
        else
        {
            x_DebugMsg("MaterialMgr: ERROR - vram_GetSRV returned NULL for slot %d\n", slot);
        }
    }
    else
    {
        // Clear texture slot
        ID3D11ShaderResourceView* pNullSRV = NULL;
        g_pd3dContext->PSSetShaderResources( slot, 1, &pNullSRV );
        
        // Update cache
        if( slot == TEXTURE_SLOT_DIFFUSE )
            m_pCurrentTexture = NULL;
        else if( slot == TEXTURE_SLOT_DETAIL )
            m_pCurrentDetailTexture = NULL;
    }
}

//==============================================================================

void material_mgr::SetBlendMode( s32 BlendMode )
{
    switch( BlendMode )
    {
        case render::BLEND_MODE_NORMAL:
            state_SetState( STATE_TYPE_BLEND, STATE_BLEND_NONE );
            break;
        case render::BLEND_MODE_ADDITIVE:
            state_SetState( STATE_TYPE_BLEND, STATE_BLEND_ADD );
            break;
        case render::BLEND_MODE_SUBTRACTIVE:
            state_SetState( STATE_TYPE_BLEND, STATE_BLEND_SUB );
            break;
        case render::BLEND_MODE_INTENSITY:
            state_SetState( STATE_TYPE_BLEND, STATE_BLEND_INTENSITY );
            break;
        default:
            ASSERT(FALSE);
            break;
    }
}

//==============================================================================

void material_mgr::SetDepthTestEnabled( xbool ZTestEnabled )
{       
    if( !ZTestEnabled )
    {
        state_SetState( STATE_TYPE_DEPTH, STATE_DEPTH_DISABLED );
    }
    else
    {
        state_SetState( STATE_TYPE_DEPTH, STATE_DEPTH_NO_WRITE );
    }
}

//==============================================================================

void material_mgr::InvalidateCache( void )
{
    m_pCurrentTexture = NULL;
    m_pCurrentDetailTexture = NULL;
    m_bRigidMatricesDirty = TRUE;
    m_bRigidLightingDirty = TRUE;
    m_bSkinMatricesDirty = TRUE;
    m_bSkinLightingDirty = TRUE;
}

//==============================================================================

// Skin bone buffer access for SoftVertexMgr
ID3D11Buffer* material_mgr::GetSkinBoneBuffer( void )
{
    return m_pSkinBoneBuffer;
}