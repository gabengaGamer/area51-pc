//==============================================================================
//
//  DxcCompiler.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "ShaderTool.hpp"

#if defined( TARGET_PC )
#include <windows.h>
#include <unknwn.h>
#include <objidl.h>
#include <oleauto.h>
#include <dxcapi.h>
#include <d3d12shader.h>
#include <limits.h>
#include <string>
#include <vector>
#endif

//==============================================================================

#if defined( TARGET_PC )

template <class T>
class dxc_ptr
{
public:
    dxc_ptr( void ) : m_p( NULL ) {}
    ~dxc_ptr( void ) { Release(); }

    T*  operator ->( void ) const { return m_p; }
    T*  Get( void ) const { return m_p; }
    T** Put( void ) { Release(); return &m_p; }

private:
    void Release( void )
    {
        if( m_p )
        {
            m_p->Release();
            m_p = NULL;
        }
    }

    dxc_ptr( const dxc_ptr& );
    const dxc_ptr& operator = ( const dxc_ptr& );

    T* m_p;
};

//==============================================================================
//  STATIC FUNCTIONS
//==============================================================================

static
std::wstring ToWide( const xstring& Text )
{
    const s32 Length = MultiByteToWideChar( CP_ACP,
                                           0,
                                           (const char*)Text,
                                           Text.GetLength(),
                                           NULL,
                                           0 );
    if( Length <= 0 )
        return std::wstring();

    std::wstring Result;
    Result.resize( Length );
    MultiByteToWideChar( CP_ACP,
                         0,
                         (const char*)Text,
                         Text.GetLength(),
                         &Result[0],
                         Length );

    return Result;
}

//==============================================================================

static
void SplitArgString( const xstring& Text, xarray<xstring>& Args )
{
    xstring Current;
    xbool   InQuote = FALSE;

    for( s32 i=0; i<Text.GetLength(); i++ )
    {
        const char C = Text[i];

        if( C == '"' )
        {
            InQuote = !InQuote;
        }
        else if( !InQuote && x_isspace( C ) )
        {
            if( !Current.IsEmpty() )
                Args.Append( Current );

            Current.Clear();
        }
        else
        {
            Current += C;
        }
    }

    if( !Current.IsEmpty() )
        Args.Append( Current );
}

//==============================================================================

static
void AppendArgs( xarray<xstring>& Args, const xstring& Text )
{
    if( !Text.IsEmpty() )
        SplitArgString( Text, Args );
}

//==============================================================================

static
const char* StageDefine( const xstring& Stage )
{
    if( Stage == "vertex" )
        return "A51_SHADER_STAGE_VERTEX=1";
    if( (Stage == "pixel") || (Stage == "fragment") )
        return "A51_SHADER_STAGE_PIXEL=1";
    if( Stage == "compute" )
        return "A51_SHADER_STAGE_COMPUTE=1";
    if( Stage == "geometry" )
        return "A51_SHADER_STAGE_GEOMETRY=1";
    if( (Stage == "hull") || (Stage == "tess_control") )
        return "A51_SHADER_STAGE_HULL=1";
    if( (Stage == "domain") || (Stage == "tess_evaluation") )
        return "A51_SHADER_STAGE_DOMAIN=1";
    if( Stage == "mesh" )
        return "A51_SHADER_STAGE_MESH=1";
    if( (Stage == "amplification") || (Stage == "task") )
        return "A51_SHADER_STAGE_AMPLIFICATION=1";

    return "A51_SHADER_STAGE_UNKNOWN=1";
}

//==============================================================================

static
void AppendBindingArgs( const shader_script& Script,
                        const shader_entry&  Shader,
                        xarray<xstring>&     Args )
{
    Args.Append( "-D" );
    if( Script.BindingModel == SHADER_BINDING_SDL )
        Args.Append( "A51_SHADER_BINDING_SDL=1" );
    else if( Script.BindingModel == SHADER_BINDING_NATIVE )
        Args.Append( "A51_SHADER_BINDING_NATIVE=1" );

    Args.Append( "-D" );
    Args.Append( StageDefine( Shader.Stage ) );
}

//==============================================================================

static
void BuildDxcArgs( const shader_script& Script,
                          const shader_entry&  Shader,
                          const char*          pTarget,
                          const xstring&       D3D12PdbPath,
                          xarray<xstring>&     Args )
{
    const xbool Debug = (Script.Config != "release");

    Args.Append( "-I" );
    Args.Append( GetDirectory( Shader.Source ) );

    AppendBindingArgs( Script, Shader, Args );

    if( x_stricmp( pTarget, "vulkan" ) == 0 )
    {
        Args.Append( "-spirv" );
        Args.Append( "-fspv-target-env=vulkan1.0" );
    }

    if( Debug )
    {
        Args.Append( "-Zi" );
        Args.Append( "-Od" );

        if( x_stricmp( pTarget, "d3d12" ) == 0 )
        {
            Args.Append( "-Qembed_debug" );
            Args.Append( "-Fd" );
            Args.Append( D3D12PdbPath );
        }
    }
    else
    {
        Args.Append( "-O3" );
    }

    AppendArgs( Args, Shader.CommonArgs );

    if( x_stricmp( pTarget, "vulkan" ) == 0 )
        AppendArgs( Args, Shader.VulkanArgs );
    else if( x_stricmp( pTarget, "d3d12" ) == 0 )
        AppendArgs( Args, Shader.D3D12Args );
    else if( x_stricmp( pTarget, "metal" ) == 0 )
        AppendArgs( Args, Shader.MetalArgs );
    else
        x_printf( "Error: Unknown target '%s' in BuildDxcArgs\n", pTarget );
}

//==============================================================================

static
xbool WriteBlob( const xstring& PathName, IDxcBlob* pBlob )
{
    if( !pBlob || (pBlob->GetBufferSize() == 0) )
        return FALSE;

    return WriteBinaryFile( PathName,
                            pBlob->GetBufferPointer(),
                            (s32)pBlob->GetBufferSize() );
}

//==============================================================================

static
void PrintErrors( IDxcResult* pResult )
{
    dxc_ptr<IDxcBlobUtf8> Errors;

    if( SUCCEEDED( pResult->GetOutput( DXC_OUT_ERRORS,
                                       __uuidof( IDxcBlobUtf8 ),
                                       (void**)Errors.Put(),
                                       NULL ) ) &&
        Errors.Get() &&
        Errors->GetStringLength() )
    {
        x_printf( "%s\n", Errors->GetStringPointer() );
    }
}

//==============================================================================

static
xbool IsUnboundedBinding( const D3D12_SHADER_INPUT_BIND_DESC& Binding )
{
    return (Binding.BindCount == UINT_MAX);
}

//==============================================================================

static
u32 BindingEnd( const D3D12_SHADER_INPUT_BIND_DESC& Binding )
{
    if( IsUnboundedBinding( Binding ) )
        return 0;

    return Binding.BindPoint + Binding.BindCount;
}

//==============================================================================

static
xbool IsStorageBufferType( D3D_SHADER_INPUT_TYPE Type )
{
    return (Type == D3D_SIT_TBUFFER)                       ||
           (Type == D3D_SIT_STRUCTURED)                    ||
           (Type == D3D_SIT_BYTEADDRESS)                   ||
           (Type == D3D_SIT_UAV_RWSTRUCTURED)              ||
           (Type == D3D_SIT_UAV_RWBYTEADDRESS)             ||
           (Type == D3D_SIT_UAV_APPEND_STRUCTURED)         ||
           (Type == D3D_SIT_UAV_CONSUME_STRUCTURED)        ||
           (Type == D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER) ||
           (Type == D3D_SIT_RTACCELERATIONSTRUCTURE);
}

//==============================================================================

static
xbool IsReadWriteStorageBufferType( D3D_SHADER_INPUT_TYPE Type )
{
    return (Type == D3D_SIT_UAV_RWSTRUCTURED)              ||
           (Type == D3D_SIT_UAV_RWBYTEADDRESS)             ||
           (Type == D3D_SIT_UAV_APPEND_STRUCTURED)         ||
           (Type == D3D_SIT_UAV_CONSUME_STRUCTURED)        ||
           (Type == D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER);
}

//==============================================================================

static
xbool IsStorageTextureType( D3D_SHADER_INPUT_TYPE Type )
{
    return (Type == D3D_SIT_UAV_RWTYPED) ||
           (Type == D3D_SIT_UAV_FEEDBACKTEXTURE);
}

//==============================================================================

static
xbool HasSamplerSlot( const xarray<D3D12_SHADER_INPUT_BIND_DESC>& Bindings,
                      u32                                         Slot,
                      u32                                         Space )
{
    for( s32 i=0; i<Bindings.GetCount(); i++ )
    {
        const D3D12_SHADER_INPUT_BIND_DESC& Binding = Bindings[i];
        if( (Binding.Type == D3D_SIT_SAMPLER) &&
            (Binding.Space == Space) &&
            (Slot >= Binding.BindPoint) &&
            (Slot < BindingEnd( Binding )) )
        {
            return TRUE;
        }
    }

    return FALSE;
}

//==============================================================================

static
xbool IsSampledTexture( const xarray<D3D12_SHADER_INPUT_BIND_DESC>& Bindings,
                        const D3D12_SHADER_INPUT_BIND_DESC&          Texture )
{
    const u32 End = BindingEnd( Texture );
    if( End == 0 )
        return FALSE;

    for( u32 Slot=Texture.BindPoint; Slot<End; Slot++ )
    {
        if( !HasSamplerSlot( Bindings, Slot, Texture.Space ) )
            return FALSE;
    }

    return TRUE;
}

//==============================================================================

static
xbool HasTextureSlot( const xarray<D3D12_SHADER_INPUT_BIND_DESC>& Bindings,
                      u32                                         Slot,
                      u32                                         Space )
{
    for( s32 i=0; i<Bindings.GetCount(); i++ )
    {
        const D3D12_SHADER_INPUT_BIND_DESC& Binding = Bindings[i];
        if( (Binding.Type == D3D_SIT_TEXTURE) &&
            (Binding.Space == Space) &&
            (Slot >= Binding.BindPoint) &&
            (Slot < BindingEnd( Binding )) )
        {
            return TRUE;
        }
    }

    return FALSE;
}

//==============================================================================

static
xbool HasUniformBufferSlot( const xarray<D3D12_SHADER_INPUT_BIND_DESC>& Bindings,
                            u32                                         Slot,
                            u32                                         Space )
{
    for( s32 i=0; i<Bindings.GetCount(); i++ )
    {
        const D3D12_SHADER_INPUT_BIND_DESC& Binding = Bindings[i];
        if( (Binding.Type == D3D_SIT_CBUFFER) &&
            (Binding.Space == Space) &&
            (Slot >= Binding.BindPoint) &&
            (Slot < BindingEnd( Binding )) )
        {
            return TRUE;
        }
    }

    return FALSE;
}

//==============================================================================

static
xbool HasStorageBufferSlot( const xarray<D3D12_SHADER_INPUT_BIND_DESC>& Bindings,
                            u32                                         Slot,
                            u32                                         Space )
{
    for( s32 i=0; i<Bindings.GetCount(); i++ )
    {
        const D3D12_SHADER_INPUT_BIND_DESC& Binding = Bindings[i];
        if( IsStorageBufferType( Binding.Type ) &&
            (Binding.Space == Space) &&
            (Slot >= Binding.BindPoint) &&
            (Slot < BindingEnd( Binding )) )
        {
            return TRUE;
        }
    }

    return FALSE;
}

//==============================================================================

static
void AddReflectionBinding( xarray<shader_reflection_binding>& Bindings,
                           shader_reflection_binding_type     Type,
                           const char*                        pName,
                           u32                                Slot,
                           u32                                Count )
{
    if( !pName || !pName[0] || (Count == 0) )
        return;

    shader_reflection_binding& Binding = Bindings.Append();
    Binding.Type  = Type;
    Binding.Name  = pName;
    Binding.Slot  = Slot;
    Binding.Count = Count;
}

//==============================================================================

static
xbool ReflectResources( IDxcUtils*              pUtils,
                        IDxcResult*             pResult,
                        const shader_script&    Script,
                        const shader_entry&     Shader,
                        shader_resource_counts& Resources,
                        xarray<shader_reflection_binding>& OutBindings )
{
    OutBindings.Clear();

    dxc_ptr<IDxcBlob> ReflectionData;
    HRESULT hr = pResult->GetOutput( DXC_OUT_REFLECTION,
                                     __uuidof( IDxcBlob ),
                                     (void**)ReflectionData.Put(),
                                     NULL );
    if( FAILED( hr ) || !ReflectionData.Get() )
    {
        x_printf( "Error: DXC produced no reflection data for %s\n", (const char*)Shader.Name );
        return FALSE;
    }

    DxcBuffer ReflectionBuffer;
    ReflectionBuffer.Ptr      = ReflectionData->GetBufferPointer();
    ReflectionBuffer.Size     = ReflectionData->GetBufferSize();
    ReflectionBuffer.Encoding = 0;

    dxc_ptr<ID3D12ShaderReflection> Reflection;
    hr = pUtils->CreateReflection( &ReflectionBuffer,
                                   __uuidof( ID3D12ShaderReflection ),
                                   (void**)Reflection.Put() );
    if( FAILED( hr ) || !Reflection.Get() )
    {
        x_printf( "Error: DXC reflection creation failed for %s: 0x%08X\n",
                  (const char*)Shader.Name,
                  (u32)hr );
        return FALSE;
    }

    D3D12_SHADER_DESC Desc;
    hr = Reflection->GetDesc( &Desc );
    if( FAILED( hr ) )
        return FALSE;

    xarray<D3D12_SHADER_INPUT_BIND_DESC> Bindings;
    Bindings.SetCount( (s32)Desc.BoundResources );

    for( u32 i=0; i<Desc.BoundResources; i++ )
    {
        hr = Reflection->GetResourceBindingDesc( i, &Bindings[(s32)i] );
        if( FAILED( hr ) )
        {
            x_printf( "Error: Failed to get resource binding desc %u in shader %s: 0x%08X\n",
                      i, (const char*)Shader.Name, (u32)hr );
            return FALSE;
        }
    
        if( IsUnboundedBinding( Bindings[(s32)i] ) )
        {
            x_printf( "Error: Unsupported unbounded resource '%s' in shader %s\n",
                      Bindings[(s32)i].Name, (const char*)Shader.Name );
            return FALSE;
        }
    
        if( Bindings[(s32)i].BindCount == 0 )
        {
            x_printf( "Error: Resource '%s' in shader %s has zero bind count (DXC reflection anomaly)\n",
                      Bindings[(s32)i].Name, (const char*)Shader.Name );
            return FALSE;
        }
    }

    Resources = shader_resource_counts();

    u32 ResourceSpace = 0;
    u32 UniformSpace  = 0;
    if( Script.BindingModel == SHADER_BINDING_SDL )
    {
        if( Shader.StageShort == "vs" )
        {
            ResourceSpace = 0;
            UniformSpace  = 1;
        }
        else if( Shader.StageShort == "ps" )
        {
            ResourceSpace = 2;
            UniformSpace  = 3;
        }
        // TODO: GS: Implement this.
        else
        {
            x_printf( "Error: SDL graphics reflection is not defined for stage '%s'\n",
                      (const char*)Shader.Stage );
            return FALSE;
        }
    }

    for( s32 i=0; i<Bindings.GetCount(); i++ )
    {
        const D3D12_SHADER_INPUT_BIND_DESC& Binding = Bindings[i];
        const u32 End = BindingEnd( Binding );

        if( Binding.Type == D3D_SIT_SAMPLER )
        {
            if( (Script.BindingModel == SHADER_BINDING_SDL) && (Binding.Space != ResourceSpace) )
            {
                x_printf( "Error: SDL sampler '%s' uses space%u; expected space%u in shader %s\n",
                          Binding.Name, Binding.Space, ResourceSpace, (const char*)Shader.Name );
                return FALSE;
            }
            Resources.Samplers = MAX( Resources.Samplers, End );
        }
        else if( Binding.Type == D3D_SIT_CBUFFER )
        {
            if( (Script.BindingModel == SHADER_BINDING_SDL) && (Binding.Space != UniformSpace) )
            {
                x_printf( "Error: SDL uniform buffer '%s' uses space%u; expected space%u in shader %s\n",
                          Binding.Name, Binding.Space, UniformSpace, (const char*)Shader.Name );
                return FALSE;
            }
            Resources.UniformBuffers = MAX( Resources.UniformBuffers, End );
            AddReflectionBinding( OutBindings,
                                  SHADER_REFLECT_UNIFORM_BUFFER,
                                  Binding.Name,
                                  Binding.BindPoint,
                                  Binding.BindCount );
        }
    }

    if( (Script.BindingModel == SHADER_BINDING_SDL) && (Resources.UniformBuffers > 4) )
    {
        x_printf( "Error: SDL supports at most four uniform buffers per graphics stage; shader %s needs %u\n",
                  (const char*)Shader.Name,
                  Resources.UniformBuffers );
        return FALSE;
    }

    for( s32 i=0; i<Bindings.GetCount(); i++ )
    {
        const D3D12_SHADER_INPUT_BIND_DESC& Binding = Bindings[i];
        if( Binding.Type != D3D_SIT_TEXTURE )
            continue;

        const u32 End = BindingEnd( Binding );
        if( IsSampledTexture( Bindings, Binding ) )
        {
            if( End > Resources.Samplers )
            {
                x_printf( "Error: Sampled texture '%s' at slot %u has no matching sampler in shader %s\n",
                          Binding.Name, End - 1, (const char*)Shader.Name );
                return FALSE;
            }

            AddReflectionBinding( OutBindings,
                                  SHADER_REFLECT_SAMPLED_TEXTURE,
                                  Binding.Name,
                                  Binding.BindPoint,
                                  Binding.BindCount );
        }
        else if( Script.BindingModel == SHADER_BINDING_SDL )
        {
            if( (Binding.Space != ResourceSpace) || (Binding.BindPoint < Resources.Samplers) )
            {
                x_printf( "Error: SDL storage texture '%s' must follow %u sampled textures in space%u in shader %s\n",
                          Binding.Name, Resources.Samplers, ResourceSpace, (const char*)Shader.Name );
                return FALSE;
            }
            Resources.StorageTextures = MAX( Resources.StorageTextures,
                                             End - Resources.Samplers );
            AddReflectionBinding( OutBindings,
                                  SHADER_REFLECT_STORAGE_TEXTURE,
                                  Binding.Name,
                                  Binding.BindPoint - Resources.Samplers,
                                  Binding.BindCount );
        }
        else if( Script.BindingModel == SHADER_BINDING_NATIVE )
        {
            Resources.StorageTextures = MAX( Resources.StorageTextures, End );
            AddReflectionBinding( OutBindings,
                                  SHADER_REFLECT_STORAGE_TEXTURE,
                                  Binding.Name,
                                  Binding.BindPoint,
                                  Binding.BindCount );
        }
    }

    const u32 StorageBufferBase = Resources.Samplers + Resources.StorageTextures;
    for( s32 i=0; i<Bindings.GetCount(); i++ )
    {
        const D3D12_SHADER_INPUT_BIND_DESC& Binding = Bindings[i];
        const u32 End = BindingEnd( Binding );

        if( IsStorageBufferType( Binding.Type ) )
        {
            if( Script.BindingModel == SHADER_BINDING_SDL )
            {
                if( IsReadWriteStorageBufferType( Binding.Type ) )
                {
                    x_printf( "Error: SDL graphics shaders cannot use read-write storage buffer '%s'\n",
                              Binding.Name );
                    return FALSE;
                }

                if( (Binding.Space != ResourceSpace) || (Binding.BindPoint < StorageBufferBase) )
                {
                    x_printf( "Error: SDL storage buffer '%s' must follow sampled/storage textures at t%u in space%u in shader %s\n",
                              Binding.Name, StorageBufferBase, ResourceSpace, (const char*)Shader.Name );
                    return FALSE;
                }
                Resources.StorageBuffers = MAX( Resources.StorageBuffers,
                                                End - StorageBufferBase );
                AddReflectionBinding( OutBindings,
                                      SHADER_REFLECT_STORAGE_BUFFER,
                                      Binding.Name,
                                      Binding.BindPoint - StorageBufferBase,
                                      Binding.BindCount );
            }
            else if( Script.BindingModel == SHADER_BINDING_NATIVE )
            {
                Resources.StorageBuffers = MAX( Resources.StorageBuffers, End );
                AddReflectionBinding( OutBindings,
                                      SHADER_REFLECT_STORAGE_BUFFER,
                                      Binding.Name,
                                      Binding.BindPoint,
                                      Binding.BindCount );
            }
        }
        else if( IsStorageTextureType( Binding.Type ) )
        {
            if( Script.BindingModel == SHADER_BINDING_SDL )
            {
                x_printf( "Error: SDL graphics shaders cannot use UAV resource '%s'\n", Binding.Name );
                return FALSE;
            }
            Resources.StorageTextures = MAX( Resources.StorageTextures, End );
        }
    }

    if( Script.BindingModel == SHADER_BINDING_SDL )
    {
        for( u32 Slot=0; Slot<Resources.Samplers; Slot++ )
        {
            if( !HasSamplerSlot( Bindings, Slot, ResourceSpace ) ||
                !HasTextureSlot( Bindings, Slot, ResourceSpace ) )
            {
                x_printf( "Error: SDL sampled-texture slot %u is not contiguous in shader %s\n",
                          Slot,
                          (const char*)Shader.Name );
                return FALSE;
            }
        }

        for( u32 Slot=0; Slot<Resources.UniformBuffers; Slot++ )
        {
            if( !HasUniformBufferSlot( Bindings, Slot, UniformSpace ) )
            {
                x_printf( "Error: SDL uniform-buffer slot %u is not contiguous in shader %s\n",
                          Slot,
                          (const char*)Shader.Name );
                return FALSE;
            }
        }

        for( u32 Slot=0; Slot<Resources.StorageTextures; Slot++ )
        {
            const u32 BindingSlot = Resources.Samplers + Slot;
            if( !HasTextureSlot( Bindings, BindingSlot, ResourceSpace ) ||
                HasSamplerSlot( Bindings, BindingSlot, ResourceSpace ) )
            {
                x_printf( "Error: SDL storage-texture slot %u is not contiguous in shader %s\n",
                          Slot,
                          (const char*)Shader.Name );
                return FALSE;
            }
        }

        for( u32 Slot=0; Slot<Resources.StorageBuffers; Slot++ )
        {
            const u32 BindingSlot = StorageBufferBase + Slot;
            if( !HasStorageBufferSlot( Bindings, BindingSlot, ResourceSpace ) )
            {
                x_printf( "Error: SDL storage-buffer slot %u is not contiguous in shader %s\n",
                          Slot,
                          (const char*)Shader.Name );
                return FALSE;
            }
        }
    }

    return TRUE;
}

//==============================================================================

static
xbool CompileDxc( const shader_script& Script,
                  const shader_entry&  Shader,
                  const compile_options& Options,
                  const char*          pTarget,
                  dxc_ptr<IDxcUtils>&  Utils,
                  dxc_ptr<IDxcResult>& Result,
                  xarray<xstring>&     Args )
{
    BuildDxcArgs( Script, Shader, pTarget, PdbPath( Script, Shader ), Args );

    if( Options.Verbose )
    {
        x_printf( "  DXC %s:", pTarget );
        for( s32 i=0; i<Args.GetCount(); i++ )
            x_printf( " %s", (const char*)Args[i] );
        x_printf( "\n" );
    }

    xarray<byte> Source;
    if( !LoadBinaryFile( Shader.Source, Source ) )
    {
        x_printf( "Error: Failed to read shader source %s\n", (const char*)Shader.Source );
        return FALSE;
    }

    dxc_ptr<IDxcCompiler3>      Compiler;
    dxc_ptr<IDxcIncludeHandler> IncludeHandler;

    HRESULT hr = DxcCreateInstance( CLSID_DxcUtils, __uuidof( IDxcUtils ), (void**)Utils.Put() );
    if( FAILED( hr ) )
    {
        x_printf( "Error: DxcCreateInstance(CLSID_DxcUtils) failed: 0x%08X\n", (u32)hr );
        return FALSE;
    }

    hr = DxcCreateInstance( CLSID_DxcCompiler, __uuidof( IDxcCompiler3 ), (void**)Compiler.Put() );
    if( FAILED( hr ) )
    {
        x_printf( "Error: DxcCreateInstance(CLSID_DxcCompiler) failed: 0x%08X\n", (u32)hr );
        return FALSE;
    }

    hr = Utils->CreateDefaultIncludeHandler( IncludeHandler.Put() );
    if( FAILED( hr ) )
    {
        x_printf( "Error: CreateDefaultIncludeHandler failed: 0x%08X\n", (u32)hr );
        return FALSE;
    }

    std::vector<std::wstring> WideArgs;
    std::vector<LPCWSTR>      ArgPtrs;
    WideArgs.reserve( Args.GetCount() );
    ArgPtrs.reserve( Args.GetCount() );

    for( s32 i=0; i<Args.GetCount(); i++ )
        WideArgs.push_back( ToWide( Args[i] ) );
    for( size_t i=0; i<WideArgs.size(); i++ )
        ArgPtrs.push_back( WideArgs[i].c_str() );

    std::wstring SourceName = ToWide( Shader.Source );
    std::wstring Entry      = ToWide( Shader.Entry );
    std::wstring Profile    = ToWide( Shader.Profile );

    dxc_ptr<IDxcCompilerArgs> CompilerArgs;
    hr = Utils->BuildArguments( SourceName.c_str(),
                                Entry.c_str(),
                                Profile.c_str(),
                                ArgPtrs.empty() ? NULL : &ArgPtrs[0],
                                (UINT32)ArgPtrs.size(),
                                NULL,
                                0,
                                CompilerArgs.Put() );
    if( FAILED( hr ) )
    {
        x_printf( "Error: DXC BuildArguments failed: 0x%08X\n", (u32)hr );
        return FALSE;
    }

    DxcBuffer SourceBuffer;
    SourceBuffer.Ptr      = Source.GetPtr();
    SourceBuffer.Size     = (SIZE_T)Source.GetCount();
    SourceBuffer.Encoding = DXC_CP_UTF8;

    hr = Compiler->Compile( &SourceBuffer,
                            CompilerArgs->GetArguments(),
                            CompilerArgs->GetCount(),
                            IncludeHandler.Get(),
                            __uuidof( IDxcResult ),
                            (void**)Result.Put() );
    if( FAILED( hr ) || !Result.Get() )
    {
        x_printf( "Error: DXC failed to start %s compile: 0x%08X\n", pTarget, (u32)hr );
        return FALSE;
    }

    PrintErrors( Result.Get() );

    HRESULT Status = S_OK;
    hr = Result->GetStatus( &Status );
    if( FAILED( hr ) || FAILED( Status ) )
    {
        x_printf( "Error: %s compile failed: 0x%08X\n", pTarget, (u32)(FAILED( hr ) ? hr : Status) );
        return FALSE;
    }

    return TRUE;
}

//==============================================================================
//  FUNCTIONS
//==============================================================================

xbool CompileDxcTarget( const shader_script&    Script,
                        const shader_entry&     Shader,
                        const compile_options&  Options,
                        const char*             pTarget,
                        shader_compile_output&  Output )
{
    Output          = shader_compile_output();
    Output.Target   = pTarget;
    Output.Pdb      = PdbPath( Script, Shader );
    Output.TempPdb  = TempPathFor( Output.Pdb );

    if( Options.Verbose )
        x_printf( "  compile %s\n", pTarget );

    xarray<xstring>   Args;
    dxc_ptr<IDxcUtils> Utils;
    dxc_ptr<IDxcResult> Result;
    if( !CompileDxc( Script, Shader, Options, pTarget, Utils, Result, Args ) )
        return FALSE;

    if( x_stricmp( pTarget, "d3d12" ) == 0 )
    {
        if( !ReflectResources( Utils.Get(), Result.Get(), Script, Shader, Output.Resources, Output.Bindings ) )
            return FALSE;
    }

    dxc_ptr<IDxcBlob> Object;
    HRESULT hr = Result->GetOutput( DXC_OUT_OBJECT,
                                    __uuidof( IDxcBlob ),
                                    (void**)Object.Put(),
                                    NULL );
    if( FAILED( hr ) || !Object.Get() )
    {
        x_printf( "Error: %s produced no object output\n", pTarget );
        return FALSE;
    }

    const size_t ObjectSize = Object->GetBufferSize();
    if( (ObjectSize == 0) || (ObjectSize > 0x7FFFFFFFu) )
    {
        x_printf( "Error: Invalid %s object size\n", pTarget );
        return FALSE;
    }

    Output.Code.SetCount( (s32)ObjectSize );
    x_memcpy( Output.Code.GetPtr(), Object->GetBufferPointer(), ObjectSize );

    if( (x_stricmp( pTarget, "d3d12" ) == 0) && Result->HasOutput( DXC_OUT_PDB ) )
    {
        dxc_ptr<IDxcBlob> PdbBlob;
        hr = Result->GetOutput( DXC_OUT_PDB,
                                __uuidof( IDxcBlob ),
                                (void**)PdbBlob.Put(),
                                NULL );
        if( FAILED( hr ) || !PdbBlob.Get() || !PdbBlob->GetBufferSize() )
        {
            x_printf( "Error: DXC reported an invalid PDB for %s\n", (const char*)Shader.Name );
            return FALSE;
        }

        RemoveFile( Output.TempPdb );
        if( !WriteBlob( Output.TempPdb, PdbBlob.Get() ) )
        {
            x_printf( "Error: Failed to write temp PDB %s\n", (const char*)Output.TempPdb );
            return FALSE;
        }

        Output.HasPdb = TRUE;
    }

    return TRUE;
}

//==============================================================================

xbool ReflectDxcShader( const shader_script&    Script,
                        const shader_entry&     Shader,
                        const compile_options&  Options,
                        shader_resource_counts& Resources,
                        xarray<shader_reflection_binding>& Bindings )
{
    xarray<xstring>    Args;
    dxc_ptr<IDxcUtils> Utils;
    dxc_ptr<IDxcResult> Result;

    if( !CompileDxc( Script, Shader, Options, "d3d12", Utils, Result, Args ) )
        return FALSE;

    return ReflectResources( Utils.Get(), Result.Get(), Script, Shader, Resources, Bindings );
}

#else

xbool CompileDxcTarget( const shader_script&,
                        const shader_entry&,
                        const compile_options&,
                        const char*,
                        shader_compile_output& )
{
    x_printf( "Error: DXC API backend is not configured for this platform.\n" );
    return FALSE;
}

//==============================================================================

xbool ReflectDxcShader( const shader_script&,
                        const shader_entry&,
                        const compile_options&,
                        shader_resource_counts&,
                        xarray<shader_reflection_binding>& )
{
    x_printf( "Error: DXC reflection backend is not configured for this platform.\n" );
    return FALSE;
}

#endif
