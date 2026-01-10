//==============================================================================
//  
//  d3deng_shader.hpp
//  
//  Shader manager for D3DENG.
//
//==============================================================================

#ifndef D3DENG_SHADER_HPP
#define D3DENG_SHADER_HPP

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

#ifndef X_TYPES_HPP
#include "x_types.hpp"
#endif

#ifndef X_STDIO_HPP
#include "x_stdio.hpp"
#endif

#include "d3deng_private.hpp"

//==============================================================================
//  STRUCTURES
//==============================================================================

enum shader_type
{
    SHADER_TYPE_VERTEX = 0,
    SHADER_TYPE_PIXEL,
    SHADER_TYPE_GEOMETRY,
    SHADER_TYPE_COMPUTE,
    SHADER_TYPE_COUNT
};

//------------------------------------------------------------------------------

enum constant_buffer_type
{
    CB_TYPE_DYNAMIC = 0,
    CB_TYPE_DEFAULT,
    CB_TYPE_IMMUTABLE,
	CB_TYPE_COUNT
};

//------------------------------------------------------------------------------

struct shader_blob
{
    void*   pData;
    u32     Size;
};

//==============================================================================
//  SYSTEM FUNCTIONS
//==============================================================================

// Initialize shader system. Useless ? Probably...
void                shader_Init                     ( void );
void                shader_Kill                     ( void );

//==============================================================================
//  FILE I/O FUNCTIONS
//==============================================================================

// Load shader source from file
char*               shader_LoadSourceFromFile       ( const char* pFileName );

// Load/Save shader blobs from/to files
xbool               shader_LoadBlobFromFile         ( const char* pFileName, shader_blob& Blob );
xbool               shader_SaveBlobToFile           ( const char* pFileName, const shader_blob& Blob );

//==============================================================================
//  SHADER COMPILATION FROM SOURCE
//==============================================================================

// Compile shaders from source
ID3D11VertexShader* shader_CompileVertex            ( const char* pSource,
                                                      const char* pEntryPoint,
                                                      const char* pProfile = NULL,
                                                      const char* pSourceName = NULL );

ID3D11PixelShader*  shader_CompilePixel             ( const char* pSource,
                                                      const char* pEntryPoint,
                                                      const char* pProfile = NULL,
                                                      const char* pSourceName = NULL );

ID3D11GeometryShader* shader_CompileGeometry        ( const char* pSource,
                                                      const char* pEntryPoint,
                                                      const char* pProfile = NULL,
                                                      const char* pSourceName = NULL );

ID3D11ComputeShader* shader_CompileCompute          ( const char* pSource,
                                                      const char* pEntryPoint,
                                                      const char* pProfile = NULL,
                                                      const char* pSourceName = NULL );

// Compile vertex shader with input layout
ID3D11VertexShader* shader_CompileVertexWithLayout  ( const char* pSource,
                                                      ID3D11InputLayout** ppLayout,
                                                      const D3D11_INPUT_ELEMENT_DESC* pLayoutDesc,
                                                      u32 NumElements,
                                                      const char* pEntryPoint,
                                                      const char* pProfile = NULL,
                                                      const char* pSourceName = NULL );

//==============================================================================
//  SHADER COMPILATION FROM FILES
//==============================================================================

// Compile shaders from files
ID3D11VertexShader* shader_CompileVertexFromFile    ( const char* pFileName,
                                                      const char* pEntryPoint,
                                                      const char* pProfile = NULL );

ID3D11PixelShader*  shader_CompilePixelFromFile     ( const char* pFileName,
                                                      const char* pEntryPoint,
                                                      const char* pProfile = NULL );

ID3D11GeometryShader* shader_CompileGeometryFromFile( const char* pFileName,
                                                      const char* pEntryPoint,
                                                      const char* pProfile = NULL );

ID3D11ComputeShader* shader_CompileComputeFromFile  ( const char* pFileName,
                                                      const char* pEntryPoint,
                                                      const char* pProfile = NULL );

// Compile vertex shader from file with input layout
ID3D11VertexShader* shader_CompileVertexFromFileWithLayout( const char* pFileName,
                                                           ID3D11InputLayout** ppLayout,
                                                           const D3D11_INPUT_ELEMENT_DESC* pLayoutDesc,
                                                           u32 NumElements,
                                                           const char* pEntryPoint,
                                                           const char* pProfile = NULL );

//==============================================================================
//  SHADER BLOB OPERATIONS
//==============================================================================

// Compile shader to blob
xbool               shader_CompileToBlob           ( const char* pSource,
                                                     shader_type Type,
                                                     shader_blob& Blob,
                                                     const char* pEntryPoint,
                                                     const char* pProfile = NULL,
                                                     const char* pSourceName = NULL );

// Compile shader from file to blob
xbool               shader_CompileFileToBlob       ( const char* pFileName,
                                                     shader_type Type,
                                                     shader_blob& Blob,
                                                     const char* pEntryPoint,
                                                     const char* pProfile = NULL );

// Create shaders from blob
ID3D11VertexShader*   shader_CreateVertexFromBlob  ( const shader_blob& Blob );
ID3D11PixelShader*    shader_CreatePixelFromBlob   ( const shader_blob& Blob );
ID3D11GeometryShader* shader_CreateGeometryFromBlob( const shader_blob& Blob );
ID3D11ComputeShader*  shader_CreateComputeFromBlob ( const shader_blob& Blob );

// Create input layout from vertex shader blob
ID3D11InputLayout*  shader_CreateInputLayout       ( const shader_blob& VertexBlob,
                                                     const D3D11_INPUT_ELEMENT_DESC* pLayoutDesc,
                                                     u32 NumElements );

// Free shader blob
void                shader_FreeBlob                ( shader_blob& Blob );

//==============================================================================
//  STANDARD CONSTANT BUFFER UTILITIES
//==============================================================================

// Constant buffer management
ID3D11Buffer*       shader_CreateConstantBuffer    ( u32 Size, 
                                                     constant_buffer_type Type = CB_TYPE_DYNAMIC,
                                                     const void* pInitialData = NULL );										  
void                shader_UpdateConstantBuffer    ( ID3D11Buffer* pBuffer, const void* pData, u32 Size );
void                shader_ReleaseConstantBuffer   ( ID3D11Buffer* pBuffer );

//==============================================================================
#endif // D3DENG_SHADER_HPP
//==============================================================================
