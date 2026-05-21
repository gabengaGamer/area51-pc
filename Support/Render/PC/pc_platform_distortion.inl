//=============================================================================
//
//  PC Distortion Helpers
//
//=============================================================================

static xbool                     s_bInDistortionPass      = FALSE;
static ID3D11Texture2D*          s_pDistortionSceneTexture = NULL;
static ID3D11ShaderResourceView* s_pDistortionSceneSRV    = NULL;
static u32                       s_DistortionSceneWidth   = 0;
static u32                       s_DistortionSceneHeight  = 0;
static DXGI_FORMAT               s_DistortionSceneFormat  = DXGI_FORMAT_UNKNOWN;
static material                  s_DefaultDistortionMaterial;

//=============================================================================

static
void platform_ReleaseDistortionScene( void )
{
    if( s_pDistortionSceneSRV )
    {
        s_pDistortionSceneSRV->Release();
        s_pDistortionSceneSRV = NULL;
    }

    if( s_pDistortionSceneTexture )
    {
        s_pDistortionSceneTexture->Release();
        s_pDistortionSceneTexture = NULL;
    }

    s_DistortionSceneWidth  = 0;
    s_DistortionSceneHeight = 0;
    s_DistortionSceneFormat = DXGI_FORMAT_UNKNOWN;
}

//=============================================================================

static
void platform_InitDefaultDistortionMaterial( void )
{
    x_memset( &s_DefaultDistortionMaterial, 0, sizeof(s_DefaultDistortionMaterial) );
    s_DefaultDistortionMaterial.m_Type        = Material_Distortion;
    s_DefaultDistortionMaterial.m_DetailScale = 1.0f;
    s_DefaultDistortionMaterial.m_FixedAlpha  = 0.0f;
}

//=============================================================================

static
void platform_ClearDistortionTextureBindings( void )
{
    g_GeomMgr.SetBitmap( NULL, TEXTURE_SLOT_DIFFUSE );
    g_GeomMgr.SetBitmap( NULL, TEXTURE_SLOT_DETAIL );
    g_GeomMgr.SetEnvironmentCubemap( NULL );
    g_GeomMgr.SetBitmap( NULL, TEXTURE_SLOT_ENVIRONMENT );
}

//=============================================================================

static
void platform_ActivateDistortionTextureBindings( const material* pMaterial )
{
    platform_ClearDistortionTextureBindings();

    if( !pMaterial || (pMaterial->m_Type != Material_Distortion_PerPolyEnv) )
        return;

    if( pMaterial->m_Flags & geom::material::FLAG_ENV_CUBE_MAP )
    {
        if( s_pCurrCubeMap && s_pCurrCubeMap->m_hTexture )
            g_GeomMgr.SetEnvironmentCubemap( s_pCurrCubeMap );

        return;
    }

    texture* pEnvironment = pMaterial->m_EnvironmentMap.GetPointer();
    const xbitmap* pEnvironmentMap = pEnvironment ? &pEnvironment->m_Bitmap : NULL;

    if( pEnvironmentMap && (pEnvironmentMap->GetVRAMID() > 0) )
        g_GeomMgr.SetBitmap( pEnvironmentMap, TEXTURE_SLOT_ENVIRONMENT );
}

//=============================================================================

static
xbool platform_EnsureDistortionScene( const rtarget* pSourceTarget )
{
    if( !g_pd3dDevice || !g_pd3dContext || !pSourceTarget || !pSourceTarget->pTexture )
        return FALSE;

    D3D11_TEXTURE2D_DESC sourceDesc;
    pSourceTarget->pTexture->GetDesc( &sourceDesc );

    if( !s_pDistortionSceneTexture ||
        !s_pDistortionSceneSRV ||
        (s_DistortionSceneWidth  != sourceDesc.Width) ||
        (s_DistortionSceneHeight != sourceDesc.Height) ||
        (s_DistortionSceneFormat != sourceDesc.Format) )
    {
        platform_ReleaseDistortionScene();

        D3D11_TEXTURE2D_DESC copyDesc = sourceDesc;
        copyDesc.BindFlags          = D3D11_BIND_SHADER_RESOURCE;
        copyDesc.CPUAccessFlags     = 0;
        copyDesc.MiscFlags          = 0;
        copyDesc.Usage              = D3D11_USAGE_DEFAULT;
        copyDesc.MipLevels          = 1;
        copyDesc.ArraySize          = 1;
        copyDesc.SampleDesc.Count   = 1;
        copyDesc.SampleDesc.Quality = 0;

        HRESULT hr = g_pd3dDevice->CreateTexture2D( &copyDesc, NULL, &s_pDistortionSceneTexture );
        if( FAILED(hr) || !s_pDistortionSceneTexture )
        {
            platform_ReleaseDistortionScene();
            x_DebugMsg( "PCDistortion: Failed to create scene texture, HRESULT 0x%08X\n", hr );
            return FALSE;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
        x_memset( &srvDesc, 0, sizeof(srvDesc) );
        srvDesc.Format                    = copyDesc.Format;
        srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels       = 1;

        hr = g_pd3dDevice->CreateShaderResourceView( s_pDistortionSceneTexture, &srvDesc, &s_pDistortionSceneSRV );
        if( FAILED(hr) || !s_pDistortionSceneSRV )
        {
            platform_ReleaseDistortionScene();
            x_DebugMsg( "PCDistortion: Failed to create scene SRV, HRESULT 0x%08X\n", hr );
            return FALSE;
        }

        s_DistortionSceneWidth  = sourceDesc.Width;
        s_DistortionSceneHeight = sourceDesc.Height;
        s_DistortionSceneFormat = sourceDesc.Format;
    }

    return TRUE;
}

//=============================================================================

static
void platform_ActivateDistortionMaterial( const material* pMaterial, const radian3& NormalRot )
{
    if( pMaterial )
        platform_ActivateMaterial( *pMaterial );
    else
    {
        s_pMaterial = &s_DefaultDistortionMaterial;
        platform_ActivateDistortionTextureBindings( NULL );
    }

    g_GeomMgr.SetDistortionState( NormalRot );
}

//=============================================================================

static
void platform_SetDistortionMaterial( s32 BlendMode, xbool ZTestEnabled )
{
    ASSERTS( FALSE, "Not implemented yet!" );
    (void)BlendMode;
    (void)ZTestEnabled;
}

//=============================================================================

static
void platform_BeginDistortion( void )
{
    s_bInDistortionPass = TRUE;

    if( !g_pd3dContext )
        return;

    const rtarget* pFinalColor = platform_GetFinalColorTarget();
    const rtarget* pDepthTarget = platform_GetFinalDepthTarget();

    if( !pFinalColor )
        return;

    ID3D11ShaderResourceView* pNullSRV = NULL;
    g_pd3dContext->PSSetShaderResources( TEXTURE_SLOT_DISTORTION_SCENE, 1, &pNullSRV );

    if( platform_EnsureDistortionScene( pFinalColor ) )
    {
        D3D11_TEXTURE2D_DESC sourceDesc;
        pFinalColor->pTexture->GetDesc( &sourceDesc );

        if( sourceDesc.SampleDesc.Count > 1 )
        {
            g_pd3dContext->ResolveSubresource( s_pDistortionSceneTexture,
                                               0,
                                               pFinalColor->pTexture,
                                               0,
                                               sourceDesc.Format );
        }
        else
        {
            g_pd3dContext->CopyResource( s_pDistortionSceneTexture, pFinalColor->pTexture );
        }

        g_pd3dContext->PSSetShaderResources( TEXTURE_SLOT_DISTORTION_SCENE, 1, &s_pDistortionSceneSRV );
    }

    rtarget_SetTargets( pFinalColor, 1, pDepthTarget );
}

//=============================================================================

static
void platform_EndDistortion( void )
{
    if( g_pd3dContext )
    {
        ID3D11ShaderResourceView* pNullSRV = NULL;
        g_pd3dContext->PSSetShaderResources( TEXTURE_SLOT_DISTORTION_SCENE, 1, &pNullSRV );
    }

    g_GeomMgr.ClearDistortionState();
    s_bInDistortionPass = FALSE;
}

