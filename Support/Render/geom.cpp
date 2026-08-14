//=========================================================================
//
//  Geom.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "geom.hpp"

//=========================================================================
// IMPLEMENTATION
//=========================================================================

geom::geom( void )
{
    x_memset( this, 0, sizeof( geom ) );
}

//=========================================================================

geom::~geom( void )
{
    delete[] m_pBone;
    delete[] m_pBoneMasks;
    delete[] m_pPropertySections;
    delete[] m_pProperties;
    delete[] m_pRigidBodies;
    delete[] m_pMesh;
    delete[] m_pSubMesh;
    delete[] m_pMaterial;
    delete[] m_pTexture;
    delete[] m_pUVKey;
    delete[] m_pLODSizes;
    delete[] m_pLODMasks;
    delete[] m_pVirtualMeshes;
    delete[] m_pVirtualTextures;
    delete[] m_pStringData;
}

//=========================================================================

geom::bone_masks const* geom::FindBoneMasks( char const* pName ) const
{
    // Loop through all bone masks
    for ( s32 i = 0; i < m_nBoneMasks; i++ )
    {
        // Lookup name of bone masks entry
        char const* pBoneMasksName = &m_pStringData[m_pBoneMasks[i].NameOffset];

        // Match?
        if ( x_strcmp( pBoneMasksName, pName ) == 0 )
        {
            return &m_pBoneMasks[i];
        }
    }

    // Not found
    return NULL;
}

//=========================================================================

geom::property_section const* geom::FindPropertySection( char const* pSection ) const
{
    ASSERT( pSection );

    // Search all property sections
    for ( s32 iSection = 0; iSection < m_nPropertySections; iSection++ )
    {
        // Lookup section info
        geom::property_section const& section = m_pPropertySections[iSection];
        char const*                   pSectionName = &m_pStringData[section.NameOffset];

        // Matching section?
        if ( x_strcmp( pSectionName, pSection ) == 0 )
        {
            return &section;
        }
    }

    // Not found
    return NULL;
}

//=========================================================================

geom::property const* geom::FindProperty( property_section const* pSection, char const* pName,
                                          geom::property::type type ) const
{
    // Make sure section is valid
    ASSERT( pSection );
    ASSERT( pSection >= &m_pPropertySections[0] );
    ASSERT( pSection < &m_pPropertySections[m_nPropertySections] );

    // Search all properties in the section
    geom::property const* pProperties = &m_pProperties[pSection->iProperty];
    for ( s32 iProperty = 0; iProperty < pSection->nProperties; iProperty++ )
    {
        // Lookup property
        geom::property const& property = pProperties[iProperty];

        // Matching type?
        if ( type == static_cast<geom::property::type>( property.Type ) )
        {
            // Matching name?
            char const* pPropName = &m_pStringData[property.NameOffset];
            if ( x_strcmp( pPropName, pName ) == 0 )
            {
                return &property;
            }
        }
    }

    // Not found
    return NULL;
}

//=========================================================================

xbool geom::GetPropertyFloat( geom::property_section const* pSection, char const* pName, f32* pValue ) const
{
    ASSERT( pSection );
    ASSERT( pName );
    ASSERT( pValue );

    // Lookup property and fail if not found
    geom::property const* pProp = FindProperty( pSection, pName, geom::property::TYPE_FLOAT );
    if ( !pProp )
    {
        return FALSE;
    }

    // Grab value and return success
    *pValue = pProp->Value.Float;
    return TRUE;
}

//=========================================================================

xbool geom::GetPropertyInteger( geom::property_section const* pSection, char const* pName, s32* pValue ) const
{
    ASSERT( pSection );
    ASSERT( pName );
    ASSERT( pValue );

    // Lookup property and fail if not found
    geom::property const* pProp = FindProperty( pSection, pName, geom::property::TYPE_INTEGER );
    if ( !pProp )
    {
        return FALSE;
    }

    // Grab value and return success
    *pValue = pProp->Value.Integer;
    return TRUE;
}

//=========================================================================

xbool geom::GetPropertyAngle( geom::property_section const* pSection, char const* pName, radian* pValue ) const
{
    ASSERT( pSection );
    ASSERT( pName );
    ASSERT( pValue );

    // Lookup property and fail if not found
    geom::property const* pProp = FindProperty( pSection, pName, geom::property::TYPE_ANGLE );
    if ( !pProp )
    {
        return FALSE;
    }

    // Grab value and return success
    *pValue = pProp->Value.Angle;
    return TRUE;
}

//=========================================================================

xbool geom::GetPropertyString( geom::property_section const* pSection, char const* pName, char* pValue,
                               s32 nChars ) const
{
    ASSERT( pSection );
    ASSERT( pName );
    ASSERT( pValue );

    // Lookup property and fail if not found
    geom::property const* pProp = FindProperty( pSection, pName, geom::property::TYPE_STRING );
    if ( !pProp )
    {
        return FALSE;
    }

    // Grab value and return success
    x_strsavecpy( pValue, &m_pStringData[pProp->Value.StringOffset], nChars );
    return TRUE;
}
