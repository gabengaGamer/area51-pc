
#ifndef GEOM_COMPILER_HPP
#define GEOM_COMPILER_HPP

#include "x_files.hpp"
#include "RawMesh2.hpp"
#include "texinfo.hpp"
#include "../../Render/geom.hpp"
#include "../../Render/CollisionVolume.hpp"
#include "Auxiliary/MiscUtils/dictionary.hpp"

struct rawmesh2;
struct rawanim;
struct skin_geom;
struct rigid_geom;
class geom_rsc_desc;

class geom_compiler
{
public:
        
    enum comp_type
    {
        TYPE_NONE,
        TYPE_RIGID,
        TYPE_SKIN
    };

            geom_compiler       ( void );
    void    SetUserInfo         ( const char* pUserName, const char* pComputerName );
    void    SetExportDate       ( s32 Month, s32 Day, s32 Year );
    void    ReportWarning       ( const char* pWarning );
    void    ReportError         ( const char* pError );
    void    ThrowError          ( const char* pError );

    void    Export              ( const char* pFileName,
                                  const char* pOutputFile,
                                  comp_type   Type,
                                  const char* pTexturePath );
    void    AddFastCollision    ( const char* pFileName );
    void    SetPhysicsSource    ( const char* pFileName );
    void    SetSettingsFile     ( const char* pFileName );

    void    CompileLowCollision ( rigid_geom&   RigidGeom, 
                                  rawmesh2&     LowMesh, 
                                  rawmesh2&     HighMesh );
   
    void    CompileLowCollisionFromBBox ( rigid_geom& RigidGeom, rawmesh2&   HighMesh );

    void    CompileHighCollisionFromGeometry( rigid_geom& RigidGeom,
                                              collision_data::mat_info* pMatList );

public:

    //
    // Fixed layout of "Map" buttons in 3DS Max material editor
    //

    enum
    {
        Max_Diffuse1,           // 3DS Max Defaults
        Max_Diffuse2,           // 3DS Max Defaults
        Max_Blend,              // 3DS Max Defaults 
        Max_LightMap,           // 3DS Max Defaults 
        Max_Opacity,            // 3DS Max Defaults 
        Max_Intensity,          // Environment Intensity Map
        Max_Environment,        // Environment Map
        Max_SelfIllumination,   // Per-Pixel Self-Illumination
        Max_DetailMap,          // Detail Map
        Max_PunchThrough,       // Punch-Through Map
        NumMaps
    };

protected:

    struct surface
    {
        s32                 iMaterial;
        s32                 iBone;
        xarray<s32>         Facets;
    };

    struct sub_mesh
    {
        char                        Name[256];
        const rawmesh2*             pRawMesh;
        const rawmesh2::sub_mesh*   pRawSubMesh;
        xarray<surface>             Surfaces;
    };

    struct map_info
    {
        xbitmap Bitmap;
        xstring InputBitmapName;
        xstring OutputBitmapName;
        xstring CompiledBitmapName;
    };

    struct map_slot
    {
        xarray<map_info> MapList;
    };

    struct material
    {
        s32                 iRawMaterial;
        const rawmesh2*     pRawMesh;
        tex_info            TexInfo;
        map_slot            Maps[NumMaps];
    };

    struct mesh
    {
        xarray<material>    Material;
        xarray<sub_mesh>    SubMesh;
    };

protected:
    
    void    LoadResource        ( const char* pFileName, xbool bSkin );
    void    LoadSourceMesh      ( const char* pFileName, rawmesh2& Mesh );
    void    LoadSourceAnimation ( const char* pFileName, rawanim& Animation );
    
    void    BuildBone           ( geom::bone& Bone, const rawmesh2::bone& RawBone );

    void    BuildBones          (       geom&     Geom, 
                                  const rawmesh2& GeomRawMesh,
                                  const rawmesh2& PhysicsRawMesh );
    
    void    BuildRigidBody      ( geom::rigid_body& RigidBody, const rawmesh2::rigid_body& RawRigidBody );
    
    void    BuildRigidBodies    (       geom&     Geom, 
                                  const rawmesh2& GeomRawMesh,
                                  const rawmesh2& PhysicsRawMesh );
    
    
    void    BuildSettings       ( geom& Geom, const char* pSettingsFile, const rawmesh2& GeomRawMesh );

    xbool   IsSameMaterial      ( const rawmesh2::material& RawMatA,
                                  const rawmesh2::material& RawMatB,
                                  const rawmesh2&           RawMesh );
    void    BuildCompileModel   ( geom& Geom, const rawmesh2& RawMesh, mesh& Mesh, xbool IsRigid );

    void    RemoveUnusedVMeshes ( rawmesh2& RawMesh );

    void    ExportRigidGeom     ( const char* pFileName );
    void    BuildRigidGeometry  ( mesh& Mesh, rigid_geom& RigidGeom );

    void    ExportSkinGeom      ( const char* pFileName );
    void    BuildSkinGeometry   ( mesh& Mesh, skin_geom& SkinGeom );
    void    ExportUVAnimation   ( const rawmesh2::material& RawMat,
                                  const f32*                pParamKey,
                                  geom::material&           Material,
                                  geom&                     Geom );
    void    ReadBitmap          ( map_info&                 MapInfo );
    void    ComputeMapNames     ( map_slot&                 Map,
                                  s32                       MapType,
                                  u32                       CheckSum,
                                  pref_bpp                  PreferredBPP,
                                  const char*               pInputFile );
    void    FillMapSlots        ( mesh& Mesh );
    void    ExportMaterial      ( mesh& Mesh, geom& Geom );
    void    ExportVirtualMeshes ( mesh& Mesh, geom& Geom );
    void    BuildTexturePath    ( char* pPath, const char* pName );
    void    ExportDiffuse       ( const xbitmap& Bitmap, const char* pName, pref_bpp BPP, s32 nMips );
    void    ExportEnvironment   ( const xbitmap& Bitmap, const char* pName );
    void    ExportDetail        ( const xbitmap& Bitmap, const char* pName );

    void    ProcessPunchThruMap( map_info& DiffuseMap, const char* pPunchThruMap );

    void    CompileDictionary   ( geom& Geom );

    void    PrintSummary        ( geom& Geom );

protected:

    char                m_OutputFile[X_MAX_PATH];
    char                m_TexturePath[X_MAX_PATH] ;
    char                m_FastCollision[X_MAX_PATH];
    char                m_PhysicsSource[X_MAX_PATH];
    char                m_SettingsFile[X_MAX_PATH];
    geom_rsc_desc*      m_pGeomRscDesc;
    dictionary          m_Dictionary;
    xarray<s32>         m_RawMeshToCompiled;
    char                m_UserName[256];
    char                m_ComputerName[256];
    s32                 m_ExportDate[3];
};

#endif
