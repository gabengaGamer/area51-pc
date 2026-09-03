//==============================================================================
//
//  ShaderTool.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "ShaderTool.hpp"

#include <errno.h>
#include <stdio.h>

#if defined( TARGET_WINDOWS )
#include <direct.h>
#include <windows.h>
#else
#include <sys/stat.h>
#endif

//==============================================================================
//  STATIC FUNCTIONS
//==============================================================================

static
s32 FindOption( command_line& CommandLine, const char* pName )
{
    xstring Name( pName );
    return CommandLine.FindOption( Name );
}

//==============================================================================

static
xbool IsSlash( char C )
{
    return (C == '/') || (C == '\\');
}

//==============================================================================

static 
xbool CreateDirectoryOne( const xstring& Path )
{
    if( Path.IsEmpty() )
        return TRUE;

#if defined( TARGET_WINDOWS )
    DWORD Attrib = GetFileAttributesA( (const char*)Path );
    if( (Attrib != INVALID_FILE_ATTRIBUTES) && (Attrib & FILE_ATTRIBUTE_DIRECTORY) )
        return TRUE;

    if( _mkdir( (const char*)Path ) == 0 )
        return TRUE;

    Attrib = GetFileAttributesA( (const char*)Path );
    return ((Attrib != INVALID_FILE_ATTRIBUTES) && (Attrib & FILE_ATTRIBUTE_DIRECTORY));
#else
    struct stat Info;
    if( stat( (const char*)Path, &Info ) == 0 )
        return S_ISDIR( Info.st_mode ) ? TRUE : FALSE;

    if( mkdir( (const char*)Path, 0755 ) == 0 )
        return TRUE;

    if( errno != EEXIST )
        return FALSE;

    return (stat( (const char*)Path, &Info ) == 0) && S_ISDIR( Info.st_mode );
#endif
}

//==============================================================================

static 
const char* TargetFolder( const char* pTarget )
{
    if( x_stricmp( pTarget, "vulkan" ) == 0 )
        return "vulkan";

    if( x_stricmp( pTarget, "d3d12" ) == 0 )
        return "d3d12";

    if( x_stricmp( pTarget, "metal" ) == 0 )
        return "metal";

    // Should never get here! How?
    x_printf( "Error: Unknown target '%s' in TargetFolder\n", pTarget );
    return "";
}

//==============================================================================

static 
const char* TargetExt( const char* pTarget )
{
    if( x_stricmp( pTarget, "vulkan" ) == 0 )
        return "spv";

    if( x_stricmp( pTarget, "d3d12" ) == 0 )
        return "dxil";

    if( x_stricmp( pTarget, "metal" ) == 0 )
        return "metal";

    // Should never get here! How?
    x_printf( "Error: Unknown target '%s' in TargetExt\n", pTarget );
    return "";
}

//==============================================================================

static 
xstring BlobFileName( const shader_entry& Shader, const char* pExt )
{
    return xstring( (const char*)xfs( "%s.%s.%s", (const char*)Shader.Name,
                                                  (const char*)Shader.StageShort,
                                                  pExt ) );
}

//==============================================================================

static 
xstring MetaFileName( const shader_entry& Shader )
{
    return xstring( (const char*)xfs( "%s.%s.meta", (const char*)Shader.Name,
                                                   (const char*)Shader.StageShort ) );
}

//==============================================================================

static
xstring EcsFileName( const shader_entry& Shader )
{
    return xstring( (const char*)xfs( "%s.%s.ecs", (const char*)Shader.Name,
                                                  (const char*)Shader.StageShort ) );
}

//==============================================================================

static
xbool EcsAppendZeros( xarray<byte>& Data, u32 Count )
{
    const u32 OldCount = (u32)Data.GetCount();
    if( Count > (0x7FFFFFFFu - OldCount) )
        return FALSE;

    Data.SetCount( (s32)(OldCount + Count) );
    if( Count )
        x_memset( Data.GetPtr() + OldCount, 0, Count );
    return TRUE;
}

//==============================================================================

static
xbool EcsAppendBytes( xarray<byte>& Data, const void* pBytes, u32 Count )
{
    const u32 Offset = (u32)Data.GetCount();
    if( !EcsAppendZeros( Data, Count ) )
        return FALSE;

    if( Count )
        x_memcpy( Data.GetPtr() + Offset, pBytes, Count );
    return TRUE;
}

//==============================================================================

static
xbool EcsAlignData( xarray<byte>& Data, u32 Alignment )
{
    const u32 Count = (u32)Data.GetCount();
    u32 Aligned = 0;
    if( Alignment == 4 )
        Aligned = (u32)ALIGN_4( Count );
    else if( Alignment == 16 )
        Aligned = (u32)ALIGN_16( Count );
    else
        return FALSE;

    if( Aligned < Count )
        return FALSE;

    return EcsAppendZeros( Data, Aligned - Count );
}

//==============================================================================

static
u32 EcsAddString( xarray<byte>&          Strings,
                  xarray<xstring>&       Values,
                  xarray<u32>&           Offsets,
                  const char*            pValue )
{
    if( !pValue || !pValue[0] )
        return 0;

    for( s32 i=0; i<Values.GetCount(); i++ )
    {
        if( x_strcmp( Values[i], pValue ) == 0 )
            return Offsets[i];
    }

    const s32 Length = x_strlen( pValue ) + 1;
    const u32 Offset = (u32)Strings.GetCount();
    if( !EcsAppendBytes( Strings, pValue, (u32)Length ) )
        return 0;

    Values.Append( xstring( pValue ) );
    Offsets.Append( Offset );
    return Offset;
}

//==============================================================================

static
xbool EcsShaderStage( const shader_entry& Shader, u32& Stage )
{
    if( Shader.Stage == "vertex" )
        Stage = ECS_SHADER_STAGE_VERTEX;
    else if( (Shader.Stage == "pixel") || (Shader.Stage == "fragment") )
        Stage = ECS_SHADER_STAGE_FRAGMENT;
    else if( Shader.Stage == "compute" )
        Stage = ECS_SHADER_STAGE_COMPUTE;
    else
        return FALSE;

    return TRUE;
}

//==============================================================================

static
u32 EcsBindingKind( shader_reflection_binding_type Type )
{
    switch( Type )
    {
        case SHADER_REFLECT_SAMPLED_TEXTURE: return ECS_BINDING_SAMPLED_TEXTURE;
        case SHADER_REFLECT_UNIFORM_BUFFER:  return ECS_BINDING_UNIFORM_BUFFER;
        case SHADER_REFLECT_STORAGE_TEXTURE: return ECS_BINDING_STORAGE_TEXTURE;
        case SHADER_REFLECT_STORAGE_BUFFER:  return ECS_BINDING_STORAGE_BUFFER;
        default:                             return 0xFFFFFFFFu;
    }
}

//==============================================================================

static
xbool BuildEcsMeta( const shader_entry&                         Shader,
                    const shader_resource_counts&              Resources,
                    const xarray<shader_reflection_binding>&   Bindings,
                    xarray<byte>&                              Meta )
{
    u32 Stage = 0;
    if( !EcsShaderStage( Shader, Stage ) )
        return FALSE;

    xarray<byte>    BindingData;
    xarray<byte>    Strings;
    xarray<xstring> StringValues;
    xarray<u32>     StringOffsets;

    if( !EcsAppendZeros( Strings, 1 ) )
        return FALSE;

    const u32 NameOffset  = EcsAddString( Strings, StringValues, StringOffsets, Shader.Name );
    const u32 EntryOffset = EcsAddString( Strings, StringValues, StringOffsets, Shader.Entry );
    if( (NameOffset == 0) || (EntryOffset == 0) )
        return FALSE;

    u32 BindingCount = 0;
    for( u32 Type=SHADER_REFLECT_SAMPLED_TEXTURE; Type<=SHADER_REFLECT_STORAGE_BUFFER; Type++ )
    {
        for( s32 i=0; i<Bindings.GetCount(); i++ )
        {
            const shader_reflection_binding& Binding = Bindings[i];
            if( Binding.Type != (shader_reflection_binding_type)Type )
                continue;

            const u32 Kind       = EcsBindingKind( Binding.Type );
            const u32 Name       = EcsAddString( Strings, StringValues, StringOffsets, Binding.Name );
            const u32 RecordBase = (u32)BindingData.GetCount();
            if( (Kind == 0xFFFFFFFFu) || (Name == 0) || (Binding.Count == 0) ||
                !EcsAppendZeros( BindingData, ECS_BINDING_RECORD_SIZE ) )
            {
                return FALSE;
            }

            x_store_le32( BindingData.GetPtr() + RecordBase + ECS_BINDING_KIND,        Kind );
            x_store_le32( BindingData.GetPtr() + RecordBase + ECS_BINDING_SLOT,        Binding.Slot );
            x_store_le32( BindingData.GetPtr() + RecordBase + ECS_BINDING_COUNT,       Binding.Count );
            x_store_le32( BindingData.GetPtr() + RecordBase + ECS_BINDING_NAME_OFFSET, Name );
            BindingCount++;
        }
    }

    Meta.Clear();
    if( !EcsAppendZeros( Meta, ECS_META_HEADER_SIZE ) )
        return FALSE;

    const u32 BindingsOffset = (u32)Meta.GetCount();
    if( !EcsAppendBytes( Meta, BindingData.GetPtr(), (u32)BindingData.GetCount() ) ||
        !EcsAlignData( Meta, 4 ) )
    {
        return FALSE;
    }

    const u32 StringsOffset = (u32)Meta.GetCount();
    if( !EcsAppendBytes( Meta, Strings.GetPtr(), (u32)Strings.GetCount() ) )
        return FALSE;

    byte* pMeta = Meta.GetPtr();
    x_store_le32( pMeta + ECS_META_HEADER_SIZE_FIELD,      ECS_META_HEADER_SIZE );
    x_store_le32( pMeta + ECS_META_INTERFACE_ID,           0 );
    x_store_le32( pMeta + ECS_META_STAGE,                  Stage );
    x_store_le32( pMeta + ECS_META_BINDING_ABI,            ECS_BINDING_ABI_V1 );
    x_store_le32( pMeta + ECS_META_FLAGS,                  0 );
    x_store_le32( pMeta + ECS_META_SAMPLER_COUNT,          Resources.SamplerCount );
    x_store_le32( pMeta + ECS_META_UNIFORM_COUNT,          Resources.UniformBufferCount );
    x_store_le32( pMeta + ECS_META_STORAGE_TEXTURE_COUNT,  Resources.StorageTextureCount );
    x_store_le32( pMeta + ECS_META_STORAGE_BUFFER_COUNT,   Resources.StorageBufferCount );
    x_store_le32( pMeta + ECS_META_BINDING_COUNT,          BindingCount );
    x_store_le32( pMeta + ECS_META_BINDING_STRIDE,         ECS_BINDING_RECORD_SIZE );
    x_store_le32( pMeta + ECS_META_BINDINGS_OFFSET,        BindingsOffset );
    x_store_le32( pMeta + ECS_META_STRINGS_OFFSET,         StringsOffset );
    x_store_le32( pMeta + ECS_META_STRINGS_SIZE,           (u32)Strings.GetCount() );
    x_store_le32( pMeta + ECS_META_NAME_OFFSET,            NameOffset );
    x_store_le32( pMeta + ECS_META_ENTRY_OFFSET,           EntryOffset );
    return TRUE;
}

//==============================================================================

static
u32 EcsTargetFormat( const xstring& Target )
{
    if( Target == "vulkan" ) return ECS_SHADER_FORMAT_SPIRV;
    if( Target == "d3d12" )  return ECS_SHADER_FORMAT_DXIL;
    if( Target == "metal" )  return ECS_SHADER_FORMAT_METALLIB;
    return 0;
}

//==============================================================================

static
xbool ValidateEcsBytecode( u32 Format, const byte* pCode, u32 CodeSize )
{
    if( !pCode || !CodeSize )
        return FALSE;

    if( Format == ECS_SHADER_FORMAT_SPIRV )
    {
        return (CodeSize >= 20) &&
               ((CodeSize & 3) == 0) &&
               (x_load_le32( pCode ) == 0x07230203u) &&
               (x_load_le32( pCode + 16 ) == 0);
    }

    if( Format == ECS_SHADER_FORMAT_DXIL )
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
        for( u32 i=0; i<PartCount; i++ )
        {
            const u32 PartOffset = x_load_le32( pCode + 32 + (i * 4) );
            if( (PartOffset < PartTableEnd) || (PartOffset > (CodeSize - 8)) )
                return FALSE;

            const u32 PartSize = x_load_le32( pCode + PartOffset + 4 );
            if( PartSize > (CodeSize - PartOffset - 8) )
                return FALSE;
        }
        return TRUE;
    }

    return (Format == ECS_SHADER_FORMAT_METALLIB);
}

//==============================================================================

static
xbool BuildEcsCode( const shader_compile_output& Output, xarray<byte>& CodeChunk )
{
    const u32 Format = EcsTargetFormat( Output.Target );
    if( !Format || !ValidateEcsBytecode( Format, Output.Code.GetPtr(), (u32)Output.Code.GetCount() ) )
    {
        x_printf( "Error: Invalid %s bytecode for ECS output\n", (const char*)Output.Target );
        return FALSE;
    }

    CodeChunk.Clear();
    if( !EcsAppendZeros( CodeChunk, ECS_CODE_HEADER_SIZE ) ||
        !EcsAppendBytes( CodeChunk, Output.Code.GetPtr(), (u32)Output.Code.GetCount() ) )
    {
        return FALSE;
    }

    byte* pCode = CodeChunk.GetPtr();
    x_store_le32( pCode + ECS_CODE_HEADER_SIZE_FIELD, ECS_CODE_HEADER_SIZE );
    x_store_le32( pCode + ECS_CODE_FORMAT,            Format );
    x_store_le32( pCode + ECS_CODE_FLAGS,             0 );
    x_store_le32( pCode + ECS_CODE_VARIANT_ID,        0 );
    x_store_le32( pCode + ECS_CODE_INTERFACE_ID,      0 );
    x_store_le32( pCode + ECS_CODE_ENTRY_OFFSET,      0 );
    x_store_le32( pCode + ECS_CODE_DATA_OFFSET,       ECS_CODE_HEADER_SIZE );
    x_store_le32( pCode + ECS_CODE_DATA_SIZE,         (u32)Output.Code.GetCount() );
    return TRUE;
}

//==============================================================================

static
void WriteEcsChunkDesc( xarray<byte>& Container,
                        u32           Index,
                        u32           Type,
                        u16           Major,
                        u16           Minor,
                        u32           Flags,
                        u32           Offset,
                        u32           Size )
{
    byte* pDesc = Container.GetPtr() + ECS_HEADER_SIZE + (Index * ECS_CHUNK_DESC_SIZE);
    x_store_le32( pDesc + ECS_CHUNK_TYPE,         Type );
    x_store_le16( pDesc + ECS_CHUNK_MAJOR,        Major );
    x_store_le16( pDesc + ECS_CHUNK_MINOR,        Minor );
    x_store_le32( pDesc + ECS_CHUNK_FLAGS,        Flags );
    x_store_le32( pDesc + ECS_CHUNK_OFFSET,       Offset );
    x_store_le32( pDesc + ECS_CHUNK_STORED_SIZE,  Size );
    x_store_le32( pDesc + ECS_CHUNK_DECODED_SIZE, Size );
    x_store_le32( pDesc + ECS_CHUNK_CRC32,        x_chksum( Container.GetPtr() + Offset, (s32)Size ) );
    x_store_le32( pDesc + ECS_CHUNK_RESERVED,     0 );
}

//==============================================================================

static
xbool BuildEcsContainer( const shader_script&                         Script,
                         const shader_entry&                          Shader,
                         const shader_resource_counts&                Resources,
                         const xarray<shader_reflection_binding>&     Bindings,
                         const xarray<shader_compile_output>&         Outputs,
                         xarray<byte>&                                Container )
{
    if( Script.BindingModel != SHADER_BINDING_SDL )
    {
        x_printf( "Error: ECS v1 requires binding_model=sdl for shader %s\n",
                  (const char*)Shader.Name );
        return FALSE;
    }

    if( (Outputs.GetCount() <= 0) || ((Outputs.GetCount() + 1) > ECS_MAX_CHUNK_COUNT) )
        return FALSE;

    const u32 ChunkCount = (u32)Outputs.GetCount() + 1;
    Container.Clear();
    if( !EcsAppendZeros( Container, ECS_HEADER_SIZE + (ChunkCount * ECS_CHUNK_DESC_SIZE) ) ||
        !EcsAlignData( Container, ECS_CHUNK_ALIGNMENT ) )
    {
        return FALSE;
    }

    xarray<byte> Meta;
    if( !BuildEcsMeta( Shader, Resources, Bindings, Meta ) )
        return FALSE;

    const u32 MetaOffset = (u32)Container.GetCount();
    if( !EcsAppendBytes( Container, Meta.GetPtr(), (u32)Meta.GetCount() ) )
        return FALSE;
    WriteEcsChunkDesc( Container, 0, ecs_ChunkMeta(), ECS_META_MAJOR, ECS_META_MINOR,
                       ECS_CHUNK_REQUIRED, MetaOffset, (u32)Meta.GetCount() );

    u32 CodeIndex = 1;
    for( u32 Format=ECS_SHADER_FORMAT_SPIRV; Format<=ECS_SHADER_FORMAT_PRIVATE; Format++ )
    {
        for( s32 i=0; i<Outputs.GetCount(); i++ )
        {
            if( EcsTargetFormat( Outputs[i].Target ) != Format )
                continue;

            if( !EcsAlignData( Container, ECS_CHUNK_ALIGNMENT ) )
                return FALSE;

            xarray<byte> Code;
            if( !BuildEcsCode( Outputs[i], Code ) )
                return FALSE;

            const u32 CodeOffset = (u32)Container.GetCount();
            if( !EcsAppendBytes( Container, Code.GetPtr(), (u32)Code.GetCount() ) )
                return FALSE;
            WriteEcsChunkDesc( Container, CodeIndex++, ecs_ChunkCode(), ECS_CODE_MAJOR, ECS_CODE_MINOR,
                               0, CodeOffset, (u32)Code.GetCount() );
        }
    }

    if( CodeIndex != ChunkCount )
        return FALSE;

    byte* pHeader = Container.GetPtr();
    pHeader[ECS_HEADER_MAGIC + 0] = 'E';
    pHeader[ECS_HEADER_MAGIC + 1] = 'C';
    pHeader[ECS_HEADER_MAGIC + 2] = 'S';
    pHeader[ECS_HEADER_MAGIC + 3] = 0;
    x_store_le16( pHeader + ECS_HEADER_MAJOR,           ECS_CONTAINER_MAJOR );
    x_store_le16( pHeader + ECS_HEADER_MINOR,           ECS_CONTAINER_MINOR );
    x_store_le32( pHeader + ECS_HEADER_HEADER_SIZE,     ECS_HEADER_SIZE );
    x_store_le32( pHeader + ECS_HEADER_FILE_SIZE,       (u32)Container.GetCount() );
    x_store_le32( pHeader + ECS_HEADER_CHUNK_COUNT,     ChunkCount );
    x_store_le32( pHeader + ECS_HEADER_CHUNK_DESC_SIZE, ECS_CHUNK_DESC_SIZE );
    x_store_le32( pHeader + ECS_HEADER_FLAGS,           0 );
    x_store_le32( pHeader + ECS_HEADER_RESERVED,        0 );
    return TRUE;
}

//==============================================================================

static
xbool ValidateBuiltEcs( const xarray<byte>& Container )
{
    const u32 Size = (u32)Container.GetCount();
    if( Size < ECS_HEADER_SIZE )
        return FALSE;

    const byte* pData = Container.GetPtr();
    if( (pData[0] != 'E') || (pData[1] != 'C') || (pData[2] != 'S') || pData[3] ||
        (x_load_le16( pData + ECS_HEADER_MAJOR ) != ECS_CONTAINER_MAJOR) ||
        (x_load_le16( pData + ECS_HEADER_MINOR ) != ECS_CONTAINER_MINOR) ||
        (x_load_le32( pData + ECS_HEADER_HEADER_SIZE ) != ECS_HEADER_SIZE) ||
        (x_load_le32( pData + ECS_HEADER_FILE_SIZE ) != Size) ||
        (x_load_le32( pData + ECS_HEADER_CHUNK_DESC_SIZE ) != ECS_CHUNK_DESC_SIZE) )
    {
        return FALSE;
    }

    const u32 ChunkCount = x_load_le32( pData + ECS_HEADER_CHUNK_COUNT );
    if( (ChunkCount < 2) || (ChunkCount > ECS_MAX_CHUNK_COUNT) ||
        (ChunkCount > ((Size - ECS_HEADER_SIZE) / ECS_CHUNK_DESC_SIZE)) )
    {
        return FALSE;
    }

    u32 MetaCount = 0;
    u32 CodeCount = 0;
    for( u32 i=0; i<ChunkCount; i++ )
    {
        const byte* pDesc = pData + ECS_HEADER_SIZE + (i * ECS_CHUNK_DESC_SIZE);
        const u32 Offset  = x_load_le32( pDesc + ECS_CHUNK_OFFSET );
        const u32 Stored  = x_load_le32( pDesc + ECS_CHUNK_STORED_SIZE );
        const u32 Decoded = x_load_le32( pDesc + ECS_CHUNK_DECODED_SIZE );
        if( !Stored || (Stored != Decoded) || (Offset & (ECS_CHUNK_ALIGNMENT - 1)) ||
            (Offset > Size) || (Stored > (Size - Offset)) ||
            (x_load_le32( pDesc + ECS_CHUNK_CRC32 ) != x_chksum( pData + Offset, (s32)Stored )) )
        {
            return FALSE;
        }

        const u32 Type = x_load_le32( pDesc + ECS_CHUNK_TYPE );
        if( Type == ecs_ChunkMeta() )
            MetaCount++;
        else if( Type == ecs_ChunkCode() )
        {
            CodeCount++;
            if( Stored < ECS_CODE_HEADER_SIZE )
                return FALSE;

            const byte* pCodeChunk = pData + Offset;
            const u32 CodeOffset = x_load_le32( pCodeChunk + ECS_CODE_DATA_OFFSET );
            const u32 CodeSize   = x_load_le32( pCodeChunk + ECS_CODE_DATA_SIZE );
            const u32 Format     = x_load_le32( pCodeChunk + ECS_CODE_FORMAT );
            if( (CodeOffset < ECS_CODE_HEADER_SIZE) || (CodeOffset > Stored) ||
                (CodeSize != (Stored - CodeOffset)) ||
                !ValidateEcsBytecode( Format, pCodeChunk + CodeOffset, CodeSize ) )
            {
                return FALSE;
            }
        }
        else if( x_load_le32( pDesc + ECS_CHUNK_FLAGS ) & ECS_CHUNK_REQUIRED )
        {
            return FALSE;
        }
    }

    return (MetaCount == 1) && (CodeCount > 0);
}

//==============================================================================

static
void RemoveTempOutputs( const xarray<shader_compile_output>& Outputs )
{
    for( s32 i=0; i<Outputs.GetCount(); i++ )
        RemoveFile( Outputs[i].TempPdb );
}

//==============================================================================

static
xbool RemovePublishedPdbs( const xarray<shader_compile_output>& Outputs )
{
    for( s32 i=0; i<Outputs.GetCount(); i++ )
    {
        if( (Outputs[i].Target == "d3d12") && !RemoveFile( Outputs[i].Pdb ) )
            return FALSE;
    }
    return TRUE;
}

//==============================================================================

static
xbool CommitPdbOutputs( const xarray<shader_compile_output>& Outputs )
{
    for( s32 i=0; i<Outputs.GetCount(); i++ )
    {
        const shader_compile_output& Output = Outputs[i];
        if( (Output.Target == "d3d12") && Output.HasPdb &&
            !ReplaceFile( Output.TempPdb, Output.Pdb ) )
        {
            x_printf( "Error: Failed to replace PDB %s\n", (const char*)Output.Pdb );
            return FALSE;
        }
    }
    return TRUE;
}

//==============================================================================
//  FUNCTIONS
//==============================================================================

xstring ToLower( const xstring& Text )
{
    xstring Result = Text;
    Result.MakeLower();
    return Result;
}

//==============================================================================

xbool IsAbsolutePath( const xstring& Path )
{
    if( (Path.GetLength() >= 2) && (Path[1] == ':') )
        return TRUE;

    if( (Path.GetLength() >= 1) && IsSlash( Path[0] ) )
        return TRUE;

    return FALSE;
}

//==============================================================================

xstring JoinPath( const xstring& Path, const xstring& File )
{
    if( Path.IsEmpty() )
        return File;

    if( File.IsEmpty() )
        return Path;

    if( IsSlash( Path[Path.GetLength() - 1] ) )
        return Path + File;

#if defined( TARGET_WINDOWS )
    return Path + "\\" + File;
#else
    return Path + "/" + File;
#endif
}

//==============================================================================

xstring NormalizePath( const xstring& Path )
{
#if defined( TARGET_WINDOWS )
    char  Buffer[MAX_PATH];
    DWORD Length = GetFullPathNameA( (const char*)Path, MAX_PATH, Buffer, NULL );

    if( (Length > 0) && (Length < MAX_PATH) )
        return xstring( Buffer );
#endif

    return Path;
}

//==============================================================================

xstring MakeRelativePath( const xstring& BasePath, const xstring& Path )
{
    if( IsAbsolutePath( Path ) )
        return NormalizePath( Path );

    return NormalizePath( JoinPath( BasePath, Path ) );
}

//==============================================================================

xstring GetDirectory( const xstring& PathName )
{
    xstring Path;
    xstring File;

    command_line::SplitPath( PathName, Path, File );
    return Path;
}

//==============================================================================

xbool EnsureParentDir( const xstring& PathName )
{
    xstring Path = GetDirectory( PathName );
    if( Path.IsEmpty() )
        return TRUE;

    xstring Current;
    s32     Start = 0;

    if( (Path.GetLength() >= 2) && (Path[1] == ':') )
    {
        Current = Path.Left( 2 );
        Start   = 2;
    }
    else if( (Path.GetLength() > 0) && IsSlash( Path[0] ) )
    {
        Current = Path.Left( 1 );
        Start   = 1;
    }

    for( s32 i=Start; i<Path.GetLength(); i++ )
    {
        Current += Path[i];

        if( IsSlash( Path[i] ) )
        {
            if( !CreateDirectoryOne( Current ) )
                return FALSE;
        }
    }

    return CreateDirectoryOne( Path );
}

//==============================================================================

xbool LoadBinaryFile( const xstring& PathName, xarray<byte>& Data )
{
    X_FILE* pFile = x_fopen( PathName, "rb" );
    if( !pFile )
        return FALSE;

    const s32 Size = x_flength( pFile );
    if( Size <= 0 )
    {
        x_fclose( pFile );
        return FALSE;
    }

    Data.SetCount( Size );
    const s32 Read = x_fread( Data.GetPtr(), 1, Size, pFile );
    x_fclose( pFile );

    return (Read == Size);
}

//==============================================================================

xbool WriteBinaryFile( const xstring& PathName, const void* pData, s32 Size )
{
    if( !EnsureParentDir( PathName ) )
        return FALSE;

    X_FILE* pFile = x_fopen( PathName, "wb" );
    if( !pFile )
        return FALSE;

    const s32 Written = x_fwrite( pData, Size, 1, pFile );
    x_fclose( pFile );

    return (Written == 1);
}

//==============================================================================

xbool RemoveFile( const xstring& PathName )
{
#if defined( TARGET_WINDOWS )
    if( DeleteFileA( (const char*)PathName ) )
        return TRUE;

    const DWORD Error = GetLastError();
    return (Error == ERROR_FILE_NOT_FOUND) || (Error == ERROR_PATH_NOT_FOUND);
#else
    if( remove( (const char*)PathName ) == 0 )
        return TRUE;

    X_FILE* pFile = x_fopen( PathName, "rb" );
    if( !pFile )
        return TRUE;

    x_fclose( pFile );
    return FALSE;
#endif
}

//==============================================================================

xbool ReplaceFile( const xstring& TempPath, const xstring& FinalPath )
{
#if defined( TARGET_WINDOWS )
    return MoveFileExA( (const char*)TempPath,
                        (const char*)FinalPath,
                        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH );
#else
    RemoveFile( FinalPath );
    return (rename( (const char*)TempPath, (const char*)FinalPath ) == 0);
#endif
}

//==============================================================================

xstring BlobPath( const shader_script& Script,
                  const shader_entry&  Shader,
                  const char*          pTarget )
{
    return JoinPath( JoinPath( JoinPath( Script.OutRoot, Script.Config ), TargetFolder( pTarget ) ),
                     BlobFileName( Shader, TargetExt( pTarget ) ) );
}

//==============================================================================

xstring EcsPath( const shader_script& Script, const shader_entry& Shader )
{
    return JoinPath( JoinPath( Script.OutRoot, Script.Config ), EcsFileName( Shader ) );
}

//==============================================================================

xstring MetaPath( const shader_script& Script, const shader_entry& Shader )
{
    return JoinPath( JoinPath( Script.OutRoot, Script.Config ), MetaFileName( Shader ) );
}

//==============================================================================

xstring PdbPath( const shader_script& Script, const shader_entry& Shader )
{
    return JoinPath( JoinPath( JoinPath( Script.OutRoot, Script.Config ), "d3d12" ),
                     BlobFileName( Shader, "pdb" ) );
}

//==============================================================================

xstring TempPathFor( const xstring& PathName )
{
#if defined( TARGET_WINDOWS )
    return xstring( (const char*)xfs( "%s.tmp.%u", (const char*)PathName,
                                                  (u32)GetCurrentProcessId() ) );
#else
    return PathName + ".tmp";
#endif
}

//==============================================================================

const char* BindingModelName( shader_binding_model Model )
{
    if( Model == SHADER_BINDING_SDL )
        return "sdl";
    
    if( Model == SHADER_BINDING_NATIVE )
        return "native";    

    // Should never get here! How?
    x_printf( "Error: Unknown model '%d' in BindingModelName\n", Model );
    return "";
}

//==============================================================================

void DisplayHelp( void )
{
    x_printf( "\n" );
    x_printf( "ShaderTool (c)2002 Inevitable Entertainment Inc.\n" );
    x_printf( "\n" );
    x_printf( "  usage:\n" );
    x_printf( "         ShaderTool [-clean] [-v] <script.shaderscript>\n" );
    x_printf( "\n" );
    x_printf( "options:\n" );
    x_printf( "         -clean      - Remove old blobs before writing new ones\n" );
    x_printf( "         -v          - Verbose output\n" );
    x_printf( "\n" );
}

//==============================================================================

xbool ParseCommandLine( s32 argc, char** argv, compile_options& Options )
{
    command_line CommandLine;

    CommandLine.AddOptionDef( "CLEAN"   );
    CommandLine.AddOptionDef( "V"       );

    xbool NeedHelp = CommandLine.Parse( argc, argv );
    if( NeedHelp || (CommandLine.GetNumArguments() == 0) )
        return FALSE;

    Options.Clean   = (FindOption( CommandLine, "CLEAN" ) != -1);
    Options.Verbose = (FindOption( CommandLine, "V"     ) != -1);

    for( s32 i=0; i<CommandLine.GetNumArguments(); i++ )
        Options.Scripts.Append( CommandLine.GetArgument( i ) );

    return TRUE;
}
//==============================================================================

xbool CompileShader( const shader_script&  Script,
                     const shader_entry&   Shader,
                     const compile_options& Options )
{
    x_printf( "Shader: %s\n", (const char*)Shader.Name );

    if( Options.Clean )
    {
        RemoveFile( EcsPath( Script, Shader ) );
        RemoveFile( MetaPath( Script, Shader ) );
        RemoveFile( BlobPath( Script, Shader, "vulkan" ) );
        RemoveFile( BlobPath( Script, Shader, "d3d12" ) );
        RemoveFile( BlobPath( Script, Shader, "metal" ) );        
        RemoveFile( PdbPath( Script, Shader ) );
    }

    // TODO: GS: Implement this.
    if( Script.TargetMask & SHADER_TARGET_METAL )
    {
        x_printf( "Error: Metal target is not implemented yet\n" );
        return FALSE;
    }

    shader_resource_counts       Resources;
    xarray<shader_reflection_binding>
                                 Bindings;
    xarray<shader_compile_output> Outputs;

    if( Script.TargetMask & SHADER_TARGET_D3D12 )
    {
        shader_compile_output& Output = Outputs.Append();
        if( !CompileDxcTarget( Script, Shader, Options, "d3d12", Output ) )
        {
            RemoveTempOutputs( Outputs );
            return FALSE;
        }

        Resources = Output.Resources;
        Bindings  = Output.Bindings;
    }
    else
    {
        // D3D12 not requested as an output target; still need its
        // reflection data to populate the .meta resource counts.
        if( !ReflectDxcShader( Script, Shader, Options, Resources, Bindings ) )
            return FALSE;
    }

    if( Script.TargetMask & SHADER_TARGET_VULKAN )
    {
        shader_compile_output& Output = Outputs.Append();
        if( !CompileDxcTarget( Script, Shader, Options, "vulkan", Output ) )
        {
            RemoveTempOutputs( Outputs );
            return FALSE;
        }
    }

    xarray<byte> EcsData;
    if( !BuildEcsContainer( Script, Shader, Resources, Bindings, Outputs, EcsData ) ||
        !ValidateBuiltEcs( EcsData ) )
    {
        x_printf( "Error: Failed to build ECS container for %s\n", (const char*)Shader.Name );
        RemoveTempOutputs( Outputs );
        return FALSE;
    }

    const xstring Ecs     = EcsPath( Script, Shader );
    const xstring TempEcs = TempPathFor( Ecs );
    RemoveFile( TempEcs );
    if( !WriteBinaryFile( TempEcs, EcsData.GetPtr(), EcsData.GetCount() ) )
    {
        x_printf( "Error: Failed to write ECS output %s\n", (const char*)TempEcs );
        RemoveTempOutputs( Outputs );
        RemoveFile( TempEcs );
        return FALSE;
    }

    xarray<byte> WrittenEcs;
    if( !LoadBinaryFile( TempEcs, WrittenEcs ) || !ValidateBuiltEcs( WrittenEcs ) )
    {
        x_printf( "Error: ECS post-write validation failed for %s\n", (const char*)TempEcs );
        RemoveTempOutputs( Outputs );
        RemoveFile( TempEcs );
        return FALSE;
    }

    if( !RemovePublishedPdbs( Outputs ) )
    {
        x_printf( "Error: Failed to remove stale shader PDB\n" );
        RemoveTempOutputs( Outputs );
        RemoveFile( TempEcs );
        return FALSE;
    }

    if( !ReplaceFile( TempEcs, Ecs ) )
    {
        x_printf( "Error: Failed to replace ECS output %s\n", (const char*)Ecs );
        RemoveTempOutputs( Outputs );
        RemoveFile( TempEcs );
        return FALSE;
    }

    if( !CommitPdbOutputs( Outputs ) )
    {
        RemoveTempOutputs( Outputs );
        return FALSE;
    }

    if( Options.Verbose )
        x_printf( "  ECS -> %s\n", (const char*)Ecs );

    return TRUE;
}

//==============================================================================

xbool ProcessScript( const char* pFileName, const compile_options& Options )
{
    shader_script Script;

    if( !ParseScript( pFileName, Script ) )
        return FALSE;

    x_printf( "Processing script '%s'\n", pFileName );
    x_printf( "Config:        %s\n", (const char*)Script.Config );
    x_printf( "Binding model: %s\n", BindingModelName( Script.BindingModel ) );
    x_printf( "OutRoot:       %s\n", (const char*)Script.OutRoot );

    xbool Success = TRUE;
    for( s32 i=0; i<Script.Shaders.GetCount(); i++ )
        Success = CompileShader( Script, Script.Shaders[i], Options ) && Success;

    return Success;
}

//==============================================================================

int main( int argc, char** argv )
{
    compile_options Options;

    x_Init( argc, argv );

    if( !ParseCommandLine( argc, argv, Options ) )
    {
        DisplayHelp();
        x_Kill();
        return 10;
    }

    xbool Success = TRUE;
    for( s32 i=0; i<Options.Scripts.GetCount(); i++ )
        Success = ProcessScript( (const char*)Options.Scripts[i], Options ) && Success;

    x_Kill();
    return Success ? 0 : 1;
}
