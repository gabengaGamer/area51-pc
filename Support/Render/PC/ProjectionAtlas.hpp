//==============================================================================
//
//  ProjectionAtlas.hpp
//
//  Shared GPU atlas for projected textures and local-light cookies.
//
//==============================================================================

#ifndef PROJECTION_ATLAS_HPP
#define PROJECTION_ATLAS_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "../Texture.hpp"

//==============================================================================
//  TYPES
//==============================================================================

enum class ProjectionAtlasEncoding
{
    BlueMask = 0,
    LuminanceAlpha,
    Count
};

//------------------------------------------------------------------------------

struct ProjectionAtlasRegion
{
    ProjectionAtlasRegion( void ) : m_uvScaleBias( 0.0f, 0.0f, 0.0f, 0.0f ), m_layer( 0 ), m_maxMip( 0.0f )
    {
    }

    //-------------------------------------------------------------------------

    vector4 m_uvScaleBias;
    u32     m_layer;
    f32     m_maxMip;
};

//==============================================================================
//  PROJECTION ATLAS
//==============================================================================

class ProjectionAtlas
{
public:
    ProjectionAtlas  ( void );
    ~ProjectionAtlas ( void );
    
    xbool Init ( void );
    void  Kill ( void );
    
    void  BeginFrame ( void );
    xbool Request    ( texture::handle const& texture, ProjectionAtlasEncoding encoding );
    xbool Prepare    ( void );
    
    xbool                  GetRegion ( texture::handle const& texture, ProjectionAtlasEncoding encoding,
                                       ProjectionAtlasRegion& region ) const;
    shader_resource const* GetShaderResource ( void ) const;

private:
    enum
    {
        PAGE_SIZE = 2048,
        PAGE_COUNT = 4
    };
    
    struct FreeNode
    {
        s32 m_page;
        s32 m_x;
        s32 m_y;
        s32 m_size;
    };
    
    struct Entry
    {
        Entry( void );
    
        //-------------------------------------------------------------------------
    
        texture::handle             m_texture;
        xstring                     m_resourceName;
        ProjectionAtlasEncoding     m_encoding;
        ProjectionAtlasRegion       m_region;
        vram_texture_backend const* m_pSourceBackend;
        byte const*                 m_pSourcePixels;
        s32                         m_tileX;
        s32                         m_tileY;
        s32                         m_tileSize;
        s32                         m_imageWidth;
        s32                         m_imageHeight;
        u32                         m_lastRequestedFrame;
        xbool                       m_isResident;
    };
    
    Entry*       FindEntry        ( char const* pResourceName, ProjectionAtlasEncoding encoding );
    Entry const* FindEntry        ( char const* pResourceName, ProjectionAtlasEncoding encoding ) const;
    void         ResetAllocator   ( void );
    xbool        Allocate         ( Entry& entry );
    xbool        Upload           ( Entry const& entry );
    xbool        RebuildRequested ( void );
    xbool        IsRequested      ( Entry const& entry ) const;
    
    vram_texture     m_texture;
    xarray<Entry>    m_entries;
    xarray<FreeNode> m_freeNodes;
    u32              m_frameIndex;
    xbool            m_isInitialized;
    xbool            m_isRebuildRequired;
};

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

extern ProjectionAtlas g_ProjectionAtlas;

//==============================================================================
#endif // PROJECTION_ATLAS_HPP
//==============================================================================
