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

struct shader_blob
{
    void*   pData;
    s32     Size;
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
//  SHADER COMPILATION FROM CODE/FILES
//==============================================================================

void*               shader_CompileShader            ( const char* pSource,
                                                      shader_type Type,
                                                      const char* pEntryPoint = NULL,
                                                      const char* pProfile = NULL,
                                                      const char* pSourceName = NULL,
                                                      ID3D11InputLayout** ppLayout = NULL,
                                                      const D3D11_INPUT_ELEMENT_DESC* pLayoutDesc = NULL,
                                                      u32 NumElements = 0 );

void*               shader_CompileShaderFromFile    ( const char* pFileName,
                                                      shader_type Type,
                                                      const char* pEntryPoint = NULL,
                                                      const char* pProfile = NULL,
                                                      ID3D11InputLayout** ppLayout = NULL,
                                                      const D3D11_INPUT_ELEMENT_DESC* pLayoutDesc = NULL,
                                                      u32 NumElements = 0 );

//==============================================================================
//  SHADER BLOB OPERATIONS
//==============================================================================

// Compile shader to blob
xbool               shader_CompileToBlob           ( const char* pSource,
                                                     shader_type Type,
                                                     shader_blob& Blob,
                                                     const char* pEntryPoint = NULL,
                                                     const char* pProfile = NULL,
                                                     const char* pSourceName = NULL );

// Compile shader from file to blob
xbool               shader_CompileFileToBlob       ( const char* pFileName,
                                                     shader_type Type,
                                                     shader_blob& Blob,
                                                     const char* pEntryPoint = NULL,
                                                     const char* pProfile = NULL );

// Create shader from blob
void*               shader_CreateShaderFromBlob     ( const shader_blob& Blob,
                                                      shader_type Type );

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
ID3D11Buffer*       shader_CreateConstantBuffer    ( s32 Size );
void                shader_UpdateConstantBuffer    ( ID3D11Buffer* pBuffer, const void* pData, s32 Size );
void                shader_ReleaseConstantBuffer   ( ID3D11Buffer* pBuffer );

//==============================================================================
#endif // D3DENG_SHADER_HPP
//==============================================================================
