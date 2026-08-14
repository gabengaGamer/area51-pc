#include "GeomCompiler.hpp"
#include "GeomDesc.hpp"
#include "BMPUtil.hpp"
#include "Auxiliary/Bitmap/aux_Bitmap.hpp"

namespace
{

const s32 MaxUVKeys       = 10000;
const s32 MaxTextureWidth = 512;
const s32 MaxTextureHeight = 512;

enum material_parameter
{
    DETAIL_SCALE,
    ENV_TYPE,
    ENV_BLEND,
    FIXED_ALPHA,
    FORCE_ZFILL,
    USE_DIFFUSE,
};

xbool LoadBitmap( xbitmap& Bitmap, const char* pFileName )
{
    return auxbmp_Load( Bitmap, pFileName );
}

} // namespace

void geom_compiler::BuildTexturePath( char* pPath, const char* pName )
{
    char pFilename[256];
    char pDrive[256];
    char pDir  [256];
    x_splitpath( pName, NULL, NULL, pFilename, NULL );
    x_splitpath( m_OutputFile, pDrive, pDir, NULL, NULL );
    x_makepath ( pPath, pDrive, pDir, pFilename, ".xbmp" );
}

//=============================================================================

void geom_compiler::ExportDiffuse( const xbitmap& Bitmap,
                                   const char* pName,
                                   pref_bpp PrefBPP,
                                   s32 nMips )
{
    (void)PrefBPP;
    (void)nMips;

    // Make a copy of the bitmap since we are going to modify it
    xbitmap BMPToSave = Bitmap;
    auxbmp_ConvertToD3D( BMPToSave );

    // Save the bitmap
    char pPath[256];
    BuildTexturePath( pPath, pName );
    BMPToSave.Save( pPath );
}

//=============================================================================

void geom_compiler::ExportEnvironment( const xbitmap& Bitmap, const char* pName )
{
    // Make a copy of the bitmap since we are going to modify it
    xbitmap BMPToSave = Bitmap;

    auxbmp_ConvertToD3D( BMPToSave );

    // Save the bitmap
    char pPath[256];
    BuildTexturePath( pPath, pName );
    BMPToSave.Save( pPath );
}

//=============================================================================

void geom_compiler::ExportDetail( const xbitmap& Bitmap, const char* pName )
{
    // Make a copy of the bitmap since we are going to modify it
    xbitmap BMPToSave = Bitmap;

    bmp_util::ProcessDetailMap( BMPToSave, FALSE );
    auxbmp_ConvertToD3D( BMPToSave );

    // Save the bitmap
    char pPath[256];
    BuildTexturePath( pPath, pName );
    BMPToSave.Save( pPath );
}

//=============================================================================

void geom_compiler::ProcessPunchThruMap( map_info& DiffuseMap, const char* pPunchThruMap )
{
    // load the punch through map
    xbitmap PunchMap;
    if( LoadBitmap( PunchMap, pPunchThruMap ) == FALSE )
    {
        ThrowError( xfs( "Unable to load punch-through map (%s)", pPunchThruMap ) );
    }

    PunchMap.ConvertFormat( xbitmap::FMT_32_ARGB_8888 );
    if( bmp_util::SetPunchThrough( DiffuseMap.Bitmap, PunchMap ) == FALSE )
    {
        ThrowError( xfs( "Diffuse [%s] and PunchThru texture [%s] have different dimensions",
                    (const char*)DiffuseMap.InputBitmapName,
                    pPunchThruMap ) );
    }
}

//=============================================================================

void geom_compiler::ExportUVAnimation( const rawmesh2::material& RawMat,
                                       const f32*                pParamKey,
                                       geom::material&           Material,
                                       geom&                     Geom )
{
    const rawmesh2::param_pkg& Param  = RawMat.Map[ Max_Diffuse1 ].UVTranslation;
    geom::material::uvanim&    UVAnim = Material.UVAnim;

    x_memset( &UVAnim, 0, sizeof( UVAnim ) );

    // Check if we have any animation data
    if( Param.nKeys == 0 )
        return;
    
    // For now we will only handle U and V pairs
    if( Param.nParamsPerKey != 2 )
        ThrowError( xfs( "Number of UV Animation Params per Key is not 2! [%s]\n", RawMat.Name ) );
    
    // try to match up the uv animation from the resource description
    const geom_rsc_desc::uv_animation* pRscUVAnim = m_pGeomRscDesc->GetUVAnimation( RawMat.Name );
    if( pRscUVAnim )
    {
        UVAnim.Type       = (s8)pRscUVAnim->AnimType;
        UVAnim.StartFrame = (s8)pRscUVAnim->StartFrame;
        UVAnim.FPS        = (s8)pRscUVAnim->FPS;
        UVAnim.iKey       = Geom.m_nUVKeys;
        UVAnim.nKeys      = Param.nKeys;
    }
    else
    {
        UVAnim.Type       = geom::material::uvanim::LOOPED;
        UVAnim.StartFrame = 0;
        UVAnim.FPS        = 30;
        UVAnim.iKey       = Geom.m_nUVKeys;
        UVAnim.nKeys      = Param.nKeys;
    }
    
    // Copy the UV key pairs into the Geom
    for( s32 i=0; i<Param.nKeys * Param.nParamsPerKey; i += Param.nParamsPerKey )
    {
        s32 iKey = RawMat.iFirstKey + i;
        
        Geom.m_pUVKey[ Geom.m_nUVKeys ].OffsetU = (u8)(x_fmod( pParamKey[ iKey + 0 ], 1.0f ) * 255.0f);
        Geom.m_pUVKey[ Geom.m_nUVKeys ].OffsetV = (u8)(x_fmod( pParamKey[ iKey + 1 ], 1.0f ) * 255.0f);
        
        Geom.m_nUVKeys++;
    }
}

//=============================================================================

void geom_compiler::ReadBitmap( map_info& MapInfo )
{
    // Load the bitmap
    if( LoadBitmap( MapInfo.Bitmap, MapInfo.InputBitmapName ) == FALSE )
        ThrowError( xfs( "Unable to load texture [%s]", (const char*)MapInfo.InputBitmapName ) );

    // Convert to ARGB and perform sanity checks on loaded bitmaps
    MapInfo.Bitmap.ConvertFormat( xbitmap::FMT_32_ARGB_8888 );

    // check the overall dimension size
    s32 W = MapInfo.Bitmap.GetWidth();
    s32 H = MapInfo.Bitmap.GetHeight();
    if( (W > MaxTextureWidth) || (H > MaxTextureHeight) )
    {
        ThrowError( xfs( "Texture [%s] (%d x %d) is too big. Max size is (%d x %d)",
                    (const char*)MapInfo.InputBitmapName,
                    W, H, MaxTextureWidth, MaxTextureHeight ) );
    }

    // check for a power of 2
    if( (((W-1) & W) != 0) ||
        (((H-1) & H) != 0) )
    {
        ThrowError( xfs( "Texture [%s] (%d x %d) must have a power of 2 width and height",
                    (const char*)MapInfo.InputBitmapName,
                    W, H, MaxTextureWidth, MaxTextureHeight ) );
    }
}

//=============================================================================

void geom_compiler::ComputeMapNames( geom_compiler::map_slot& Map,
                                     s32                      MapType,
                                     u32                      CheckSum,
                                     pref_bpp                 PrefBPP,
                                     const char*              pInputFile )
{
    // calculate the output path...we'll need that later
    char OutputDrive[X_MAX_DRIVE];
    char OutputDir[X_MAX_DIR];
    x_splitpath( m_OutputFile, OutputDrive, OutputDir, NULL, NULL );

    // calculate the filename with the path stripped off for virtual
    // texture comparisons
    char InputDrive[X_MAX_DRIVE];
    char InputDir[X_MAX_DIR];
    char InputFName[X_MAX_FNAME];
    char InputExt[X_MAX_EXT];
    char InputFNameExt[X_MAX_PATH];
    x_splitpath( pInputFile, InputDrive, InputDir, InputFName, InputExt );
    x_makepath( InputFNameExt, NULL, NULL, InputFName, InputExt );

    // check if this map will correspond to a virtual texture (note that
    // only diffuse slots get the virtual texture)
    s32 iVTexture = -1;
    if( MapType == Max_Diffuse1 )
    {
        s32 i;
        for( i = 0; i < m_pGeomRscDesc->GetVirtualTextureCount(); i++ )
        {
            const geom_rsc_desc::virtual_texture& VTexture = m_pGeomRscDesc->GetVirtualTexture(i);

            // is this even a valid virtual texture?
            if( VTexture.Textures.GetCount() == 0 )
                continue;

            // does this virtual texture match with our input filename?
            if( !x_stricmp( VTexture.Textures[0].FileName, InputFNameExt ) )
            {
                iVTexture = i;
                break;
            }
        }
    }

    // the first texture always comes directly from the geometry
    map_info& MapInfo = Map.MapList.Append();

    // the input name is just a straight copy
    MapInfo.InputBitmapName = pInputFile;

    // figure out what the bitmap name will be decorated with, based on its
    // checksum and map usage
    char NameDecoration[X_MAX_PATH];
    x_strcpy( NameDecoration, "[" );
    if( MapType == Max_DetailMap )
    {
        x_strcat( NameDecoration, "D" );
    }
    else if( MapType == Max_Environment )
    {
        x_strcat( NameDecoration, "E" );
    }
    else
    {
        switch( PrefBPP )
        {
        default:
        case PREF_BPP_32:       x_strcat( NameDecoration, "32_" );  break;
        case PREF_BPP_16:       x_strcat( NameDecoration, "16_" );  break;
        case PREF_BPP_8:        x_strcat( NameDecoration, "8_" );   break;
        case PREF_BPP_4:        x_strcat( NameDecoration, "4_" );   break;
        case PREF_BPP_DEFAULT:  x_strcat( NameDecoration, "8_" );   break;
        }
    }
    x_strcat( NameDecoration, xfs("%04X", CheckSum&0xffff) );
    x_strcat( NameDecoration, "]" );

    // the compiled name is decorated with the checksum, is an xbmp, and has
    // had the path stripped off
    char CompiledName[X_MAX_PATH];
    x_makepath( CompiledName, NULL, NULL, xfs("%s%s", InputFName, NameDecoration), "xbmp" );
    MapInfo.CompiledBitmapName = CompiledName;

    // and the output name contains the full path to the output file
    char OutputName[X_MAX_PATH];
    x_makepath( OutputName, OutputDrive, OutputDir, xfs("%s%s", InputFName, NameDecoration), "xbmp" );
    MapInfo.OutputBitmapName = OutputName;

    // additional textures will come from the virtual texture
    if( iVTexture != -1 )
    {
        char Drive[X_MAX_DRIVE];
        char Dir[X_MAX_DIR];
        char FName[X_MAX_FNAME];
        char Ext[X_MAX_EXT];

        // If we've overridden the default texture, we need to take care of that now.
        // An artist might choose to do this to save memory. For example, if the
        // default bitmap is used in 9 levels, but the 10th level only uses the
        // other virtual texture positions, then they would do this to avoid
        // the need for custom max objects just for that 10th level.
        const geom_rsc_desc::virtual_texture& VTexture = m_pGeomRscDesc->GetVirtualTexture(iVTexture);
        if( VTexture.OverrideDefault )
        {
            if( x_strlen( VTexture.OverrideFileName ) == 0 )
            {
                ThrowError( xfs("Diffuse override filename is invalid for virtual texture[%d] - [%s]", iVTexture, VTexture.Name) );
            }

            // the input name is just a straight copy from the override
            MapInfo.InputBitmapName = VTexture.OverrideFileName;

            // the compiled name is decorated with the checksum, is an xbmp, and has
            // had the path stripped off
            x_splitpath( MapInfo.InputBitmapName, Drive, Dir, FName, Ext );
            x_makepath( CompiledName, NULL, NULL, xfs("%s%d", FName, CheckSum), "xbmp" );
            MapInfo.CompiledBitmapName = CompiledName;

            // and the output name contains the full path to the output file
            x_makepath( OutputName, OutputDrive, OutputDir, xfs("%s%d", FName, CheckSum), "xbmp" );
            MapInfo.OutputBitmapName = OutputName;

        }

        // and now handle the rest of the virtual textures
        s32 i;
        for( i = 1; i < VTexture.Textures.GetCount(); i++ )
        {
            const geom_rsc_desc::texture_info& TexInfo = VTexture.Textures[i];
            
            // add this virtual texture
            map_info& VMapInfo = Map.MapList.Append();

            // the input name is just a straight copy
            VMapInfo.InputBitmapName = TexInfo.FileName;

            if( x_strlen( VMapInfo.InputBitmapName ) == 0 )
            {
                ThrowError( xfs("Invalid diffuse map for virtual texture[%d,%d] - [%s]", iVTexture, i, VTexture.Name) );
            }

            // the compiled name is decorated with the checksum, is an xbmp, and has
            // had the path stripped off
            x_splitpath( TexInfo.FileName, Drive, Dir, FName, Ext );
            x_makepath( CompiledName, NULL, NULL, xfs("%s%d", FName, CheckSum), "xbmp" );
            VMapInfo.CompiledBitmapName = CompiledName;

            // and the output name contains the full path to the output file
            x_makepath( OutputName, OutputDrive, OutputDir, xfs("%s%d", FName, CheckSum), "xbmp" );
            VMapInfo.OutputBitmapName = OutputName;

        }
    }
}

//=============================================================================

void geom_compiler::FillMapSlots( mesh& Mesh )
{
    s32 i, j;
    for( i = 0; i <  Mesh.Material.GetCount(); i++ )
    {
        // Get the material from the RawMesh
        material&           Mat     = Mesh.Material[i];
        const rawmesh2&     RawMesh = *Mat.pRawMesh;
        rawmesh2::material& RawMat  = RawMesh.m_pMaterial[Mat.iRawMaterial];

        //
        // Determine whether or not we need an environment map
        //        
        xbool   bNeedsEnviroment = FALSE;

        switch( RawMat.Type )
        {
        case Material_Diff : 
        case Material_Alpha :
        case Material_Diff_PerPixelIllum :
        case Material_Alpha_PerPixelIllum :
        case Material_Alpha_PerPolyIllum :
        case Material_Distortion :
            break;

        case Material_Distortion_PerPolyEnv :
            bNeedsEnviroment = TRUE;
            break;

        case Material_Diff_PerPixelEnv :
        case Material_Alpha_PerPolyEnv :
            if ( RawMat.Constants[ENV_TYPE].Current[0] != 0.0f )
            {
                bNeedsEnviroment = TRUE;
            }
            break;

        default :
            ThrowError( xfs( "Unknown Material Type %d", RawMat.Type ) );
            break;
        }

        // Set whether we have a punch-through image
        xbool bHasPunchThrough = (RawMat.Map[Max_PunchThrough].iTexture) != -1;

        // Set up the texture names and compile them if necessary
        for( j = 0; j < NumMaps; j++ )
        {
            // some maps don't need to be compiled at all or are not supported
            if ( (j == Max_Diffuse2)         ||
                 (j == Max_Blend)            ||
                 (j == Max_LightMap)         ||
                 (j == Max_Opacity)          ||
                 (j == Max_Intensity)        ||
                 (j == Max_SelfIllumination) ||
                 (j == Max_PunchThrough) )
            {
                continue;
            }

            // Get the texture index from the material
            s32 iTexture = RawMat.Map[j].iTexture;

            // check for a valid texture index
            if( iTexture >= RawMesh.m_nTextures )
                ThrowError( xfs( "Invalid texture index %d (max %d)\n", iTexture, RawMesh.m_nTextures ) );

            // Is this map used?
            if( iTexture < 0 )
                continue;

            // make sure this map is really necessary
            if( !bNeedsEnviroment && (j == Max_Environment) )
            {
                Mat.Maps[j].MapList.Clear();
                continue;
            }

            // check validity of texture path
            const char* pFileName = RawMesh.m_pTexture[iTexture].FileName;
            if( x_stristr( pFileName, m_TexturePath ) == 0 )
            {
                ThrowError( xfs("Texture [%s] is not in path [%s]", pFileName, m_TexturePath) );
            }

            // If we're baking in a punch-through map, we'll need to append all the compiled
            // files with a checksum. Calculate that now.
            u32 CheckSum = 0;
            if( (j == Max_Diffuse1) && bHasPunchThrough )
            {
                s32 Index = RawMat.Map[Max_PunchThrough].iTexture;
                char PathName[256];
                x_strcpy( PathName, RawMesh.m_pTexture[Index].FileName );
                x_strtoupper( PathName );
                CheckSum += x_chksum( PathName, x_strlen( PathName ) );
            }

            // Fill in the source and output map names.
            Mat.Maps[j].MapList.Clear();
            if( j == Max_Diffuse1 )
            {
                ComputeMapNames( Mat.Maps[j],
                                 j,
                                 CheckSum,
                                 Mat.TexInfo.PreferredBPP,
                                 RawMesh.m_pTexture[iTexture].FileName );
            }
            else
            {
                ComputeMapNames( Mat.Maps[j],
                                 j,
                                 CheckSum,
                                 Mat.TexInfo.PreferredBPP,
                                 RawMesh.m_pTexture[iTexture].FileName );
            }
        }

        // process the diffuse map(s)
        if( Mat.Maps[Max_Diffuse1].MapList.GetCount() == 0 )
        {
            ThrowError( "No diffuse map specified!" );
        }
        else
        {
            for( j = 0; j < Mat.Maps[Max_Diffuse1].MapList.GetCount(); j++ )
            {
                map_info& MapInfo = Mat.Maps[Max_Diffuse1].MapList[j];
                ReadBitmap( MapInfo );

                if( bHasPunchThrough )
                {
                    s32 Index = RawMat.Map[Max_PunchThrough].iTexture;
                    ProcessPunchThruMap( MapInfo, RawMesh.m_pTexture[Index].FileName );
                }

                ExportDiffuse( MapInfo.Bitmap,
                               MapInfo.OutputBitmapName,
                               Mat.TexInfo.PreferredBPP,
                               Mat.TexInfo.nMipsToBuild );
            }
        }

        // process the environment map
        if( bNeedsEnviroment )
        {
            if( Mat.Maps[Max_Environment].MapList.GetCount() == 0 )
            {
                ThrowError( "No environment map specified!" );
            }
            else
            {
                map_info& MapInfo = Mat.Maps[Max_Environment].MapList[0];
                ReadBitmap( MapInfo );
                ExportEnvironment( MapInfo.Bitmap, MapInfo.OutputBitmapName );
            }
        }

        // process the detail map
        if( Mat.Maps[Max_DetailMap].MapList.GetCount() )
        {
            map_info& MapInfo = Mat.Maps[Max_DetailMap].MapList[0];
            ReadBitmap( MapInfo );
            ExportDetail( MapInfo.Bitmap, MapInfo.OutputBitmapName );
        }
    }
}

//=============================================================================

void geom_compiler::ExportMaterial( mesh& Mesh, geom& Geom )
{
    s32 i, j, k;

    // generate the compiled bitmaps and fill in the mat slots appropriately
    FillMapSlots( Mesh );

    // allocate space for the materials
    Geom.m_nMaterials = Mesh.Material.GetCount();
    Geom.m_pMaterial  = new geom::material[Geom.m_nMaterials];

    // allocate space for the uv animations
    Geom.m_nUVKeys    = 0;
    Geom.m_pUVKey     = new geom::uvkey[ MaxUVKeys ];

    // build up the materials
    s32 nTotalTextures = 0;
    s32 nTotalVMats    = 0;
    for( i = 0; i < Mesh.Material.GetCount(); i++ )
    {
        material&           MeshMat = Mesh.Material[i];
        rawmesh2::material& RawMat  = MeshMat.pRawMesh->m_pMaterial[MeshMat.iRawMaterial];
        geom::material&     Mat     = Geom.m_pMaterial[i];

        // fill in the basic material info
        Mat.DetailScale  = RawMat.Constants[DETAIL_SCALE].Current[0];
        Mat.FixedAlpha   = RawMat.Constants[FIXED_ALPHA].Current[0];
        Mat.Flags        = 0;
        Mat.Type         = RawMat.Type;
        Mat.nTextures    = MeshMat.Maps[Max_Diffuse1].MapList.GetCount() +
                           MeshMat.Maps[Max_Environment].MapList.GetCount() +
                           MeshMat.Maps[Max_DetailMap].MapList.GetCount();
        Mat.iTexture     = nTotalTextures;
        Mat.nVirtualMats = MeshMat.Maps[Max_Diffuse1].MapList.GetCount();
        Mat.iVirtualMat  = nTotalVMats;
        if( RawMat.bTwoSided )
            Mat.Flags |= geom::material::FLAG_DOUBLE_SIDED;
        if( MeshMat.Maps[Max_Environment].MapList.GetCount() )
            Mat.Flags |= geom::material::FLAG_HAS_ENV_MAP;
        if( MeshMat.Maps[Max_DetailMap].MapList.GetCount() )
            Mat.Flags |= geom::material::FLAG_HAS_DETAIL_MAP;
        if( RawMat.Constants[ENV_TYPE].Current[0] == 0.0f )
            Mat.Flags |= geom::material::FLAG_ENV_CUBE_MAP;
        else if( RawMat.Constants[ENV_TYPE].Current[0] == 1.0f )
            Mat.Flags |= geom::material::FLAG_ENV_VIEW_SPACE;
        else
            Mat.Flags |= geom::material::FLAG_ENV_WORLD_SPACE;
        if( RawMat.Constants[FORCE_ZFILL].Current[0] )
            Mat.Flags |= geom::material::FLAG_FORCE_ZFILL;
        if( RawMat.Constants[USE_DIFFUSE].Current[0] )
            Mat.Flags |= geom::material::FLAG_ILLUM_USES_DIFFUSE;
        if( RawMat.Map[Max_PunchThrough].iTexture >= 0 )
            Mat.Flags |= geom::material::FLAG_IS_PUNCH_THRU;
        if( RawMat.Constants[ENV_BLEND].Current[0] == 1.0f )
            Mat.Flags |= geom::material::FLAG_IS_ADDITIVE;
        else if( RawMat.Constants[ENV_BLEND].Current[0] == 2.0f )
            Mat.Flags |= geom::material::FLAG_IS_SUBTRACTIVE;

        // copy out the uv animation data
        ExportUVAnimation( RawMat, MeshMat.pRawMesh->m_pParamKey, Mat, Geom );

        // and increment our counters
        nTotalTextures += Mat.nTextures;
        nTotalVMats    += Mat.nVirtualMats;
    }

    // create space for the virtual materials
    Geom.m_nVirtualMaterials = nTotalVMats;

    // allocate space for the textures
    Geom.m_nTextures = nTotalTextures;
    Geom.m_pTexture  = new geom::texture[nTotalTextures];

    // set up the texture file names
    nTotalTextures = 0;
    for( i = 0; i < Mesh.Material.GetCount(); i++ )
    {
        material&           MeshMat = Mesh.Material[i];
        geom::material&     Mat     = Geom.m_pMaterial[i];

        // set up the diffuse texture
        for( j = 0; j < Mat.nVirtualMats; j++ )
        {
            Geom.m_pTexture[nTotalTextures].DescOffset     = m_Dictionary.Add( "" );
            Geom.m_pTexture[nTotalTextures].FileNameOffset = m_Dictionary.Add( MeshMat.Maps[Max_Diffuse1].MapList[j].CompiledBitmapName );
            nTotalTextures++;
        }

        // set up the environment map
        if( MeshMat.Maps[Max_Environment].MapList.GetCount() )
        {
            Geom.m_pTexture[nTotalTextures].DescOffset     = m_Dictionary.Add( "" );
            Geom.m_pTexture[nTotalTextures].FileNameOffset = m_Dictionary.Add( MeshMat.Maps[Max_Environment].MapList[0].CompiledBitmapName );
            nTotalTextures++;
        }

        // set up the detail map
        if( MeshMat.Maps[Max_DetailMap].MapList.GetCount() )
        {
            Geom.m_pTexture[nTotalTextures].DescOffset     = m_Dictionary.Add( "" );
            Geom.m_pTexture[nTotalTextures].FileNameOffset = m_Dictionary.Add( MeshMat.Maps[Max_DetailMap].MapList[0].CompiledBitmapName );
            nTotalTextures++;
        }
    }
    ASSERT( nTotalTextures == Geom.m_nTextures );

    // create a list of virtual textures based on materials
    xarray<geom::virtual_texture> VirtualTextures;
    VirtualTextures.Clear();
    for( i = 0; i < m_pGeomRscDesc->GetVirtualTextureCount(); i++ )
    {
        u32 MatMask = 0;

        // grab a handy reference to the resource virtual texture
        const geom_rsc_desc::virtual_texture& RscVTex = m_pGeomRscDesc->GetVirtualTexture( i );
        
        // grab the file name, sans dir, drive, and ext
        char RscFileName[X_MAX_FNAME];
        x_splitpath( RscVTex.Textures[0].FileName, NULL, NULL, RscFileName, NULL );

        // loop through all the materials, and see if we have a match for this virtual texture    
        for( j = 0; j < Geom.m_nMaterials; j++ )
        {
            material& MeshMat = Mesh.Material[j];
            if( (MeshMat.Maps[Max_Diffuse1].MapList.GetCount() > 1) &&
                (RscVTex.Textures.GetCount() > 1) )
            {
                char MapFileName[X_MAX_FNAME];
                x_splitpath( MeshMat.Maps[Max_Diffuse1].MapList[0].InputBitmapName, NULL, NULL, MapFileName, NULL );
                if( !x_stricmp( RscFileName, MapFileName ) )
                {
                    // we've got a material match!
                    MatMask |= (1<<j);

                    // sanity check
                    if( RscVTex.Textures.GetCount() != MeshMat.Maps[Max_Diffuse1].MapList.GetCount() )
                    {
                        ThrowError( "Virtual texture count mismatch. Two virtual textures effecting the same diffuse?" );
                    }

                    // fill in the more descriptive texture names
                    for( k = 0; k < RscVTex.Textures.GetCount(); k++ )
                    {
                        Geom.m_pTexture[Geom.m_pMaterial[j].iTexture+k].DescOffset = m_Dictionary.Add( RscVTex.Textures[k].Name );
                    }
                }
            }
        }

        // if this virtual texture is used, then mark it to be added to
        // the geom data
        if( MatMask != 0 )
        {
            geom::virtual_texture& GeomVTex = VirtualTextures.Append();
            GeomVTex.MaterialMask = MatMask;
            GeomVTex.NameOffset   = m_Dictionary.Add( RscVTex.Name );
        }
    }

    // now we can set up a proper virtual texture array for the geom
    Geom.m_nVirtualTextures = VirtualTextures.GetCount();
    Geom.m_pVirtualTextures = new geom::virtual_texture[Geom.m_nVirtualTextures];
    for( i = 0; i < Geom.m_nVirtualTextures; i++ )
    {
        Geom.m_pVirtualTextures[i] = VirtualTextures[i];
    }
    
#ifdef X_DEBUG
    for( i = 0; i < Geom.m_nMaterials; i++ )
    {
        geom::material& GeomMat = Geom.m_pMaterial[i];
        ASSERT( (GeomMat.iTexture>=0) && (GeomMat.iTexture<Geom.m_nTextures) );
    }
#endif
}

//=============================================================================

void geom_compiler::ExportVirtualMeshes( mesh& Mesh, geom& Geom )
{
    Geom.m_nVirtualMeshes = m_pGeomRscDesc->GetVirtualMeshCount();
    Geom.m_pVirtualMeshes = NULL;
    Geom.m_nLODs          = 0;
    Geom.m_pLODSizes      = NULL;
    Geom.m_pLODMasks      = 0;
    
    if( Geom.m_nVirtualMeshes )
    {
        // count up how many lods we will need
        s32 i, j, k;
        for( i = 0; i < Geom.m_nVirtualMeshes; i++ )
        {
            const geom_rsc_desc::virtual_mesh& VMesh = m_pGeomRscDesc->GetVirtualMesh( i );
            Geom.m_nLODs += VMesh.LODs.GetCount();
        }

        // allocate space for the new data
        Geom.m_pLODSizes      = new u16[Geom.m_nLODs];
        Geom.m_pLODMasks      = new u64[Geom.m_nLODs];
        Geom.m_pVirtualMeshes = new geom::virtual_mesh[Geom.m_nVirtualMeshes];

        // fill in the virtual mesh data
        s32 nLODsAdded = 0;
        for( i = 0; i < Geom.m_nVirtualMeshes; i++ )
        {
            const geom_rsc_desc::virtual_mesh& VMesh   = m_pGeomRscDesc->GetVirtualMesh( i );
            geom::virtual_mesh&                DstMesh = Geom.m_pVirtualMeshes[i];
            DstMesh.iLOD       = nLODsAdded;
            DstMesh.nLODs      = VMesh.LODs.GetCount();
            DstMesh.NameOffset = m_Dictionary.Add( VMesh.Name );
            nLODsAdded += VMesh.LODs.GetCount();

            // add the LODs
            for( j = 0; j < VMesh.LODs.GetCount(); j++ )
            {
                geom_rsc_desc::lod_info& SrcLOD = VMesh.LODs[j];
                Geom.m_pLODSizes[DstMesh.iLOD + j] = (u16)SrcLOD.ScreenSize;
                Geom.m_pLODMasks[DstMesh.iLOD + j] = 0;
                for( k = 0; k < SrcLOD.nMeshes; k++ )
                {
                    // note that we can't use the normal GetMeshIndex function
                    // because we're still working inside the dictionary!
                    s32 MeshId;
                    for( MeshId = 0; MeshId < Geom.m_nMeshes; MeshId++ )
                    {
                        if( !x_strcmp( m_Dictionary.GetString( Geom.m_pMesh[MeshId].NameOffset ),
                                       SrcLOD.MeshName[k] ) )
                        {
                            Geom.m_pLODMasks[DstMesh.iLOD + j] |= ((u64)1<<MeshId);
                            break;
                        }
                    }

                    if( MeshId == Geom.m_nMeshes )
                    {
                        ReportWarning( xfs( "VMesh (%s) refers to mesh (%s) which isn't present in the geometry",
                                            VMesh.Name, SrcLOD.MeshName[k] ) );
                    }
                }
            }
        }
    }
}

//=============================================================================
//=============================================================================
//=============================================================================
// BUILDING LOW POLY COLLISION GEOMETRY
//=============================================================================
//=============================================================================
//=============================================================================


