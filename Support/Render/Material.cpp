//=============================================================================
//  
//  Material.cpp  
//
//=============================================================================

//=============================================================================
//  INCLUDES
//=============================================================================

#include "Material.hpp"

//=============================================================================

material::material( void )
{
    m_RefCount = 0;
}

//=============================================================================

material::~material( void )
{
}

//=============================================================================

xbool material::operator== ( material& RHS ) const
{
    if ( m_DiffuseMap.GetIndex()     != RHS.m_DiffuseMap.GetIndex() )       return FALSE;
    if ( m_EnvironmentMap.GetIndex() != RHS.m_EnvironmentMap.GetIndex() )   return FALSE;
    if ( m_DetailMap.GetIndex()      != RHS.m_DetailMap.GetIndex() )        return FALSE;
    if ( m_Type                      != RHS.m_Type )                        return FALSE;
    if ( m_DetailScale               != RHS.m_DetailScale )                 return FALSE;
    if ( m_FixedAlpha                != RHS.m_FixedAlpha )                  return FALSE;
    if ( m_Flags                     != RHS.m_Flags )                       return FALSE;

    if ( x_memcmp( &m_UVAnim, &RHS.m_UVAnim, sizeof(m_UVAnim) ) )
        return FALSE;

    return TRUE;
}