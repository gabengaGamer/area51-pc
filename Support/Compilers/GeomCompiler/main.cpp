
#include "x_files.hpp"
#include "RawMesh.hpp"
#include "CommandLine.hpp"
#include "GeomCompiler.hpp"

//=========================================================================
//=========================================================================

static xstring                  s_SourceFile;
static xstring                  s_OutputFile;
static xstring                  s_FastCollision;
static geom_compiler::comp_type s_Type = geom_compiler::TYPE_NONE; 
static xstring                  s_TexturePath ;

xbool g_Verbose     = FALSE;
xbool g_DoCollision = TRUE;

//=========================================================================

void ExecuteScript( command_line& CommandLine )
{
    geom_compiler   Compiler;
    // Setup default texture path
    // NOTE: Although this is a hardcoded path, it can be overridden
    //       with a -TEXTURE_PATH "c:\..." argument likeit is from the editor
    s_TexturePath = xstring("C:\\GameData\\A51\\Source") ; // THIS IS OKAY FOR A DEFAULT PATH!

    //
    // Parse all the options
    //
    for( s32 i=0; i<CommandLine.GetNumOptions(); i++ )
    {
        // Get option name and string
        xstring OptName   = CommandLine.GetOptionName( i );
        xstring OptString = CommandLine.GetOptionString( i );
        xbool   bOption   = FALSE;

        if( OptName == xstring( "F" ) )
            s_SourceFile = OptString;

        if( OptName == xstring( "C" ) )
            s_FastCollision = OptString;
        
        if( OptName == xstring( "PHYSICS" ) )
            Compiler.SetPhysicsSource( OptString );

        if( OptName == xstring( "SETTINGS" ) )
            Compiler.SetSettingsFile( OptString );

        if( OptName == xstring( "SKIN" ) )
            s_Type = geom_compiler::TYPE_SKIN;
        
        if( OptName == xstring( "TEXTURE_PATH" ) )
            s_TexturePath = OptString ;

        if( OptName == xstring( "RIGID" ) )
            s_Type = geom_compiler::TYPE_RIGID;

        if( OptName == xstring( "OUTPUT" ) )
            s_OutputFile = OptString;

        if( OptName == xstring( "LOG" ) )
            g_Verbose = TRUE;

        if( OptName == xstring( "NO_COLLISION" ) )
            g_DoCollision = FALSE;

    }

    if( s_OutputFile.IsEmpty() )
    {
        x_printf( "Error: No output file specified\n" );
        return;
    }

    //
    // Execute the Commands
    //
    x_try;

        Compiler.AddFastCollision( s_FastCollision );
        
        Compiler.Export( s_SourceFile, s_OutputFile, s_Type, s_TexturePath );

    x_catch_begin;
    #ifdef X_EXCEPTIONS
        x_printf( "Error: %s\n", xExceptionGetErrorString() );
    #endif
    x_catch_end;
}

//=========================================================================

void PrintHelp( void )
{
    x_printf( "Error: Compiling\n" );
    x_printf( "Source geometry:                 -F GeometrySource                            \n" );
    x_printf( "Output geometry:                 -OUTPUT RawMesh.geom                           \n" );
    x_printf( "Type of geom:                    -RIGID (For Rigid Models)                      \n" );
    x_printf( "Type of geom:                    -SKIN  (For Skinned Models)                    \n" );
    x_printf( "Texture source path:             -TEXTURE_PATH \"C:\\GameData\\???\\Source\"    \n" );
    x_printf( "Fast Collision Geometry:         -C \"CollisionSource\"                        \n" );
    x_printf( "Physics geometry source:         -PHYSICS \"PhysicsSource\"                    \n" );
    x_printf( "Settings .txt file:              -SETTINGS \"NPC_Generic_SETTINGS.txt\"         \n" );
    x_printf( "Verbose                          -LOG                                           \n" );
    x_printf( "Make geometry have not collision -NO_COLLISION                  \n" );
}

//=========================================================================

void main( s32 argc, char* argv[] )
{
    #if defined(athyssen) && defined(X_DEBUG)
    {
        X_FILE* fp = x_fopen("c:\\geomcompiler.txt","wt");
        if( fp )
        {
            for( s32 i=0; i<argc; i++ )
                x_fprintf(fp,"%s ",argv[i] );
            x_fclose(fp);
        }
    }
    #endif

    x_try;

    LOG_APP_NAME( "GeomCompiler" );
    for( s32 i=0; i<argc; i++ )
        LOG_MESSAGE( "Args", "%d = '%s'", i, argv[i] );
    LOG_FLUSH();

    command_line CommandLine;
    
    // Specify all the options
    CommandLine.AddOptionDef( "RIGID" );
    CommandLine.AddOptionDef( "SKIN"  );
    CommandLine.AddOptionDef( "LOG"   );

    CommandLine.AddOptionDef( "OUTPUT",       command_line::STRING );
    CommandLine.AddOptionDef( "C",            command_line::STRING );
    CommandLine.AddOptionDef( "PHYSICS",      command_line::STRING );
    CommandLine.AddOptionDef( "SETTINGS",     command_line::STRING );
    CommandLine.AddOptionDef( "F",            command_line::STRING );
    CommandLine.AddOptionDef( "TEXTURE_PATH", command_line::STRING );
    CommandLine.AddOptionDef( "NO_COLLISION" );

    // Parse the command line
    if( CommandLine.Parse( argc, argv ) )
    {
        PrintHelp();
        return;
    }
        
    // Do the script
    ExecuteScript( CommandLine );

    x_catch_begin;
    #ifdef X_EXCEPTIONS
        x_printf( "Error: %s\n", xExceptionGetErrorString() );
    #endif
    x_catch_end;
}
