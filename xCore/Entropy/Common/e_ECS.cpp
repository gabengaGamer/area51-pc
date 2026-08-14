//==============================================================================
//
//  e_ECS.cpp
//
//  Entropy Compiled Shader (.ecs) reader.
//
//==============================================================================

#include "e_ECS.hpp"

#ifndef X_STDIO_HPP
#include "x_stdio.hpp"
#endif

//==============================================================================
//  TYPES
//==============================================================================

struct ecs_chunk
{
    u32 Type;
    u16 Major;
    u16 Minor;
    u32 Flags;
    u32 Offset;
    u32 StoredSize;
    u32 DecodedSize;
    u32 CRC32;

    ecs_chunk( void ) :
        Type       ( 0 ),
        Major      ( 0 ),
        Minor      ( 0 ),
        Flags      ( 0 ),
        Offset     ( 0 ),
        StoredSize ( 0 ),
        DecodedSize( 0 ),
        CRC32      ( 0 )
    {
    }
};

//------------------------------------------------------------------------------

struct ecs_code_info
{
    shader_format Format;
    u32           EntryOffset;
    u32           CodeOffset;
    u32           CodeSize;

    ecs_code_info( void ) :
        Format     ( SHADER_FORMAT_UNKNOWN ),
        EntryOffset( 0 ),
        CodeOffset ( 0 ),
        CodeSize   ( 0 )
    {
    }
};

//==============================================================================
//  HELPERS
//==============================================================================

static
xbool ecs_IsAbsolutePath( const char* pPath )
{
    if( !pPath || !pPath[0] )
        return FALSE;

    if( pPath[1] == ':' )
        return TRUE;

    return (pPath[0] == '/') || (pPath[0] == '\\');
}

//==============================================================================

static
xbool ecs_ReadAt( X_FILE* pFile, u32 Offset, void* pData, u32 Size )
{
    if( !pFile || !pData || !Size ||
        (Offset > 0x7FFFFFFFu) || (Size > 0x7FFFFFFFu) )
    {
        return FALSE;
    }

    if( x_fseek( pFile, (s32)Offset, X_SEEK_SET ) != 0 )
        return FALSE;

    return (x_fread( pData, 1, (s32)Size, pFile ) == (s32)Size);
}

//==============================================================================

static
xbool ecs_ReadChunk( X_FILE* pFile, const ecs_chunk& Chunk, xarray<byte>& Data )
{
    if( Chunk.StoredSize > 0x7FFFFFFFu )
        return FALSE;

    Data.Clear();
    Data.SetCount( (s32)Chunk.StoredSize );
    if( !ecs_ReadAt( pFile, Chunk.Offset, Data.GetPtr(), Chunk.StoredSize ) )
        return FALSE;

    return (x_chksum( Data.GetPtr(), Data.GetCount() ) == Chunk.CRC32);
}

//==============================================================================

static
xbool ecs_ReadString( const byte* pStrings,
                      u32         StringsSize,
                      u32         Offset,
                      xbool       Required,
                      xstring&    Value )
{
    Value.Clear();
    if( Offset == 0 )
        return !Required;

    if( !pStrings || (Offset >= StringsSize) )
        return FALSE;

    u32 End = Offset;
    while( (End < StringsSize) && pStrings[End] )
        End++;

    if( (End == Offset) || (End >= StringsSize) )
        return FALSE;

    Value = (const char*)(pStrings + Offset);
    return TRUE;
}

//==============================================================================

static
xbool ecs_ToShaderStage( u32 Value, shader_stage& Stage )
{
    switch( Value )
    {
        case ECS_SHADER_STAGE_VERTEX:
        {
            Stage = SHADER_STAGE_VERTEX;
        }
        return TRUE;

        case ECS_SHADER_STAGE_FRAGMENT:
        {
            Stage = SHADER_STAGE_PIXEL;
        }
        return TRUE;

        case ECS_SHADER_STAGE_COMPUTE:
        {
            Stage = SHADER_STAGE_COMPUTE;
        }
        return TRUE;

        default:
        {
        }
        return FALSE;
    }
}

//==============================================================================

static
xbool ecs_ToBindingKind( u32 Value, shader_binding_kind& Kind )
{
    switch( Value )
    {
        case ECS_BINDING_SAMPLED_TEXTURE:
        {
            Kind = SHADER_BINDING_SAMPLED_TEXTURE;
        }
        return TRUE;

        case ECS_BINDING_UNIFORM_BUFFER:
        {
            Kind = SHADER_BINDING_UNIFORM_BUFFER;
        }
        return TRUE;

        case ECS_BINDING_STORAGE_TEXTURE:
        {
            Kind = SHADER_BINDING_STORAGE_TEXTURE;
        }
        return TRUE;

        case ECS_BINDING_STORAGE_BUFFER:
        {
            Kind = SHADER_BINDING_STORAGE_BUFFER;
        }
        return TRUE;

        default:
        {
        }
        return FALSE;
    }
}

//==============================================================================

static
xbool ecs_ToShaderFormat( u32 Value, shader_format& Format )
{
    switch( Value )
    {
        case ECS_SHADER_FORMAT_SPIRV:
        {
            Format = SHADER_FORMAT_SPIRV;
        }
        return TRUE;

        case ECS_SHADER_FORMAT_DXIL:
        {
            Format = SHADER_FORMAT_DXIL;
        }
        return TRUE;

        case ECS_SHADER_FORMAT_METALLIB:
        {
            Format = SHADER_FORMAT_METALLIB;
        }
        return TRUE;

        case ECS_SHADER_FORMAT_MSL:
        {
            Format = SHADER_FORMAT_MSL;
        }
        return TRUE;

        case ECS_SHADER_FORMAT_PRIVATE:
        {
            Format = SHADER_FORMAT_PRIVATE;
        }
        return TRUE;

        default:
        {
        }
        return FALSE;
    }
}

//==============================================================================

static
u32 ecs_GetResourceLimit( const shader_resource_counts& Resources,
                          shader_binding_kind             Kind )
{
    switch( Kind )
    {
        case SHADER_BINDING_SAMPLED_TEXTURE:
        {
            return Resources.SamplerCount;
        }

        case SHADER_BINDING_UNIFORM_BUFFER:
        {
            return Resources.UniformBufferCount;
        }

        case SHADER_BINDING_STORAGE_TEXTURE:
        {
            return Resources.StorageTextureCount;
        }

        case SHADER_BINDING_STORAGE_BUFFER:
        {
            return Resources.StorageBufferCount;
        }

        default:
        {
        }
        return 0;
    }
}

//==============================================================================

static
xbool ecs_ValidateBindings( const ecs_shader_container& Container )
{
    for( s32 i = 0; i < Container.Bindings.GetCount(); i++ )
    {
        const ecs_shader_binding& Binding = Container.Bindings[i];
        const u32 Limit = ecs_GetResourceLimit( Container.Resources, Binding.Kind );
        if( (Binding.Count == 0) || (Binding.Slot >= Limit) ||
            (Binding.Count > (Limit - Binding.Slot)) )
        {
            return FALSE;
        }

        for( s32 j = 0; j < i; j++ )
        {
            const ecs_shader_binding& Existing = Container.Bindings[j];
            if( (Existing.Kind == Binding.Kind) && (Existing.Name == Binding.Name) )
                return FALSE;
        }
    }

    return TRUE;
}

//==============================================================================

static
xbool ecs_ParseMeta( const xarray<byte>&    MetaBytes,
                     ecs_shader_container&  Container,
                     xarray<byte>&          Strings )
{
    const u32 MetaSize = (u32)MetaBytes.GetCount();
    if( MetaSize < ECS_META_HEADER_SIZE )
        return FALSE;

    const byte* pMeta = MetaBytes.GetPtr();
    if( (x_load_le32( pMeta + ECS_META_HEADER_SIZE_FIELD ) != ECS_META_HEADER_SIZE) ||
        (x_load_le32( pMeta + ECS_META_INTERFACE_ID ) != 0) ||
        (x_load_le32( pMeta + ECS_META_BINDING_ABI ) != ECS_BINDING_ABI_V1) ||
        (x_load_le32( pMeta + ECS_META_FLAGS ) != 0) ||
        !ecs_ToShaderStage( x_load_le32( pMeta + ECS_META_STAGE ), Container.Stage ) )
    {
        return FALSE;
    }

    Container.Resources.SamplerCount        = x_load_le32( pMeta + ECS_META_SAMPLER_COUNT );
    Container.Resources.UniformBufferCount  = x_load_le32( pMeta + ECS_META_UNIFORM_COUNT );
    Container.Resources.StorageTextureCount = x_load_le32( pMeta + ECS_META_STORAGE_TEXTURE_COUNT );
    Container.Resources.StorageBufferCount  = x_load_le32( pMeta + ECS_META_STORAGE_BUFFER_COUNT );

    const u32 BindingCount   = x_load_le32( pMeta + ECS_META_BINDING_COUNT );
    const u32 BindingStride  = x_load_le32( pMeta + ECS_META_BINDING_STRIDE );
    const u32 BindingsOffset = x_load_le32( pMeta + ECS_META_BINDINGS_OFFSET );
    const u32 StringsOffset  = x_load_le32( pMeta + ECS_META_STRINGS_OFFSET );
    const u32 StringsSize    = x_load_le32( pMeta + ECS_META_STRINGS_SIZE );

    if( (BindingStride != ECS_BINDING_RECORD_SIZE) ||
        (BindingsOffset < ECS_META_HEADER_SIZE) || (BindingsOffset & 3) ||
        (BindingsOffset > MetaSize) || (StringsOffset < BindingsOffset) ||
        (StringsOffset > MetaSize) || (StringsSize != (MetaSize - StringsOffset)) ||
        (BindingCount > ((StringsOffset - BindingsOffset) / BindingStride)) ||
        (StringsSize == 0) || (pMeta[StringsOffset] != 0) )
    {
        return FALSE;
    }

    Strings.SetCount( (s32)StringsSize );
    x_memcpy( Strings.GetPtr(), pMeta + StringsOffset, StringsSize );
    if( !ecs_ReadString( Strings.GetPtr(), StringsSize,
                         x_load_le32( pMeta + ECS_META_NAME_OFFSET ), FALSE, Container.Name ) ||
        !ecs_ReadString( Strings.GetPtr(), StringsSize,
                         x_load_le32( pMeta + ECS_META_ENTRY_OFFSET ), TRUE, Container.Entry ) )
    {
        return FALSE;
    }

    Container.Bindings.SetCount( (s32)BindingCount );
    for( u32 i = 0; i < BindingCount; i++ )
    {
        const byte* pRecord = pMeta + BindingsOffset + (i * BindingStride);
        ecs_shader_binding& Binding = Container.Bindings[(s32)i];
        if( !ecs_ToBindingKind( x_load_le32( pRecord + ECS_BINDING_KIND ), Binding.Kind ) ||
            !ecs_ReadString( Strings.GetPtr(), StringsSize,
                             x_load_le32( pRecord + ECS_BINDING_NAME_OFFSET ), TRUE, Binding.Name ) )
        {
            return FALSE;
        }

        Binding.Slot  = x_load_le32( pRecord + ECS_BINDING_SLOT );
        Binding.Count = x_load_le32( pRecord + ECS_BINDING_COUNT );
    }

    return ecs_ValidateBindings( Container );
}

//==============================================================================

static
xbool ecs_ValidateBytecode( shader_format Format, const byte* pCode, u32 CodeSize )
{
    if( !pCode || !CodeSize )
        return FALSE;

    if( Format == SHADER_FORMAT_SPIRV )
    {
        return (CodeSize >= 20) && ((CodeSize & 3) == 0) &&
               (x_load_le32( pCode ) == 0x07230203u) &&
               (x_load_le32( pCode + 16 ) == 0);
    }

    if( Format == SHADER_FORMAT_DXIL )
    {
        if( (CodeSize < 32) ||
            (pCode[0] != 'D') || (pCode[1] != 'X') ||
            (pCode[2] != 'B') || (pCode[3] != 'C') ||
            (x_load_le32( pCode + 24 ) != CodeSize) )
        {
            return FALSE;
        }

        const u32 PartCount = x_load_le32( pCode + 28 );
        if( PartCount > ((CodeSize - 32) / 4) )
            return FALSE;

        const u32 PartTableEnd = 32 + (PartCount * 4);
        for( u32 i = 0; i < PartCount; i++ )
        {
            const u32 PartOffset = x_load_le32( pCode + 32 + (i * 4) );
            if( (PartOffset < PartTableEnd) || (PartOffset > (CodeSize - 8)) )
                return FALSE;

            const u32 PartSize = x_load_le32( pCode + PartOffset + 4 );
            if( PartSize > (CodeSize - PartOffset - 8) )
                return FALSE;
        }
    }

    return TRUE;
}

//==============================================================================

static
xbool ecs_ParseCodeHeader( const byte*              pHeader,
                           u32                      ChunkSize,
                           const xarray<byte>&      Strings,
                           ecs_code_info&           Info )
{
    if( !pHeader || (ChunkSize < ECS_CODE_HEADER_SIZE) ||
        (x_load_le32( pHeader + ECS_CODE_HEADER_SIZE_FIELD ) != ECS_CODE_HEADER_SIZE) ||
        (x_load_le32( pHeader + ECS_CODE_FLAGS ) != 0) ||
        (x_load_le32( pHeader + ECS_CODE_VARIANT_ID ) != 0) ||
        (x_load_le32( pHeader + ECS_CODE_INTERFACE_ID ) != 0) ||
        !ecs_ToShaderFormat( x_load_le32( pHeader + ECS_CODE_FORMAT ), Info.Format ) )
    {
        return FALSE;
    }

    Info.EntryOffset = x_load_le32( pHeader + ECS_CODE_ENTRY_OFFSET );
    Info.CodeOffset  = x_load_le32( pHeader + ECS_CODE_DATA_OFFSET );
    Info.CodeSize    = x_load_le32( pHeader + ECS_CODE_DATA_SIZE );
    if( (Info.CodeOffset < ECS_CODE_HEADER_SIZE) || (Info.CodeOffset & 15) ||
        (Info.CodeOffset > ChunkSize) || (Info.CodeSize == 0) ||
        (Info.CodeSize != (ChunkSize - Info.CodeOffset)) )
    {
        return FALSE;
    }

    if( Info.EntryOffset )
    {
        xstring Entry;
        if( !ecs_ReadString( Strings.GetPtr(), (u32)Strings.GetCount(),
                             Info.EntryOffset, TRUE, Entry ) )
        {
            return FALSE;
        }
    }

    return TRUE;
}

//==============================================================================

static
xbool ecs_ReadContainer( X_FILE* pFile, const char* pFileName, ecs_shader_container& Container )
{
    const s32 SignedFileSize = x_flength( pFile );
    if( SignedFileSize < ECS_HEADER_SIZE )
        return FALSE;
    const u32 FileSize = (u32)SignedFileSize;

    byte Header[ECS_HEADER_SIZE];
    if( !ecs_ReadAt( pFile, 0, Header, ECS_HEADER_SIZE ) ||
        (Header[0] != 'E') || (Header[1] != 'C') || (Header[2] != 'S') || Header[3] ||
        (x_load_le16( Header + ECS_HEADER_MAJOR ) != ECS_CONTAINER_MAJOR) ||
        (x_load_le16( Header + ECS_HEADER_MINOR ) != ECS_CONTAINER_MINOR) ||
        (x_load_le32( Header + ECS_HEADER_HEADER_SIZE ) != ECS_HEADER_SIZE) ||
        (x_load_le32( Header + ECS_HEADER_FILE_SIZE ) != FileSize) ||
        (x_load_le32( Header + ECS_HEADER_CHUNK_DESC_SIZE ) != ECS_CHUNK_DESC_SIZE) ||
        (x_load_le32( Header + ECS_HEADER_FLAGS ) != 0) ||
        (x_load_le32( Header + ECS_HEADER_RESERVED ) != 0) )
    {
        x_DebugMsg( "ECS: invalid header '%s'\n", pFileName );
        return FALSE;
    }

    const u32 ChunkCount = x_load_le32( Header + ECS_HEADER_CHUNK_COUNT );
    if( !ChunkCount || (ChunkCount > ECS_MAX_CHUNK_COUNT) ||
        (ChunkCount > ((FileSize - ECS_HEADER_SIZE) / ECS_CHUNK_DESC_SIZE)) )
    {
        x_DebugMsg( "ECS: invalid chunk count in '%s'\n", pFileName );
        return FALSE;
    }

    const u32 DirectorySize = ChunkCount * ECS_CHUNK_DESC_SIZE;
    const u32 PayloadStart  = (u32)ALIGN_16( ECS_HEADER_SIZE + DirectorySize );
    if( PayloadStart > FileSize )
        return FALSE;

    xarray<byte> Directory;
    Directory.SetCount( (s32)DirectorySize );
    if( !ecs_ReadAt( pFile, ECS_HEADER_SIZE, Directory.GetPtr(), DirectorySize ) )
        return FALSE;

    xarray<ecs_chunk> Chunks;
    Chunks.SetCount( (s32)ChunkCount );
    s32 MetaIndex = -1;
    for( u32 i = 0; i < ChunkCount; i++ )
    {
        const byte* pDesc = Directory.GetPtr() + (i * ECS_CHUNK_DESC_SIZE);
        ecs_chunk& Chunk = Chunks[(s32)i];
        Chunk.Type        = x_load_le32( pDesc + ECS_CHUNK_TYPE );
        Chunk.Major       = x_load_le16( pDesc + ECS_CHUNK_MAJOR );
        Chunk.Minor       = x_load_le16( pDesc + ECS_CHUNK_MINOR );
        Chunk.Flags       = x_load_le32( pDesc + ECS_CHUNK_FLAGS );
        Chunk.Offset      = x_load_le32( pDesc + ECS_CHUNK_OFFSET );
        Chunk.StoredSize  = x_load_le32( pDesc + ECS_CHUNK_STORED_SIZE );
        Chunk.DecodedSize = x_load_le32( pDesc + ECS_CHUNK_DECODED_SIZE );
        Chunk.CRC32       = x_load_le32( pDesc + ECS_CHUNK_CRC32 );

        if( (x_load_le32( pDesc + ECS_CHUNK_RESERVED ) != 0) ||
            (Chunk.Flags & ~ECS_CHUNK_REQUIRED) || !Chunk.StoredSize ||
            (Chunk.StoredSize != Chunk.DecodedSize) ||
            (Chunk.Offset < PayloadStart) || (Chunk.Offset & (ECS_CHUNK_ALIGNMENT - 1)) ||
            (Chunk.Offset > FileSize) || (Chunk.StoredSize > (FileSize - Chunk.Offset)) )
        {
            x_DebugMsg( "ECS: invalid chunk %u in '%s'\n", i, pFileName );
            return FALSE;
        }

        for( u32 j = 0; j < i; j++ )
        {
            const ecs_chunk& Other = Chunks[(s32)j];
            if( (Chunk.Offset < (Other.Offset + Other.StoredSize)) &&
                (Other.Offset < (Chunk.Offset + Chunk.StoredSize)) )
            {
                x_DebugMsg( "ECS: overlapping chunks in '%s'\n", pFileName );
                return FALSE;
            }
        }

        if( Chunk.Type == ecs_ChunkMeta() )
        {
            if( (MetaIndex >= 0) || (Chunk.Major != ECS_META_MAJOR) ||
                (Chunk.Minor != ECS_META_MINOR) || (Chunk.Flags != ECS_CHUNK_REQUIRED) )
            {
                return FALSE;
            }
            MetaIndex = (s32)i;
        }
        else if( Chunk.Type == ecs_ChunkCode() )
        {
            if( (Chunk.Major != ECS_CODE_MAJOR) ||
                (Chunk.Minor != ECS_CODE_MINOR) || Chunk.Flags )
            {
                return FALSE;
            }
        }
        else if( Chunk.Flags & ECS_CHUNK_REQUIRED )
        {
            x_DebugMsg( "ECS: unknown required chunk in '%s'\n", pFileName );
            return FALSE;
        }
    }

    if( MetaIndex < 0 )
        return FALSE;

    xarray<byte> MetaBytes;
    xarray<byte> Strings;
    if( !ecs_ReadChunk( pFile, Chunks[MetaIndex], MetaBytes ) ||
        !ecs_ParseMeta( MetaBytes, Container, Strings ) )
    {
        x_DebugMsg( "ECS: invalid metadata in '%s'\n", pFileName );
        return FALSE;
    }

    for( u32 i = 0; i < ChunkCount; i++ )
    {
        if( Chunks[(s32)i].Type != ecs_ChunkCode() )
            continue;

        xarray<byte> CodeChunk;
        if( !ecs_ReadChunk( pFile, Chunks[(s32)i], CodeChunk ) )
        {
            x_DebugMsg( "ECS: bytecode CRC failed in '%s'\n", pFileName );
            return FALSE;
        }

        ecs_code_info Info;
        if( !ecs_ParseCodeHeader( CodeChunk.GetPtr(),
                                  (u32)CodeChunk.GetCount(),
                                  Strings,
                                  Info ) )
        {
            x_DebugMsg( "ECS: invalid code header in '%s'\n", pFileName );
            return FALSE;
        }

        for( s32 j = 0; j < Container.Codes.GetCount(); j++ )
        {
            if( Container.Codes[j].Format == Info.Format )
                return FALSE;
        }

        ecs_shader_code& Code = Container.Codes.Append();
        Code.Format = Info.Format;
        if( Info.EntryOffset &&
            !ecs_ReadString( Strings.GetPtr(), (u32)Strings.GetCount(),
                             Info.EntryOffset, TRUE, Code.Entry ) )
        {
            return FALSE;
        }

        Code.Code.SetCount( (s32)Info.CodeSize );
        x_memcpy( Code.Code.GetPtr(), CodeChunk.GetPtr() + Info.CodeOffset, Info.CodeSize );
        if( !ecs_ValidateBytecode( Code.Format, Code.Code.GetPtr(), Info.CodeSize ) )
        {
            x_DebugMsg( "ECS: invalid bytecode in '%s'\n", pFileName );
            return FALSE;
        }
    }

    if( Container.Codes.GetCount() == 0 )
        return FALSE;

    return TRUE;
}

//==============================================================================
//  FUNCTIONS
//==============================================================================

xbool ecs_LoadShaderContainer( const char* pFileName, ecs_shader_container& Container )
{
    Container = ecs_shader_container();
    if( !pFileName || !pFileName[0] || ecs_IsAbsolutePath( pFileName ) )
    {
        x_DebugMsg( "ECS: shader path must be relative to the mounted SHADERS filesystem\n" );
        return FALSE;
    }

    X_FILE* pFile = x_fopen( pFileName, "rb" );
    if( !pFile )
    {
        x_DebugMsg( "ECS: failed to open shader file '%s'\n", pFileName );
        return FALSE;
    }

    const xbool Result = ecs_ReadContainer( pFile, pFileName, Container );
    x_fclose( pFile );
    if( !Result )
        Container = ecs_shader_container();

    return Result;
}

//==============================================================================

const ecs_shader_code* ecs_FindShaderCode( const ecs_shader_container& Container,
                                           shader_format                 Format )
{
    for( s32 i = 0; i < Container.Codes.GetCount(); i++ )
    {
        if( Container.Codes[i].Format == Format )
            return &Container.Codes[i];
    }

    return NULL;
}

//==============================================================================
