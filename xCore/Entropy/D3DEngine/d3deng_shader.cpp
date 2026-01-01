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
    // TODO: GS: Maybe add here kill for all loaded shaders ? Like a x_Kill or something
}

//==============================================================================
//  FILE I/O FUNCTIONS
//==============================================================================

char* shader_LoadSourceFromFile( const char* pFileName )
{
    if( !pFileName )
    {
        x_DebugMsg( "ShaderMgr: NULL filename provided\n" );
        ASSERT(FALSE);
        return NULL;
    }

    X_FILE* pFile = x_fopen( pFileName, "rb" );
    if( !pFile )
    {
        x_DebugMsg( "ShaderMgr: Failed to open file '%s'\n", pFileName );
        ASSERT(FALSE);
        return NULL;
    }

    s32 FileLength = x_flength( pFile );
    if( FileLength <= 0 )
    {
        x_DebugMsg( "ShaderMgr: File '%s' is empty or invalid\n", pFileName );
        ASSERT(FALSE);
        x_fclose( pFile );
        return NULL;
    }

    // Allocate buffer with extra byte for null terminator
    char* pBuffer = (char*)x_malloc( FileLength + 1 );
    if( !pBuffer )
    {
        x_DebugMsg( "ShaderMgr: Failed to allocate memory for file '%s'\n", pFileName );
        ASSERT(FALSE);
        x_fclose( pFile );
        return NULL;
    }

    s32 BytesRead = x_fread( pBuffer, 1, FileLength, pFile );
    x_fclose( pFile );

    if( BytesRead != FileLength )
    {
        x_DebugMsg( "ShaderMgr: Failed to read complete file '%s' (%d/%d bytes)\n",
                    pFileName, BytesRead, FileLength );
        ASSERT(FALSE);
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
        ASSERT(FALSE);
        return FALSE;
    }

    X_FILE* pFile = x_fopen( pFileName, "rb" );
    if( !pFile )
    {
        x_DebugMsg( "ShaderMgr: Failed to open blob file '%s'\n", pFileName );
        ASSERT(FALSE);
        return FALSE;
    }

    s32 FileLength = x_flength( pFile );
    if( FileLength <= 0 )
    {
        x_DebugMsg( "ShaderMgr: Blob file '%s' is empty or invalid\n", pFileName );
        ASSERT(FALSE);
        x_fclose( pFile );
        return FALSE;
    }

    void* pBuffer = x_malloc( FileLength );
    if( !pBuffer )
    {
        x_DebugMsg( "ShaderMgr: Failed to allocate memory for blob file '%s'\n", pFileName );
        ASSERT(FALSE);
        x_fclose( pFile );
        return FALSE;
    }

    s32 BytesRead = x_fread( pBuffer, 1, FileLength, pFile );
    x_fclose( pFile );

    if( BytesRead != FileLength )
    {
        x_DebugMsg( "ShaderMgr: Failed to read complete blob file '%s' (%d/%d bytes)\n",
                    pFileName, BytesRead, FileLength );
        ASSERT(FALSE);
        x_free( pBuffer );
        return FALSE;
    }

    Blob.pData = pBuffer;
    Blob.Size  = (s32)FileLength;

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
        ASSERT(FALSE);
        return FALSE;
    }

    if( !Blob.pData || Blob.Size == 0 )
    {
        x_DebugMsg( "ShaderMgr: Invalid blob for saving to file '%s'\n", pFileName );
        ASSERT(FALSE);
        return FALSE;
    }

    X_FILE* pFile = x_fopen( pFileName, "wb" );
    if( !pFile )
    {
        x_DebugMsg( "ShaderMgr: Failed to create blob file '%s'\n", pFileName );
        ASSERT(FALSE);
        return FALSE;
    }

    s32 BytesWritten = x_fwrite( Blob.pData, 1, (s32)Blob.Size, pFile );
    x_fclose( pFile );

    if( BytesWritten != (s32)Blob.Size )
    {
        x_DebugMsg( "ShaderMgr: Failed to write complete blob to file '%s' (%d/%d bytes)\n",
                    pFileName, BytesWritten, (s32)Blob.Size );
        ASSERT(FALSE);
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
            ASSERT(FALSE);
            return "undefined";
    }
}

//==============================================================================
//  INCLUDE HANDLER
//==============================================================================

class shader_include_handler : public ID3DInclude
{
    xstring m_BasePath;

public:
    explicit shader_include_handler( const char* pSourceName )
    {
        if( pSourceName )
        {
            char drive[X_MAX_DRIVE], dir[X_MAX_DIR];
            x_splitpath( pSourceName, drive, dir, NULL, NULL );
            m_BasePath = xstring(drive) + dir;
            //x_DebugMsg( "ShaderMgr: Include handler base path: '%s'\n", (const char*)m_BasePath );
        }
    }

    STDMETHOD(Open)( THIS_ D3D_INCLUDE_TYPE, LPCSTR pFileName, LPCVOID, LPCVOID* ppData, UINT* pBytes ) override
    {
        xstring path = m_BasePath + pFileName;
        X_FILE* fp = x_fopen( path, "rb" );
        
        if( !fp )
        {
            x_DebugMsg( "ShaderMgr: Failed to open include '%s', trying raw path\n", (const char*)path );
            fp = x_fopen( pFileName, "rb" );
        }
            
        if( !fp )
        {
            x_DebugMsg( "ShaderMgr: Failed to open include file '%s'\n", pFileName );
            return E_FAIL;
        }

        s32 len = x_flength( fp );
        if( len <= 0 )
        {
            x_DebugMsg( "ShaderMgr: Include file '%s' is empty or invalid\n", pFileName );
            x_fclose( fp );
            return E_FAIL;
        }

        const s32 MAX_INCLUDE_SIZE = 1 * 1024 * 1024;
        if( len > MAX_INCLUDE_SIZE )
        {
            x_DebugMsg( "ShaderMgr: Include file '%s' too large (%d bytes)\n", pFileName, len );
            x_fclose( fp );
            return E_FAIL;
        }

        char* pBuffer = (char*)x_malloc( len );
        if( !pBuffer )
        {
            x_DebugMsg( "ShaderMgr: Failed to allocate memory for include '%s'\n", pFileName );
            x_fclose( fp );
            return E_OUTOFMEMORY;
        }

        s32 bytesRead = x_fread( pBuffer, 1, len, fp );
        x_fclose( fp );
        
        if( bytesRead != len )
        {
            x_DebugMsg( "ShaderMgr: Failed to read complete include '%s' (%d/%d bytes)\n", 
                       pFileName, bytesRead, len );
            x_free( pBuffer );
            return E_FAIL;
        }

        *ppData = pBuffer;
        *pBytes = (UINT)len;
        x_DebugMsg( "ShaderMgr: Successfully loaded include '%s' (%d bytes)\n", pFileName, len );
        return S_OK;
    }

    STDMETHOD(Close)( THIS_ LPCVOID pData ) override
    {
        x_free( (void*)pData );
        return S_OK;
    }
};

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

static
xbool shader_CompileShaderInternal( const char* pSource,
                                    const char* pSourceName,
                                    const char* pEntryPoint,
                                    const char* pProfile,
                                    ID3DBlob** ppBlob,
                                    ID3DBlob** ppErrorBlob )
{
    if( !pSource || !pEntryPoint || !pProfile || !ppBlob || !g_pd3dDevice )
    {
        x_DebugMsg( "ShaderMgr: Invalid parameters for compilation\n" );
        ASSERT(FALSE);
        return FALSE;
    }

    const s32 MAX_SOURCE_SIZE = 2 * 1024 * 1024;
    s32 sourceLen = strlen(pSource);
    if( sourceLen > MAX_SOURCE_SIZE )
    {
        x_DebugMsg( "ShaderMgr: Source too large (%d bytes)\n", (s32)sourceLen );
        ASSERT(FALSE);
        return FALSE;
    }

    shader_include_handler includeHandler( pSourceName );
	
    u32 compileFlags = 0;
	
#if defined(X_DEBUG)
    compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION; //| D3DCOMPILE_WARNINGS_ARE_ERRORS;
#elif defined(X_RETAIL)
    compileFlags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

    HRESULT hr = D3DCompile( pSource,
                            sourceLen,
                            pSourceName,
                            NULL,
                            &includeHandler,
                            pEntryPoint,
                            pProfile,
                            compileFlags, 
                            0,
                            ppBlob,
                            ppErrorBlob );

    if( FAILED(hr) )
    {
        if( ppErrorBlob && *ppErrorBlob )
        {
            x_DebugMsg( "ShaderMgr: Compilation failed\n%s\n",
                       (const char*)(*ppErrorBlob)->GetBufferPointer() );
        }
        else
        {
            x_DebugMsg( "ShaderMgr: Compilation failed with HRESULT 0x%08X\n", hr );
        }
        ASSERT(FALSE);
        return FALSE;
    }

    return TRUE;
}

//==============================================================================
//  SHADER COMPILATION FROM SOURCE
//==============================================================================

void* shader_CompileShader( const char* pSource,
                            shader_type Type,
                            const char* pEntryPoint,
                            const char* pProfile,
                            const char* pSourceName,
                            ID3D11InputLayout** ppLayout,
                            const D3D11_INPUT_ELEMENT_DESC* pLayoutDesc,
                            u32 NumElements )
{
    ID3DBlob* pBlob = NULL;
    ID3DBlob* pErrorBlob = NULL;
    
    if( !pEntryPoint )
        pEntryPoint = "main";
    
    if( !pProfile )
        pProfile = shader_GetDefaultProfile( Type );
    
    if( !shader_CompileShaderInternal( pSource, pSourceName, pEntryPoint, pProfile, &pBlob, &pErrorBlob ) )
    {
        if( pErrorBlob ) pErrorBlob->Release();
        return NULL;
    }
    
    void* pShader = NULL;
    HRESULT hr = E_FAIL;
    
    switch( Type )
    {
        case SHADER_TYPE_VERTEX:
            hr = g_pd3dDevice->CreateVertexShader( pBlob->GetBufferPointer(), 
                                                   pBlob->GetBufferSize(), 
                                                   NULL, 
                                                   (ID3D11VertexShader**)&pShader );
            break;
            
        case SHADER_TYPE_PIXEL:
            hr = g_pd3dDevice->CreatePixelShader( pBlob->GetBufferPointer(), 
                                                  pBlob->GetBufferSize(), 
                                                  NULL, 
                                                  (ID3D11PixelShader**)&pShader );
            break;
            
        case SHADER_TYPE_GEOMETRY:
            hr = g_pd3dDevice->CreateGeometryShader( pBlob->GetBufferPointer(), 
                                                     pBlob->GetBufferSize(), 
                                                     NULL, 
                                                     (ID3D11GeometryShader**)&pShader );
            break;
            
        case SHADER_TYPE_COMPUTE:
            hr = g_pd3dDevice->CreateComputeShader( pBlob->GetBufferPointer(), 
                                                    pBlob->GetBufferSize(), 
                                                    NULL, 
                                                    (ID3D11ComputeShader**)&pShader );
            break;
            
        default:
            ASSERT(FALSE);
            pBlob->Release();
            if( pErrorBlob ) pErrorBlob->Release();
            return NULL;
    }
    
    if( FAILED(hr) )
    {
        x_DebugMsg( "ShaderMgr: Failed to create shader, HRESULT 0x%08X\n", hr );
        ASSERT(FALSE);
        pBlob->Release();
        if( pErrorBlob ) pErrorBlob->Release();
        return NULL;
    }
    
    if( ppLayout && pLayoutDesc && NumElements > 0 )
    {
        if( Type != SHADER_TYPE_VERTEX )
        {
            x_DebugMsg( "ShaderMgr: Input layout can only be created for vertex shaders\n" );
            ASSERT(FALSE);
        }
        else
        {
            hr = g_pd3dDevice->CreateInputLayout( pLayoutDesc,
                                                  NumElements,
                                                  pBlob->GetBufferPointer(),
                                                  pBlob->GetBufferSize(),
                                                  ppLayout );
            if( FAILED(hr) )
            {
                x_DebugMsg( "ShaderMgr: Failed to create input layout, HRESULT 0x%08X\n", hr );
                ASSERT(FALSE);
                ((ID3D11VertexShader*)pShader)->Release();
                pBlob->Release();
                if( pErrorBlob ) pErrorBlob->Release();
                return NULL;
            }
        }
    }
    
    pBlob->Release();
    if( pErrorBlob ) pErrorBlob->Release();
    
    x_DebugMsg( "ShaderMgr: Shader compiled successfully\n" );
    return pShader;
}

//==============================================================================

void* shader_CompileShaderFromFile( const char* pFileName,
                                    shader_type Type,
                                    const char* pEntryPoint,
                                    const char* pProfile,
                                    ID3D11InputLayout** ppLayout,
                                    const D3D11_INPUT_ELEMENT_DESC* pLayoutDesc,
                                    u32 NumElements )
{
    char* pSource = shader_LoadSourceFromFile( pFileName );
    if( !pSource )
        return NULL;
    
    void* pShader = shader_CompileShader( pSource, Type, pEntryPoint, pProfile, pFileName, ppLayout, pLayoutDesc, NumElements );
    x_free( pSource );
    return pShader;
}

//==============================================================================
//  SHADER BLOB OPERATIONS
//==============================================================================

xbool shader_CompileToBlob( const char* pSource,
                            shader_type Type,
                            shader_blob& Blob,
                            const char* pEntryPoint,
                            const char* pProfile,
                            const char* pSourceName )
{
    ID3DBlob* pD3DBlob = NULL;
    ID3DBlob* pErrorBlob = NULL;
    
    if( !pEntryPoint )
        pEntryPoint = "main";
    
    if( !pProfile )
        pProfile = shader_GetDefaultProfile( Type );

    if( !shader_CompileShaderInternal( pSource, pSourceName, pEntryPoint, pProfile, &pD3DBlob, &pErrorBlob ) )
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

    xbool Result = shader_CompileToBlob( pSource, Type, Blob, pEntryPoint, pProfile, pFileName );
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

void* shader_CreateShaderFromBlob( const shader_blob& Blob, shader_type Type )
{
    if( !Blob.pData || Blob.Size == 0 )
    {
        x_DebugMsg( "ShaderMgr: Invalid blob for shader creation\n" );
        ASSERT(FALSE);
        return NULL;
    }
    
    void* pShader = NULL;
    HRESULT hr = E_FAIL;
    
    switch( Type )
    {
        case SHADER_TYPE_VERTEX:
            hr = g_pd3dDevice->CreateVertexShader( Blob.pData, Blob.Size, NULL, (ID3D11VertexShader**)&pShader );
            break;
            
        case SHADER_TYPE_PIXEL:
            hr = g_pd3dDevice->CreatePixelShader( Blob.pData, Blob.Size, NULL, (ID3D11PixelShader**)&pShader );
            break;
            
        case SHADER_TYPE_GEOMETRY:
            hr = g_pd3dDevice->CreateGeometryShader( Blob.pData, Blob.Size, NULL, (ID3D11GeometryShader**)&pShader );
            break;
            
        case SHADER_TYPE_COMPUTE:
            hr = g_pd3dDevice->CreateComputeShader( Blob.pData, Blob.Size, NULL, (ID3D11ComputeShader**)&pShader );
            break;
            
        default:
            ASSERT(FALSE);
            return NULL;
    }
    
    if( FAILED(hr) )
    {
        x_DebugMsg( "ShaderMgr: Failed to create shader from blob, HRESULT 0x%08X\n", hr );
        ASSERT(FALSE);
        return NULL;
    }
    
    x_DebugMsg( "ShaderMgr: Shader created from blob successfully\n" );
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
        ASSERT(FALSE);
        return NULL;
    }

    if( !pLayoutDesc || NumElements == 0 )
    {
        x_DebugMsg( "ShaderMgr: Invalid layout description\n" );
        ASSERT(FALSE);
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
        ASSERT(FALSE);
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

ID3D11Buffer* shader_CreateConstantBuffer( s32 Size )
{
    if( !g_pd3dDevice )
        return FALSE;

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
        ASSERT(FALSE);
        return NULL;
    }

    x_DebugMsg( "ShaderMgr: Constant buffer created (%d bytes)\n", (s32)Size );
    return pBuffer;
}

//==============================================================================

void shader_UpdateConstantBuffer( ID3D11Buffer* pBuffer, const void* pData, s32 Size )
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
        ASSERT(FALSE);
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