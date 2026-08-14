//==============================================================================
//
//  ProjectionAtlas.cpp
//
//==============================================================================

#include "ProjectionAtlas.hpp"

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

ProjectionAtlas g_ProjectionAtlas;

//==============================================================================
//  HELPERS
//==============================================================================

namespace
{
static s32 NextPowerOfTwo( s32 value )
{
    s32 result = 1;
    while ( result < value )
    {
        result <<= 1;
    }
    return result;
}

//----------------------------------------------------------------------------

static f32 Log2PowerOfTwo( s32 value )
{
    f32 result = 0.0f;
    while ( value > 1 )
    {
        value >>= 1;
        result += 1.0f;
    }
    return result;
}

//----------------------------------------------------------------------------

static u8 EncodeMask( xcolor const& color, ProjectionAtlasEncoding encoding )
{
    if ( encoding == ProjectionAtlasEncoding::BlueMask )
    {
        return color.B;
    }

    f32 const luminance = 0.299f * static_cast<f32>( color.R ) + 0.587f * static_cast<f32>( color.G ) +
                          0.114f * static_cast<f32>( color.B );
    f32 const mask = luminance * ( static_cast<f32>( color.A ) / 255.0f );
    return static_cast<u8>( MINMAX( 0.0f, mask + 0.5f, 255.0f ) );
}
} // namespace

//==============================================================================
//  ENTRY
//==============================================================================

ProjectionAtlas::Entry::Entry( void )
    : m_texture(), m_resourceName(), m_encoding( ProjectionAtlasEncoding::BlueMask ), m_region(),
      m_pSourceBackend( NULL ), m_pSourcePixels( NULL ), m_tileX( 0 ), m_tileY( 0 ), m_tileSize( 0 ), m_imageWidth( 0 ),
      m_imageHeight( 0 ), m_lastRequestedFrame( 0 ), m_isResident( FALSE )
{
}

//==============================================================================
//  LIFETIME
//==============================================================================

ProjectionAtlas::ProjectionAtlas( void )
    : m_texture(), m_entries(), m_freeNodes(), m_frameIndex( 0 ), m_isInitialized( FALSE ), m_isRebuildRequired( FALSE )
{
}

//==============================================================================

ProjectionAtlas::~ProjectionAtlas( void )
{
    Kill();
}

//==============================================================================

xbool ProjectionAtlas::Init( void )
{
    if ( m_isInitialized )
    {
        return TRUE;
    }

    vram_texture_desc desc;
    desc.Type       = VRAM_TEXTURE_TYPE_2D_ARRAY;
    desc.Width      = PAGE_SIZE;
    desc.Height     = PAGE_SIZE;
    desc.LayerCount = PAGE_COUNT;
    desc.MipCount   = vram_CalcMipCount( PAGE_SIZE, PAGE_SIZE );
    desc.Format     = VRAM_TEXTURE_FORMAT_R8;
    desc.UsageFlags = VRAM_TEXTURE_USAGE_SAMPLED | VRAM_TEXTURE_USAGE_COLOR_TARGET;
    desc.pDebugName = "ProjectionAtlas";

    if ( !vram_CreateTexture( m_texture, desc ) )
    {
        x_DebugMsg( "ProjectionAtlas: failed to create %dx%dx%d atlas\n", PAGE_SIZE, PAGE_SIZE, PAGE_COUNT );
        return FALSE;
    }

    m_frameIndex = 0;
    m_isInitialized = TRUE;
    m_isRebuildRequired = FALSE;
    ResetAllocator();
    return TRUE;
}

//==============================================================================

void ProjectionAtlas::Kill( void )
{
    for ( s32 i = 0; i < m_entries.GetCount(); ++i )
    {
        m_entries[i].m_texture.Destroy();
    }

    m_entries.Clear();
    m_freeNodes.Clear();
    vram_DestroyTexture( m_texture );

    m_frameIndex = 0;
    m_isInitialized = FALSE;
    m_isRebuildRequired = FALSE;
}

//==============================================================================
//  FRAME PREPARATION
//==============================================================================

void ProjectionAtlas::BeginFrame( void )
{
    ASSERT( m_isInitialized );

    ++m_frameIndex;
    if ( m_frameIndex == 0 )
    {
        m_frameIndex = 1;
        for ( s32 i = 0; i < m_entries.GetCount(); ++i )
        {
            m_entries[i].m_lastRequestedFrame = 0;
        }
    }
}

//==============================================================================

xbool ProjectionAtlas::Request( texture::handle const& textureHandle, ProjectionAtlasEncoding encoding )
{
    if ( !m_isInitialized || ( encoding >= ProjectionAtlasEncoding::Count ) )
    {
        return FALSE;
    }

    texture* pTexture = textureHandle.GetPointer();
    if ( !pTexture )
    {
        return FALSE;
    }

    char const* pResourceName = textureHandle.GetName();
    if ( !pResourceName || !pResourceName[0] )
    {
        return FALSE;
    }

    vram_texture_backend const* pSourceBackend = pTexture->m_texture.pBackend;
    byte const*                 pSourcePixels = pTexture->m_bitmap.GetPixelData();
    if ( !pSourcePixels )
    {
        return FALSE;
    }

    Entry*      pEntry = FindEntry( pResourceName, encoding );
    xbool const bNewEntry = ( pEntry == NULL );
    if ( !pEntry )
    {
        pEntry = &m_entries.Append();
        pEntry->m_resourceName = pResourceName;
        pEntry->m_encoding = encoding;
    }

    if ( ( pEntry->m_pSourceBackend != pSourceBackend ) || ( pEntry->m_pSourcePixels != pSourcePixels ) )
    {
        if ( !bNewEntry && pEntry->m_isResident )
        {
            m_isRebuildRequired = TRUE;
        }

        pEntry->m_isResident = FALSE;
        pEntry->m_pSourceBackend = pSourceBackend;
        pEntry->m_pSourcePixels = pSourcePixels;
    }

    pEntry->m_texture = textureHandle;

    pEntry->m_lastRequestedFrame = m_frameIndex;
    return TRUE;
}

//==============================================================================

xbool ProjectionAtlas::Prepare( void )
{
    if ( !m_isInitialized )
    {
        return FALSE;
    }

    if ( m_isRebuildRequired )
    {
        return RebuildRequested();
    }

    xarray<s32> pending;
    for ( s32 i = 0; i < m_entries.GetCount(); ++i )
    {
        if ( IsRequested( m_entries[i] ) && !m_entries[i].m_isResident )
        {
            pending.Append() = i;
        }
    }

    for ( s32 i = 1; i < pending.GetCount(); ++i )
    {
        s32 const      entryIndex = pending[i];
        texture const* pTexture = m_entries[entryIndex].m_texture.GetPointer();
        s32 const      entrySize =
            pTexture ? NextPowerOfTwo( MAX( pTexture->m_bitmap.GetWidth(), pTexture->m_bitmap.GetHeight() ) ) : 0;

        s32 insert = i;
        while ( insert > 0 )
        {
            texture const* pPrevious = m_entries[pending[insert - 1]].m_texture.GetPointer();
            s32 const      previousSize =
                pPrevious ? NextPowerOfTwo( MAX( pPrevious->m_bitmap.GetWidth(), pPrevious->m_bitmap.GetHeight() ) )
                               : 0;
            if ( previousSize >= entrySize )
            {
                break;
            }

            pending[insert] = pending[insert - 1];
            --insert;
        }
        pending[insert] = entryIndex;
    }

    xbool bUploaded = FALSE;
    for ( s32 i = 0; i < pending.GetCount(); ++i )
    {
        Entry& entry = m_entries[pending[i]];
        if ( !Allocate( entry ) )
        {
            return RebuildRequested();
        }

        if ( !Upload( entry ) )
        {
            entry.m_isResident = FALSE;
            m_isRebuildRequired = TRUE;
            return FALSE;
        }

        entry.m_isResident = TRUE;
        bUploaded = TRUE;
    }

    if ( bUploaded && !vram_GenerateMipmaps( m_texture ) )
    {
        m_isRebuildRequired = TRUE;
        return FALSE;
    }
    return TRUE;
}

//==============================================================================

xbool ProjectionAtlas::RebuildRequested( void )
{
    m_isRebuildRequired = TRUE;
    ResetAllocator();

    for ( s32 i = m_entries.GetCount() - 1; i >= 0; --i )
    {
        if ( !IsRequested( m_entries[i] ) )
        {
            m_entries[i].m_texture.Destroy();
            m_entries.Delete( i );
        }
    }

    xarray<s32> requested;
    for ( s32 i = 0; i < m_entries.GetCount(); ++i )
    {
        m_entries[i].m_isResident = FALSE;
        if ( IsRequested( m_entries[i] ) )
        {
            requested.Append() = i;
        }
    }

    for ( s32 i = 1; i < requested.GetCount(); ++i )
    {
        s32 const      entryIndex = requested[i];
        texture const* pTexture = m_entries[entryIndex].m_texture.GetPointer();
        s32 const      entrySize =
            pTexture ? NextPowerOfTwo( MAX( pTexture->m_bitmap.GetWidth(), pTexture->m_bitmap.GetHeight() ) ) : 0;

        s32 insert = i;
        while ( insert > 0 )
        {
            texture const* pPrevious = m_entries[requested[insert - 1]].m_texture.GetPointer();
            s32 const      previousSize =
                pPrevious ? NextPowerOfTwo( MAX( pPrevious->m_bitmap.GetWidth(), pPrevious->m_bitmap.GetHeight() ) )
                               : 0;
            if ( previousSize >= entrySize )
            {
                break;
            }

            requested[insert] = requested[insert - 1];
            --insert;
        }
        requested[insert] = entryIndex;
    }

    for ( s32 i = 0; i < requested.GetCount(); ++i )
    {
        Entry& entry = m_entries[requested[i]];
        if ( !Allocate( entry ) || !Upload( entry ) )
        {
            x_DebugMsg( "ProjectionAtlas: requested projection images exceed atlas capacity\n" );
            return FALSE;
        }
        entry.m_isResident = TRUE;
    }

    xbool const bPrepared = ( requested.GetCount() == 0 ) || vram_GenerateMipmaps( m_texture );
    if ( bPrepared )
    {
        m_isRebuildRequired = FALSE;
    }
    return bPrepared;
}

//==============================================================================
//  ALLOCATION AND UPLOAD
//==============================================================================

void ProjectionAtlas::ResetAllocator( void )
{
    m_freeNodes.SetCount( 0 );
    for ( s32 page = 0; page < PAGE_COUNT; ++page )
    {
        FreeNode& node = m_freeNodes.Append();
        node.m_page = page;
        node.m_x = 0;
        node.m_y = 0;
        node.m_size = PAGE_SIZE;
    }
}

//==============================================================================

xbool ProjectionAtlas::Allocate( Entry& entry )
{
    texture const* pTexture = entry.m_texture.GetPointer();
    if ( !pTexture )
    {
        return FALSE;
    }

    s32 const sourceWidth = pTexture->m_bitmap.GetWidth();
    s32 const sourceHeight = pTexture->m_bitmap.GetHeight();
    if ( ( sourceWidth <= 0 ) || ( sourceHeight <= 0 ) )
    {
        return FALSE;
    }

    s32       imageWidth   = sourceWidth;
    s32       imageHeight  = sourceHeight;
    s32 const sourceExtent = MAX( sourceWidth, sourceHeight );
    if ( sourceExtent > PAGE_SIZE )
    {
        f32 const scale = static_cast<f32>( PAGE_SIZE ) / static_cast<f32>( sourceExtent );
        imageWidth      = MAX( 1, static_cast<s32>( static_cast<f32>( sourceWidth ) * scale + 0.5f ) );
        imageHeight     = MAX( 1, static_cast<s32>( static_cast<f32>( sourceHeight ) * scale + 0.5f ) );
    }

    s32 const tileSize = NextPowerOfTwo( MAX( imageWidth, imageHeight ) );
    s32       bestNode = -1;
    for ( s32 i = 0; i < m_freeNodes.GetCount(); ++i )
    {
        FreeNode const& node = m_freeNodes[i];
        if ( node.m_size < tileSize )
        {
            continue;
        }

        if ( ( bestNode < 0 ) || ( node.m_size < m_freeNodes[bestNode].m_size ) ||
             ( ( node.m_size == m_freeNodes[bestNode].m_size ) && ( node.m_page < m_freeNodes[bestNode].m_page ) ) ||
             ( ( node.m_size == m_freeNodes[bestNode].m_size ) && ( node.m_page == m_freeNodes[bestNode].m_page ) &&
               ( node.m_y < m_freeNodes[bestNode].m_y ) ) ||
             ( ( node.m_size == m_freeNodes[bestNode].m_size ) && ( node.m_page == m_freeNodes[bestNode].m_page ) &&
               ( node.m_y == m_freeNodes[bestNode].m_y ) && ( node.m_x < m_freeNodes[bestNode].m_x ) ) )
        {
            bestNode = i;
        }
    }

    if ( bestNode < 0 )
    {
        return FALSE;
    }

    FreeNode node = m_freeNodes[bestNode];
    m_freeNodes.Delete( bestNode );

    while ( node.m_size > tileSize )
    {
        s32 const childSize = node.m_size / 2;

        FreeNode& topRight = m_freeNodes.Append();
        topRight.m_page    = node.m_page;
        topRight.m_x       = node.m_x + childSize;
        topRight.m_y       = node.m_y;
        topRight.m_size    = childSize;

        FreeNode& bottomLeft = m_freeNodes.Append();
        bottomLeft.m_page    = node.m_page;
        bottomLeft.m_x       = node.m_x;
        bottomLeft.m_y       = node.m_y + childSize;
        bottomLeft.m_size    = childSize;

        FreeNode& bottomRight = m_freeNodes.Append();
        bottomRight.m_page    = node.m_page;
        bottomRight.m_x       = node.m_x + childSize;
        bottomRight.m_y       = node.m_y + childSize;
        bottomRight.m_size    = childSize;

        node.m_size = childSize;
    }

    s32 const imageX = node.m_x + ( tileSize - imageWidth ) / 2;
    s32 const imageY = node.m_y + ( tileSize - imageHeight ) / 2;

    entry.m_tileX           = node.m_x;
    entry.m_tileY           = node.m_y;
    entry.m_tileSize        = tileSize;
    entry.m_imageWidth      = imageWidth;
    entry.m_imageHeight     = imageHeight;
    entry.m_region.m_layer  = static_cast<u32>( node.m_page );
    entry.m_region.m_maxMip = Log2PowerOfTwo( tileSize );
    entry.m_region.m_uvScaleBias.Set(
        ( imageWidth > 1 ) ? static_cast<f32>( imageWidth - 1 ) / static_cast<f32>( PAGE_SIZE ) : 0.0f,
        ( imageHeight > 1 ) ? static_cast<f32>( imageHeight - 1 ) / static_cast<f32>( PAGE_SIZE ) : 0.0f,
        ( static_cast<f32>( imageX ) + 0.5f ) / static_cast<f32>( PAGE_SIZE ),
        ( static_cast<f32>( imageY ) + 0.5f ) / static_cast<f32>( PAGE_SIZE ) );
    return TRUE;
}

//==============================================================================

xbool ProjectionAtlas::Upload( Entry const& entry )
{
    texture const* pTexture = entry.m_texture.GetPointer();
    if ( !pTexture || ( entry.m_tileSize <= 0 ) )
    {
        return FALSE;
    }

    xbitmap const& bitmap = pTexture->m_bitmap;
    xarray<u8>     image;
    image.SetCount( entry.m_imageWidth * entry.m_imageHeight );

    xbool const bResize = ( entry.m_imageWidth != bitmap.GetWidth() ) || ( entry.m_imageHeight != bitmap.GetHeight() );
    for ( s32 y = 0; y < entry.m_imageHeight; ++y )
    {
        for ( s32 x = 0; x < entry.m_imageWidth; ++x )
        {
            xcolor color;
            if ( bResize )
            {
                f32 const u = ( static_cast<f32>( x ) + 0.5f ) / static_cast<f32>( entry.m_imageWidth );
                f32 const v = ( static_cast<f32>( y ) + 0.5f ) / static_cast<f32>( entry.m_imageHeight );
                color = bitmap.GetBilinearColor( u, v, TRUE );
            }
            else
            {
                color = bitmap.GetPixelColor( x, y );
            }

            image[y * entry.m_imageWidth + x] = EncodeMask( color, entry.m_encoding );
        }
    }

    xarray<u8> tile;
    tile.SetCount( entry.m_tileSize * entry.m_tileSize );
    s32 const imageOffsetX = ( entry.m_tileSize - entry.m_imageWidth ) / 2;
    s32 const imageOffsetY = ( entry.m_tileSize - entry.m_imageHeight ) / 2;
    for ( s32 y = 0; y < entry.m_tileSize; ++y )
    {
        s32 const sourceY = MINMAX( 0, y - imageOffsetY, entry.m_imageHeight - 1 );
        for ( s32 x = 0; x < entry.m_tileSize; ++x )
        {
            s32 const sourceX = MINMAX( 0, x - imageOffsetX, entry.m_imageWidth - 1 );
            tile[y * entry.m_tileSize + x] = image[sourceY * entry.m_imageWidth + sourceX];
        }
    }

    vram_texture_upload_desc upload;
    upload.Region.Layer  = entry.m_region.m_layer;
    upload.Region.X      = entry.m_tileX;
    upload.Region.Y      = entry.m_tileY;
    upload.Region.Width  = entry.m_tileSize;
    upload.Region.Height = entry.m_tileSize;
    upload.pData         = tile.GetPtr();
    upload.Size          = tile.GetCount();
    upload.RowPitch      = entry.m_tileSize;
    upload.SlicePitch    = tile.GetCount();
    return vram_UploadTexture( m_texture, upload );
}

//==============================================================================
//  QUERIES
//==============================================================================

ProjectionAtlas::Entry* ProjectionAtlas::FindEntry( char const* pResourceName, ProjectionAtlasEncoding encoding )
{
    for ( s32 i = 0; i < m_entries.GetCount(); ++i )
    {
        if ( ( m_entries[i].m_resourceName == pResourceName ) && ( m_entries[i].m_encoding == encoding ) )
        {
            return &m_entries[i];
        }
    }
    return NULL;
}

//==============================================================================

ProjectionAtlas::Entry const* ProjectionAtlas::FindEntry( char const*             pResourceName,
                                                          ProjectionAtlasEncoding encoding ) const
{
    for ( s32 i = 0; i < m_entries.GetCount(); ++i )
    {
        if ( ( m_entries[i].m_resourceName == pResourceName ) && ( m_entries[i].m_encoding == encoding ) )
        {
            return &m_entries[i];
        }
    }
    return NULL;
}

//==============================================================================

xbool ProjectionAtlas::IsRequested( Entry const& entry ) const
{
    return entry.m_lastRequestedFrame == m_frameIndex;
}

//==============================================================================

xbool ProjectionAtlas::GetRegion( texture::handle const& textureHandle, ProjectionAtlasEncoding encoding,
                                  ProjectionAtlasRegion& region ) const
{
    if ( !textureHandle.GetPointer() )
    {
        return FALSE;
    }

    Entry const* pEntry = FindEntry( textureHandle.GetName(), encoding );
    if ( !pEntry || !pEntry->m_isResident )
    {
        return FALSE;
    }

    region = pEntry->m_region;
    return TRUE;
}

//==============================================================================

shader_resource const* ProjectionAtlas::GetShaderResource( void ) const
{
    return vram_GetShaderResource( m_texture );
}

//==============================================================================
