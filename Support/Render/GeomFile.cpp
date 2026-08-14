//=========================================================================
//
//  GeomFile.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "GeomFile.hpp"
#include "Auxiliary/MiscUtils/BitseryIO.hpp"
#include "RigidGeom.hpp"
#include "SkinGeom.hpp"

//=========================================================================
//  PRIVATE CONSTANTS
//=========================================================================

namespace
{

enum
{
    MAX_SMALL_ARRAY_COUNT = ( 1 << 20 ),
    MAX_LARGE_ARRAY_COUNT = ( 1 << 25 ),
    MAX_STRING_DATA_SIZE = ( 1 << 27 ),
};

using bitsery_io::ReadEmptyArray;
using bitsery_io::ReadObjectArray;
using bitsery_io::ReadS32;
using bitsery_io::ReadValueArray;
using bitsery_io::SetInvalidData;

bitsery_io::file_format const RIGID_FILE_FORMAT = {
    { 'R', 'I', 'G', 'M' },
    geom_file::VERSION,
};

bitsery_io::file_format const SKIN_FILE_FORMAT = {
    { 'S', 'K', 'N', 'M' },
    geom_file::VERSION,
};

//=========================================================================

xbool RangeIsValid( s32 first, s32 count, s32 arrayCount )
{
    return ( ( first >= 0 ) && ( count >= 0 ) && ( first <= arrayCount ) && ( count <= ( arrayCount - first ) ) );
}

//=========================================================================

xbool IsIndexOrMinusOne( s32 index, s32 arrayCount )
{
    return ( ( index == -1 ) || ( ( index >= 0 ) && ( index < arrayCount ) ) );
}

//=========================================================================

xbool HasString( geom const& geom, s32 offset )
{
    if ( ( offset < 0 ) || ( offset >= geom.m_stringDataSize ) )
    {
        return ( FALSE );
    }

    for ( s32 i = offset; i < geom.m_stringDataSize; i++ )
    {
        if ( geom.m_pStringData[i] == 0 )
        {
            return ( TRUE );
        }
    }

    return ( FALSE );
}

} // namespace

//=========================================================================
//  VALUE DESERIALIZATION
//=========================================================================

template <class SERIALIZER> void serialize( SERIALIZER& serializer, vector2& value )
{
    serializer.value4b( value.X );
    serializer.value4b( value.Y );
}

//=========================================================================

template <class SERIALIZER> void serialize( SERIALIZER& serializer, vector3p& value )
{
    serializer.value4b( value.X );
    serializer.value4b( value.Y );
    serializer.value4b( value.Z );
}

//=========================================================================

template <class SERIALIZER> void serialize( SERIALIZER& serializer, vector3& value )
{
    serializer.value4b( value.GetX() );
    serializer.value4b( value.GetY() );
    serializer.value4b( value.GetZ() );
    value.GetIW() = 0;
}

//=========================================================================

template <class SERIALIZER> void serialize( SERIALIZER& serializer, quaternion& value )
{
    serializer.value4b( value.X );
    serializer.value4b( value.Y );
    serializer.value4b( value.Z );
    serializer.value4b( value.W );
}

//=========================================================================

template <class SERIALIZER> void serialize( SERIALIZER& serializer, bbox& value )
{
    serializer.object( value.Min );
    serializer.object( value.Max );
}

//=========================================================================

template <class SERIALIZER> void serialize( SERIALIZER& serializer, geom::bone& value )
{
    serializer.object( value.BindRotation );
    serializer.object( value.BindPosition );
    serializer.object( value.BBox );
    serializer.value2b( value.HitLocation );
    serializer.value2b( value.iRigidBody );
}

//=========================================================================

template <class SERIALIZER> void serialize( SERIALIZER& serializer, geom::property_section& value )
{
    ReadS32( serializer, value.NameOffset );
    ReadS32( serializer, value.iProperty );
    ReadS32( serializer, value.nProperties );
}

//=========================================================================

template <class SERIALIZER> void serialize( SERIALIZER& serializer, geom::property& value )
{
    u8 type = 0;

    ReadS32( serializer, value.NameOffset );
    serializer.value1b( type );
    serializer.value4b( value.Value.Integer );

    value.Type = type;
}

//=========================================================================

template <class SERIALIZER> void serialize( SERIALIZER& serializer, geom::rigid_body::dof& value )
{
    serializer.value4b( value.Flags );
    serializer.value4b( value.Min );
    serializer.value4b( value.Max );
}

//=========================================================================

template <class SERIALIZER> void serialize( SERIALIZER& serializer, geom::rigid_body& value )
{
    serializer.object( value.BodyBindRotation );
    serializer.object( value.BodyBindPosition );
    serializer.object( value.PivotBindRotation );
    serializer.object( value.PivotBindPosition );
    ReadS32( serializer, value.NameOffset );
    serializer.value4b( value.Mass );
    serializer.value4b( value.Radius );
    serializer.value4b( value.Width );
    serializer.value4b( value.Height );
    serializer.value4b( value.Length );
    serializer.value2b( value.Type );
    serializer.value2b( value.Flags );
    serializer.value2b( value.iParentBody );
    serializer.value2b( value.iBone );
    serializer.value4b( value.CollisionMask );

    for ( s32 i = 0; i < 6; i++ )
    {
        serializer.object( value.DOF[i] );
    }
}

//=========================================================================

template <class SERIALIZER> void serialize( SERIALIZER& serializer, geom::mesh& value )
{
    serializer.object( value.BBox );
    ReadS32( serializer, value.NameOffset );
    ReadS32( serializer, value.iSubMesh );
    ReadS32( serializer, value.nSubMeshes );
    ReadS32( serializer, value.nBones );
    ReadS32( serializer, value.nFaces );
    ReadS32( serializer, value.nVertices );
}

//=========================================================================

template <class SERIALIZER> void serialize( SERIALIZER& serializer, geom::submesh& value )
{
    ReadS32( serializer, value.iSection );
    ReadS32( serializer, value.nSections );
    ReadS32( serializer, value.iMaterial );
    serializer.value4b( value.WorldPixelSize );
}

//=========================================================================

template <class SERIALIZER> void serialize( SERIALIZER& serializer, geom::material::uvanim& value )
{
    serializer.value1b( value.Type );
    serializer.value1b( value.StartFrame );
    serializer.value1b( value.FPS );
    ReadS32( serializer, value.iKey );
    ReadS32( serializer, value.nKeys );
}

//=========================================================================

template <class SERIALIZER> void serialize( SERIALIZER& serializer, geom::material& value )
{
    serializer.object( value.UVAnim );
    serializer.value4b( value.DetailScale );
    serializer.value4b( value.FixedAlpha );
    serializer.value2b( value.Flags );
    serializer.value1b( value.Type );
    ReadS32( serializer, value.iTexture );
    ReadS32( serializer, value.nTextures );
    ReadS32( serializer, value.iVirtualMat );
    ReadS32( serializer, value.nVirtualMats );
}

//=========================================================================

template <class SERIALIZER> void serialize( SERIALIZER& serializer, geom::texture& value )
{
    ReadS32( serializer, value.DescOffset );
    ReadS32( serializer, value.FileNameOffset );
}

//=========================================================================

template <class SERIALIZER> void serialize( SERIALIZER& serializer, geom::uvkey& value )
{
    serializer.value1b( value.OffsetU );
    serializer.value1b( value.OffsetV );
}

//=========================================================================

template <class SERIALIZER> void serialize( SERIALIZER& serializer, geom::virtual_mesh& value )
{
    ReadS32( serializer, value.NameOffset );
    ReadS32( serializer, value.iLOD );
    ReadS32( serializer, value.nLODs );
}

//=========================================================================

template <class SERIALIZER> void serialize( SERIALIZER& serializer, geom::virtual_texture& value )
{
    ReadS32( serializer, value.NameOffset );
    serializer.value4b( value.MaterialMask );
}

//=========================================================================

template <class SERIALIZER> void serialize( SERIALIZER& serializer, collision_data::mat_info& value )
{
    serializer.value2b( value.SoundType );
    serializer.value2b( value.Flags );
}

//=========================================================================

template <class SERIALIZER> void serialize( SERIALIZER& serializer, collision_data::high_cluster& value )
{
    serializer.object( value.BBox );
    ReadS32( serializer, value.nTris );
    serializer.value4b( value.iMesh );
    serializer.value4b( value.iBone );
    serializer.value4b( value.iSection );
    ReadS32( serializer, value.iOffset );
    serializer.object( value.MaterialInfo );
}

//=========================================================================

template <class SERIALIZER> void serialize( SERIALIZER& serializer, collision_data::low_cluster& value )
{
    serializer.object( value.BBox );
    ReadS32( serializer, value.iVectorOffset );
    ReadS32( serializer, value.nPoints );
    ReadS32( serializer, value.nNormals );
    ReadS32( serializer, value.iQuadOffset );
    ReadS32( serializer, value.nQuads );
    serializer.value4b( value.iMesh );
    serializer.value4b( value.iBone );
}

//=========================================================================

template <class SERIALIZER> void serialize( SERIALIZER& serializer, collision_data::low_quad& value )
{
    for ( s32 i = 0; i < 4; i++ )
    {
        serializer.value1b( value.iP[i] );
    }

    serializer.value1b( value.iN );
    serializer.value1b( value.Flags );
}

//=========================================================================

template <class SERIALIZER> void serialize( SERIALIZER& serializer, rigid_geom::vertex& value )
{
    serializer.object( value.Pos );
    serializer.object( value.Normal );
    serializer.object( value.UV );
}

//=========================================================================

template <class SERIALIZER> void serialize( SERIALIZER& serializer, skin_geom::vertex& value )
{
    serializer.object( value.Position );
    serializer.object( value.Normal );
    serializer.object( value.UV );
    serializer.value4b( value.Weights.X );
    serializer.value4b( value.Weights.Y );
    serializer.value2b( value.Bones[0] );
    serializer.value2b( value.Bones[1] );
}

//=========================================================================

template <class SERIALIZER> void serialize( SERIALIZER& serializer, rigid_geom::section& value )
{
    ReadS32( serializer, value.FirstVertex );
    ReadS32( serializer, value.nVertices );
    ReadS32( serializer, value.FirstIndex );
    ReadS32( serializer, value.nIndices );
    serializer.value4b( value.iBone );
    serializer.value4b( value.iColor );
}

//=========================================================================

template <class SERIALIZER> void serialize( SERIALIZER& serializer, skin_geom::section& value )
{
    ReadS32( serializer, value.FirstVertex );
    ReadS32( serializer, value.nVertices );
    ReadS32( serializer, value.FirstIndex );
    ReadS32( serializer, value.nIndices );
    ReadS32( serializer, value.FirstBone );
    ReadS32( serializer, value.nBones );
}

//=========================================================================
//  GEOMETRY DESERIALIZATION
//=========================================================================

namespace
{

template <class SERIALIZER> void ReadBoneMasks( SERIALIZER& serializer, geom& geom )
{
    u32 maskCount = 0;
    serializer.value4b( maskCount );

    if ( maskCount > MAX_SMALL_ARRAY_COUNT )
    {
        SetInvalidData( serializer );
        return;
    }

    geom.m_nBoneMasks = static_cast<s32>( maskCount );
    geom.m_pBoneMasks = geom.m_nBoneMasks > 0 ? new geom::bone_masks[geom.m_nBoneMasks] : NULL;

    xarray<u32> firstWeights;
    xarray<u32> weightCounts;
    firstWeights.SetCount( geom.m_nBoneMasks );
    weightCounts.SetCount( geom.m_nBoneMasks );

    for ( s32 i = 0; i < geom.m_nBoneMasks; i++ )
    {
        u32 nameOffset = 0;
        serializer.value4b( nameOffset );
        serializer.value4b( firstWeights[i] );
        serializer.value4b( weightCounts[i] );

        if ( nameOffset > 0x7fffffffu )
        {
            SetInvalidData( serializer );
            return;
        }

        geom.m_pBoneMasks[i].NameOffset = static_cast<s32>( nameOffset );
        geom.m_pBoneMasks[i].nBones = 0;
        x_memset( geom.m_pBoneMasks[i].Weights, 0, sizeof( geom.m_pBoneMasks[i].Weights ) );
    }

    f32* pWeights = NULL;
    s32  nWeights = 0;
    ReadValueArray<4>( serializer, pWeights, nWeights, MAX_LARGE_ARRAY_COUNT );

    for ( s32 i = 0; i < geom.m_nBoneMasks; i++ )
    {
        if ( ( weightCounts[i] > MAX_ANIM_BONES ) || ( firstWeights[i] > static_cast<u32>( nWeights ) ) ||
             ( weightCounts[i] > ( static_cast<u32>( nWeights ) - firstWeights[i] ) ) )
        {
            delete[] pWeights;
            SetInvalidData( serializer );
            return;
        }

        geom.m_pBoneMasks[i].nBones = static_cast<s32>( weightCounts[i] );
        for ( s32 j = 0; j < geom.m_pBoneMasks[i].nBones; j++ )
        {
            geom.m_pBoneMasks[i].Weights[j] = pWeights[firstWeights[i] + j];
        }
    }

    delete[] pWeights;
}

//=========================================================================

template <class SERIALIZER> void ReadCommonGeom( SERIALIZER& serializer, geom& geom )
{
    serializer.object( geom.m_BBox );
    ReadS32( serializer, geom.m_nFaces );
    ReadS32( serializer, geom.m_nVertices );
    ReadS32( serializer, geom.m_nVirtualMaterials );
    if ( geom.m_nVirtualMaterials > MAX_SMALL_ARRAY_COUNT )
    {
        SetInvalidData( serializer );
        return;
    }

    ReadObjectArray( serializer, geom.m_pBone, geom.m_nBones, MAX_SMALL_ARRAY_COUNT );
    ReadBoneMasks( serializer, geom );
    ReadObjectArray( serializer, geom.m_pPropertySections, geom.m_nPropertySections, MAX_SMALL_ARRAY_COUNT );
    ReadObjectArray( serializer, geom.m_pProperties, geom.m_nProperties, MAX_SMALL_ARRAY_COUNT );
    ReadObjectArray( serializer, geom.m_pRigidBodies, geom.m_nRigidBodies, MAX_SMALL_ARRAY_COUNT );
    ReadObjectArray( serializer, geom.m_pMesh, geom.m_nMeshes, MAX_SMALL_ARRAY_COUNT );
    ReadObjectArray( serializer, geom.m_pSubMesh, geom.m_nSubMeshes, MAX_SMALL_ARRAY_COUNT );
    ReadObjectArray( serializer, geom.m_pMaterial, geom.m_nMaterials, MAX_SMALL_ARRAY_COUNT );
    ReadObjectArray( serializer, geom.m_pTexture, geom.m_nTextures, MAX_SMALL_ARRAY_COUNT );
    ReadObjectArray( serializer, geom.m_pUVKey, geom.m_nUVKeys, MAX_LARGE_ARRAY_COUNT );

    s32 lodMaskCount = 0;
    ReadValueArray<2>( serializer, geom.m_pLODSizes, geom.m_nLODs, MAX_SMALL_ARRAY_COUNT );
    ReadValueArray<8>( serializer, geom.m_pLODMasks, lodMaskCount, MAX_SMALL_ARRAY_COUNT );
    if ( lodMaskCount != geom.m_nLODs )
    {
        SetInvalidData( serializer );
        return;
    }

    ReadObjectArray( serializer, geom.m_pVirtualMeshes, geom.m_nVirtualMeshes, MAX_SMALL_ARRAY_COUNT );
    ReadObjectArray( serializer, geom.m_pVirtualTextures, geom.m_nVirtualTextures, MAX_SMALL_ARRAY_COUNT );
    ReadValueArray<1>( serializer, geom.m_pStringData, geom.m_stringDataSize, MAX_STRING_DATA_SIZE );
}

//=========================================================================

template <class SERIALIZER> void ReadCollision( SERIALIZER& serializer, collision_data& collision )
{
    serializer.object( collision.BBox );
    ReadObjectArray( serializer, collision.pHighCluster, collision.nHighClusters, MAX_SMALL_ARRAY_COUNT );
    ReadValueArray<2>( serializer, collision.pHighIndexToVert0, collision.nHighIndices, MAX_LARGE_ARRAY_COUNT );
    ReadObjectArray( serializer, collision.pLowCluster, collision.nLowClusters, MAX_SMALL_ARRAY_COUNT );
    ReadObjectArray( serializer, collision.pLowVector, collision.nLowVectors, MAX_LARGE_ARRAY_COUNT );
    ReadObjectArray( serializer, collision.pLowQuad, collision.nLowQuads, MAX_LARGE_ARRAY_COUNT );
}

//=========================================================================

template <class SERIALIZER> void ReadEmptyCollision( SERIALIZER& serializer )
{
    bbox ignored;
    serializer.object( ignored );
    ReadEmptyArray( serializer );
    ReadEmptyArray( serializer );
    ReadEmptyArray( serializer );
    ReadEmptyArray( serializer );
    ReadEmptyArray( serializer );
}

//=========================================================================

xbool ValidateCommon( geom const& geom )
{
    if ( ( geom.m_nFaces < 0 ) || ( geom.m_nVertices < 0 ) || ( geom.m_nVirtualMaterials < 0 ) )
    {
        return ( FALSE );
    }

    for ( s32 i = 0; i < geom.m_nBones; i++ )
    {
        if ( !IsIndexOrMinusOne( geom.m_pBone[i].iRigidBody, geom.m_nRigidBodies ) )
        {
            return ( FALSE );
        }
    }

    for ( s32 i = 0; i < geom.m_nBoneMasks; i++ )
    {
        if ( !HasString( geom, geom.m_pBoneMasks[i].NameOffset ) || ( geom.m_pBoneMasks[i].nBones < 0 ) ||
             ( geom.m_pBoneMasks[i].nBones > MAX_ANIM_BONES ) )
        {
            return ( FALSE );
        }
    }

    for ( s32 i = 0; i < geom.m_nPropertySections; i++ )
    {
        geom::property_section const& section = geom.m_pPropertySections[i];
        if ( !HasString( geom, section.NameOffset ) ||
             !RangeIsValid( section.iProperty, section.nProperties, geom.m_nProperties ) )
        {
            return ( FALSE );
        }
    }

    for ( s32 i = 0; i < geom.m_nProperties; i++ )
    {
        geom::property const& property = geom.m_pProperties[i];
        if ( !HasString( geom, property.NameOffset ) || ( property.Type < 0 ) ||
             ( property.Type >= geom::property::TYPE_TOTAL ) )
        {
            return ( FALSE );
        }

        if ( ( property.Type == geom::property::TYPE_STRING ) && !HasString( geom, property.Value.StringOffset ) )
        {
            return ( FALSE );
        }
    }

    for ( s32 i = 0; i < geom.m_nRigidBodies; i++ )
    {
        geom::rigid_body const& body = geom.m_pRigidBodies[i];
        if ( !HasString( geom, body.NameOffset ) || !IsIndexOrMinusOne( body.iParentBody, geom.m_nRigidBodies ) ||
             !IsIndexOrMinusOne( body.iBone, geom.m_nBones ) )
        {
            return ( FALSE );
        }
    }

    for ( s32 i = 0; i < geom.m_nMeshes; i++ )
    {
        geom::mesh const& mesh = geom.m_pMesh[i];
        if ( !HasString( geom, mesh.NameOffset ) || !RangeIsValid( mesh.iSubMesh, mesh.nSubMeshes, geom.m_nSubMeshes ) )
        {
            return ( FALSE );
        }
    }

    for ( s32 i = 0; i < geom.m_nMaterials; i++ )
    {
        geom::material const& material = geom.m_pMaterial[i];
        if ( !RangeIsValid( material.iTexture, material.nTextures, geom.m_nTextures ) ||
             !RangeIsValid( material.iVirtualMat, material.nVirtualMats, geom.m_nVirtualMaterials ) ||
             !RangeIsValid( material.UVAnim.iKey, material.UVAnim.nKeys, geom.m_nUVKeys ) )
        {
            return ( FALSE );
        }
    }

    for ( s32 i = 0; i < geom.m_nTextures; i++ )
    {
        if ( !HasString( geom, geom.m_pTexture[i].DescOffset ) ||
             !HasString( geom, geom.m_pTexture[i].FileNameOffset ) )
        {
            return ( FALSE );
        }
    }

    for ( s32 i = 0; i < geom.m_nVirtualMeshes; i++ )
    {
        geom::virtual_mesh const& mesh = geom.m_pVirtualMeshes[i];
        if ( !HasString( geom, mesh.NameOffset ) || !RangeIsValid( mesh.iLOD, mesh.nLODs, geom.m_nLODs ) )
        {
            return ( FALSE );
        }
    }

    for ( s32 i = 0; i < geom.m_nVirtualTextures; i++ )
    {
        if ( !HasString( geom, geom.m_pVirtualTextures[i].NameOffset ) )
        {
            return ( FALSE );
        }
    }

    return ( TRUE );
}

//=========================================================================

xbool ValidateRigid( rigid_geom const& geom )
{
    if ( !ValidateCommon( geom ) )
    {
        return ( FALSE );
    }

    for ( s32 i = 0; i < geom.m_nSubMeshes; i++ )
    {
        geom::submesh const& submesh = geom.m_pSubMesh[i];
        if ( !RangeIsValid( submesh.iSection, submesh.nSections, geom.m_nSections ) || ( submesh.iMaterial < 0 ) ||
             ( submesh.iMaterial >= geom.m_nMaterials ) )
        {
            return ( FALSE );
        }
    }

    for ( s32 i = 0; i < geom.m_nSections; i++ )
    {
        rigid_geom::section const& section = geom.m_pSection[i];
        if ( !RangeIsValid( section.FirstVertex, section.nVertices, geom.m_nVertexData ) ||
             !RangeIsValid( section.FirstIndex, section.nIndices, geom.m_nIndices ) ||
             !IsIndexOrMinusOne( section.iBone, geom.m_nBones ) )
        {
            return ( FALSE );
        }

        for ( s32 j = 0; j < section.nIndices; j++ )
        {
            u32 const vertexIndex = geom.m_pIndex[section.FirstIndex + j];
            if ( ( vertexIndex < static_cast<u32>( section.FirstVertex ) ) ||
                 ( vertexIndex >= static_cast<u32>( section.FirstVertex + section.nVertices ) ) )
            {
                return ( FALSE );
            }
        }
    }

    for ( s32 i = 0; i < geom.m_collision.nHighClusters; i++ )
    {
        collision_data::high_cluster const& cluster = geom.m_collision.pHighCluster[i];

        if ( !RangeIsValid( cluster.iOffset, cluster.nTris, geom.m_collision.nHighIndices ) ||
             !IsIndexOrMinusOne( cluster.iMesh, geom.m_nMeshes ) ||
             !IsIndexOrMinusOne( cluster.iBone, geom.m_nBones ) ||
             !IsIndexOrMinusOne( cluster.iSection, geom.m_nSections ) )
        {
            return ( FALSE );
        }
    }

    for ( s32 i = 0; i < geom.m_collision.nLowClusters; i++ )
    {
        collision_data::low_cluster const& cluster = geom.m_collision.pLowCluster[i];
        u64 const vectorCount = static_cast<u64>( cluster.nPoints ) + static_cast<u64>( cluster.nNormals );

        if ( ( vectorCount > 0x7fffffffu ) ||
             !RangeIsValid( cluster.iVectorOffset, static_cast<s32>( vectorCount ), geom.m_collision.nLowVectors ) ||
             !RangeIsValid( cluster.iQuadOffset, cluster.nQuads, geom.m_collision.nLowQuads ) ||
             !IsIndexOrMinusOne( cluster.iMesh, geom.m_nMeshes ) || !IsIndexOrMinusOne( cluster.iBone, geom.m_nBones ) )
        {
            return ( FALSE );
        }
    }

    return ( TRUE );
}

//=========================================================================

xbool ValidateSkin( skin_geom const& geom )
{
    if ( !ValidateCommon( geom ) )
    {
        return ( FALSE );
    }

    for ( s32 i = 0; i < geom.m_nSubMeshes; i++ )
    {
        geom::submesh const& submesh = geom.m_pSubMesh[i];
        if ( !RangeIsValid( submesh.iSection, submesh.nSections, geom.m_nSections ) || ( submesh.iMaterial < 0 ) ||
             ( submesh.iMaterial >= geom.m_nMaterials ) )
        {
            return ( FALSE );
        }
    }

    for ( s32 i = 0; i < geom.m_nSections; i++ )
    {
        skin_geom::section const& section = geom.m_pSection[i];
        if ( !RangeIsValid( section.FirstVertex, section.nVertices, geom.m_nVertexData ) ||
             !RangeIsValid( section.FirstIndex, section.nIndices, geom.m_nIndices ) ||
             !RangeIsValid( section.FirstBone, section.nBones, geom.m_nBonePalette ) )
        {
            return ( FALSE );
        }

        for ( s32 j = 0; j < section.nIndices; j++ )
        {
            u32 const vertexIndex = geom.m_pIndex[section.FirstIndex + j];
            if ( ( vertexIndex < static_cast<u32>( section.FirstVertex ) ) ||
                 ( vertexIndex >= static_cast<u32>( section.FirstVertex + section.nVertices ) ) )
            {
                return ( FALSE );
            }

            skin_geom::vertex const& vertex = geom.m_pVertex[vertexIndex];
            for ( s32 weightIndex = 0; weightIndex < 2; weightIndex++ )
            {
                if ( vertex.Weights[weightIndex] == 0.0f )
                {
                    continue;
                }

                u16 const paletteIndex = vertex.Bones[weightIndex];
                if ( ( paletteIndex >= section.nBones ) ||
                     ( geom.m_pBonePalette[section.FirstBone + paletteIndex] == 0xffffu ) )
                {
                    return ( FALSE );
                }
            }
        }
    }

    return ( TRUE );
}

} // namespace

//=========================================================================
//  ARCHIVE SERIALIZATION
//=========================================================================

namespace
{

using OutputSerializer = bitsery::Serializer<bitsery_io::output_adapter>;

//=========================================================================

void WriteS32( OutputSerializer& serializer, s32 value )
{
    u32 wireValue = static_cast<u32>( value );
    serializer.value4b( wireValue );
}

//=========================================================================

template <size_t SIZE, class TYPE> void WriteValueArray( OutputSerializer& serializer, TYPE* pArray, s32 count )
{
    u32 wireCount = static_cast<u32>( count );
    serializer.value4b( wireCount );

    for ( s32 i = 0; i < count; i++ )
    {
        serializer.template value<SIZE>( pArray[i] );
    }
}

//=========================================================================

template <class TYPE> void WriteObjectArray( OutputSerializer& serializer, TYPE* pArray, s32 count )
{
    u32 wireCount = static_cast<u32>( count );
    serializer.value4b( wireCount );

    for ( s32 i = 0; i < count; i++ )
    {
        serializer.object( pArray[i] );
    }
}

//=========================================================================

void WriteEmptyArray( OutputSerializer& serializer )
{
    u32 count = 0;
    serializer.value4b( count );
}

//=========================================================================

void WriteBoneMasks( OutputSerializer& serializer, geom& geom )
{
    u32 maskCount = static_cast<u32>( geom.m_nBoneMasks );
    serializer.value4b( maskCount );

    u32 firstWeight = 0;
    for ( s32 i = 0; i < geom.m_nBoneMasks; i++ )
    {
        geom::bone_masks& mask = geom.m_pBoneMasks[i];
        u32               nameOffset = static_cast<u32>( mask.NameOffset );
        u32               weightCount = static_cast<u32>( mask.nBones );

        serializer.value4b( nameOffset );
        serializer.value4b( firstWeight );
        serializer.value4b( weightCount );
        firstWeight += weightCount;
    }

    serializer.value4b( firstWeight );
    for ( s32 i = 0; i < geom.m_nBoneMasks; i++ )
    {
        geom::bone_masks& mask = geom.m_pBoneMasks[i];
        for ( s32 j = 0; j < mask.nBones; j++ )
        {
            serializer.value4b( mask.Weights[j] );
        }
    }
}

//=========================================================================

void WriteCommonGeom( OutputSerializer& serializer, geom& geom )
{
    serializer.object( geom.m_BBox );
    WriteS32( serializer, geom.m_nFaces );
    WriteS32( serializer, geom.m_nVertices );
    WriteS32( serializer, geom.m_nVirtualMaterials );

    WriteObjectArray( serializer, geom.m_pBone, geom.m_nBones );
    WriteBoneMasks( serializer, geom );
    WriteObjectArray( serializer, geom.m_pPropertySections, geom.m_nPropertySections );
    WriteObjectArray( serializer, geom.m_pProperties, geom.m_nProperties );
    WriteObjectArray( serializer, geom.m_pRigidBodies, geom.m_nRigidBodies );
    WriteObjectArray( serializer, geom.m_pMesh, geom.m_nMeshes );
    WriteObjectArray( serializer, geom.m_pSubMesh, geom.m_nSubMeshes );
    WriteObjectArray( serializer, geom.m_pMaterial, geom.m_nMaterials );
    WriteObjectArray( serializer, geom.m_pTexture, geom.m_nTextures );
    WriteObjectArray( serializer, geom.m_pUVKey, geom.m_nUVKeys );
    WriteValueArray<2>( serializer, geom.m_pLODSizes, geom.m_nLODs );
    WriteValueArray<8>( serializer, geom.m_pLODMasks, geom.m_nLODs );
    WriteObjectArray( serializer, geom.m_pVirtualMeshes, geom.m_nVirtualMeshes );
    WriteObjectArray( serializer, geom.m_pVirtualTextures, geom.m_nVirtualTextures );
    WriteValueArray<1>( serializer, geom.m_pStringData, geom.m_stringDataSize );
}

//=========================================================================

void WriteCollision( OutputSerializer& serializer, collision_data& collision )
{
    serializer.object( collision.BBox );
    WriteObjectArray( serializer, collision.pHighCluster, collision.nHighClusters );
    WriteValueArray<2>( serializer, collision.pHighIndexToVert0, collision.nHighIndices );
    WriteObjectArray( serializer, collision.pLowCluster, collision.nLowClusters );
    WriteObjectArray( serializer, collision.pLowVector, collision.nLowVectors );
    WriteObjectArray( serializer, collision.pLowQuad, collision.nLowQuads );
}

//=========================================================================

void WriteEmptyCollision( OutputSerializer& serializer )
{
    bbox emptyBBox;
    emptyBBox.Min.Set( 0.0f, 0.0f, 0.0f );
    emptyBBox.Max.Set( 0.0f, 0.0f, 0.0f );
    serializer.object( emptyBBox );
    WriteEmptyArray( serializer );
    WriteEmptyArray( serializer );
    WriteEmptyArray( serializer );
    WriteEmptyArray( serializer );
    WriteEmptyArray( serializer );
}

} // namespace

//=========================================================================
//  OUTPUT OBJECT SERIALIZATION
//=========================================================================

void serialize( OutputSerializer& serializer, geom::property_section& value )
{
    WriteS32( serializer, value.NameOffset );
    WriteS32( serializer, value.iProperty );
    WriteS32( serializer, value.nProperties );
}

//=========================================================================

void serialize( OutputSerializer& serializer, geom::property& value )
{
    WriteS32( serializer, value.NameOffset );
    u8 type = static_cast<u8>( value.Type );
    serializer.value1b( type );
    serializer.value4b( value.Value.Integer );
}

//=========================================================================

void serialize( OutputSerializer& serializer, geom::rigid_body& value )
{
    serializer.object( value.BodyBindRotation );
    serializer.object( value.BodyBindPosition );
    serializer.object( value.PivotBindRotation );
    serializer.object( value.PivotBindPosition );
    WriteS32( serializer, value.NameOffset );
    serializer.value4b( value.Mass );
    serializer.value4b( value.Radius );
    serializer.value4b( value.Width );
    serializer.value4b( value.Height );
    serializer.value4b( value.Length );
    serializer.value2b( value.Type );
    serializer.value2b( value.Flags );
    serializer.value2b( value.iParentBody );
    serializer.value2b( value.iBone );
    serializer.value4b( value.CollisionMask );

    for ( s32 i = 0; i < 6; i++ )
    {
        serializer.object( value.DOF[i] );
    }
}

//=========================================================================

void serialize( OutputSerializer& serializer, geom::mesh& value )
{
    serializer.object( value.BBox );
    WriteS32( serializer, value.NameOffset );
    WriteS32( serializer, value.iSubMesh );
    WriteS32( serializer, value.nSubMeshes );
    WriteS32( serializer, value.nBones );
    WriteS32( serializer, value.nFaces );
    WriteS32( serializer, value.nVertices );
}

//=========================================================================

void serialize( OutputSerializer& serializer, geom::submesh& value )
{
    WriteS32( serializer, value.iSection );
    WriteS32( serializer, value.nSections );
    WriteS32( serializer, value.iMaterial );
    serializer.value4b( value.WorldPixelSize );
}

//=========================================================================

void serialize( OutputSerializer& serializer, geom::material::uvanim& value )
{
    serializer.value1b( value.Type );
    serializer.value1b( value.StartFrame );
    serializer.value1b( value.FPS );
    WriteS32( serializer, value.iKey );
    WriteS32( serializer, value.nKeys );
}

//=========================================================================

void serialize( OutputSerializer& serializer, geom::material& value )
{
    serializer.object( value.UVAnim );
    serializer.value4b( value.DetailScale );
    serializer.value4b( value.FixedAlpha );
    serializer.value2b( value.Flags );
    serializer.value1b( value.Type );
    WriteS32( serializer, value.iTexture );
    WriteS32( serializer, value.nTextures );
    WriteS32( serializer, value.iVirtualMat );
    WriteS32( serializer, value.nVirtualMats );
}

//=========================================================================

void serialize( OutputSerializer& serializer, geom::texture& value )
{
    WriteS32( serializer, value.DescOffset );
    WriteS32( serializer, value.FileNameOffset );
}

//=========================================================================

void serialize( OutputSerializer& serializer, geom::virtual_mesh& value )
{
    WriteS32( serializer, value.NameOffset );
    WriteS32( serializer, value.iLOD );
    WriteS32( serializer, value.nLODs );
}

//=========================================================================

void serialize( OutputSerializer& serializer, geom::virtual_texture& value )
{
    WriteS32( serializer, value.NameOffset );
    serializer.value4b( value.MaterialMask );
}

//=========================================================================

void serialize( OutputSerializer& serializer, collision_data::high_cluster& value )
{
    serializer.object( value.BBox );
    WriteS32( serializer, value.nTris );
    serializer.value4b( value.iMesh );
    serializer.value4b( value.iBone );
    serializer.value4b( value.iSection );
    WriteS32( serializer, value.iOffset );
    serializer.object( value.MaterialInfo );
}

//=========================================================================

void serialize( OutputSerializer& serializer, collision_data::low_cluster& value )
{
    serializer.object( value.BBox );
    WriteS32( serializer, value.iVectorOffset );
    WriteS32( serializer, value.nPoints );
    WriteS32( serializer, value.nNormals );
    WriteS32( serializer, value.iQuadOffset );
    WriteS32( serializer, value.nQuads );
    serializer.value4b( value.iMesh );
    serializer.value4b( value.iBone );
}

//=========================================================================

void serialize( OutputSerializer& serializer, rigid_geom::section& value )
{
    WriteS32( serializer, value.FirstVertex );
    WriteS32( serializer, value.nVertices );
    WriteS32( serializer, value.FirstIndex );
    WriteS32( serializer, value.nIndices );
    serializer.value4b( value.iBone );
    serializer.value4b( value.iColor );
}

//=========================================================================

void serialize( OutputSerializer& serializer, skin_geom::section& value )
{
    WriteS32( serializer, value.FirstVertex );
    WriteS32( serializer, value.nVertices );
    WriteS32( serializer, value.FirstIndex );
    WriteS32( serializer, value.nIndices );
    WriteS32( serializer, value.FirstBone );
    WriteS32( serializer, value.nBones );
}

//=========================================================================

void serialize( OutputSerializer& serializer, rigid_geom& geom )
{
    WriteCommonGeom( serializer, geom );
    WriteCollision( serializer, geom.m_collision );
    WriteValueArray<4>( serializer, geom.m_pIndex, geom.m_nIndices );
    WriteObjectArray( serializer, geom.m_pVertex, geom.m_nVertexData );
    WriteEmptyArray( serializer );
    WriteObjectArray( serializer, geom.m_pSection, geom.m_nSections );
    WriteEmptyArray( serializer );
    WriteEmptyArray( serializer );
}

//=========================================================================

void serialize( OutputSerializer& serializer, skin_geom& geom )
{
    WriteCommonGeom( serializer, geom );
    WriteEmptyCollision( serializer );
    WriteValueArray<4>( serializer, geom.m_pIndex, geom.m_nIndices );
    WriteEmptyArray( serializer );
    WriteObjectArray( serializer, geom.m_pVertex, geom.m_nVertexData );
    WriteEmptyArray( serializer );
    WriteObjectArray( serializer, geom.m_pSection, geom.m_nSections );
    WriteValueArray<2>( serializer, geom.m_pBonePalette, geom.m_nBonePalette );
}

//=========================================================================
//  ARCHIVE DESERIALIZATION
//=========================================================================

template <class SERIALIZER> void serialize( SERIALIZER& serializer, rigid_geom& geom )
{
    ReadCommonGeom( serializer, geom );
    ReadCollision( serializer, geom.m_collision );
    ReadValueArray<4>( serializer, geom.m_pIndex, geom.m_nIndices, MAX_LARGE_ARRAY_COUNT );
    ReadObjectArray( serializer, geom.m_pVertex, geom.m_nVertexData, MAX_LARGE_ARRAY_COUNT );
    ReadEmptyArray( serializer );
    ReadObjectArray( serializer, geom.m_pSection, geom.m_nSections, MAX_SMALL_ARRAY_COUNT );
    ReadEmptyArray( serializer );
    ReadEmptyArray( serializer );
}

//=========================================================================

template <class SERIALIZER> void serialize( SERIALIZER& serializer, skin_geom& geom )
{
    ReadCommonGeom( serializer, geom );
    ReadEmptyCollision( serializer );
    ReadValueArray<4>( serializer, geom.m_pIndex, geom.m_nIndices, MAX_LARGE_ARRAY_COUNT );
    ReadEmptyArray( serializer );
    ReadObjectArray( serializer, geom.m_pVertex, geom.m_nVertexData, MAX_LARGE_ARRAY_COUNT );
    ReadEmptyArray( serializer );
    ReadObjectArray( serializer, geom.m_pSection, geom.m_nSections, MAX_SMALL_ARRAY_COUNT );
    ReadValueArray<2>( serializer, geom.m_pBonePalette, geom.m_nBonePalette, MAX_LARGE_ARRAY_COUNT );
}

//=========================================================================
//  GEOMETRY FILE
//=========================================================================

xbool geom_file::LoadRigid( X_FILE* pFile, rigid_geom*& pGeom, xstring& error )
{
    error.Clear();
    pGeom = NULL;

    pGeom = new rigid_geom;
    if ( !bitsery_io::Read( pFile, RIGID_FILE_FORMAT, *pGeom, error ) )
    {
        delete pGeom;
        pGeom = NULL;
        return ( FALSE );
    }

    if ( !ValidateRigid( *pGeom ) )
    {
        delete pGeom;
        pGeom = NULL;
        return ( bitsery_io::Fail( error, "Rigid geometry payload failed validation." ) );
    }

    return ( TRUE );
}

//=========================================================================

xbool geom_file::LoadSkin( X_FILE* pFile, skin_geom*& pGeom, xstring& error )
{
    error.Clear();
    pGeom = NULL;

    pGeom = new skin_geom;
    if ( !bitsery_io::Read( pFile, SKIN_FILE_FORMAT, *pGeom, error ) )
    {
        delete pGeom;
        pGeom = NULL;
        return ( FALSE );
    }

    if ( !ValidateSkin( *pGeom ) )
    {
        delete pGeom;
        pGeom = NULL;
        return ( bitsery_io::Fail( error, "Skin geometry payload failed validation." ) );
    }

    return ( TRUE );
}

//=========================================================================

xbool geom_file::Validate( rigid_geom const& geom, xstring& error )
{
    error.Clear();
    if ( ValidateRigid( geom ) )
    {
        return ( TRUE );
    }

    return ( bitsery_io::Fail( error, "Rigid geometry payload failed validation." ) );
}

//=========================================================================

xbool geom_file::Validate( skin_geom const& geom, xstring& error )
{
    error.Clear();
    if ( ValidateSkin( geom ) )
    {
        return ( TRUE );
    }

    return ( bitsery_io::Fail( error, "Skin geometry payload failed validation." ) );
}

//=========================================================================

xbool geom_file::SaveRigid( X_FILE* pFile, rigid_geom const& geom, xstring& error )
{
    if ( !Validate( geom, error ) )
    {
        return ( FALSE );
    }

    return ( bitsery_io::Write( pFile, RIGID_FILE_FORMAT, geom, error ) );
}

//=========================================================================

xbool geom_file::SaveSkin( X_FILE* pFile, skin_geom const& geom, xstring& error )
{
    if ( !Validate( geom, error ) )
    {
        return ( FALSE );
    }

    return ( bitsery_io::Write( pFile, SKIN_FILE_FORMAT, geom, error ) );
}

//=========================================================================

xbool geom_file::SaveRigid( char const* pFileName, rigid_geom const& geom, xstring& error )
{
    if ( !pFileName || !pFileName[0] )
    {
        return ( bitsery_io::Fail( error, "Rigid geometry output filename is empty." ) );
    }

    X_FILE* pFile = x_fopen( pFileName, "wb" );
    if ( !pFile )
    {
        return ( bitsery_io::Fail( error, "Failed to open the rigid geometry output file." ) );
    }

    xbool const result = SaveRigid( pFile, geom, error );
    x_fclose( pFile );
    return ( result );
}

//=========================================================================

xbool geom_file::SaveSkin( char const* pFileName, skin_geom const& geom, xstring& error )
{
    if ( !pFileName || !pFileName[0] )
    {
        return ( bitsery_io::Fail( error, "Skin geometry output filename is empty." ) );
    }

    X_FILE* pFile = x_fopen( pFileName, "wb" );
    if ( !pFile )
    {
        return ( bitsery_io::Fail( error, "Failed to open the skin geometry output file." ) );
    }

    xbool const result = SaveSkin( pFile, geom, error );
    x_fclose( pFile );
    return ( result );
}

//=========================================================================
