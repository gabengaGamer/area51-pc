//==============================================================================
//
//  e_Shader.hpp
//
//  Shader declarations for explicit PSO render backends.
//
//==============================================================================

#ifndef E_SHADER_HPP
#define E_SHADER_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#ifndef X_TYPES_HPP
#include "x_types.hpp"
#endif

//==============================================================================
//  ENUMS
//==============================================================================

enum shader_stage
{
    SHADER_STAGE_VERTEX = 0,
    SHADER_STAGE_PIXEL,
    SHADER_STAGE_COMPUTE,
    SHADER_STAGE_COUNT
};

//------------------------------------------------------------------------------

enum shader_format
{
    SHADER_FORMAT_UNKNOWN = 0,
    SHADER_FORMAT_SPIRV,
    SHADER_FORMAT_DXIL,
    SHADER_FORMAT_MSL,
    SHADER_FORMAT_METALLIB,
    SHADER_FORMAT_PRIVATE
};

//------------------------------------------------------------------------------

enum shader_topology
{
    SHADER_TOPOLOGY_UNDEFINED = 0,
    SHADER_TOPOLOGY_POINT_LIST,
    SHADER_TOPOLOGY_LINE_LIST,
    SHADER_TOPOLOGY_LINE_STRIP,
    SHADER_TOPOLOGY_TRIANGLE_LIST,
    SHADER_TOPOLOGY_TRIANGLE_STRIP
};

//------------------------------------------------------------------------------

enum shader_vertex_format
{
    SHADER_VERTEX_FORMAT_FLOAT1 = 0,
    SHADER_VERTEX_FORMAT_FLOAT2,
    SHADER_VERTEX_FORMAT_FLOAT3,
    SHADER_VERTEX_FORMAT_FLOAT4,
    SHADER_VERTEX_FORMAT_UBYTE4N_BGRA,
    SHADER_VERTEX_FORMAT_UINT1,
    SHADER_VERTEX_FORMAT_COUNT
};

//==============================================================================
//  STRUCTURES
//==============================================================================

struct shader_backend;
struct shader_resource_backend;
struct rstate_sampler;

//------------------------------------------------------------------------------

struct shader_blob
{
    const void* pData;
    u32         Size;

    shader_blob( void ) :
        pData( NULL ),
        Size ( 0 )
    {
    }

    shader_blob( const void* pBlobData, u32 BlobSize ) :
        pData( pBlobData ),
        Size ( BlobSize )
    {
    }
};

//------------------------------------------------------------------------------

struct shader_resource_counts
{
    u32 SamplerCount;
    u32 StorageTextureCount;
    u32 StorageBufferCount;
    u32 UniformBufferCount;

    shader_resource_counts( void ) :
        SamplerCount       ( 0 ),
        StorageTextureCount( 0 ),
        StorageBufferCount ( 0 ),
        UniformBufferCount ( 0 )
    {
    }
};

//------------------------------------------------------------------------------

enum shader_binding_kind
{
    SHADER_BINDING_SAMPLED_TEXTURE = 0,
    SHADER_BINDING_UNIFORM_BUFFER,
    SHADER_BINDING_STORAGE_TEXTURE,
    SHADER_BINDING_STORAGE_BUFFER
};

//------------------------------------------------------------------------------

struct shader_binding_desc
{
    shader_binding_kind Kind;
    const char*         pName;
    u32                 Slot;
    u32                 Count;

    shader_binding_desc( void ) :
        Kind ( SHADER_BINDING_SAMPLED_TEXTURE ),
        pName( NULL ),
        Slot ( 0 ),
        Count( 0 )
    {
    }
};

//------------------------------------------------------------------------------

struct shader_desc
{
    shader_stage           Stage;
    shader_format          Format;
    shader_blob            Blob;
    const char*            pEntryPoint;
    shader_resource_counts Resources;
    const shader_binding_desc*
                           pBindings;
    u32                    BindingCount;
    const char*            pDebugName;

    shader_desc( void ) :
        Stage      ( SHADER_STAGE_VERTEX ),
        Format     ( SHADER_FORMAT_UNKNOWN ),
        Blob       (),
        pEntryPoint( NULL ),
        Resources  (),
        pBindings  ( NULL ),
        BindingCount( 0 ),
        pDebugName ( NULL )
    {
    }
};

//------------------------------------------------------------------------------

struct shader_file_desc
{
    const char*            pFileName;
    shader_stage           Stage;
    shader_format          Format;
    const char*            pEntryPoint;
    shader_resource_counts Resources;
    const shader_binding_desc*
                           pBindings;
    u32                    BindingCount;
    const char*            pDebugName;

    shader_file_desc( void ) :
        pFileName  ( NULL ),
        Stage      ( SHADER_STAGE_VERTEX ),
        Format     ( SHADER_FORMAT_UNKNOWN ),
        pEntryPoint( NULL ),
        Resources  (),
        pBindings  ( NULL ),
        BindingCount( 0 ),
        pDebugName ( NULL )
    {
    }
};

//------------------------------------------------------------------------------

struct shader_ecs_desc
{
    const char* pFileName;
    const char* pDebugName;

    shader_ecs_desc( void ) :
        pFileName ( NULL ),
        pDebugName( NULL )
    {
    }
};

//------------------------------------------------------------------------------

struct shader
{
    shader_stage    Stage;
    shader_format   Format;
    shader_backend* pBackend;

    shader( void ) :
        Stage   ( SHADER_STAGE_VERTEX ),
        Format  ( SHADER_FORMAT_UNKNOWN ),
        pBackend( NULL )
    {
    }

    operator xbool( void ) const { return pBackend != NULL; }
};

//==============================================================================
//  PIPELINE SHADER STATE
//==============================================================================

//------------------------------------------------------------------------------

struct shader_vertex_buffer_desc
{
    u32 Slot;
    u32 Stride;
    u32 InstanceStepRate;
    xbool bPerInstance;

    shader_vertex_buffer_desc( void ) :
        Slot            ( 0 ),
        Stride          ( 0 ),
        InstanceStepRate( 0 ),
        bPerInstance    ( FALSE )
    {
    }
};

//------------------------------------------------------------------------------

struct shader_vertex_element
{
    u32                  Location;
    u32                  InputSlot;
    shader_vertex_format Format;
    u32                  AlignedByteOffset;

    shader_vertex_element( void ) :
        Location         ( 0 ),
        InputSlot        ( 0 ),
        Format           ( SHADER_VERTEX_FORMAT_FLOAT3 ),
        AlignedByteOffset( 0 )
    {
    }

    shader_vertex_element( u32                  ShaderLocation,
                           u32                  Slot,
                           shader_vertex_format VertexFormat,
                           u32                  Offset ) :
        Location         ( ShaderLocation ),
        InputSlot        ( Slot ),
        Format           ( VertexFormat ),
        AlignedByteOffset( Offset )
    {
    }
};

//------------------------------------------------------------------------------

struct shader_pipeline_desc
{
    const shader*                    pVertexShader;
    const shader*                    pPixelShader;
    const shader_vertex_buffer_desc* pVertexBuffers;
    u32                              VertexBufferCount;
    const shader_vertex_element*     pInputElements;
    u32                              InputElementCount;
    shader_topology                  Topology;

    shader_pipeline_desc( void ) :
        pVertexShader    ( NULL ),
        pPixelShader     ( NULL ),
        pVertexBuffers   ( NULL ),
        VertexBufferCount( 0 ),
        pInputElements   ( NULL ),
        InputElementCount( 0 ),
        Topology         ( SHADER_TOPOLOGY_UNDEFINED )
    {
    }
};

//------------------------------------------------------------------------------

struct shader_resource
{
    shader_resource_backend* pBackend;

    shader_resource( void ) : pBackend( NULL ) {}
    operator xbool( void ) const { return pBackend != NULL; }
};

//==============================================================================
//  RESOURCE BINDING
//==============================================================================

struct shader_uniform_binding
{
    shader_stage Stage;
    u32          Slot;
    const void*  pData;
    u32          Size;

    shader_uniform_binding( void ) :
        Stage( SHADER_STAGE_VERTEX ),
        Slot ( 0 ),
        pData( NULL ),
        Size ( 0 )
    {
    }

    shader_uniform_binding( shader_stage BindStage,
                            u32          BindSlot,
                            const void*  pUniformData,
                            u32          UniformSize ) :
        Stage( BindStage ),
        Slot ( BindSlot ),
        pData( pUniformData ),
        Size ( UniformSize )
    {
    }
};

//------------------------------------------------------------------------------

struct shader_sampler_binding
{
    shader_stage            Stage;
    u32                     Slot;
    const shader_resource*  pTexture;
    const rstate_sampler*   pSampler;

    shader_sampler_binding( void ) :
        Stage   ( SHADER_STAGE_PIXEL ),
        Slot    ( 0 ),
        pTexture( NULL ),
        pSampler( NULL )
    {
    }

    shader_sampler_binding( shader_stage           BindStage,
                            u32                    BindSlot,
                            const shader_resource* pBindTexture,
                            const rstate_sampler*  pBindSampler ) :
        Stage   ( BindStage ),
        Slot    ( BindSlot ),
        pTexture( pBindTexture ),
        pSampler( pBindSampler )
    {
    }
};

//------------------------------------------------------------------------------

struct shader_storage_texture_binding
{
    shader_stage           Stage;
    u32                    Slot;
    const shader_resource* pTexture;

    shader_storage_texture_binding( void ) :
        Stage   ( SHADER_STAGE_PIXEL ),
        Slot    ( 0 ),
        pTexture( NULL )
    {
    }

    shader_storage_texture_binding( shader_stage           BindStage,
                                    u32                    BindSlot,
                                    const shader_resource* pBindTexture ) :
        Stage   ( BindStage ),
        Slot    ( BindSlot ),
        pTexture( pBindTexture )
    {
    }
};

//------------------------------------------------------------------------------

struct shader_storage_buffer_binding
{
    shader_stage           Stage;
    u32                    Slot;
    const shader_resource* pBuffer;

    shader_storage_buffer_binding( void ) :
        Stage  ( SHADER_STAGE_PIXEL ),
        Slot   ( 0 ),
        pBuffer( NULL )
    {
    }

    shader_storage_buffer_binding( shader_stage           BindStage,
                                   u32                    BindSlot,
                                   const shader_resource* pBindBuffer ) :
        Stage  ( BindStage ),
        Slot   ( BindSlot ),
        pBuffer( pBindBuffer )
    {
    }
};

//------------------------------------------------------------------------------

struct shader_binding_set_desc
{
    const shader_uniform_binding*         pUniforms;
    u32                                   UniformCount;
    const shader_sampler_binding*         pSamplers;
    u32                                   SamplerCount;
    const shader_storage_texture_binding* pStorageTextures;
    u32                                   StorageTextureCount;
    const shader_storage_buffer_binding*  pStorageBuffers;
    u32                                   StorageBufferCount;

    shader_binding_set_desc( void ) :
        pUniforms          ( NULL ),
        UniformCount       ( 0 ),
        pSamplers          ( NULL ),
        SamplerCount       ( 0 ),
        pStorageTextures   ( NULL ),
        StorageTextureCount( 0 ),
        pStorageBuffers    ( NULL ),
        StorageBufferCount ( 0 )
    {
    }
};

//==============================================================================
//  SHADER API
//==============================================================================

void                shader_Init             ( void );
void                shader_Kill             ( void );

xbool               shader_Create           ( shader&            Shader,
                                              const shader_desc& Desc );
xbool               shader_LoadFromFile     ( shader&                 Shader,
                                              const shader_file_desc& Desc );
xbool               shader_LoadFromEcs      ( shader&                Shader,
                                              const shader_ecs_desc& Desc );
xbool               shader_LoadFromEcs      ( shader&      Shader,
                                              const char*  pEcsFileName );
void                shader_Destroy          ( shader&            Shader );

xbool               shader_PushUniformData  ( shader_stage Stage,
                                              u32          Slot,
                                              const void*  pData,
                                              u32          Size );
xbool               shader_PushUniformData  ( const shader& Shader,
                                              shader_stage  Stage,
                                              const char*   pName,
                                              const void*   pData,
                                              u32           Size );
xbool               shader_PushUniformData  ( const shader_uniform_binding& Binding );

xbool               shader_BindSampler      ( const shader_sampler_binding& Binding );
xbool               shader_BindSampler      ( const shader&          Shader,
                                              shader_stage           Stage,
                                              const char*            pName,
                                              const shader_resource* pTexture,
                                              const rstate_sampler*  pSampler );
xbool               shader_BindSamplers     ( const shader_sampler_binding* pBindings,
                                              u32                           Count );

xbool               shader_BindStorageTexture
                                            ( const shader_storage_texture_binding& Binding );
xbool               shader_BindStorageTexture
                                            ( const shader&          Shader,
                                              shader_stage           Stage,
                                              const char*            pName,
                                              const shader_resource* pTexture );
xbool               shader_BindStorageTextures
                                            ( const shader_storage_texture_binding* pBindings,
                                              u32                                   Count );

xbool               shader_BindStorageBuffer
                                            ( const shader_storage_buffer_binding& Binding );
xbool               shader_BindStorageBuffer
                                            ( const shader&          Shader,
                                              shader_stage           Stage,
                                              const char*            pName,
                                              const shader_resource* pBuffer );
xbool               shader_BindStorageBuffers
                                            ( const shader_storage_buffer_binding* pBindings,
                                              u32                                   Count );

xbool               shader_BindSet          ( const shader_binding_set_desc& Desc );

xbool               shader_FindBindingSlot  ( const shader&       Shader,
                                              shader_binding_kind Kind,
                                              const char*         pName,
                                              u32&                Slot );
xbool               shader_FindSampledTextureSlot
                                            ( const shader& Shader,
                                              const char*   pName,
                                              u32&          Slot );
xbool               shader_FindUniformSlot  ( const shader& Shader,
                                              const char*   pName,
                                              u32&          Slot );
xbool               shader_FindStorageTextureSlot
                                            ( const shader& Shader,
                                              const char*   pName,
                                              u32&          Slot );
xbool               shader_FindStorageBufferSlot
                                            ( const shader& Shader,
                                              const char*   pName,
                                              u32&          Slot );

//==============================================================================
#endif // E_SHADER_HPP
//==============================================================================
