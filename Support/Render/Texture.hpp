//=========================================================================
//  
//  Texture.hpp  
//
//=========================================================================

#ifndef TEXTURE_HPP
#define TEXTURE_HPP

//=========================================================================
// INCLUDES
//=========================================================================

#include "..\ResourceMgr\ResourceMgr.hpp"
#include "x_files.hpp"
#include "x_bitmap.hpp"

//=========================================================================
// CLASS
//=========================================================================

class texture
{
public:

    texture( void );

    static void GetStats( s32* pNumTextureLoaded, s32* pTextureMemorySize );

    typedef rhandle<texture> handle;

    xbitmap m_Bitmap;

    friend struct texture_loader;
};

//=========================================================================

class cubemap
{
public:
    cubemap( void );

    enum sides
    {
        TOP = 0,
        BOTTOM,
        FRONT,
        BACK,
        LEFT,
        RIGHT
    };

    typedef rhandle<cubemap> handle;

    void* m_hTexture;

    xbitmap m_Bitmap[6];

    friend struct cubemap_loader;
};

//=========================================================================
#endif
//=========================================================================

