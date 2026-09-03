//==============================================================================
//
//  ShaderTool.hpp
//
//==============================================================================

#ifndef SHADERTOOL_HPP
#define SHADERTOOL_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "x_files.hpp"
#include "Auxiliary/CommandLine/CommandLine.hpp"
#include "Entropy/e_ECS.hpp"
#include "Parsing/tokenizer.hpp"

//==============================================================================
//  DEFINES
//==============================================================================

#define SHADER_TARGET_VULKAN    (1<<0)
#define SHADER_TARGET_D3D12     (1<<1)
#define SHADER_TARGET_METAL     (1<<2)
#define SHADER_TARGET_ALL       (SHADER_TARGET_VULKAN|SHADER_TARGET_D3D12) //|SHADER_TARGET_METAL) // TODO: GS: Implement this.

//==============================================================================
//  ENUMS
//==============================================================================

enum shader_binding_model
{
    SHADER_BINDING_NATIVE,
    SHADER_BINDING_SDL,
};

//------------------------------------------------------------------------------

enum shader_reflection_binding_type
{
    SHADER_REFLECT_SAMPLED_TEXTURE = 0,
    SHADER_REFLECT_UNIFORM_BUFFER,
    SHADER_REFLECT_STORAGE_TEXTURE,
    SHADER_REFLECT_STORAGE_BUFFER
};

//==============================================================================
//  STRUCTS
//==============================================================================

struct shader_reflection_binding
{
    shader_reflection_binding_type Type;
    xstring                        Name;
    u32                            Slot;
    u32                            Count;

    shader_reflection_binding( void ) :
        Type ( SHADER_REFLECT_SAMPLED_TEXTURE ),
        Name (),
        Slot ( 0 ),
        Count( 0 )
    {
    }
};

//------------------------------------------------------------------------------

struct shader_compile_output
{
    xstring                Target;
    xarray<byte>           Code;
    xstring                Pdb;
    xstring                TempPdb;
    xbool                  HasPdb;
    shader_resource_counts Resources;
    xarray<shader_reflection_binding>
                           Bindings;

    shader_compile_output( void ) :
        HasPdb( FALSE )
    {
    }
};

//------------------------------------------------------------------------------

struct compile_options
{
    xbool           Clean;
    xbool           Verbose;
    xarray<xstring> Scripts;

    compile_options( void ) :
        Clean  ( FALSE ),
        Verbose( FALSE )
    {
    }
};

//------------------------------------------------------------------------------

struct shader_entry
{
    xstring Name;
    xstring Stage;
    xstring StageShort;
    xstring Source;
    xstring Entry;
    xstring Profile;
    xstring CommonArgs;
    xstring VulkanArgs;
    xstring D3D12Args;
    xstring MetalArgs;    
};

//------------------------------------------------------------------------------

struct shader_script
{
    xstring              ScriptPath;
    xstring              ScriptDir;
    xstring              SourceRoot;
    xstring              OutRoot;
    xstring              Config;
    u32                  TargetMask;
    shader_binding_model BindingModel;
    xarray<shader_entry> Shaders;

    shader_script( void ) :
        Config      ( "debug" ),
        TargetMask  ( SHADER_TARGET_ALL ),
        BindingModel( SHADER_BINDING_NATIVE )
    {
    }
};

//==============================================================================
//  FUNCTIONS
//==============================================================================

void    DisplayHelp             ( void );
xbool   ParseCommandLine        ( s32 argc, char** argv, compile_options& Options );
xbool   ParseScript             ( const char* pFileName, shader_script& Script );
xbool   ProcessScript           ( const char* pFileName, const compile_options& Options );
xbool   CompileShader           ( const shader_script& Script,
                                  const shader_entry&  Shader,
                                  const compile_options& Options );
xbool   CompileDxcTarget        ( const shader_script& Script,
                                  const shader_entry&  Shader,
                                  const compile_options& Options,
                                  const char*          pTarget,
                                  shader_compile_output& Output );
xbool   ReflectDxcShader        ( const shader_script& Script,
                                  const shader_entry&  Shader,
                                  const compile_options& Options,
                                  shader_resource_counts& Resources,
                                  xarray<shader_reflection_binding>& Bindings );

xstring ToLower                 ( const xstring& Text );
xbool   IsAbsolutePath          ( const xstring& Path );
xstring JoinPath                ( const xstring& Path, const xstring& File );
xstring NormalizePath           ( const xstring& Path );
xstring MakeRelativePath        ( const xstring& BasePath, const xstring& Path );
xstring GetDirectory            ( const xstring& PathName );
xbool   EnsureParentDir         ( const xstring& PathName );
xbool   LoadBinaryFile          ( const xstring& PathName, xarray<byte>& Data );
xbool   WriteBinaryFile         ( const xstring& PathName, const void* pData, s32 Size );
xbool   RemoveFile              ( const xstring& PathName );
xbool   ReplaceFile             ( const xstring& TempPath, const xstring& FinalPath );
xstring EcsPath                 ( const shader_script& Script, const shader_entry& Shader );
xstring MetaPath                ( const shader_script& Script, const shader_entry& Shader );
xstring BlobPath                ( const shader_script& Script,
                                  const shader_entry&  Shader,
                                  const char*          pTarget );
xstring PdbPath                 ( const shader_script& Script, const shader_entry& Shader );
xstring TempPathFor             ( const xstring& PathName );
const char* BindingModelName    ( shader_binding_model Model );

//==============================================================================
#endif // SHADERTOOL_HPP
//==============================================================================
