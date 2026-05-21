//=============================================================================
//  
//  Material.hpp  
//
//=============================================================================

#ifndef MATERIAL_HPP
#define MATERIAL_HPP

//=============================================================================
//  INCLUDES
//=============================================================================

#include "Texture.hpp"
#include "Material_Prefs.hpp"

//=============================================================================

class material
{
public:

    struct uvanim
    {
        f32     CurrentFrame;   // current frame as float
        s16     iKey;           // offset into geometry
        s8      iFrame;         // current frame as int
        s8      Dir;            // direction of animation {-1,0,1}
        s8      Type;           // type of animation
        s8      nFrames;        // total number of frames in animation
        s8      FPS;            // frames per second to run this animation
        s8      StartFrame;     // starting frame for this animation
    };

    material            ( void );
   ~material            ( void );
    xbool   operator==  ( material& RHS ) const;

    xbool               HasUVAnimation  ( void ) const;
    s32                 AddRef          ( void );
    s32                 Release         ( void );
    s32                 GetRefCount     ( void ) const;

    s8                  m_Type;
    f32                 m_DetailScale;
    f32                 m_FixedAlpha;
    u16                 m_Flags;                          // flags

    texture::handle     m_DiffuseMap;
    texture::handle     m_EnvironmentMap;
    texture::handle     m_DetailMap;
    uvanim              m_UVAnim;
    s32                 m_RefCount;
};

//=============================================================================

inline xbool material::HasUVAnimation( void ) const
{
    return( m_UVAnim.nFrames > 0 );
}

//=============================================================================

inline s32 material::AddRef( void )
{
    return (++m_RefCount);
}

//=============================================================================

inline s32 material::Release( void )
{
    return (--m_RefCount);
}

//=============================================================================

inline s32 material::GetRefCount( void ) const
{
    return m_RefCount;
}

//=============================================================================
#endif
//=============================================================================
