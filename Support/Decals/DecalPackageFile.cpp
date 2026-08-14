//==============================================================================
//
//  DecalPackageFile.cpp
//
//==============================================================================

#include "DecalPackageFile.hpp"
#include "DecalPackage.hpp"
#include "Auxiliary/MiscUtils/BitseryIO.hpp"

#include <vector>

namespace
{

enum
{
    DECAL_PACKAGE_FILE_VERSION = 1,
    MAX_DECAL_GROUPS           = decal_package_file::MAX_GROUPS,
    MAX_DECAL_DEFINITIONS      = decal_package_file::MAX_DEFINITIONS,
    DECAL_FLAG_MASK            = 0x000000ff,
};

const bitsery_io::file_format DECAL_PACKAGE_FILE_FORMAT =
{
    { 'D', 'C', 'P', 'K' },
    DECAL_PACKAGE_FILE_VERSION,
};

//==============================================================================

struct group_data
{
    group_data( void ) : Color( 0 ), DefinitionCount( 0 ), DefinitionStart( 0 )
    {
        Name[0] = '\0';
    }

    char Name[32];
    u32  Color;
    s32  DefinitionCount;
    s32  DefinitionStart;
};

//==============================================================================

struct definition_data
{
    definition_data( void ) :
        MinWidth ( 0.0f ),
        MinHeight( 0.0f ),
        MaxWidth ( 0.0f ),
        MaxHeight( 0.0f ),
        MinRoll  ( 0.0f ),
        MaxRoll  ( 0.0f ),
        Color    ( 0 ),
        MaxVisible( 0 ),
        FadeTime ( 0.0f ),
        Flags    ( 0 ),
        BlendMode( 0 )
    {
        Name[0]       = '\0';
        BitmapName[0] = '\0';
    }

    char Name[32];
    f32  MinWidth;
    f32  MinHeight;
    f32  MaxWidth;
    f32  MaxHeight;
    f32  MinRoll;
    f32  MaxRoll;
    u32  Color;
    char BitmapName[256];
    u32  MaxVisible;
    f32  FadeTime;
    u32  Flags;
    u16  BlendMode;
};

//==============================================================================

struct package_data
{
    std::vector<group_data>      Groups;
    std::vector<definition_data> Definitions;
};

//==============================================================================

template<class SERIALIZER>
void serialize( SERIALIZER& Serializer, group_data& Value )
{
    Serializer.text1b( Value.Name );
    Serializer.value4b( Value.Color );
    Serializer.value4b( Value.DefinitionCount );
    Serializer.value4b( Value.DefinitionStart );
}

//==============================================================================

template<class SERIALIZER>
void serialize( SERIALIZER& Serializer, definition_data& Value )
{
    Serializer.text1b( Value.Name );
    Serializer.value4b( Value.MinWidth );
    Serializer.value4b( Value.MinHeight );
    Serializer.value4b( Value.MaxWidth );
    Serializer.value4b( Value.MaxHeight );
    Serializer.value4b( Value.MinRoll );
    Serializer.value4b( Value.MaxRoll );
    Serializer.value4b( Value.Color );
    Serializer.text1b( Value.BitmapName );
    Serializer.value4b( Value.MaxVisible );
    Serializer.value4b( Value.FadeTime );
    Serializer.value4b( Value.Flags );
    Serializer.value2b( Value.BlendMode );
}

//==============================================================================

void serialize( bitsery::Serializer<bitsery_io::output_adapter>& Serializer,
                package_data&                                    Value )
{
    u32 GroupCount = (u32)Value.Groups.size();
    Serializer.value4b( GroupCount );
    for( group_data& Group : Value.Groups )
    {
        Serializer.object( Group );
    }

    u32 DefinitionCount = (u32)Value.Definitions.size();
    Serializer.value4b( DefinitionCount );
    for( definition_data& Definition : Value.Definitions )
    {
        Serializer.object( Definition );
    }
}

//==============================================================================

void serialize( bitsery::Deserializer<bitsery_io::input_adapter>& Serializer,
                package_data&                                     Value )
{
    u32 GroupCount = 0;
    Serializer.value4b( GroupCount );
    if( GroupCount > MAX_DECAL_GROUPS )
    {
        bitsery_io::SetInvalidData( Serializer );
        return;
    }

    Value.Groups.resize( GroupCount );
    for( group_data& Group : Value.Groups )
    {
        Serializer.object( Group );
    }

    u32 DefinitionCount = 0;
    Serializer.value4b( DefinitionCount );
    if( DefinitionCount > MAX_DECAL_DEFINITIONS )
    {
        bitsery_io::SetInvalidData( Serializer );
        Value.Groups.clear();
        return;
    }

    Value.Definitions.resize( DefinitionCount );
    for( definition_data& Definition : Value.Definitions )
    {
        Serializer.object( Definition );
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

xbool ValidateData( const package_data& Data, xstring& Error )
{
    if( Data.Groups.size() > MAX_DECAL_GROUPS )
    {
        return( bitsery_io::Fail( Error, "Decal package has too many groups." ) );
    }
    if( Data.Definitions.size() > MAX_DECAL_DEFINITIONS )
    {
        return( bitsery_io::Fail(
            Error,
            "Decal package has too many definitions." ) );
    }

    const s32 DefinitionCount = (s32)Data.Definitions.size();
    for( const group_data& Group : Data.Groups )
    {
        if( !RangeIsValid( Group.DefinitionStart,
                           Group.DefinitionCount,
                           DefinitionCount ) )
        {
            return( bitsery_io::Fail(
                Error,
                "Decal package group definition range is invalid." ) );
        }
    }

    for( const definition_data& Definition : Data.Definitions )
    {
        if( !x_isvalid( Definition.MinWidth )  ||
            !x_isvalid( Definition.MinHeight ) ||
            !x_isvalid( Definition.MaxWidth )  ||
            !x_isvalid( Definition.MaxHeight ) ||
            !x_isvalid( Definition.MinRoll )   ||
            !x_isvalid( Definition.MaxRoll )   ||
            !x_isvalid( Definition.FadeTime ) )
        {
            return( bitsery_io::Fail(
                Error,
                "Decal package contains a non-finite numeric value." ) );
        }
        if( (Definition.MinWidth > Definition.MaxWidth) ||
            (Definition.MinHeight > Definition.MaxHeight) ||
            (Definition.MinRoll > Definition.MaxRoll) ||
            (Definition.FadeTime < 0.0f) )
        {
            return( bitsery_io::Fail(
                Error,
                "Decal package contains an invalid numeric range." ) );
        }
        if( Definition.Flags & ~DECAL_FLAG_MASK )
        {
            return( bitsery_io::Fail(
                Error,
                "Decal package contains unsupported definition flags." ) );
        }
        if( Definition.BlendMode > decal_definition::DECAL_BLEND_INTENSITY )
        {
            return( bitsery_io::Fail(
                Error,
                "Decal package contains an invalid blend mode." ) );
        }
    }

    return( TRUE );
}

//==============================================================================

xbool MakeData( const decal_package& Package,
                package_data&       Data,
                xstring&            Error )
{
    if( (Package.GetNGroups() < 0) ||
        (Package.GetNGroups() > MAX_DECAL_GROUPS) ||
        (Package.GetNDecalDefs() < 0) ||
        (Package.GetNDecalDefs() > MAX_DECAL_DEFINITIONS) )
    {
        return( bitsery_io::Fail( Error, "Decal package count is invalid." ) );
    }

    Data.Groups.resize( Package.GetNGroups() );
    for( s32 i = 0; i < Package.GetNGroups(); i++ )
    {
        if( !HasTerminator( Package.GetGroupName( i ), 32 ) )
        {
            return( bitsery_io::Fail( Error, "Decal package group name is not terminated." ) );
        }

        group_data& Group = Data.Groups[i];
        x_strsavecpy( Group.Name, Package.GetGroupName( i ), 32 );
        Group.Color           = (u32)Package.GetGroupColor( i );
        Group.DefinitionCount = Package.GetNDecalDefs( i );
        Group.DefinitionStart = Package.GetGroupDecalDefStart( i );
    }

    Data.Definitions.resize( Package.GetNDecalDefs() );
    for( s32 i = 0; i < Package.GetNDecalDefs(); i++ )
    {
        const decal_definition& Source = Package.GetDecalDef( i );
        if( !HasTerminator( Source.m_Name, 32 ) ||
            !HasTerminator( Source.m_BitmapName, 256 ) )
        {
            return( bitsery_io::Fail( Error, "Decal package string is not terminated." ) );
        }

        definition_data&        Target = Data.Definitions[i];
        x_strsavecpy( Target.Name, Source.m_Name, 32 );
        Target.MinWidth   = Source.m_MinSize.X;
        Target.MinHeight  = Source.m_MinSize.Y;
        Target.MaxWidth   = Source.m_MaxSize.X;
        Target.MaxHeight  = Source.m_MaxSize.Y;
        Target.MinRoll    = Source.m_MinRoll;
        Target.MaxRoll    = Source.m_MaxRoll;
        Target.Color      = (u32)Source.m_Color;
        x_strsavecpy( Target.BitmapName, Source.m_BitmapName, 256 );
        Target.MaxVisible = Source.m_MaxVisible;
        Target.FadeTime   = Source.m_FadeTime;
        Target.Flags      = Source.m_Flags;
        Target.BlendMode  = Source.m_BlendMode;
    }

    return( TRUE );
}

//==============================================================================

decal_package* MakePackage( const package_data& Data )
{
    decal_package* pPackage = new decal_package;
    pPackage->AllocGroups( (s32)Data.Groups.size() );
    pPackage->AllocDecals( (s32)Data.Definitions.size() );

    for( s32 i = 0; i < (s32)Data.Groups.size(); i++ )
    {
        const group_data& Group = Data.Groups[i];
        pPackage->SetGroupName( i, Group.Name );
        pPackage->SetGroupColor( i, xcolor( Group.Color ) );
        pPackage->SetGroupDecalDefStart( i, Group.DefinitionStart );
        pPackage->SetGroupDecalDefCount( i, Group.DefinitionCount );
    }

    for( s32 i = 0; i < (s32)Data.Definitions.size(); i++ )
    {
        const definition_data& Source = Data.Definitions[i];
        decal_definition&      Target = pPackage->GetDecalDef( i );
        x_strsavecpy( Target.m_Name, Source.Name, 32 );
        Target.m_MinSize.Set( Source.MinWidth, Source.MinHeight );
        Target.m_MaxSize.Set( Source.MaxWidth, Source.MaxHeight );
        Target.m_MinRoll = Source.MinRoll;
        Target.m_MaxRoll = Source.MaxRoll;
        Target.m_Color   = xcolor( Source.Color );
        x_strsavecpy( Target.m_BitmapName, Source.BitmapName, 256 );
        Target.m_MaxVisible = Source.MaxVisible;
        Target.m_FadeTime   = Source.FadeTime;
        Target.m_Flags      = Source.Flags;
        Target.m_BlendMode  = Source.BlendMode;
        Target.m_Handle     = HNULL;
    }

    return( pPackage );
}

} // namespace

//==============================================================================

xbool decal_package_file::Load( X_FILE*         pFile,
                                decal_package*& pPackage,
                                xstring&        Error )
{
    pPackage = NULL;

    package_data Data;
    if( !bitsery_io::Read( pFile,
                           DECAL_PACKAGE_FILE_FORMAT,
                           Data,
                           Error ) ||
        !ValidateData( Data, Error ) )
    {
        return( FALSE );
    }

    pPackage = MakePackage( Data );
    return( TRUE );
}

//==============================================================================

xbool decal_package_file::Validate( const decal_package& Package,
                                    xstring&             Error )
{
    package_data Data;
    return( MakeData( Package, Data, Error ) && ValidateData( Data, Error ) );
}

//==============================================================================

xbool decal_package_file::Save( X_FILE*             pFile,
                                const decal_package& Package,
                                xstring&            Error )
{
    package_data Data;
    if( !MakeData( Package, Data, Error ) || !ValidateData( Data, Error ) )
    {
        return( FALSE );
    }

    return( bitsery_io::Write( pFile,
                               DECAL_PACKAGE_FILE_FORMAT,
                               Data,
                               Error ) );
}

//==============================================================================

xbool decal_package_file::Save( const char*          pFileName,
                                const decal_package& Package,
                                xstring&            Error )
{
    if( !pFileName || !pFileName[0] )
    {
        return( bitsery_io::Fail(
            Error,
            "Decal package output filename is empty." ) );
    }

    if( !Validate( Package, Error ) )
    {
        return( FALSE );
    }

    X_FILE* pFile = x_fopen( pFileName, "wb" );
    if( !pFile )
    {
        return( bitsery_io::Fail(
            Error,
            "Unable to open the decal package output file." ) );
    }

    const xbool Result = Save( pFile, Package, Error );
    x_fclose( pFile );
    return( Result );
}

//==============================================================================
