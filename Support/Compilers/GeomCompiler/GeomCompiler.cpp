//=============================================================================
//
//  Rigid and Skin Geom Compiler by JP and TA
//
//=============================================================================

#include "GeomCompiler.hpp"
#include "GeomSource.hpp"
#include "RigidDesc.hpp"
#include "SkinDesc.hpp"

//=============================================================================

geom_compiler::geom_compiler( void )
{
    m_FastCollision     [0] = 0;
    m_PhysicsSource     [0] = 0;
    m_SettingsFile      [0] = 0;
    m_TexturePath       [0] = 0 ;
    m_OutputFile        [0] = 0;
    m_pGeomRscDesc          = NULL;
    x_strcpy( m_UserName, "Unknown user" );
    x_strcpy( m_ComputerName, "Unknown computer" );
    m_ExportDate        [0] = 0;
    m_ExportDate        [1] = 0;
    m_ExportDate        [2] = 0;
}

//=============================================================================

void geom_compiler::SetUserInfo( const char* pUserName, const char* pComputerName )
{
    if( pUserName )
        x_strsavecpy( m_UserName, pUserName, sizeof(m_UserName) );
    else
        x_strcpy( m_UserName, "Unknown user" );

    if( pComputerName )
        x_strsavecpy( m_ComputerName, pComputerName, sizeof(m_ComputerName) );
    else
        x_strcpy( m_ComputerName, "Unknown computer" );
}

//=============================================================================

void geom_compiler::SetExportDate( s32 Month, s32 Day, s32 Year )
{
    m_ExportDate[0] = Month;
    m_ExportDate[1] = Day;
    m_ExportDate[2] = Year;
}

//=============================================================================

void geom_compiler::ReportWarning( const char* pWarning )
{
    x_printf( "WARNING: %s\n", pWarning );
    x_printf( "  Last exported by: %s, on: %d/%d/%d\n", m_UserName, m_ExportDate[0], m_ExportDate[1], m_ExportDate[2] );
}

//=============================================================================

void geom_compiler::ReportError( const char* pError )
{
    x_printf( "ERROR: %s\n", pError );
    x_printf( "  Last exported by: %s, on: %d/%d/%d\n", m_UserName, m_ExportDate[0], m_ExportDate[1], m_ExportDate[2] );
}

//=============================================================================

void geom_compiler::ThrowError( const char* pError )
{
    ReportError( pError );
    x_throw( "GeomCompiler error occurred" );
}

//=============================================================================

void geom_compiler::AddFastCollision( const char* pFileName )
{
    x_strsavecpy( m_FastCollision, pFileName, sizeof(m_FastCollision) );
}

//=============================================================================

void geom_compiler::SetPhysicsSource( const char* pFileName )
{
    x_strsavecpy( m_PhysicsSource, pFileName, sizeof(m_PhysicsSource) );
}

//=============================================================================

void geom_compiler::SetSettingsFile( const char* pFileName )
{
    x_strsavecpy( m_SettingsFile, pFileName, sizeof(m_SettingsFile) );
}

//=============================================================================

void geom_compiler::LoadSourceMesh( const char* pFileName, rawmesh2& Mesh )
{
    xstring Error;
    if( !geom_source::LoadMesh( pFileName, Mesh, Error ) )
        ThrowError( Error );
}

//=============================================================================

void geom_compiler::LoadSourceAnimation( const char* pFileName,
                                         rawanim&    Animation )
{
    xstring Error;
    if( !geom_source::LoadAnimation( pFileName, Animation, Error ) )
        ThrowError( Error );
}

//=============================================================================

void geom_compiler::LoadResource( const char* pFileName, xbool bSkin )
{
    if( geom_source::IsSourceFile( pFileName ) )
    {
        // we need to create and fill in a resource description, making the
        // vmeshes match to the rawmesh submeshes

        // create a geom resource
        if( bSkin )
        {
            m_pGeomRscDesc = (geom_rsc_desc*)&g_RescDescMGR.CreateRscDesc( ".skingeom" );
            if( x_strcmp( m_pGeomRscDesc->GetType(), "SkinGeom" ) )
            {
                ThrowError( "Internal error - geom type mismatch" );
            }
        }
        else
        {
            m_pGeomRscDesc = (geom_rsc_desc*)&g_RescDescMGR.CreateRscDesc( ".rigidgeom" );
            if( x_strcmp( m_pGeomRscDesc->GetType(), "RigidGeom" ) )
            {
                ThrowError( "Internal error - geom type mismatch" );
            }
        }
        
        // copy in the filename
        m_pGeomRscDesc->SetSourceFileName( pFileName );
        
        // load the rawmesh so we can match up vmeshes
        rawmesh2 RawMesh;
        LoadSourceMesh( pFileName, RawMesh );

        // match up vmeshes to submeshes
        s32 i;
        for( i = 0; i < RawMesh.m_nSubMeshs; i++ )
        {
            geom_rsc_desc::virtual_mesh& VMesh = m_pGeomRscDesc->AppendVirtualMesh();
            x_strsavecpy( VMesh.Name, RawMesh.m_pSubMesh[i].Name, geom_rsc_desc::MAX_NAME_LENGTH );
            
            geom_rsc_desc::lod_info& LodInfo = VMesh.LODs.Append();
            LodInfo.nMeshes = 1;
            x_strsavecpy( LodInfo.MeshName[0], RawMesh.m_pSubMesh[i].Name, geom_rsc_desc::MAX_NAME_LENGTH );
            LodInfo.ScreenSize = 10000;
        }
    }
    else
    {
        char Extension[X_MAX_EXT];
        x_splitpath( pFileName, NULL, NULL, NULL, Extension );
        const char* pResourceExtension = bSkin ? ".skingeom" : ".rigidgeom";
        if( x_stricmp( Extension, pResourceExtension ) != 0 )
        {
            ThrowError( xfs( "No source loader is registered for [%s]",
                             Extension ) );
        }

        // Load a resource descriptor through the manager.
        if( bSkin )
        {
            skingeom_rsc_desc& RscDesc = (skingeom_rsc_desc&)g_RescDescMGR.Load( pFileName );
            if( x_strcmp( RscDesc.GetType(), "SkinGeom" ) )
            {
                x_throw( xfs("Trying to compile non-skin geom resource as skingeom (%s)", RscDesc.GetName()) );
            }
            m_pGeomRscDesc = (geom_rsc_desc*)&RscDesc;
        }
        else
        {
            rigidgeom_rsc_desc& RscDesc = (rigidgeom_rsc_desc&)g_RescDescMGR.Load( pFileName );
            if( x_strcmp( RscDesc.GetType(), "RigidGeom" ) )
            {
                x_throw( xfs("Trying to compile non-rigid geom resource as rigidgeom (%s)", RscDesc.GetName()) );
            }
            m_pGeomRscDesc = (geom_rsc_desc*)&RscDesc;
        }
    }
}

//=============================================================================

void geom_compiler::Export( const char* pFileName,
                            const char* pOutputFile,
                            comp_type   Type,
                            const char* pTexturePath )
{
    if( !pFileName || !pFileName[0] )
        x_throw( "No geometry source file was specified" );
    if( !pOutputFile || !pOutputFile[0] )
        x_throw( "No geometry output file was specified" );

    // reset the dictionary
    m_Dictionary.Reset();
    x_strsavecpy( m_OutputFile, pOutputFile, sizeof(m_OutputFile) );

    // Keep source path
    if( pTexturePath )
        x_strsavecpy( m_TexturePath, pTexturePath, sizeof(m_TexturePath) );

    //
    switch( Type )
    {
        case TYPE_RIGID:
            ExportRigidGeom( pFileName );
            break;
        
        case TYPE_SKIN:
            ExportSkinGeom( pFileName );
            break;

        default:
            x_throw( "Unknown compiler type" );
            break;
    }
}

//=============================================================================
