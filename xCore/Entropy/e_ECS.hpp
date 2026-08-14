//==============================================================================
//
//  e_ECS.hpp
//
//  Entropy Compiled Shader (.ecs) format and runtime declarations.
//
//  NOTE: The wire declarations describe byte offsets only. ECS files are never
//  serialized by writing native C++ structures.
//
//==============================================================================

#ifndef E_ECS
#define E_ECS

//==============================================================================
//  INCLUDES
//==============================================================================

#ifndef X_PLUS_HPP
#include "x_plus.hpp"
#endif

#include "e_Shader.hpp"

#ifndef X_ARRAY_HPP
#include "x_array.hpp"
#endif

#ifndef X_STRING_HPP
#include "x_string.hpp"
#endif

//==============================================================================
//  CONTAINER CONSTANTS
//==============================================================================

enum ecs_container_constant
{
    ECS_CONTAINER_MAJOR       = 1,
    ECS_CONTAINER_MINOR       = 0,
    ECS_HEADER_SIZE           = 32,
    ECS_CHUNK_DESC_SIZE       = 32,
    ECS_CHUNK_ALIGNMENT       = 16,
    ECS_MAX_CHUNK_COUNT       = 64,

    ECS_CHUNK_REQUIRED        = (1 << 0)
};

//------------------------------------------------------------------------------

enum ecs_header_offset
{
    ECS_HEADER_MAGIC          = 0,
    ECS_HEADER_MAJOR          = 4,
    ECS_HEADER_MINOR          = 6,
    ECS_HEADER_HEADER_SIZE    = 8,
    ECS_HEADER_FILE_SIZE      = 12,
    ECS_HEADER_CHUNK_COUNT    = 16,
    ECS_HEADER_CHUNK_DESC_SIZE= 20,
    ECS_HEADER_FLAGS          = 24,
    ECS_HEADER_RESERVED       = 28
};

//------------------------------------------------------------------------------

enum ecs_chunk_desc_offset
{
    ECS_CHUNK_TYPE            = 0,
    ECS_CHUNK_MAJOR           = 4,
    ECS_CHUNK_MINOR           = 6,
    ECS_CHUNK_FLAGS           = 8,
    ECS_CHUNK_OFFSET          = 12,
    ECS_CHUNK_STORED_SIZE     = 16,
    ECS_CHUNK_DECODED_SIZE    = 20,
    ECS_CHUNK_CRC32           = 24,
    ECS_CHUNK_RESERVED        = 28
};

//==============================================================================
//  META CHUNK
//==============================================================================

enum ecs_meta_constant
{
    ECS_META_MAJOR            = 1,
    ECS_META_MINOR            = 0,
    ECS_META_HEADER_SIZE      = 64,
    ECS_BINDING_RECORD_SIZE   = 16
};

//------------------------------------------------------------------------------

enum ecs_meta_offset
{
    ECS_META_HEADER_SIZE_FIELD= 0,
    ECS_META_INTERFACE_ID     = 4,
    ECS_META_STAGE            = 8,
    ECS_META_BINDING_ABI      = 12,
    ECS_META_FLAGS            = 16,
    ECS_META_SAMPLER_COUNT    = 20,
    ECS_META_UNIFORM_COUNT    = 24,
    ECS_META_STORAGE_TEXTURE_COUNT = 28,
    ECS_META_STORAGE_BUFFER_COUNT  = 32,
    ECS_META_BINDING_COUNT    = 36,
    ECS_META_BINDING_STRIDE   = 40,
    ECS_META_BINDINGS_OFFSET  = 44,
    ECS_META_STRINGS_OFFSET   = 48,
    ECS_META_STRINGS_SIZE     = 52,
    ECS_META_NAME_OFFSET      = 56,
    ECS_META_ENTRY_OFFSET     = 60
};

//------------------------------------------------------------------------------

enum ecs_binding_offset
{
    ECS_BINDING_KIND          = 0,
    ECS_BINDING_SLOT          = 4,
    ECS_BINDING_COUNT         = 8,
    ECS_BINDING_NAME_OFFSET   = 12
};

//------------------------------------------------------------------------------

enum ecs_shader_stage
{
    ECS_SHADER_STAGE_VERTEX   = 0,
    ECS_SHADER_STAGE_FRAGMENT = 1,
    ECS_SHADER_STAGE_COMPUTE  = 2
};

//------------------------------------------------------------------------------

enum ecs_binding_abi
{
    ECS_BINDING_ABI_INVALID = 0,
    ECS_BINDING_ABI_V1      = 1
};

//------------------------------------------------------------------------------

enum ecs_binding_kind
{
    ECS_BINDING_SAMPLED_TEXTURE = 0,
    ECS_BINDING_UNIFORM_BUFFER,
    ECS_BINDING_STORAGE_TEXTURE,
    ECS_BINDING_STORAGE_BUFFER
};

//==============================================================================
//  CODE CHUNK
//==============================================================================

enum ecs_code_constant
{
    ECS_CODE_MAJOR            = 1,
    ECS_CODE_MINOR            = 0,
    ECS_CODE_HEADER_SIZE      = 32
};

//------------------------------------------------------------------------------

enum ecs_code_offset
{
    ECS_CODE_HEADER_SIZE_FIELD= 0,
    ECS_CODE_FORMAT           = 4,
    ECS_CODE_FLAGS            = 8,
    ECS_CODE_VARIANT_ID       = 12,
    ECS_CODE_INTERFACE_ID     = 16,
    ECS_CODE_ENTRY_OFFSET     = 20,
    ECS_CODE_DATA_OFFSET      = 24,
    ECS_CODE_DATA_SIZE        = 28
};

//------------------------------------------------------------------------------

enum ecs_shader_format
{
    ECS_SHADER_FORMAT_SPIRV   = 1,
    ECS_SHADER_FORMAT_DXIL,
    ECS_SHADER_FORMAT_METALLIB,
    ECS_SHADER_FORMAT_MSL,
    ECS_SHADER_FORMAT_PRIVATE
};

//==============================================================================
//  ECS IDENTIFIERS
//==============================================================================

inline u32 ecs_ChunkMeta( void ) { return x_make_fourcc( 'M', 'E', 'T', 'A' ); }
inline u32 ecs_ChunkCode( void ) { return x_make_fourcc( 'C', 'O', 'D', 'E' ); }
inline u32 ecs_ChunkComb( void ) { return x_make_fourcc( 'C', 'O', 'M', 'B' ); } // TODO
inline u32 ecs_ChunkInfo( void ) { return x_make_fourcc( 'I', 'N', 'F', 'O' ); }

//==============================================================================
//  RUNTIME TYPES
//==============================================================================

struct ecs_shader_binding
{
    shader_binding_kind Kind;
    xstring             Name;
    u32                 Slot;
    u32                 Count;

    ecs_shader_binding( void ) :
        Kind ( SHADER_BINDING_SAMPLED_TEXTURE ),
        Name (),
        Slot ( 0 ),
        Count( 0 )
    {
    }
};

//------------------------------------------------------------------------------

struct ecs_shader_code
{
    shader_format Format;
    xstring       Entry;
    xarray<byte>  Code;

    ecs_shader_code( void ) :
        Format( SHADER_FORMAT_UNKNOWN ),
        Entry (),
        Code  ()
    {
    }
};

//------------------------------------------------------------------------------

struct ecs_shader_container
{
    shader_stage           Stage;
    shader_resource_counts Resources;
    xstring                Entry;
    xstring                Name;
    xarray<ecs_shader_binding>
                           Bindings;
    xarray<ecs_shader_code>
                           Codes;

    ecs_shader_container( void ) :
        Stage    ( SHADER_STAGE_VERTEX ),
        Resources(),
        Entry    (),
        Name     (),
        Bindings (),
        Codes    ()
    {
    }
};

//==============================================================================
//  FUNCTIONS
//==============================================================================

xbool                       ecs_LoadShaderContainer( const char*              pFileName,
                                                      ecs_shader_container&     Container );
const ecs_shader_code*      ecs_FindShaderCode     ( const ecs_shader_container& Container,
                                                      shader_format              Format );

//==============================================================================
#endif // E_ECS
//==============================================================================
