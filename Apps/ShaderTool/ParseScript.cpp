//==============================================================================
//
//  ParseScript.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "ShaderTool.hpp"

//==============================================================================
//  ENUMS
//==============================================================================

enum parse_section
{
    SECTION_NONE,
    SECTION_SETTINGS,
    SECTION_SHADERS,
};

//==============================================================================
//  STATIC FUNCTIONS
//==============================================================================

static
xbool IsToken( token_stream& Tokenizer, const char* pText )
{
    return (Tokenizer.Type() == token_stream::TOKEN_SYMBOL) &&
           (x_stricmp( Tokenizer.String(), pText ) == 0);
}

//==============================================================================

static
xbool ReadValue( token_stream& Tokenizer, xstring& Value )
{
    Tokenizer.Read();

    if( (Tokenizer.Type() != token_stream::TOKEN_SYMBOL) &&
        (Tokenizer.Type() != token_stream::TOKEN_STRING) &&
        (Tokenizer.Type() != token_stream::TOKEN_NUMBER) )
    {
        return FALSE;
    }

    Value = Tokenizer.String();
    return TRUE;
}

//==============================================================================

static
xstring StageShortName( const xstring& Stage )
{
    if( Stage == "vertex" )
        return "vs";

    if( (Stage == "pixel") || (Stage == "fragment") )
        return "ps";

    if( Stage == "compute" )
        return "cs";

    if( Stage == "geometry" )
        return "gs";

    if( (Stage == "hull") || (Stage == "tess_control") )
        return "hs";

    if( (Stage == "domain") || (Stage == "tess_evaluation") )
        return "ds";

    if( Stage == "mesh" )
        return "ms";

    if( (Stage == "amplification") || (Stage == "task") )
        return "as";

    return "";
}

//==============================================================================

static
u32 ParseTargetMask( const xstring& Text )
{
    const xstring Target = ToLower( Text );

    if( Target == "all" )
        return SHADER_TARGET_ALL;

    if( (Target == "vulkan") )        
        return SHADER_TARGET_VULKAN;

    if( (Target == "d3d12") || (Target == "dx12") )
        return SHADER_TARGET_D3D12;

    if( (Target == "metal") || (Target == "govno") )    
        return SHADER_TARGET_METAL;

    return 0;
}

//==============================================================================

static
xbool ParseSetting( token_stream& Tokenizer, shader_script& Script )
{
    xstring Key = ToLower( xstring( Tokenizer.String() ) );
    xstring Value;

    if( Key == "config" )
    {
        if( !ReadValue( Tokenizer, Value ) )
            return FALSE;

        Value = ToLower( Value );
        if( (Value != "debug") && (Value != "release") )
            return FALSE;

        Script.Config = Value;
    }
    else if( (Key == "target") || (Key == "targets") )
    {
        if( !ReadValue( Tokenizer, Value ) )
            return FALSE;

        Script.TargetMask = ParseTargetMask( Value );
        if( !Script.TargetMask )
            return FALSE;
    }
    else if( (Key == "binding_model") || (Key == "bindings") )
    {
        if( !ReadValue( Tokenizer, Value ) )
            return FALSE;

        Value = ToLower( Value );
        if( Value == "native" )
            Script.BindingModel = SHADER_BINDING_NATIVE;
        else if( Value == "sdl" )
            Script.BindingModel = SHADER_BINDING_SDL;
        else
            return FALSE;
    }
    else if( Key == "source_root" )
    {
        if( !ReadValue( Tokenizer, Value ) )
            return FALSE;

        Script.SourceRoot = MakeRelativePath( Script.ScriptDir, Value );
    }
    else if( Key == "out_root" )
    {
        if( !ReadValue( Tokenizer, Value ) )
            return FALSE;

        Script.OutRoot = MakeRelativePath( Script.ScriptDir, Value );
    }
    else
    {
        x_printf( "Error: Unknown setting '%s' in %s line %d\n",
                  Tokenizer.String(),
                  Tokenizer.GetFilename(),
                  Tokenizer.GetLineNumber() );
        return FALSE;
    }

    return TRUE;
}

//==============================================================================

static
xbool ExpectOpenBrace( token_stream& Tokenizer )
{
    Tokenizer.Read();
    return (Tokenizer.Type() == token_stream::TOKEN_DELIMITER) &&
           (Tokenizer.Delimiter() == '{');
}

//==============================================================================

static
xbool ParseShaderProperties( token_stream& Tokenizer, shader_entry& Shader )
{
    while( Tokenizer.Read() != token_stream::TOKEN_EOF )
    {
        if( (Tokenizer.Type() == token_stream::TOKEN_DELIMITER) &&
            (Tokenizer.Delimiter() == '}') )
        {
            return TRUE;
        }

        if( Tokenizer.Type() != token_stream::TOKEN_SYMBOL )
            return FALSE;

        xstring Key = ToLower( xstring( Tokenizer.String() ) );

        if( Key == "common_args" )
        {
            if( !ReadValue( Tokenizer, Shader.CommonArgs ) )
                return FALSE;
        }
        else if( Key == "vulkan_args" )
        {
            if( !ReadValue( Tokenizer, Shader.VulkanArgs ) )
                return FALSE;
        }
        else if( Key == "d3d12_args" )
        {
            if( !ReadValue( Tokenizer, Shader.D3D12Args ) )
                return FALSE;
        }
        else if( Key == "metal_args" )
        {
            if( !ReadValue( Tokenizer, Shader.MetalArgs ) )
                return FALSE;
        }        
        else
        {
            x_printf( "Error: Unknown shader property '%s' in %s line %d\n",
                      Tokenizer.String(),
                      Tokenizer.GetFilename(),
                      Tokenizer.GetLineNumber() );
            return FALSE;
        }
    }

    return FALSE;
}

//==============================================================================

static
xbool ParseShader( token_stream& Tokenizer, shader_script& Script )
{
    shader_entry& Shader = Script.Shaders.Append();

    xstring Value;

    if( !ReadValue( Tokenizer, Shader.Name ) )
        return FALSE;

    if( !ReadValue( Tokenizer, Shader.Stage ) )
        return FALSE;
    Shader.Stage      = ToLower( Shader.Stage );
    Shader.StageShort = StageShortName( Shader.Stage );
    if( Shader.StageShort.IsEmpty() )
        return FALSE;

    if( !ReadValue( Tokenizer, Value ) )
        return FALSE;

    if( Script.SourceRoot.IsEmpty() )
        Shader.Source = MakeRelativePath( Script.ScriptDir, Value );
    else
        Shader.Source = MakeRelativePath( Script.SourceRoot, Value );

    if( !ReadValue( Tokenizer, Shader.Entry ) )
        return FALSE;

    if( !ReadValue( Tokenizer, Shader.Profile ) )
        return FALSE;

    xstring ProfileStage = Shader.Profile.Left( 2 );
    ProfileStage.MakeLower();
    if( ProfileStage != Shader.StageShort )
    {
        x_printf( "Error: Shader stage '%s' does not match profile '%s' in %s line %d\n",
                  (const char*)Shader.Stage,
                  (const char*)Shader.Profile,
                  Tokenizer.GetFilename(),
                  Tokenizer.GetLineNumber() );
        return FALSE;
    }

    if( !ExpectOpenBrace( Tokenizer ) )
        return FALSE;

    if( !ParseShaderProperties( Tokenizer, Shader ) )
        return FALSE;

    if( !command_line::FileExists( (const char*)Shader.Source ) )
    {
        x_printf( "Error: Shader source does not exist: %s\n", (const char*)Shader.Source );
        return FALSE;
    }

    return TRUE;
}

//==============================================================================
//  FUNCTIONS
//==============================================================================

xbool ParseScript( const char* pFileName, shader_script& Script )
{
    token_stream  Tokenizer;
    parse_section Section = SECTION_NONE;

    Script.ScriptPath = pFileName;
    Script.ScriptDir  = GetDirectory( pFileName );

    char Delimiters[] = " ,{}()=";
    Tokenizer.SetDelimeter( Delimiters );

    if( !Tokenizer.OpenFile( pFileName ) )
    {
        x_printf( "Error: Can't open script '%s'\n", pFileName );
        return FALSE;
    }

    while( Tokenizer.Read() != token_stream::TOKEN_EOF )
    {
        if( IsToken( Tokenizer, "settings:" ) )
        {
            Section = SECTION_SETTINGS;
        }
        else if( IsToken( Tokenizer, "shaders:" ) )
        {
            Section = SECTION_SHADERS;
        }
        else if( Section == SECTION_SETTINGS )
        {
            if( !ParseSetting( Tokenizer, Script ) )
            {
                x_printf( "Error: Bad setting in %s line %d\n",
                          Tokenizer.GetFilename(),
                          Tokenizer.GetLineNumber() );
                Tokenizer.CloseFile();
                return FALSE;
            }
        }
        else if( Section == SECTION_SHADERS )
        {
            if( !IsToken( Tokenizer, "shader" ) )
            {
                x_printf( "Error: 'shader' keyword expected in %s line %d\n",
                          Tokenizer.GetFilename(),
                          Tokenizer.GetLineNumber() );
                Tokenizer.CloseFile();
                return FALSE;
            }

            if( !ParseShader( Tokenizer, Script ) )
            {
                x_printf( "Error: Bad shader block in %s line %d\n",
                          Tokenizer.GetFilename(),
                          Tokenizer.GetLineNumber() );
                Tokenizer.CloseFile();
                return FALSE;
            }
        }
        else
        {
            x_printf( "Error: Unknown token '%s' in %s line %d\n",
                      Tokenizer.String(),
                      Tokenizer.GetFilename(),
                      Tokenizer.GetLineNumber() );
            Tokenizer.CloseFile();
            return FALSE;
        }
    }

    Tokenizer.CloseFile();

    if( Script.OutRoot.IsEmpty() )
        Script.OutRoot = MakeRelativePath( Script.ScriptDir, "Build" );

    if( Script.Shaders.GetCount() == 0 )
    {
        x_printf( "Error: Script has no shaders: %s\n", pFileName );
        return FALSE;
    }

    for( s32 i=0; i<Script.Shaders.GetCount(); i++ )
    {
        for( s32 j=i+1; j<Script.Shaders.GetCount(); j++ )
        {
            if( (x_stricmp( Script.Shaders[i].Name, Script.Shaders[j].Name ) == 0) &&
                (x_stricmp( Script.Shaders[i].StageShort, Script.Shaders[j].StageShort ) == 0) )
            {
                x_printf( "Error: Duplicate shader output '%s.%s' in %s\n",
                          (const char*)Script.Shaders[i].Name,
                          (const char*)Script.Shaders[i].StageShort,
                          pFileName );
                return FALSE;
            }
        }
    }

    return TRUE;
}
