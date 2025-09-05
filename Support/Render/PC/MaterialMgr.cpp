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

    m_pProjTextureBuffer    = NULL;

    m_pSkinVertexShader     = NULL;
    m_pSkinPixelShader      = NULL;
    m_pSkinInputLayout      = NULL;
    m_pSkinVSConstBuffer    = NULL;
    m_pSkinBoneBuffer       = NULL;

    m_pCurrentTexture       = NULL;
    m_pCurrentDetailTexture = NULL;
    
    m_bRigidMatricesDirty   = TRUE;
    m_bSkinConstsDirty      = TRUE;
	
	m_LastProjLightCount    = 0;
    m_LastProjShadowCount   = 0;

    // Initialize shaders and resources
    InitRigidShaders();
    InitSkinShaders();
	
	m_pProjTextureBuffer = shader_CreateConstantBuffer( sizeof(cb_proj_textures) );

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
    if( !g_pd3dDevice )
    {
        x_DebugMsg( "MaterialMgr: No D3D device available for rigid shader creation\n" );
        return FALSE;
    }

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
    m_pRigidConstantBuffer = shader_CreateConstantBuffer( sizeof(rigid_vert_matrices) );

    x_DebugMsg( "MaterialMgr: Rigid shaders initialized successfully\n" );
    return TRUE;
}

//==============================================================================

xbool material_mgr::InitSkinShaders( void )
{
    if( !g_pd3dDevice )
    {
        x_DebugMsg( "MaterialMgr: No D3D device available for skin shader creation\n" );
        return FALSE;
    }

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
    m_pSkinVSConstBuffer = shader_CreateConstantBuffer( sizeof(cb_skin_vs_consts) );
    m_pSkinBoneBuffer = shader_CreateConstantBuffer( sizeof(cb_skin_bone) * MAX_SKIN_BONES );

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

    x_DebugMsg( "MaterialMgr: All shaders released\n" );
}

//==============================================================================

void material_mgr::SetRigidMaterial( const matrix4* pL2W, const bbox* pBBox, const d3d_rigid_lighting* pLighting, const material* pMaterial )
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
    
    if( !UpdateRigidConstants( pL2W, pMaterial ) )
    {
        x_DebugMsg( "MaterialMgr: Failed to update rigid constants\n" );
        return;
    }

    if( pL2W && pBBox )
        UpdateProjTextures( *pL2W, *pBBox, 1 );
}

//==============================================================================

void material_mgr::SetSkinMaterial( const matrix4* pL2W, const bbox* pBBox, const d3d_skin_lighting* pLighting )
{
    if( !g_pd3dDevice || !g_pd3dContext || !pLighting )
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
    
    if( !UpdateSkinConstants( pLighting ) )
    {
        x_DebugMsg( "MaterialMgr: Failed to update skin constants\n" );
        return;
    }

    if( pL2W && pBBox )
        UpdateProjTextures( *pL2W, *pBBox, 2 );
}

//==============================================================================

void material_mgr::ResetProjTextures( void )
{
    if( !g_pd3dContext )
        return;

    if( m_LastProjLightCount )
    {
        ID3D11ShaderResourceView* nullSRV[proj_texture_mgr::MAX_PROJ_LIGHTS] = { NULL };
        g_pd3dContext->PSSetShaderResources( PROJ_LIGHT_TEX_SLOT, m_LastProjLightCount, nullSRV );
        m_LastProjLightCount = 0;
    }

    if( m_LastProjShadowCount )
    {
        ID3D11ShaderResourceView* nullSRV[proj_texture_mgr::MAX_PROJ_SHADOWS] = { NULL };
        g_pd3dContext->PSSetShaderResources( PROJ_SHADOW_TEX_SLOT, m_LastProjShadowCount, nullSRV );
        m_LastProjShadowCount = 0;
    }
}

//==============================================================================

xbool material_mgr::UpdateProjTextures( const matrix4& L2W, const bbox& B, u32 Slot )
{
    if( !m_pProjTextureBuffer || !g_pd3dContext )
        return FALSE;

    cb_proj_textures cb;
    x_memset( &cb, 0, sizeof(cb) );
    ID3D11ShaderResourceView* lightSRV[proj_texture_mgr::MAX_PROJ_LIGHTS] = { NULL };
    ID3D11ShaderResourceView* shadSRV [proj_texture_mgr::MAX_PROJ_SHADOWS] = { NULL };

    s32 nProjLights = g_ProjTextureMgr.CollectLights( L2W, B );
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

    s32 nProjShadows = g_ProjTextureMgr.CollectShadows( L2W, B, 2 );
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
        g_pd3dContext->PSSetShaderResources( PROJ_LIGHT_TEX_SLOT, nAppliedLights, lightSRV );
    if( nAppliedShadows )
        g_pd3dContext->PSSetShaderResources( PROJ_SHADOW_TEX_SLOT, nAppliedShadows, shadSRV );

    m_LastProjLightCount  = nAppliedLights;
    m_LastProjShadowCount = nAppliedShadows;
    return TRUE;
}

//==============================================================================

xbool material_mgr::UpdateRigidConstants( const matrix4* pL2W, const material* pMaterial )
{
    if( !m_pRigidConstantBuffer || !g_pd3dDevice || !g_pd3dContext )
        return FALSE;

    const view* pView = eng_GetView();
    if( !pView )
        return FALSE;

    rigid_vert_matrices newMatrices;

    if( pL2W )
        newMatrices.World = *pL2W;
    else
        newMatrices.World.Identity();
    
    newMatrices.View = pView->GetW2V();
    newMatrices.Projection = pView->GetV2C();
    newMatrices.CameraPosition = pView->GetPosition();
    
    // Analyze material and set flags
    newMatrices.MaterialFlags = 0;
    newMatrices.AlphaRef = 0.5f;

    if( pMaterial )
    {
        switch( pMaterial->m_Type )
        {
            case Material_Diff:
                break;
                
            case Material_Alpha:
                newMatrices.MaterialFlags |= MATERIAL_FLAG_ALPHA_TEST;
                break;
                
            case Material_Diff_PerPixelIllum:
                newMatrices.MaterialFlags |= MATERIAL_FLAG_PERPIXEL_ILLUM;
                break;
                
            case Material_Alpha_PerPixelIllum:
                newMatrices.MaterialFlags |= MATERIAL_FLAG_ALPHA_TEST | MATERIAL_FLAG_PERPIXEL_ILLUM;
                break;
                
            case Material_Alpha_PerPolyIllum:
                newMatrices.MaterialFlags |= MATERIAL_FLAG_ALPHA_TEST | MATERIAL_FLAG_PERPOLY_ILLUM;
                break;
                
            case Material_Diff_PerPixelEnv:
                newMatrices.MaterialFlags |= MATERIAL_FLAG_PERPIXEL_ENV;
                break;
                
            case Material_Alpha_PerPolyEnv:
                newMatrices.MaterialFlags |= MATERIAL_FLAG_ALPHA_TEST | MATERIAL_FLAG_PERPOLY_ENV;
                break;
                
            case Material_Distortion:
                newMatrices.MaterialFlags |= MATERIAL_FLAG_DISTORTION;
                break;
                
            case Material_Distortion_PerPolyEnv:
                newMatrices.MaterialFlags |= MATERIAL_FLAG_DISTORTION | MATERIAL_FLAG_PERPOLY_ENV;
                break;
        }						
        
        // Check for detail map
        if( pMaterial->m_DetailMap.GetPointer() )
        {
            newMatrices.MaterialFlags |= MATERIAL_FLAG_HAS_DETAIL;
        }
        
        // Check for ENV map
        if( pMaterial->m_EnvironmentMap.GetPointer() )
        {
            newMatrices.MaterialFlags |= MATERIAL_FLAG_HAS_ENVIRONMENT;
        }
        
        // Check blend flags
        if( pMaterial->m_Flags & geom::material::FLAG_IS_ADDITIVE )
            newMatrices.MaterialFlags |= MATERIAL_FLAG_ADDITIVE;
        else if( pMaterial->m_Flags & geom::material::FLAG_IS_SUBTRACTIVE )
            newMatrices.MaterialFlags |= MATERIAL_FLAG_SUBTRACTIVE;
        
        // Always use vertex color for now
        newMatrices.MaterialFlags |= MATERIAL_FLAG_VERTEX_COLOR;
		
		// Always use proj textures for now
		newMatrices.MaterialFlags |= MATERIAL_FLAG_PROJ_LIGHT;
		newMatrices.MaterialFlags |= MATERIAL_FLAG_PROJ_SHADOW;
    }
    
    newMatrices.Padding1 = 0.0f;
    newMatrices.Padding2 = 0.0f;

    // Only update if data changed
    if( m_bRigidMatricesDirty || x_memcmp( &m_CachedRigidMatrices, &newMatrices, sizeof(rigid_vert_matrices) ) != 0 )
    {
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        HRESULT hr = g_pd3dContext->Map( m_pRigidConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource );
        if( FAILED(hr) )
        {
            x_DebugMsg( "MaterialMgr: Failed to map rigid constant buffer, HRESULT 0x%08X\n", hr );
            return FALSE;
        }
        
        x_memcpy( mappedResource.pData, &newMatrices, sizeof(rigid_vert_matrices) );
        g_pd3dContext->Unmap( m_pRigidConstantBuffer, 0 );
        
        // Cache the data
        m_CachedRigidMatrices = newMatrices;
        m_bRigidMatricesDirty = FALSE;
        
        g_pd3dContext->VSSetConstantBuffers( 0, 1, &m_pRigidConstantBuffer );
        g_pd3dContext->PSSetConstantBuffers( 0, 1, &m_pRigidConstantBuffer );
    }
    
    return TRUE;
}

//==============================================================================

xbool material_mgr::UpdateSkinConstants( const d3d_skin_lighting* pLighting )
{
    if( !m_pSkinVSConstBuffer || !pLighting || !g_pd3dDevice || !g_pd3dContext )
        return FALSE;

    const view* pView = eng_GetView();
    if( !pView )
        return FALSE;

    cb_skin_vs_consts newConsts;
    newConsts.W2C          = pView->GetW2C();
    newConsts.Zero         = 0.0f;
    newConsts.One          = 1.0f;
    newConsts.MinusOne     = -1.0f;
    newConsts.Fog          = 0.0f;
    for( s32 i=0; i<MAX_SKIN_LIGHTS; i++ )
    {
        if( i < pLighting->LightCount )
        {
            newConsts.LightDir[i].Set( pLighting->Dir[i].GetX(), pLighting->Dir[i].GetY(), pLighting->Dir[i].GetZ(), 0.0f );
            newConsts.LightCol[i] = pLighting->DirCol[i] * 0.5f;
        }
        else
        {
            newConsts.LightDir[i].Set( 0.0f, 0.0f, 0.0f, 0.0f );
            newConsts.LightCol[i].Set( 0.0f, 0.0f, 0.0f, 0.0f );
        }
    }
    newConsts.LightAmbCol  = pLighting->AmbCol * 0.5f;
	newConsts.LightCount   = pLighting->LightCount;
    newConsts.Padding[0]   = 0.0f;
    newConsts.Padding[1]   = 0.0f;
    newConsts.Padding[2]   = 0.0f;

    // Only update if data changed
    if( m_bSkinConstsDirty || x_memcmp( &m_CachedSkinConsts, &newConsts, sizeof(cb_skin_vs_consts) ) != 0 )
    {
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        HRESULT hr = g_pd3dContext->Map( m_pSkinVSConstBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource );
        if( FAILED(hr) )
        {
            x_DebugMsg( "MaterialMgr: Failed to map skin VS constant buffer, HRESULT 0x%08X\n", hr );
            return FALSE;
        }

        x_memcpy( mappedResource.pData, &newConsts, sizeof(cb_skin_vs_consts) );
        g_pd3dContext->Unmap( m_pSkinVSConstBuffer, 0 );
        
        // Cache the data
        m_CachedSkinConsts = newConsts;
        m_bSkinConstsDirty = FALSE;

        g_pd3dContext->VSSetConstantBuffers( 0, 1, &m_pSkinVSConstBuffer );
        g_pd3dContext->VSSetConstantBuffers( 1, 1, &m_pSkinBoneBuffer );
		g_pd3dContext->PSSetConstantBuffers( 0, 1, &m_pSkinVSConstBuffer );
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
    m_bSkinConstsDirty = TRUE;
}

//==============================================================================

// Skin bone buffer access for SoftVertexMgr
ID3D11Buffer* material_mgr::GetSkinBoneBuffer( void )
{
    return m_pSkinBoneBuffer;
}