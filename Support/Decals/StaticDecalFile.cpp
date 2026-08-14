//==============================================================================
//
//  StaticDecalFile.cpp
//
//==============================================================================

#include "StaticDecalFile.hpp"
#include "Auxiliary/MiscUtils/BitseryIO.hpp"

#include <vector>

namespace
{

enum
{
    STATIC_DECAL_FILE_VERSION = 1,
    MAX_STATIC_PACKAGES       = 4096,
    MAX_STATIC_DEFINITIONS    = 65536,
    MAX_STATIC_ZONES          = 262144,
    MAX_STATIC_VERTICES       = 16 * 1024 * 1024,
    STATIC_VERTEX_FLAG_MASK   = decal_mgr::decal_vert::FLAG_SKIP_TRIANGLE,
};

const bitsery_io::file_format STATIC_DECAL_FILE_FORMAT =
{
    { 'S', 'D', 'C', 'L' },
    STATIC_DECAL_FILE_VERSION,
};

//==============================================================================

struct package_data
{
    package_data( void ) : DefinitionStart( 0 ), DefinitionCount( 0 )
    {
        Name[0] = '\0';
    }

    char Name[256];
    s32  DefinitionStart;
    s32  DefinitionCount;
};

//==============================================================================

struct definition_data
{
    definition_data( void ) :
        GroupIndex     ( 0 ),
        DecalIndex     ( 0 ),
        ZoneStart      ( 0 ),
        ZoneCount      ( 0 )
    {
    }

    s32 GroupIndex;
    s32 DecalIndex;
    s32 ZoneStart;
    s32 ZoneCount;
};

//==============================================================================

struct zone_data
{
    zone_data( void ) : VertexStart( 0 ), VertexCount( 0 ), Zone( 0 )
    {
    }

    s32 VertexStart;
    s32 VertexCount;
    u32 Zone;
};

//==============================================================================

struct vertex_data
{
    vertex_data( void ) :
        X( 0.0f ), Y( 0.0f ), Z( 0.0f ), Flags( 0 ), U( 0 ), V( 0 ), Color( 0 )
    {
    }

    f32 X;
    f32 Y;
    f32 Z;
    u32 Flags;
    s16 U;
    s16 V;
    u32 Color;
};

//==============================================================================

struct static_decal_data
{
    std::vector<package_data>    Packages;
    std::vector<definition_data> Definitions;
    std::vector<zone_data>       Zones;
    std::vector<vertex_data>     Vertices;
};

//==============================================================================

template<class SERIALIZER>
void serialize( SERIALIZER& Serializer, package_data& Value )
{
    Serializer.text1b( Value.Name );
    Serializer.value4b( Value.DefinitionStart );
    Serializer.value4b( Value.DefinitionCount );
}

//==============================================================================

template<class SERIALIZER>
void serialize( SERIALIZER& Serializer, definition_data& Value )
{
    Serializer.value4b( Value.GroupIndex );
    Serializer.value4b( Value.DecalIndex );
    Serializer.value4b( Value.ZoneStart );
    Serializer.value4b( Value.ZoneCount );
}

//==============================================================================

template<class SERIALIZER>
void serialize( SERIALIZER& Serializer, zone_data& Value )
{
    Serializer.value4b( Value.VertexStart );
    Serializer.value4b( Value.VertexCount );
    Serializer.value4b( Value.Zone );
}

//==============================================================================

template<class SERIALIZER>
void serialize( SERIALIZER& Serializer, vertex_data& Value )
{
    Serializer.value4b( Value.X );
    Serializer.value4b( Value.Y );
    Serializer.value4b( Value.Z );
    Serializer.value4b( Value.Flags );
    Serializer.value2b( Value.U );
    Serializer.value2b( Value.V );
    Serializer.value4b( Value.Color );
}

//==============================================================================

template<class TYPE>
void WriteObjects( bitsery::Serializer<bitsery_io::output_adapter>& Serializer,
                   std::vector<TYPE>&                                Values )
{
    u32 Count = (u32)Values.size();
    Serializer.value4b( Count );
    for( TYPE& Value : Values )
    {
        Serializer.object( Value );
    }
}

//==============================================================================

template<class TYPE>
xbool ReadObjects( bitsery::Deserializer<bitsery_io::input_adapter>& Serializer,
                   std::vector<TYPE>&                                 Values,
                   u32                                                MaxCount )
{
    u32 Count = 0;
    Serializer.value4b( Count );
    if( Count > MaxCount )
    {
        bitsery_io::SetInvalidData( Serializer );
        Values.clear();
        return( FALSE );
    }

    Values.resize( Count );
    for( TYPE& Value : Values )
    {
        Serializer.object( Value );
    }
    return( TRUE );
}

//==============================================================================

void serialize( bitsery::Serializer<bitsery_io::output_adapter>& Serializer,
                static_decal_data&                               Value )
{
    WriteObjects( Serializer, Value.Packages );
    WriteObjects( Serializer, Value.Definitions );
    WriteObjects( Serializer, Value.Zones );
    WriteObjects( Serializer, Value.Vertices );
}

//==============================================================================

void serialize( bitsery::Deserializer<bitsery_io::input_adapter>& Serializer,
                static_decal_data&                                Value )
{
    if( !ReadObjects( Serializer, Value.Packages, MAX_STATIC_PACKAGES ) ||
        !ReadObjects( Serializer, Value.Definitions, MAX_STATIC_DEFINITIONS ) ||
        !ReadObjects( Serializer, Value.Zones, MAX_STATIC_ZONES ) ||
        !ReadObjects( Serializer, Value.Vertices, MAX_STATIC_VERTICES ) )
    {
        Value.Packages.clear();
        Value.Definitions.clear();
        Value.Zones.clear();
        Value.Vertices.clear();
    }
}

//==============================================================================

xbool RangeIsValid( s32 Start, s32 Count, s32 Limit )
{
    return( (Start >= 0) &&
            (Count >= 0) &&
            (Start <= Limit) &&
            (Count <= (Limit - Start)) );
}

//==============================================================================

xbool HasTerminator( const char* pText, s32 Capacity )
{
    if( !pText || (Capacity < 0) )
    {
        return( FALSE );
    }

    for( s32 i = 0; i < Capacity; i++ )
    {
        if( pText[i] == '\0' )
        {
            return( TRUE );
        }
    }
    return( FALSE );
}

//==============================================================================

xbool ValidateRuntime( const decal_mgr::static_data& Data, xstring& Error )
{
    if( (Data.nPackages < 0) || (Data.nPackages > MAX_STATIC_PACKAGES) ||
        (Data.nDefinitions < 0) || (Data.nDefinitions > MAX_STATIC_DEFINITIONS) ||
        (Data.nZones < 0) || (Data.nZones > MAX_STATIC_ZONES) ||
        (Data.nVerts < 0) || (Data.nVerts > MAX_STATIC_VERTICES) )
    {
        return( bitsery_io::Fail( Error, "Static decal count is invalid." ) );
    }
    if( ((Data.nPackages > 0) && !Data.pPackage) ||
        ((Data.nDefinitions > 0) && !Data.pDefinition) ||
        ((Data.nZones > 0) && !Data.pZone) ||
        ((Data.nVerts > 0) && (!Data.pPos || !Data.pUV || !Data.pColor)) )
    {
        return( bitsery_io::Fail( Error, "Static decal array is missing." ) );
    }

    for( s32 i = 0; i < Data.nPackages; i++ )
    {
        if( !HasTerminator( Data.pPackage[i].PackageName, 256 ) ||
            !RangeIsValid( Data.pPackage[i].iDefinition,
                           Data.pPackage[i].nDefinitions,
                           Data.nDefinitions ) )
        {
            return( bitsery_io::Fail( Error, "Static decal package is invalid." ) );
        }
    }
    for( s32 i = 0; i < Data.nDefinitions; i++ )
    {
        if( (Data.pDefinition[i].iGroup < 0) ||
            (Data.pDefinition[i].iDecalDef < 0) ||
            !RangeIsValid( Data.pDefinition[i].iZoneInfo,
                           Data.pDefinition[i].nZones,
                           Data.nZones ) )
        {
            return( bitsery_io::Fail( Error, "Static decal definition is invalid." ) );
        }
    }
    for( s32 i = 0; i < Data.nZones; i++ )
    {
        if( !RangeIsValid( Data.pZone[i].iVert,
                           Data.pZone[i].nVerts,
                           Data.nVerts ) )
        {
            return( bitsery_io::Fail( Error, "Static decal zone is invalid." ) );
        }
    }
    for( s32 i = 0; i < Data.nVerts; i++ )
    {
        if( !x_isvalid( Data.pPos[i].Pos.X ) ||
            !x_isvalid( Data.pPos[i].Pos.Y ) ||
            !x_isvalid( Data.pPos[i].Pos.Z ) ||
            (Data.pPos[i].Flags & ~STATIC_VERTEX_FLAG_MASK) )
        {
            return( bitsery_io::Fail( Error, "Static decal vertex is invalid." ) );
        }
    }

    return( TRUE );
}

//==============================================================================

xbool Validate( const static_decal_data& Data, xstring& Error )
{
    if( Data.Packages.size() > MAX_STATIC_PACKAGES )
    {
        return( bitsery_io::Fail( Error, "Static decal package count is too large." ) );
    }
    if( Data.Definitions.size() > MAX_STATIC_DEFINITIONS )
    {
        return( bitsery_io::Fail( Error, "Static decal definition count is too large." ) );
    }
    if( Data.Zones.size() > MAX_STATIC_ZONES )
    {
        return( bitsery_io::Fail( Error, "Static decal zone count is too large." ) );
    }
    if( Data.Vertices.size() > MAX_STATIC_VERTICES )
    {
        return( bitsery_io::Fail( Error, "Static decal vertex count is too large." ) );
    }

    const s32 DefinitionCount = (s32)Data.Definitions.size();
    for( const package_data& Package : Data.Packages )
    {
        if( !RangeIsValid( Package.DefinitionStart,
                           Package.DefinitionCount,
                           DefinitionCount ) )
        {
            return( bitsery_io::Fail( Error, "Static decal package range is invalid." ) );
        }
    }

    const s32 ZoneCount = (s32)Data.Zones.size();
    for( const definition_data& Definition : Data.Definitions )
    {
        if( (Definition.GroupIndex < 0) || (Definition.DecalIndex < 0) ||
            !RangeIsValid( Definition.ZoneStart,
                           Definition.ZoneCount,
                           ZoneCount ) )
        {
            return( bitsery_io::Fail( Error, "Static decal definition is invalid." ) );
        }
    }

    const s32 VertexCount = (s32)Data.Vertices.size();
    for( const zone_data& Zone : Data.Zones )
    {
        if( !RangeIsValid( Zone.VertexStart, Zone.VertexCount, VertexCount ) )
        {
            return( bitsery_io::Fail( Error, "Static decal zone range is invalid." ) );
        }
    }

    for( const vertex_data& Vertex : Data.Vertices )
    {
        if( !x_isvalid( Vertex.X ) ||
            !x_isvalid( Vertex.Y ) ||
            !x_isvalid( Vertex.Z ) )
        {
            return( bitsery_io::Fail( Error, "Static decal contains a non-finite position." ) );
        }
        if( Vertex.Flags & ~STATIC_VERTEX_FLAG_MASK )
        {
            return( bitsery_io::Fail( Error, "Static decal contains unsupported vertex flags." ) );
        }
    }

    return( TRUE );
}

//==============================================================================

void MakeData( const decal_mgr::static_data& Source, static_decal_data& Target )
{
    Target.Packages.resize( Source.nPackages );
    for( s32 i = 0; i < Source.nPackages; i++ )
    {
        x_strsavecpy( Target.Packages[i].Name,
                      Source.pPackage[i].PackageName,
                      256 );
        Target.Packages[i].DefinitionStart = Source.pPackage[i].iDefinition;
        Target.Packages[i].DefinitionCount = Source.pPackage[i].nDefinitions;
    }

    Target.Definitions.resize( Source.nDefinitions );
    for( s32 i = 0; i < Source.nDefinitions; i++ )
    {
        Target.Definitions[i].GroupIndex = Source.pDefinition[i].iGroup;
        Target.Definitions[i].DecalIndex = Source.pDefinition[i].iDecalDef;
        Target.Definitions[i].ZoneStart  = Source.pDefinition[i].iZoneInfo;
        Target.Definitions[i].ZoneCount  = Source.pDefinition[i].nZones;
    }

    Target.Zones.resize( Source.nZones );
    for( s32 i = 0; i < Source.nZones; i++ )
    {
        Target.Zones[i].VertexStart = Source.pZone[i].iVert;
        Target.Zones[i].VertexCount = Source.pZone[i].nVerts;
        Target.Zones[i].Zone        = Source.pZone[i].Zone;
    }

    Target.Vertices.resize( Source.nVerts );
    for( s32 i = 0; i < Source.nVerts; i++ )
    {
        Target.Vertices[i].X     = Source.pPos[i].Pos.X;
        Target.Vertices[i].Y     = Source.pPos[i].Pos.Y;
        Target.Vertices[i].Z     = Source.pPos[i].Pos.Z;
        Target.Vertices[i].Flags = Source.pPos[i].Flags;
        Target.Vertices[i].U     = Source.pUV[i].U;
        Target.Vertices[i].V     = Source.pUV[i].V;
        Target.Vertices[i].Color = Source.pColor[i];
    }
}

//==============================================================================

decal_mgr::static_data* MakeStaticData( const static_decal_data& Source )
{
    decal_mgr::static_data* pTarget = new decal_mgr::static_data;

    pTarget->nPackages = (s32)Source.Packages.size();
    pTarget->pPackage = pTarget->nPackages > 0
                      ? new decal_mgr::static_data::package[pTarget->nPackages]
                      : NULL;
    for( s32 i = 0; i < pTarget->nPackages; i++ )
    {
        x_strsavecpy( pTarget->pPackage[i].PackageName,
                      Source.Packages[i].Name,
                      256 );
        pTarget->pPackage[i].iDefinition  = Source.Packages[i].DefinitionStart;
        pTarget->pPackage[i].nDefinitions = Source.Packages[i].DefinitionCount;
    }

    pTarget->nDefinitions = (s32)Source.Definitions.size();
    pTarget->pDefinition = pTarget->nDefinitions > 0
                         ? new decal_mgr::static_data::definition[pTarget->nDefinitions]
                         : NULL;
    for( s32 i = 0; i < pTarget->nDefinitions; i++ )
    {
        pTarget->pDefinition[i].iGroup    = Source.Definitions[i].GroupIndex;
        pTarget->pDefinition[i].iDecalDef = Source.Definitions[i].DecalIndex;
        pTarget->pDefinition[i].iZoneInfo = Source.Definitions[i].ZoneStart;
        pTarget->pDefinition[i].nZones    = Source.Definitions[i].ZoneCount;
    }

    pTarget->nZones = (s32)Source.Zones.size();
    pTarget->pZone = pTarget->nZones > 0
                   ? new decal_mgr::static_data::zone_info[pTarget->nZones]
                   : NULL;
    for( s32 i = 0; i < pTarget->nZones; i++ )
    {
        pTarget->pZone[i].iVert  = Source.Zones[i].VertexStart;
        pTarget->pZone[i].nVerts = Source.Zones[i].VertexCount;
        pTarget->pZone[i].Zone   = Source.Zones[i].Zone;
    }

    pTarget->nVerts = (s32)Source.Vertices.size();
    pTarget->pPos = pTarget->nVerts > 0
                  ? new decal_mgr::position_data[pTarget->nVerts]
                  : NULL;
    pTarget->pUV = pTarget->nVerts > 0
                 ? new decal_mgr::uv_data[pTarget->nVerts]
                 : NULL;
    pTarget->pColor = pTarget->nVerts > 0 ? new u32[pTarget->nVerts] : NULL;
    for( s32 i = 0; i < pTarget->nVerts; i++ )
    {
        pTarget->pPos[i].Pos.Set( Source.Vertices[i].X,
                                  Source.Vertices[i].Y,
                                  Source.Vertices[i].Z );
        pTarget->pPos[i].Flags = Source.Vertices[i].Flags;
        pTarget->pUV[i].U      = Source.Vertices[i].U;
        pTarget->pUV[i].V      = Source.Vertices[i].V;
        pTarget->pColor[i]     = Source.Vertices[i].Color;
    }

    return( pTarget );
}

} // namespace

//==============================================================================

decal_mgr::static_data::static_data( void ) :
    nPackages   ( 0 ),
    pPackage    ( NULL ),
    nDefinitions( 0 ),
    pDefinition ( NULL ),
    nZones      ( 0 ),
    pZone       ( NULL ),
    nVerts      ( 0 ),
    pPos        ( NULL ),
    pUV         ( NULL ),
    pColor      ( NULL )
{
}

//==============================================================================

decal_mgr::static_data::~static_data( void )
{
    delete []pPackage;
    delete []pDefinition;
    delete []pZone;
    delete []pPos;
    delete []pUV;
    delete []pColor;
}

//==============================================================================

xbool static_decal_file::Load( X_FILE*                  pFile,
                               decal_mgr::static_data*& pData,
                               xstring&                 Error )
{
    pData = NULL;

    static_decal_data Data;
    if( !bitsery_io::Read( pFile, STATIC_DECAL_FILE_FORMAT, Data, Error ) ||
        !Validate( Data, Error ) )
    {
        return( FALSE );
    }

    pData = MakeStaticData( Data );
    return( TRUE );
}

//==============================================================================

xbool static_decal_file::Save( X_FILE*                      pFile,
                               const decal_mgr::static_data& Data,
                               xstring&                     Error )
{
    if( !ValidateRuntime( Data, Error ) )
    {
        return( FALSE );
    }

    static_decal_data Serialized;
    MakeData( Data, Serialized );
    if( !Validate( Serialized, Error ) )
    {
        return( FALSE );
    }

    return( bitsery_io::Write( pFile,
                               STATIC_DECAL_FILE_FORMAT,
                               Serialized,
                               Error ) );
}

//==============================================================================

xbool static_decal_file::Save( const char*                   pFileName,
                               const decal_mgr::static_data& Data,
                               xstring&                     Error )
{
    if( !pFileName || !pFileName[0] )
    {
        return( bitsery_io::Fail( Error, "Static decal output filename is empty." ) );
    }

    X_FILE* pFile = x_fopen( pFileName, "wb" );
    if( !pFile )
    {
        return( bitsery_io::Fail( Error, "Unable to open the static decal output file." ) );
    }

    const xbool Result = Save( pFile, Data, Error );
    x_fclose( pFile );
    return( Result );
}

//==============================================================================
