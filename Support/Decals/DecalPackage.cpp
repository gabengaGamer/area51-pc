//==============================================================================
//  DecalPackage.cpp
//
//  Copyright (c) 2004 Inevitable Entertainment Inc. All rights reserved.
//
//  This class contains groups of decals the game can interface with. For
//  example, a single package may contain all of the blood decals used in the
//  game and groups within the package could correspond with each type of blood
//  (human, mutant, gray, etc.) Another package could describe bullet holes, and
//  each group could correspond to the material it likes to stick on (say wood
//  or concrete).
//==============================================================================

#include "DecalPackage.hpp"

//==============================================================================
// Implementation
//==============================================================================

decal_package::decal_package( void ) :
    m_nGroups( 0 ),
    m_pGroups( NULL ),
    m_nDecalDefs( 0 ),
    m_pDecalDefs( NULL )
{
}

//==============================================================================

decal_package::~decal_package( void )
{
    delete []m_pGroups;
    delete []m_pDecalDefs;
}

//==============================================================================
// Functions for the compiler
//==============================================================================

void decal_package::AllocGroups( s32 nGroups )
{
    ASSERT( nGroups >= 0 );

    group* pGroups = nGroups > 0 ? new group[nGroups]() : NULL;
    delete []m_pGroups;

    m_nGroups = nGroups;
    m_pGroups = pGroups;
}

//==============================================================================

void decal_package::AllocDecals( s32 nDecalDefs )
{
    ASSERT( nDecalDefs >= 0 );

    decal_definition* pDecalDefs = nDecalDefs > 0
                                 ? new decal_definition[nDecalDefs]
                                 : NULL;
    delete []m_pDecalDefs;

    m_nDecalDefs = nDecalDefs;
    m_pDecalDefs = pDecalDefs;
}

//==============================================================================

void decal_package::SetGroupDecalDefStart( s32 iGroup, s32 iDecalDef )
{
    ASSERT( (iGroup>=0) && (iGroup < m_nGroups) );
    ASSERT( (iDecalDef>=0) && (iDecalDef <= m_nDecalDefs) );

    m_pGroups[iGroup].iDecalDef = iDecalDef;
}

//==============================================================================

void decal_package::SetGroupDecalDefCount( s32 iGroup, s32 nDecalDefs )
{
    ASSERT( (iGroup>=0) && (iGroup < m_nGroups) );
    ASSERT( (nDecalDefs>=0) && (nDecalDefs <= m_nDecalDefs) );
    ASSERT( m_pGroups[iGroup].iDecalDef <= (m_nDecalDefs - nDecalDefs) );

    m_pGroups[iGroup].nDecalDefs = nDecalDefs;
}

//==============================================================================
