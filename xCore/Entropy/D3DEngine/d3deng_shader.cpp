//==============================================================================
//  
//  d3deng_shader.cpp
//  
//  Shader manager for D3DENG.
//
//==============================================================================

// TODO: Add hot reload ???

//==============================================================================
//  PLATFORM CHECK
//==============================================================================

#include "x_target.hpp"

#ifndef TARGET_PC
#error This file should only be compiled for PC platform. Please check your exclusions on your project spec.
#endif

//==============================================================================
//  INCLUDES
//==============================================================================

#include "d3deng_shader.hpp"

//==============================================================================
//  FUNCTIONS
//==============================================================================

void shader_Init( void )
{
}

//==============================================================================

void shader_Kill( void )
{
	// TODO: GS: Maybe add all loaded shader kill ?
	// Although this should be handled by local managers....
}

//==============================================================================
//  FILE I/O FUNCTIONS
//==============================================================================

char* shader_LoadSourceFromFile( const char* pFileName )
{
    if( !pFileName )
    {
        x_DebugMsg( "ShaderMgr: NULL filename provided\n" );
        return NULL;
    }

    X_FILE* pFile = x_fopen( pFileName, "rb" );
    if( !pFile )
    {
        x_DebugMsg( "ShaderMgr: Failed to open file '%s'\n", pFileName );
        return NULL;
    }

    s32 FileLength = x_flength( pFile );
    if( FileLength <= 0 )
    {
        x_DebugMsg( "ShaderMgr: File '%s' is empty or invalid\n", pFileName );
        x_fclose( pFile );
        return NULL;
    }

    // Allocate buffer with extra byte for null terminator
    char* pBuffer = (char*)x_malloc( FileLength + 1 );
    if( !pBuffer )
    {
        x_DebugMsg( "ShaderMgr: Failed to allocate memory for file '%s'\n", pFileName );
        x_fclose( pFile );
        return NULL;
    }

    s32 BytesRead = x_fread( pBuffer, 1, FileLength, pFile );
    x_fclose( pFile );

    if( BytesRead != FileLength )
    {
        x_DebugMsg( "ShaderMgr: Failed to read complete file '%s' (%d/%d bytes)\n", 
                    pFileName, BytesRead, FileLength );
        x_free( pBuffer );
        return NULL;
    }

    // Null terminate the string
    pBuffer[FileLength] = '\0';

    x_DebugMsg( "ShaderMgr: Successfully loaded source from '%s' (%d bytes)\n", 
                pFileName, FileLength );
    return pBuffer;
}

//==============================================================================

xbool shader_LoadBlobFromFile( const char* pFileName, shader_blob& Blob )
{
    if( !pFileName )
    {
        x_DebugMsg( "ShaderMgr: NULL filename provided for blob loading\n" );
        return FALSE;
    }

    X_FILE* pFile = x_fopen( pFileName, "rb" );
    if( !pFile )
    {
        x_DebugMsg( "ShaderMgr: Failed to open blob file '%s'\n", pFileName );
        return FALSE;
    }

    s32 FileLength = x_flength( pFile );
    if( FileLength <= 0 )
    {
        x_DebugMsg( "ShaderMgr: Blob file '%s' is empty or invalid\n", pFileName );
        x_fclose( pFile );
        return FALSE;
    }

    void* pBuffer = x_malloc( FileLength );
    if( !pBuffer )
    {
        x_DebugMsg( "ShaderMgr: Failed to allocate memory for blob file '%s'\n", pFileName );
        x_fclose( pFile );
        return FALSE;
    }

    s32 BytesRead = x_fread( pBuffer, 1, FileLength, pFile );
    x_fclose( pFile );

    if( BytesRead != FileLength )
    {
        x_DebugMsg( "ShaderMgr: Failed to read complete blob file '%s' (%d/%d bytes)\n", 
                    pFileName, BytesRead, FileLength );
        x_free( pBuffer );
        return FALSE;
    }

    Blob.pData = pBuffer;
    Blob.Size = (usize)FileLength;

    x_DebugMsg( "ShaderMgr: Successfully loaded blob from '%s' (%d bytes)\n", 
                pFileName, FileLength );
    return TRUE;
}

//==============================================================================

xbool shader_SaveBlobToFile( const char* pFileName, const shader_blob& Blob )
{
    if( !pFileName )
    {
        x_DebugMsg( "ShaderMgr: NULL filename provided for blob saving\n" );
        return FALSE;
    }

    if( !Blob.pData || Blob.Size == 0 )
    {
        x_DebugMsg( "ShaderMgr: Invalid blob for saving to file '%s'\n", pFileName );
        return FALSE;
    }

    X_FILE* pFile = x_fopen( pFileName, "wb" );
    if( !pFile )
    {
        x_DebugMsg( "ShaderMgr: Failed to create blob file '%s'\n", pFileName );
        return FALSE;
    }

    s32 BytesWritten = x_fwrite( Blob.pData, 1, (s32)Blob.Size, pFile );
    x_fclose( pFile );

    if( BytesWritten != (s32)Blob.Size )
    {
        x_DebugMsg( "ShaderMgr: Failed to write complete blob to file '%s' (%d/%d bytes)\n", 
                    pFileName, BytesWritten, (s32)Blob.Size );
        return FALSE;
    }

    x_DebugMsg( "ShaderMgr: Successfully saved blob to '%s' (%d bytes)\n", 
                pFileName, (s32)Blob.Size );
    return TRUE;
}

//==============================================================================
//  INTERNAL HELPER FUNCTIONS
//==============================================================================

static 
const char* shader_GetDefaultProfile( shader_type Type )
{
    static const char* s_ShaderTypeNames[] = { "vs_5_0", "ps_5_0", "gs_5_0", "cs_5_0" };  
    const char* pTypeName = (Type < SHADER_TYPE_COUNT) ? s_ShaderTypeNames[Type] : "UNKNOWN";
	
    x_DebugMsg( "ShaderMgr: No profile specified, using default profile for %s shader\n", pTypeName );
    
    switch( Type )
    {
        case SHADER_TYPE_VERTEX:    return "vs_5_0";
        case SHADER_TYPE_PIXEL:     return "ps_5_0";
        case SHADER_TYPE_GEOMETRY:  return "gs_5_0";
        case SHADER_TYPE_COMPUTE:   return "cs_5_0";
        default:    
		    // Um, how did this happen?
            x_throw( "ShaderMgr: Unknown shader type\n" );			                                                                                  
    }
}

//==============================================================================

static 
xbool shader_CompileShaderInternal( const char* pSource,
                                           const char* pEntryPoint,
                                           const char* pProfile,
                                           ID3DBlob** ppBlob,
                                           ID3DBlob** ppErrorBlob )
{
    if( !pSource || !pEntryPoint || !pProfile || !ppBlob )
    {
        x_DebugMsg( "ShaderMgr: Invalid parameters for compilation\n" );
        return FALSE;
    }

    if( !g_pd3dDevice )
    {
        x_DebugMsg( "ShaderMgr: No D3D device available\n" );
        return FALSE;
    }

    HRESULT hr = D3DCompile( pSource, 
                            strlen(pSource), 
                            NULL, 
                            NULL, 
                            NULL, 
                            pEntryPoint, 
                            pProfile, 
                            0, 
                            0, 
                            ppBlob, 
                            ppErrorBlob );

    if( FAILED(hr) )
    {
        if( ppErrorBlob && *ppErrorBlob )
        {
            const char* pErrorMsg = (const char*)(*ppErrorBlob)->GetBufferPointer();
            x_DebugMsg( "ShaderMgr: Compilation failed\n%s\n", pErrorMsg );
        }
        else
        {
            x_DebugMsg( "ShaderMgr: Compilation failed with HRESULT 0x%08X\n", hr );
        }
        return FALSE;
    }

    return TRUE;
}

//==============================================================================
//  SHADER COMPILATION FROM SOURCE
//==============================================================================

ID3D11VertexShader* shader_CompileVertex( const char* pSource, 
                                          const char* pEntryPoint,
                                          const char* pProfile )
{
    ID3DBlob* pBlob = NULL;
    ID3DBlob* pErrorBlob = NULL;
    ID3D11VertexShader* pShader = NULL;

	if( !pEntryPoint )
        pEntryPoint = "main";

    if( !pProfile )
        pProfile = shader_GetDefaultProfile( SHADER_TYPE_VERTEX );

    if( !shader_CompileShaderInternal( pSource, pEntryPoint, pProfile, &pBlob, &pErrorBlob ) )
    {
        if( pErrorBlob ) pErrorBlob->Release();
        return NULL;
    }

    HRESULT hr = g_pd3dDevice->CreateVertexShader( pBlob->GetBufferPointer(), 
                                                   pBlob->GetBufferSize(), 
                                                   NULL, 
                                                   &pShader );
    if( FAILED(hr) )
    {
        x_DebugMsg( "ShaderMgr: Failed to create vertex shader, HRESULT 0x%08X\n", hr );
        pBlob->Release();
        if( pErrorBlob ) pErrorBlob->Release();
        return NULL;
    }

    pBlob->Release();
    if( pErrorBlob ) pErrorBlob->Release();
    
    x_DebugMsg( "ShaderMgr: Vertex shader compiled successfully\n" );
    return pShader;
}

//==============================================================================

ID3D11PixelShader* shader_CompilePixel( const char* pSource,
                                        const char* pEntryPoint,
                                        const char* pProfile )
{
    ID3DBlob* pBlob = NULL;
    ID3DBlob* pErrorBlob = NULL;
    ID3D11PixelShader* pShader = NULL;

	if( !pEntryPoint )
        pEntryPoint = "main";

    if( !pProfile )
        pProfile = shader_GetDefaultProfile( SHADER_TYPE_PIXEL );

    if( !shader_CompileShaderInternal( pSource, pEntryPoint, pProfile, &pBlob, &pErrorBlob ) )
    {
        if( pErrorBlob ) pErrorBlob->Release();
        return NULL;
    }

    HRESULT hr = g_pd3dDevice->CreatePixelShader( pBlob->GetBufferPointer(), 
                                                  pBlob->GetBufferSize(), 
                                                  NULL, 
                                                  &pShader );
    if( FAILED(hr) )
    {
        x_DebugMsg( "ShaderMgr: Failed to create pixel shader, HRESULT 0x%08X\n", hr );
        pBlob->Release();
        if( pErrorBlob ) pErrorBlob->Release();
        return NULL;
    }

    pBlob->Release();
    if( pErrorBlob ) pErrorBlob->Release();
    
    x_DebugMsg( "ShaderMgr: Pixel shader compiled successfully\n" );
    return pShader;
}

//==============================================================================

ID3D11GeometryShader* shader_CompileGeometry( const char* pSource,
                                              const char* pEntryPoint,
                                              const char* pProfile )
{
    ID3DBlob* pBlob = NULL;
    ID3DBlob* pErrorBlob = NULL;
    ID3D11GeometryShader* pShader = NULL;

	if( !pEntryPoint )
        pEntryPoint = "main";

    if( !pProfile )
        pProfile = shader_GetDefaultProfile( SHADER_TYPE_GEOMETRY );

    if( !shader_CompileShaderInternal( pSource, pEntryPoint, pProfile, &pBlob, &pErrorBlob ) )
    {
        if( pErrorBlob ) pErrorBlob->Release();
        return NULL;
    }

    HRESULT hr = g_pd3dDevice->CreateGeometryShader( pBlob->GetBufferPointer(), 
                                                     pBlob->GetBufferSize(), 
                                                     NULL, 
                                                     &pShader );
    if( FAILED(hr) )
    {
        x_DebugMsg( "ShaderMgr: Failed to create geometry shader, HRESULT 0x%08X\n", hr );
        pBlob->Release();
        if( pErrorBlob ) pErrorBlob->Release();
        return NULL;
    }

    pBlob->Release();
    if( pErrorBlob ) pErrorBlob->Release();
    
    x_DebugMsg( "ShaderMgr: Geometry shader compiled successfully\n" );
    return pShader;
}

//==============================================================================

ID3D11ComputeShader* shader_CompileCompute( const char* pSource,
                                            const char* pEntryPoint,
                                            const char* pProfile )
{
    ID3DBlob* pBlob = NULL;
    ID3DBlob* pErrorBlob = NULL;
    ID3D11ComputeShader* pShader = NULL;

	if( !pEntryPoint )
        pEntryPoint = "main";

    if( !pProfile )
        pProfile = shader_GetDefaultProfile( SHADER_TYPE_COMPUTE );

    if( !shader_CompileShaderInternal( pSource, pEntryPoint, pProfile, &pBlob, &pErrorBlob ) )
    {
        if( pErrorBlob ) pErrorBlob->Release();
        return NULL;
    }

    HRESULT hr = g_pd3dDevice->CreateComputeShader( pBlob->GetBufferPointer(), 
                                                    pBlob->GetBufferSize(), 
                                                    NULL, 
                                                    &pShader );
    if( FAILED(hr) )
    {
        x_DebugMsg( "ShaderMgr: Failed to create compute shader, HRESULT 0x%08X\n", hr );
        pBlob->Release();
        if( pErrorBlob ) pErrorBlob->Release();
        return NULL;
    }

    pBlob->Release();
    if( pErrorBlob ) pErrorBlob->Release();
    
    x_DebugMsg( "ShaderMgr: Compute shader compiled successfully\n" );
    return pShader;
}

//==============================================================================

ID3D11VertexShader* shader_CompileVertexWithLayout( const char* pSource,
                                                    ID3D11InputLayout** ppLayout,
                                                    const D3D11_INPUT_ELEMENT_DESC* pLayoutDesc,
                                                    u32 NumElements,
                                                    const char* pEntryPoint,
                                                    const char* pProfile )
{
    ID3DBlob* pBlob = NULL;
    ID3DBlob* pErrorBlob = NULL;
    ID3D11VertexShader* pShader = NULL;

	if( !pEntryPoint )
        pEntryPoint = "main";

    if( !pProfile )
        pProfile = shader_GetDefaultProfile( SHADER_TYPE_VERTEX );

    if( !shader_CompileShaderInternal( pSource, pEntryPoint, pProfile, &pBlob, &pErrorBlob ) )
    {
        if( pErrorBlob ) pErrorBlob->Release();
        return NULL;
    }

    // Create vertex shader
    HRESULT hr = g_pd3dDevice->CreateVertexShader( pBlob->GetBufferPointer(), 
                                                   pBlob->GetBufferSize(), 
                                                   NULL, 
                                                   &pShader );
    if( FAILED(hr) )
    {
        x_DebugMsg( "ShaderMgr: Failed to create vertex shader, HRESULT 0x%08X\n", hr );
        pBlob->Release();
        if( pErrorBlob ) pErrorBlob->Release();
        return NULL;
    }

    // Create input layout if requested
    if( ppLayout && pLayoutDesc )
    {
        hr = g_pd3dDevice->CreateInputLayout( pLayoutDesc, 
                                             NumElements, 
                                             pBlob->GetBufferPointer(), 
                                             pBlob->GetBufferSize(), 
                                             ppLayout );
        if( FAILED(hr) )
        {
            x_DebugMsg( "ShaderMgr: Failed to create input layout, HRESULT 0x%08X\n", hr );
            pShader->Release();
            pBlob->Release();
            if( pErrorBlob ) pErrorBlob->Release();
            return NULL;
        }
        x_DebugMsg( "ShaderMgr: Input layout created successfully\n" );
    }

    pBlob->Release();
    if( pErrorBlob ) pErrorBlob->Release();
    
    x_DebugMsg( "ShaderMgr: Vertex shader with layout compiled successfully\n" );
    return pShader;
}

//==============================================================================
//  SHADER COMPILATION FROM FILES
//==============================================================================

ID3D11VertexShader* shader_CompileVertexFromFile( const char* pFileName,
                                                  const char* pEntryPoint,
                                                  const char* pProfile )
{
    char* pSource = shader_LoadSourceFromFile( pFileName );
    if( !pSource )
    {
        return NULL;
    }

    ID3D11VertexShader* pShader = shader_CompileVertex( pSource, pEntryPoint, pProfile );
    if( pSource )
    {
        x_free( pSource );
    }

    if( pShader )
    {
        x_DebugMsg( "ShaderMgr: Vertex shader compiled from file '%s'\n", pFileName );
    }
    
    return pShader;
}

//==============================================================================

ID3D11PixelShader* shader_CompilePixelFromFile( const char* pFileName,
                                                const char* pEntryPoint,
                                                const char* pProfile )
{
    char* pSource = shader_LoadSourceFromFile( pFileName );
    if( !pSource )
    {
        return NULL;
    }

    ID3D11PixelShader* pShader = shader_CompilePixel( pSource, pEntryPoint, pProfile );
    if( pSource )
    {
        x_free( pSource );
    }

    if( pShader )
    {
        x_DebugMsg( "ShaderMgr: Pixel shader compiled from file '%s'\n", pFileName );
    }
    
    return pShader;
}

//==============================================================================

ID3D11GeometryShader* shader_CompileGeometryFromFile( const char* pFileName,
                                                      const char* pEntryPoint,
                                                      const char* pProfile )
{
    char* pSource = shader_LoadSourceFromFile( pFileName );
    if( !pSource )
    {
        return NULL;
    }

    ID3D11GeometryShader* pShader = shader_CompileGeometry( pSource, pEntryPoint, pProfile );
    if( pSource )
    {
        x_free( pSource );
    }

    if( pShader )
    {
        x_DebugMsg( "ShaderMgr: Geometry shader compiled from file '%s'\n", pFileName );
    }
    
    return pShader;
}

//==============================================================================

ID3D11ComputeShader* shader_CompileComputeFromFile( const char* pFileName,
                                                    const char* pEntryPoint,
                                                    const char* pProfile )
{
    char* pSource = shader_LoadSourceFromFile( pFileName );
    if( !pSource )
    {
        return NULL;
    }

    ID3D11ComputeShader* pShader = shader_CompileCompute( pSource, pEntryPoint, pProfile );
    if( pSource )
    {
        x_free( pSource );
    }

    if( pShader )
    {
        x_DebugMsg( "ShaderMgr: Compute shader compiled from file '%s'\n", pFileName );
    }
    
    return pShader;
}

//==============================================================================

ID3D11VertexShader* shader_CompileVertexFromFileWithLayout( const char* pFileName,
                                                           ID3D11InputLayout** ppLayout,
                                                           const D3D11_INPUT_ELEMENT_DESC* pLayoutDesc,
                                                           u32 NumElements,
                                                           const char* pEntryPoint,
                                                           const char* pProfile )
{
    char* pSource = shader_LoadSourceFromFile( pFileName );
    if( !pSource )
    {
        return NULL;
    }

    ID3D11VertexShader* pShader = shader_CompileVertexWithLayout( pSource, 
                                                                 ppLayout, 
                                                                 pLayoutDesc, 
                                                                 NumElements, 
                                                                 pEntryPoint, 
                                                                 pProfile );
    if( pSource )
    {
        x_free( pSource );
    }

    if( pShader )
    {
        x_DebugMsg( "ShaderMgr: Vertex shader with layout compiled from file '%s'\n", pFileName );
    }
    
    return pShader;
}

//==============================================================================
//  SHADER BLOB OPERATIONS
//==============================================================================

xbool shader_CompileToBlob( const char* pSource,
                            shader_type Type,
                            shader_blob& Blob,
                            const char* pEntryPoint,
                            const char* pProfile )
{
    ID3DBlob* pD3DBlob = NULL;
    ID3DBlob* pErrorBlob = NULL;
    
	if( !pEntryPoint )
        pEntryPoint = "main";
	
    if( !pProfile )
        pProfile = shader_GetDefaultProfile( Type );

    if( !shader_CompileShaderInternal( pSource, pEntryPoint, pProfile, &pD3DBlob, &pErrorBlob ) )
    {
        if( pErrorBlob ) pErrorBlob->Release();
        return FALSE;
    }

    // Copy blob data
    Blob.Size = pD3DBlob->GetBufferSize();
    Blob.pData = x_malloc( Blob.Size );
    x_memcpy( Blob.pData, pD3DBlob->GetBufferPointer(), Blob.Size );

    pD3DBlob->Release();
    if( pErrorBlob ) pErrorBlob->Release();
    
    x_DebugMsg( "ShaderMgr: Compiled to blob successfully (%d bytes)\n", (s32)Blob.Size );
    return TRUE;
}

//==============================================================================

xbool shader_CompileFileToBlob( const char* pFileName,
                                shader_type Type,
                                shader_blob& Blob,
                                const char* pEntryPoint,
                                const char* pProfile )
{
    char* pSource = shader_LoadSourceFromFile( pFileName );
    if( !pSource )
    {
        return FALSE;
    }

    xbool Result = shader_CompileToBlob( pSource, Type, Blob, pEntryPoint, pProfile );
    if( pSource )
    {
        x_free( pSource );
    }

    if( Result )
    {
        x_DebugMsg( "ShaderMgr: Compiled file '%s' to blob successfully\n", pFileName );
    }
    
    return Result;
}

//==============================================================================

ID3D11VertexShader* shader_CreateVertexFromBlob( const shader_blob& Blob )
{
    if( !Blob.pData || Blob.Size == 0 )
    {
        x_DebugMsg( "ShaderMgr: Invalid blob for vertex shader creation\n" );
        return NULL;
    }

    ID3D11VertexShader* pShader = NULL;
    HRESULT hr = g_pd3dDevice->CreateVertexShader( Blob.pData, Blob.Size, NULL, &pShader );
    if( FAILED(hr) )
    {
        x_DebugMsg( "ShaderMgr: Failed to create vertex shader from blob, HRESULT 0x%08X\n", hr );
        return NULL;
    }

    x_DebugMsg( "ShaderMgr: Vertex shader created from blob successfully\n" );
    return pShader;
}

//==============================================================================

ID3D11PixelShader* shader_CreatePixelFromBlob( const shader_blob& Blob )
{
    if( !Blob.pData || Blob.Size == 0 )
    {
        x_DebugMsg( "ShaderMgr: Invalid blob for pixel shader creation\n" );
        return NULL;
    }

    ID3D11PixelShader* pShader = NULL;
    HRESULT hr = g_pd3dDevice->CreatePixelShader( Blob.pData, Blob.Size, NULL, &pShader );
    if( FAILED(hr) )
    {
        x_DebugMsg( "ShaderMgr: Failed to create pixel shader from blob, HRESULT 0x%08X\n", hr );
        return NULL;
    }

    x_DebugMsg( "ShaderMgr: Pixel shader created from blob successfully\n" );
    return pShader;
}

//==============================================================================

ID3D11GeometryShader* shader_CreateGeometryFromBlob( const shader_blob& Blob )
{
    if( !Blob.pData || Blob.Size == 0 )
    {
        x_DebugMsg( "ShaderMgr: Invalid blob for geometry shader creation\n" );
        return NULL;
    }

    ID3D11GeometryShader* pShader = NULL;
    HRESULT hr = g_pd3dDevice->CreateGeometryShader( Blob.pData, Blob.Size, NULL, &pShader );
    if( FAILED(hr) )
    {
        x_DebugMsg( "ShaderMgr: Failed to create geometry shader from blob, HRESULT 0x%08X\n", hr );
        return NULL;
    }

    x_DebugMsg( "ShaderMgr: Geometry shader created from blob successfully\n" );
    return pShader;
}

//==============================================================================

ID3D11ComputeShader* shader_CreateComputeFromBlob( const shader_blob& Blob )
{
    if( !Blob.pData || Blob.Size == 0 )
    {
        x_DebugMsg( "ShaderMgr: Invalid blob for compute shader creation\n" );
        return NULL;
    }

    ID3D11ComputeShader* pShader = NULL;
    HRESULT hr = g_pd3dDevice->CreateComputeShader( Blob.pData, Blob.Size, NULL, &pShader );
    if( FAILED(hr) )
    {
        x_DebugMsg( "ShaderMgr: Failed to create compute shader from blob, HRESULT 0x%08X\n", hr );
        return NULL;
    }

    x_DebugMsg( "ShaderMgr: Compute shader created from blob successfully\n" );
    return pShader;
}

//==============================================================================

ID3D11InputLayout* shader_CreateInputLayout( const shader_blob& VertexBlob,
                                             const D3D11_INPUT_ELEMENT_DESC* pLayoutDesc,
                                             u32 NumElements )
{
    if( !VertexBlob.pData || VertexBlob.Size == 0 )
    {
        x_DebugMsg( "ShaderMgr: Invalid vertex blob for input layout creation\n" );
        return NULL;
    }

    if( !pLayoutDesc || NumElements == 0 )
    {
        x_DebugMsg( "ShaderMgr: Invalid layout description\n" );
        return NULL;
    }

    ID3D11InputLayout* pLayout = NULL;
    HRESULT hr = g_pd3dDevice->CreateInputLayout( pLayoutDesc, 
                                                 NumElements, 
                                                 VertexBlob.pData, 
                                                 VertexBlob.Size, 
                                                 &pLayout );
    if( FAILED(hr) )
    {
        x_DebugMsg( "ShaderMgr: Failed to create input layout, HRESULT 0x%08X\n", hr );
        return NULL;
    }

    x_DebugMsg( "ShaderMgr: Input layout created successfully\n" );
    return pLayout;
}

//==============================================================================

void shader_FreeBlob( shader_blob& Blob )
{
    if( Blob.pData )
    {
        x_free( Blob.pData );
        Blob.pData = NULL;
        Blob.Size = 0;
    }
}

//==============================================================================
//  CONSTANT BUFFER UTILITIES
//==============================================================================

ID3D11Buffer* shader_CreateConstantBuffer( usize Size )
{
    if( !g_pd3dDevice )
    {
        x_DebugMsg( "ShaderMgr: No D3D device available for constant buffer creation\n" );
        return NULL;
    }

    D3D11_BUFFER_DESC cbd;
    ZeroMemory( &cbd, sizeof(cbd) );
    cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.ByteWidth = (UINT)Size;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    cbd.MiscFlags = 0;

    ID3D11Buffer* pBuffer = NULL;
    HRESULT hr = g_pd3dDevice->CreateBuffer( &cbd, NULL, &pBuffer );
    if( FAILED(hr) )
    {
        x_DebugMsg( "ShaderMgr: Failed to create constant buffer, HRESULT 0x%08X\n", hr );
        return NULL;
    }

    x_DebugMsg( "ShaderMgr: Constant buffer created (%d bytes)\n", (s32)Size );
    return pBuffer;
}

//==============================================================================

void shader_UpdateConstantBuffer( ID3D11Buffer* pBuffer, const void* pData, usize Size )
{
    if (!g_pd3dContext || !pBuffer || !pData)
        return;

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr = g_pd3dContext->Map(pBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (SUCCEEDED(hr))
    {
        x_memcpy(mappedResource.pData, pData, Size);
        g_pd3dContext->Unmap(pBuffer, 0);
        //x_DebugMsg("ShaderMgr: Constant buffer updated (%d bytes)\n", (s32)Size);
    }
    else
    {
        x_DebugMsg("ShaderMgr: Failed to map constant buffer, HRESULT 0x%08X\n", hr);
    }
}

//==============================================================================

void shader_ReleaseConstantBuffer( ID3D11Buffer* pBuffer )
{
    if( pBuffer )
    {
        pBuffer->Release();
		x_DebugMsg( "ShaderMgr: Constant buffer released successfully\n" );
    }
}