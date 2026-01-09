//==============================================================================
//  
//  d3deng_draw.cpp
//  
//==============================================================================

// TODO: GS: It might make sense to replace the current variable registration 
// with a stylized one like in D3DENG "static struct eng_locals".

// TODO: GS: At the moment d3deng_draw is more like a dump, 
// it contains essentially the code that should not be there. 
// All the logic of working with the UI should be transferred to other files. 
// Or made more universal and not UI specialized.
// FIXME!!!

//==============================================================================
//  PLATFORM CHECK
//==============================================================================

#include "x_target.hpp"

#ifndef TARGET_PC
#error This file should only be compiled for PC platform. Please check your exclusions on your project spec.
#endif

//==============================================================================
//  INCLUDES
//==============================================================================

#include "e_Engine.hpp"
#include "d3deng_shader.hpp"
#include "d3deng_draw_shaders.hpp"
#include "d3deng_state.hpp"
#include "d3deng_rtarget.hpp"
#include "d3deng_draw_rt.hpp"

#define NUM_VERTEX_BUFFERS          4                               // Number of vertex buffers to cycle through
#define NUM_VERTICES                1000                            // Number of vertices in each buffer
#define NUM_QUAD_INDICES            ((NUM_VERTICES/4)*6)            // Number of Quad indices needed

#define TRIGGER_POINTS              ((NUM_VERTICES/1)*1)            // Vertex indicies to trigger buffer dispatch
#define TRIGGER_LINES               ((NUM_VERTICES/2)*2)            // ...
#define TRIGGER_LINE_STRIPS         ((NUM_VERTICES/2)*2)            // ...
#define TRIGGER_TRIANGLES           ((NUM_VERTICES/3)*3)            // ...
#define TRIGGER_TRIANGLE_STRIPS     ((NUM_VERTICES/3)*3)            // ...
#define TRIGGER_QUADS               ((NUM_VERTICES/4)*4)            // ...
#define TRIGGER_RECTS               ((NUM_VERTICES/4))              // ...

#define NUM_SPRITES                 (NUM_VERTICES/4)                // Number of Sprites in sprite buffer

//==============================================================================
// TYPES
//==============================================================================

typedef void (*fnptr_dispatch)( void );

//==============================================================================
// DRAW VERTEX
//==============================================================================

struct drawvertex3d
{
    vector3p    Position;
    u32         Color;
    vector2     UV;
};

struct drawvertex2d
{
    vector3p    Position;
    f32         RHW;
    u32         Color;
    vector2     UV;
};

struct drawsprite
{
    vector3     Position;
    vector2     WH;
    u32         Color;
    vector2     UV0;
    vector2     UV1;
    radian      Rotation;
    xbool       IsRotated;
};

//==============================================================================
// VARIABLES
//==============================================================================

xbool                       m_Initialized = FALSE;                  // Becomes TRUE when Initialized
xbool                       m_bBegin = FALSE;                       // TRUE when between begin/end

draw_primitive              m_Primitive;                            // Primitive Type, see enum draw_primitive
u32                         m_Flags;                                // Flags, see defines
xbool                       m_Is2D;                                 // TRUE for 2D mode
xbool                       m_IsUI;                                 // TRUE for UI render target mode
xbool                       m_IsPrimitiveTarget;                    // TRUE for primitive render target mode
xbool                       m_IsTextured;                           // TRUE for Textured mode
xbool                       m_UseGBufferDepth;                      // TRUE when gbuffer depth should be applied
draw_gdepth_provider        m_pGDepthProvider = NULL;               // GBuffer depth provider

matrix4                     m_L2W;                                  // L2W matrix for draw

const vector2*              m_pUVs;                                 // Pointer to UV array
s32                         m_nUVs;                                 // Number of elements
s32                         m_sUVs;                                 // Stride of elements

const xcolor*               m_pColors;                              // Pointer to Color array
s32                         m_nColors;                              // Number of elements
s32                         m_sColors;                              // Stride of elements

const vector3*              m_pVerts;                               // Poitner to vertex array
s32                         m_nVerts;                               // Number of elements
s32                         m_sVerts;                               // Stride of elements

vector2                     m_UV;                                   // Current UV
xcolor                      m_Color;                                // Current Color

s32                         m_iActiveBuffer;                        // Active vertex buffer index
ID3D11Buffer*               m_pVertexBuffer3d[NUM_VERTEX_BUFFERS];  // Array of vertex buffer pointers
drawvertex3d*               m_pActiveBuffer3dStart;                 // Active vertex buffer data pointer
drawvertex3d*               m_pActiveBuffer3d;                      // Active vertex buffer data pointer
ID3D11Buffer*               m_pVertexBuffer2d[NUM_VERTEX_BUFFERS];  // Array of vertex buffer pointers
drawvertex2d*               m_pActiveBuffer2dStart;                 // Active vertex buffer data pointer
drawvertex2d*               m_pActiveBuffer2d;                      // Active vertex buffer data pointer

s32                         m_iVertex;                              // Index of vertex in buffer
s32                         m_iTrigger;                             // Index of vertex to trigger flush

ID3D11Buffer*               m_pIndexQuads;                          // Index array for rendering Quads

drawsprite*                 m_pSpriteBuffer;                        // Sprite Buffer
s32                         m_iSprite;                              // Next Sprite Index

fnptr_dispatch              m_pfnDispatch;                          // Dispatch Function

s32                         m_ZBias;

//==============================================================================
// ...
//==============================================================================

ID3D11Buffer*               m_pConstantBuffer;                      // Constant buffer for matrices
ID3D11Buffer*               m_pProjectionBuffer;                    // Projection constant buffer
ID3D11Buffer*               m_pFlagsBuffer;                         // Flags constant buffer
ID3D11InputLayout*          m_pInputLayout3d;                       // Input layout for 3D vertices
ID3D11InputLayout*          m_pInputLayout2d;                       // Input layout for 2D vertices
ID3D11VertexShader*         m_pVertexShader3d;                      // 3D vertex shader
ID3D11VertexShader*         m_pVertexShader2d;                      // 2D vertex shader
ID3D11PixelShader*          m_pPixelShader;                         // Pixel shader

// Staging buffers for CPU write
drawvertex3d*               m_pStagingData3d[NUM_VERTEX_BUFFERS];   // CPU-side staging data
drawvertex2d*               m_pStagingData2d[NUM_VERTEX_BUFFERS];   // CPU-side staging data

// Current bilinear mode tracking
static xbool                s_bCurrentBilinear = TRUE;

//==============================================================================
// HELPER FUNCTIONS
//==============================================================================

static 
inline u32 draw_ColorToU32( const xcolor& Color )
{
    return ((u32)Color.A << 24) | ((u32)Color.R << 16) | ((u32)Color.G << 8) | ((u32)Color.B);
}

//==============================================================================

static
xbool draw_IsPremultipliedTargetActive( void )
{
    if( m_IsUI )
        return draw_HasValidUITarget();

    return FALSE;
}

//==============================================================================
// FUNCTIONS
//==============================================================================

void draw_SetZBias( s32 Bias )
{
    ASSERT( (Bias>=0) && (Bias<=16) );
    m_ZBias = Bias;
}

//==============================================================================

void draw_RegisterGDepthProvider( draw_gdepth_provider pfnProvider )
{
    m_pGDepthProvider = pfnProvider;
}

//==============================================================================

void draw_Init( void )
{
    s32     i;
    s32     v;
    u16*    pIndexData;
    HRESULT Error;

    m_ZBias = 0;

    ASSERT( !m_Initialized );

    if( g_pd3dDevice )
    {
        // Allocate staging buffers
        for( i=0 ; i<NUM_VERTEX_BUFFERS ; i++ )
        {
            m_pStagingData3d[i] = (drawvertex3d*)x_malloc(NUM_VERTICES * sizeof(drawvertex3d));
            m_pStagingData2d[i] = (drawvertex2d*)x_malloc(NUM_VERTICES * sizeof(drawvertex2d));
        }

        // Allocate vertex buffers
        D3D11_BUFFER_DESC bd;
        ZeroMemory( &bd, sizeof(bd) );
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        bd.MiscFlags = 0;

        for( i=0 ; i<NUM_VERTEX_BUFFERS ; i++ )
        {
            bd.ByteWidth = NUM_VERTICES * sizeof(drawvertex3d);
            Error = g_pd3dDevice->CreateBuffer( &bd, NULL, &m_pVertexBuffer3d[i] );
            ASSERT( SUCCEEDED(Error) );

            bd.ByteWidth = NUM_VERTICES * sizeof(drawvertex2d);
            Error = g_pd3dDevice->CreateBuffer( &bd, NULL, &m_pVertexBuffer2d[i] );
            ASSERT( SUCCEEDED(Error) );
        }

        // Set active vertex buffer
        m_iActiveBuffer = 0;
        m_pActiveBuffer3dStart = m_pStagingData3d[m_iActiveBuffer];
        m_pActiveBuffer2dStart = m_pStagingData2d[m_iActiveBuffer];
        m_pActiveBuffer3d = m_pActiveBuffer3dStart;
        m_pActiveBuffer2d = m_pActiveBuffer2dStart;
        m_iVertex = 0;

        // Allocate index buffer for Quads
        {
            pIndexData = (u16*)x_malloc(NUM_QUAD_INDICES * sizeof(u16));
            
            for( v=i=0; i<NUM_QUAD_INDICES; i += 6, v += 4 )
            {
                pIndexData[i+0] = v;    
                pIndexData[i+1] = v+1;
                pIndexData[i+2] = v+2;

                pIndexData[i+3] = v+0;
                pIndexData[i+4] = v+2;
                pIndexData[i+5] = v+3;
            }

            D3D11_BUFFER_DESC ibd;
            ZeroMemory( &ibd, sizeof(ibd) );
            ibd.Usage = D3D11_USAGE_DEFAULT;
            ibd.ByteWidth = NUM_QUAD_INDICES * sizeof(u16);
            ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
            ibd.CPUAccessFlags = 0;
            ibd.MiscFlags = 0;

            D3D11_SUBRESOURCE_DATA initData;
            initData.pSysMem = pIndexData;
            initData.SysMemPitch = 0;
            initData.SysMemSlicePitch = 0;

            Error = g_pd3dDevice->CreateBuffer( &ibd, &initData, &m_pIndexQuads );
            ASSERT( SUCCEEDED(Error) );

            x_free(pIndexData);
        }

        // Create constant buffers using shader system
        m_pConstantBuffer = shader_CreateConstantBuffer( sizeof(cb_matrices), CB_TYPE_DYNAMIC );
        m_pProjectionBuffer = shader_CreateConstantBuffer( sizeof(cb_projection_2d), CB_TYPE_DYNAMIC );
        m_pFlagsBuffer = shader_CreateConstantBuffer( sizeof(cb_render_flags), CB_TYPE_DYNAMIC );

        ASSERT( m_pConstantBuffer && m_pProjectionBuffer && m_pFlagsBuffer );

        // Load shaders using shader system        
        m_pVertexShader3d = (ID3D11VertexShader*)shader_CompileShader( s_VertexShader3D,
                                                                       SHADER_TYPE_VERTEX,
                                                                       "main",
                                                                       "vs_5_0",
                                                                       NULL,
                                                                       &m_pInputLayout3d,
                                                                       s_InputLayout3D,
                                                                       ARRAYSIZE(s_InputLayout3D) );
        
        m_pVertexShader2d = (ID3D11VertexShader*)shader_CompileShader( s_VertexShader2D,
                                                                       SHADER_TYPE_VERTEX,
                                                                       "main",
                                                                       "vs_5_0",
                                                                       NULL,
                                                                       &m_pInputLayout2d,
                                                                       s_InputLayout2D,
                                                                       ARRAYSIZE(s_InputLayout2D) );
            
        m_pPixelShader = (ID3D11PixelShader*)shader_CompileShader( s_PixelShaderBasic,
                                                                   SHADER_TYPE_PIXEL,
                                                                   "main",
                                                                   "ps_5_0" );

        if( !m_pVertexShader3d || !m_pVertexShader2d || !m_pPixelShader )
        {
            x_DebugMsg( "Draw: Failed to load required shaders\n" );
            ASSERT( FALSE );
        }
    }

    // Allocate Sprite Buffer
    m_pSpriteBuffer = (drawsprite*)x_malloc( sizeof(drawsprite) * NUM_SPRITES );
    m_iSprite = 0;

    // Clear L2W matrix, UV, Color and Vertex
    m_L2W.Identity();
    m_UV    = vector2( 0.0f, 0.0f );
    m_Color = xcolor( 255, 255, 255, 255 );

    // Clear pointers
    m_pUVs    = NULL;
    m_pColors = NULL;
    m_pVerts  = NULL;

    // Initialize draw render targets
    draw_rt_Init();

    // Tell system we are initialized
    m_Initialized = TRUE;
}

//==============================================================================

void draw_Kill( void )
{
    s32     i;

    ASSERT( m_Initialized );

    if( g_pd3dDevice )
    {
        // Release buffers
        for( i=0 ; i<NUM_VERTEX_BUFFERS ; i++ )
        {
            if( m_pVertexBuffer3d[i] ) m_pVertexBuffer3d[i]->Release();
            if( m_pVertexBuffer2d[i] ) m_pVertexBuffer2d[i]->Release();
            x_free( m_pStagingData3d[i] );
            x_free( m_pStagingData2d[i] );
        }

        // Release index buffer
        if( m_pIndexQuads ) m_pIndexQuads->Release();

        // Release constant buffers
        if( m_pConstantBuffer ) m_pConstantBuffer->Release();
        if( m_pProjectionBuffer ) m_pProjectionBuffer->Release();
        if( m_pFlagsBuffer ) m_pFlagsBuffer->Release();

        // Release shaders and layouts
        if( m_pVertexShader3d ) m_pVertexShader3d->Release();
        if( m_pVertexShader2d ) m_pVertexShader2d->Release();
        if( m_pPixelShader ) m_pPixelShader->Release();
        if( m_pInputLayout3d ) m_pInputLayout3d->Release();
        if( m_pInputLayout2d ) m_pInputLayout2d->Release();
    }

    // Kill render targets
    draw_rt_Kill();

    // Free Sprite buffer
    x_free( m_pSpriteBuffer );

    // No longer initialized
    m_Initialized = FALSE;
}

//==============================================================================

static
void draw_SetMatrices( const view* pView )
{
    if( !g_pd3dContext )
        return;

    // Set Viewport for rendering
    eng_SetViewport( *pView );

    // 2D or 3D rendering
    if( m_Is2D )
    {
        if( m_Flags & DRAW_2D_KEEP_Z )
        {
            cb_matrices cbData;
            
            // Special World matrix for fixing Z
            cbData.World.Identity();
            f32 ZNear, ZFar;
            pView->GetZLimits(ZNear, ZFar);
            cbData.World(2,2) = 0.0f; 
            cbData.World(3,2) = ZNear;
            
            cbData.View.Identity();
            
            // Create a Screen-to-Clip projection matrix
            s32 x0,y0,x1,y1;
            pView->GetViewport(x0,y0,x1,y1);
            s32 w = x1-x0;
            s32 h = y1-y0;
            
            cbData.Projection.Identity();
            cbData.Projection(0,0) = 2.0f/w;
            cbData.Projection(1,1) = -2.0f/h;
            cbData.Projection(2,2) = 1.0f/(ZFar-ZNear);
            cbData.Projection(3,0) = -1.0f;
            cbData.Projection(3,1) = +1.0f;
            cbData.Projection(3,2) = 0.0f;
            cbData.Projection(3,3) = 1.0f;
            
            shader_UpdateConstantBuffer( m_pConstantBuffer, &cbData, sizeof(cbData) );
            g_pd3dContext->VSSetConstantBuffers( 0, 1, &m_pConstantBuffer );
        }
        else
        {
            // Standard 2D rendering
            cb_projection_2d cbData;
            s32 xRes, yRes;
            eng_GetRes( xRes, yRes );
            cbData.ScreenWidth = (f32)xRes;
            cbData.ScreenHeight = (f32)yRes;
            cbData.pad0 = 0.0f;
            cbData.pad1 = 0.0f;

            shader_UpdateConstantBuffer( m_pProjectionBuffer, &cbData, sizeof(cbData) );
            g_pd3dContext->VSSetConstantBuffers( 1, 1, &m_pProjectionBuffer );
        }
    }
    else
    {
        // 3D rendering
        cb_matrices cbData;
        
        if( m_Primitive == DRAW_SPRITES )
        {
            cbData.World.Identity();
            cbData.View.Identity();
            cbData.Projection = pView->GetV2C();
        }
        else
        {
            cbData.World = m_L2W;
            cbData.View = pView->GetW2V();
            cbData.Projection = pView->GetV2C();
        }

        shader_UpdateConstantBuffer( m_pConstantBuffer, &cbData, sizeof(cbData) );
        g_pd3dContext->VSSetConstantBuffers( 0, 1, &m_pConstantBuffer );
    }

    // Update flags buffer
    cb_render_flags cbFlags;
    cbFlags.UseTexture = m_IsTextured ? 1 : 0;
    cbFlags.UseAlpha = (m_Flags & DRAW_USE_ALPHA) ? 1 : 0;
    cbFlags.UsePremultipliedAlpha = draw_IsPremultipliedTargetActive() ? 1 : 0;
    cbFlags.pad1 = 0;

    shader_UpdateConstantBuffer( m_pFlagsBuffer, &cbFlags, sizeof(cbFlags) );
    g_pd3dContext->PSSetConstantBuffers( 1, 1, &m_pFlagsBuffer );
}

//==============================================================================

static
void draw_Dispatch( void )
{
    if( !g_pd3dContext )
        return;

    HRESULT Error;

    if( m_iVertex != 0 )
    {        
        // Copy staging data to GPU buffer
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        
        if( m_Is2D )
        {
            Error = g_pd3dContext->Map( m_pVertexBuffer2d[m_iActiveBuffer], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource );
            if( SUCCEEDED(Error) )
            {
                x_memcpy( mappedResource.pData, m_pActiveBuffer2dStart, m_iVertex * sizeof(drawvertex2d) );
                g_pd3dContext->Unmap( m_pVertexBuffer2d[m_iActiveBuffer], 0 );
            }
        }
        else
        {
            Error = g_pd3dContext->Map( m_pVertexBuffer3d[m_iActiveBuffer], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource );
            if( SUCCEEDED(Error) )
            {
                x_memcpy( mappedResource.pData, m_pActiveBuffer3dStart, m_iVertex * sizeof(drawvertex3d) );
                g_pd3dContext->Unmap( m_pVertexBuffer3d[m_iActiveBuffer], 0 );
            }
        }

        // Get View
        const view* pView = eng_GetView();
        ASSERT( pView );

        // Setup D3D Matrices
        draw_SetMatrices( pView );

        // Set input layout and shaders
        
        // GS: TODO: For now, in the case of DRAW_CUSTOM_[]_SHADER we only set a custom shader, 
        // without custom IASetVertexBuffers and IASetInputLayout, 
        // in the future it makes sense to make this code customizable too, 
        // maybe shoud use DRAW_CUSTOM_[]_BUFFER and DRAW_CUSTOM_[]_LAYOUT???    
        
        if( m_Is2D && !(m_Flags & DRAW_2D_KEEP_Z) )
        {
            g_pd3dContext->IASetInputLayout( m_pInputLayout2d );
            if( !(m_Flags & DRAW_CUSTOM_VS_SHADER) )
            {
                g_pd3dContext->VSSetShader( m_pVertexShader2d, NULL, 0 );
            }
            
            // Set vertex buffer
            UINT stride = sizeof(drawvertex2d);
            UINT offset = 0;
            g_pd3dContext->IASetVertexBuffers( 0, 1, &m_pVertexBuffer2d[m_iActiveBuffer], &stride, &offset );
        }
        else
        {
            g_pd3dContext->IASetInputLayout( m_pInputLayout3d );
            if( !(m_Flags & DRAW_CUSTOM_VS_SHADER) )
            {
                g_pd3dContext->VSSetShader( m_pVertexShader3d, NULL, 0 );
            }
            
            // Set vertex buffer
            UINT stride = sizeof(drawvertex3d);
            UINT offset = 0;
            g_pd3dContext->IASetVertexBuffers( 0, 1, &m_pVertexBuffer3d[m_iActiveBuffer], &stride, &offset );
        }

        if( !(m_Flags & DRAW_CUSTOM_PS_SHADER) )
        {
            g_pd3dContext->PSSetShader( m_pPixelShader, NULL, 0 );
        }

        // Set primitive topology and draw
        switch( m_Primitive )
        {
        case DRAW_POINTS:
            g_pd3dContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_POINTLIST );
            g_pd3dContext->Draw( m_iVertex, 0 );
            break;

        case DRAW_LINES:
            g_pd3dContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_LINELIST );
            g_pd3dContext->Draw( m_iVertex, 0 );
            break;

        case DRAW_LINE_STRIPS:
            g_pd3dContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP );
            g_pd3dContext->Draw( m_iVertex, 0 );
            break;

        case DRAW_TRIANGLES:
            g_pd3dContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
            g_pd3dContext->Draw( m_iVertex, 0 );
            break;

        case DRAW_TRIANGLE_STRIPS:
            g_pd3dContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );
            g_pd3dContext->Draw( m_iVertex, 0 );
            break;

        case DRAW_QUADS:
        case DRAW_RECTS:
        case DRAW_SPRITES:
            g_pd3dContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
            g_pd3dContext->IASetIndexBuffer( m_pIndexQuads, DXGI_FORMAT_R16_UINT, 0 );
            g_pd3dContext->DrawIndexed( (m_iVertex/4)*6, 0, 0 );
            break;
        }

        // Set new active vertex buffer
        m_iActiveBuffer++;
        if( m_iActiveBuffer == NUM_VERTEX_BUFFERS ) m_iActiveBuffer = 0;
        
        m_pActiveBuffer3dStart = m_pStagingData3d[m_iActiveBuffer];
        m_pActiveBuffer3d = m_pActiveBuffer3dStart;
        m_pActiveBuffer2dStart = m_pStagingData2d[m_iActiveBuffer];
        m_pActiveBuffer2d = m_pActiveBuffer2dStart;

        m_iVertex = 0;
    }
}

//==============================================================================

static
void draw_DispatchRects( void )
{
    if( !g_pd3dContext )
        return;

    s32             nQuads = m_iVertex/2;
    s32             i;

    // Only if there are quads to process
    if( nQuads > 0 )
    {        
        if( m_Is2D )
        {
            drawvertex2d*   ps     = &m_pActiveBuffer2dStart[(nQuads-1)*2];
            drawvertex2d*   pd     = &m_pActiveBuffer2dStart[(nQuads-1)*4];

            // Rects are specified with top-left and bottom-right corners, expand the data in place
            // to include all points to make quads, then render
            for( i=nQuads ; i>0 ; i-- )
            {
                pd[3] = ps[0];
                pd[2] = ps[1];
                pd[1] = pd[2];
                pd[0] = pd[3];

                pd[3].Position.X = pd[2].Position.X;
                pd[1].Position.X = pd[0].Position.X;
                pd[3].UV.X       = pd[2].UV.X;
                pd[1].UV.X       = pd[0].UV.X;

                ps -= 2;
                pd -= 4;
            }

            m_iVertex = nQuads*4;
        }
        else
        {
            drawvertex3d*   ps     = &m_pActiveBuffer3dStart[(nQuads-1)*2];
            drawvertex3d*   pd     = &m_pActiveBuffer3dStart[(nQuads-1)*4];

            // Rects are specified with top-left and bottom-right corners, expand the data in place
            // to include all points to make quads, then render
            for( i=nQuads ; i>0 ; i-- )
            {
                pd[3] = ps[0];
                pd[2] = ps[1];
                pd[1] = pd[2];
                pd[0] = pd[3];

                pd[3].Position.X = pd[2].Position.X;
                pd[1].Position.X = pd[0].Position.X;
                pd[3].UV.X       = pd[2].UV.X;
                pd[1].UV.X       = pd[0].UV.X;

                ps -= 2;
                pd -= 4;
            }

            m_iVertex = nQuads*4;
        }

        // Call regular Dispatch function
        draw_Dispatch();
    }
}

//==========================================================================

static 
inline void draw_sincos( radian Angle, f32& Sine, f32& Cosine )
{
    #define I_360   (1<<16)
    #define I_90    (I_360/4)
    #define I_180   (I_360/2)
    #define I_270   (I_90*3)

    s32 IAngle = ((s32)(Angle*(I_360/R_360)))&(I_360-1);
    s32 si,ci;

    if( IAngle >= I_180 )
    {
        if( IAngle >= I_270 ) { si =  IAngle - I_360; ci =  IAngle - I_270; }
        else                  { si = -IAngle + I_180; ci =  IAngle - I_270; }
    }
    else
    {
        if( IAngle >= I_90 )  { si = -IAngle + I_180; ci = -IAngle + I_90;  }
        else                  { si = IAngle;          ci = -IAngle + I_90;  }
    }

    f32 sq  = si*(R_360/I_360);
    f32 cq  = ci*(R_360/I_360);
    f32 sq2 = sq*sq;
    f32 cq2 = cq*cq;
    Sine   = (((0.00813767f*sq2) - 0.1666666f)*sq2 + 1)*sq;
    Cosine = (((0.00813767f*cq2) - 0.1666666f)*cq2 + 1)*cq;

    #undef I_360
    #undef I_90
    #undef I_180
    #undef I_270
}

//==============================================================================

static
void draw_DispatchSprites( void )
{
    if( !g_pd3dContext )
        return;

    s32     j;

    ASSERT( m_Primitive == DRAW_SPRITES );
    ASSERT( m_iVertex == 0 );

    // If there are any sprites to draw
    if( m_iSprite > 0 )
    {    
        // Get View
        const view* pView = eng_GetView();
        ASSERT( pView );

        // Get Sprite and Vertex buffer pointers
        drawsprite*   pSprite = m_pSpriteBuffer;
        drawvertex3d* pVertex3d = m_pActiveBuffer3dStart;
        drawvertex2d* pVertex2d = m_pActiveBuffer2dStart;

        // Get W2V matrix
        const matrix4& W2V = pView->GetW2V();

        // Loop through sprites
        for( j=0 ; j<m_iSprite ; j++ )
        {
            u32     Color = pSprite->Color;
            f32     U0    = pSprite->UV0.X;
            f32     V0    = pSprite->UV0.Y;
            f32     U1    = pSprite->UV1.X;
            f32     V1    = pSprite->UV1.Y;
            f32     w     = pSprite->WH.X / 2.0f;
            f32     h     = pSprite->WH.Y / 2.0f;
            xbool   isrot = pSprite->IsRotated;
            radian  a     = pSprite->Rotation;
            f32     s, c;
            vector3 v0;
            vector3 v1;

            // Construct points v0 and v1
            draw_sincos( -a, s, c );
            v0.Set( c*w - s*h,
                    s*w + c*h,
                    0.0f );
            v1.Set( c*w + s*h,
                    s*w - c*h,
                    0.0f );

            // 2D or 3D?
            if( m_Is2D )
            {
                // If not rotated then the sprites position is actually upper left corner, so offset
                if( !isrot )
                {
                    // Contruct corner points of quad
                    pVertex2d->Position.X = pSprite->Position.GetX();
                    pVertex2d->Position.Y = pSprite->Position.GetY();
                    pVertex2d->Position.Z = 0.5f;
                    pVertex2d->RHW        = 1.0f;
                    pVertex2d->Color      = Color;
                    pVertex2d->UV         = vector2(U0,V0);
                    pVertex2d++;
                    pVertex2d->Position.X = pSprite->Position.GetX();
                    pVertex2d->Position.Y = pSprite->Position.GetY()+pSprite->WH.Y;
                    pVertex2d->Position.Z = 0.5f;
                    pVertex2d->RHW        = 1.0f;
                    pVertex2d->Color      = Color;
                    pVertex2d->UV         = vector2(U0,V1);
                    pVertex2d++;
                    pVertex2d->Position.X = pSprite->Position.GetX()+pSprite->WH.X;
                    pVertex2d->Position.Y = pSprite->Position.GetY()+pSprite->WH.Y;
                    pVertex2d->Position.Z = 0.5f;
                    pVertex2d->RHW        = 1.0f;
                    pVertex2d->Color      = Color;
                    pVertex2d->UV         = vector2(U1,V1);
                    pVertex2d++;
                    pVertex2d->Position.X = pSprite->Position.GetX()+pSprite->WH.X;
                    pVertex2d->Position.Y = pSprite->Position.GetY();
                    pVertex2d->Position.Z = 0.5f;
                    pVertex2d->RHW        = 1.0f;
                    pVertex2d->Color      = Color;
                    pVertex2d->UV         = vector2(U1,V0);
                    pVertex2d++;
                }
                else
                {
                    // Get center point
                    vector3 Center = pSprite->Position;

                    // Contruct corner points of quad
                    pVertex2d->Position = Center - v0;
                    pVertex2d->RHW      = 1.0f;
                    pVertex2d->Color    = Color;
                    pVertex2d->UV       = vector2(U0,V0);
                    pVertex2d++;
                    pVertex2d->Position =  Center - v1;
                    pVertex2d->RHW      = 1.0f;
                    pVertex2d->Color    = Color;
                    pVertex2d->UV       = vector2(U0,V1);
                    pVertex2d++;
                    pVertex2d->Position = Center + v0;
                    pVertex2d->RHW      = 1.0f;
                    pVertex2d->Color    = Color;
                    pVertex2d->UV       = vector2(U1,V1);
                    pVertex2d++;
                    pVertex2d->Position = Center + v1;
                    pVertex2d->RHW      = 1.0f;
                    pVertex2d->Color    = Color;
                    pVertex2d->UV       = vector2(U1,V0);
                    pVertex2d++;
                }

                // Advance to next sprite
                pSprite++;
            }
            else
            {
                // Transform center point into view space
                vector3 Center;
                W2V.Transform( &Center, &pSprite->Position, 1 );

                // Contruct corner points of quad
                pVertex3d->Position = Center + v0;
                pVertex3d->Color    = Color;
                pVertex3d->UV       = vector2(U0,V0);
                pVertex3d++;
                pVertex3d->Position = Center + v1;
                pVertex3d->Color    = Color;
                pVertex3d->UV       = vector2(U0,V1);
                pVertex3d++;
                pVertex3d->Position = Center - v0;
                pVertex3d->Color    = Color;
                pVertex3d->UV       = vector2(U1,V1);
                pVertex3d++;
                pVertex3d->Position = Center - v1;
                pVertex3d->Color    = Color;
                pVertex3d->UV       = vector2(U1,V0);
                pVertex3d++;

                // Advance to next sprite
                pSprite++;
            }
        }

        // Set number of vertices
        m_iVertex = 4 * m_iSprite;

        // Call regular Dispatch function
        draw_Dispatch();
    }

    // Clear Sprite Buffer
    m_iSprite = 0;
}

//==============================================================================

static 
void draw_SetupPrimitiveStates( draw_primitive Primitive )
{
    // Set internal state from primitive type
    switch( Primitive )
    {
    case DRAW_POINTS:
        m_pfnDispatch = draw_Dispatch;
        m_iTrigger = TRIGGER_POINTS;
        break;
    case DRAW_LINES:
        m_pfnDispatch = draw_Dispatch;
        m_iTrigger = TRIGGER_LINES;
        break;
    case DRAW_LINE_STRIPS:
        m_pfnDispatch = draw_Dispatch;
        m_iTrigger = TRIGGER_LINE_STRIPS;
        break;
    case DRAW_TRIANGLES:
        m_pfnDispatch = draw_Dispatch;
        m_iTrigger = TRIGGER_TRIANGLES;
        break;
    case DRAW_TRIANGLE_STRIPS:
        m_pfnDispatch = draw_Dispatch;
        m_iTrigger = TRIGGER_TRIANGLE_STRIPS;
        break;
    case DRAW_QUADS:
        m_pfnDispatch = draw_Dispatch;
        m_iTrigger = TRIGGER_QUADS;
        break;
    case DRAW_RECTS:
        ASSERT( m_Is2D );
        m_pfnDispatch = draw_DispatchRects;
        m_iTrigger = TRIGGER_RECTS;
        break;
    case DRAW_SPRITES:
        m_pfnDispatch = draw_DispatchSprites;
        break;
    }
}

//==============================================================================

static
void draw_ApplyBlendState( u32 Flags )
{
    if( Flags & DRAW_BLEND_ADD )
    {
        if( draw_IsPremultipliedTargetActive() )
            state_SetBlend( STATE_BLEND_PREMULT_ADD );
        else
            state_SetBlend( STATE_BLEND_ADD );
    }
    else if( Flags & DRAW_BLEND_SUB )
    {
        if( draw_IsPremultipliedTargetActive() )
            state_SetBlend( STATE_BLEND_PREMULT_SUB );
        else
            state_SetBlend( STATE_BLEND_SUB );
    }
    else if( Flags & DRAW_BLEND_INTENSITY )
    {
        state_SetBlend( STATE_BLEND_INTENSITY );
    }
    else if( Flags & DRAW_USE_ALPHA )
    {
        if( draw_IsPremultipliedTargetActive() )
            state_SetBlend( STATE_BLEND_PREMULT_ALPHA );
        else
            state_SetBlend( STATE_BLEND_ALPHA );
    }
    else
    {
        state_SetBlend( STATE_BLEND_NONE );
    }
}

//==============================================================================

static
void draw_ApplyDepthState( u32 Flags )
{
    if( Flags & DRAW_NO_ZBUFFER )
    {
        if( Flags & DRAW_NO_ZWRITE )
            state_SetDepth( STATE_DEPTH_DISABLED_NO_WRITE );
        else
            state_SetDepth( STATE_DEPTH_DISABLED );
    }
    else
    {
        if( Flags & DRAW_NO_ZWRITE )
            state_SetDepth( STATE_DEPTH_NO_WRITE );
        else
            state_SetDepth( STATE_DEPTH_NORMAL );
    }
}

//==============================================================================

static
void draw_ApplyRasterizerState( u32 Flags )
{
    if( Flags & DRAW_WIRE_FRAME )
    {
        if( Flags & DRAW_CULL_NONE )
            state_SetRasterizer( STATE_RASTER_WIRE_NO_CULL );
        else
            state_SetRasterizer( STATE_RASTER_WIRE );
    }
    else
    {
        if( Flags & DRAW_CULL_NONE )
            state_SetRasterizer( STATE_RASTER_SOLID_NO_CULL );
        else
            state_SetRasterizer( STATE_RASTER_SOLID );
    }
}

//==============================================================================

static
void draw_ApplySamplerState( u32 Flags )
{
    xbool clamp = (Flags & (DRAW_U_CLAMP | DRAW_V_CLAMP)) != 0;

    // TODO: GS: I think that in the future it is worth making this system more flexible, 
    // giving the ability to set different types of samplers in draw_begin, instead of POINT and ANISOTROPIC

    if( s_bCurrentBilinear )
    {
        if( clamp )
            state_SetSampler( STATE_SAMPLER_ANISOTROPIC_CLAMP, 0, STATE_SAMPLER_STAGE_PS ); //state_SetSampler( STATE_SAMPLER_LINEAR_CLAMP );
        else
            state_SetSampler( STATE_SAMPLER_ANISOTROPIC_WRAP, 0, STATE_SAMPLER_STAGE_PS ); //state_SetSampler( STATE_SAMPLER_LINEAR_WRAP );
    }
    else
    {
        if( clamp )
            state_SetSampler( STATE_SAMPLER_POINT_CLAMP, 0, STATE_SAMPLER_STAGE_PS );
        else
            state_SetSampler( STATE_SAMPLER_POINT_WRAP, 0, STATE_SAMPLER_STAGE_PS );
    }
}

//==============================================================================

static
void draw_SetupRenderStates( u32 Flags, xbool IsTextured )
{
    if( !g_pd3dContext )
        return;

    draw_ApplyBlendState( Flags );
    draw_ApplyDepthState( Flags );
    draw_ApplyRasterizerState( Flags );
    draw_ApplySamplerState( Flags );
    
    if( !IsTextured )
    {
        ID3D11ShaderResourceView* nullSRV = NULL;
        g_pd3dContext->PSSetShaderResources( 0, 1, &nullSRV );
    }
}

//==============================================================================

static 
void draw_InitializeDrawState( u32 Flags )
{
    // Clear list pointers
    m_pUVs    = NULL;
    m_pColors = NULL;
    m_pVerts  = NULL;

    // Set default color and UV
    m_Color = XCOLOR_WHITE;
    m_UV.Zero();
}

//==============================================================================

void draw_Begin( draw_primitive Primitive, u32 Flags )
{
    ASSERT( m_Initialized );
    ASSERT( !m_bBegin );
    ASSERT( eng_InBeginEnd() );

    // Save primitive and flags
    m_Primitive         = Primitive;
    m_Flags             = Flags;
    m_Is2D              = Flags & (DRAW_2D | DRAW_2D_KEEP_Z);
    m_IsUI              = Flags & DRAW_UI_RTARGET;
    m_IsPrimitiveTarget = Flags & DRAW_PRIMITIVE_RTARGET;
    m_IsTextured        = Flags & DRAW_TEXTURED;
    m_UseGBufferDepth   = Flags & DRAW_USE_GDEPTH;

    if( m_IsUI && m_IsPrimitiveTarget )
    {
        x_DebugMsg( "Draw: Both UI and primitive render target flags are set, primitive target will be used\n" );
        ASSERT( FALSE );        
        m_IsUI = FALSE;
    }

    // Initialize internal state
    draw_InitializeDrawState( Flags );
    draw_SetupPrimitiveStates( Primitive );

    // Set in begin state
    m_bBegin = TRUE;

    // Exit early if no device
    if( !g_pd3dDevice )
        return;

    // Setup GBufferDepth
    if( m_UseGBufferDepth && !m_IsUI && !m_IsPrimitiveTarget )
    {
        if( m_pGDepthProvider )
        {
            if( !m_pGDepthProvider() )
            {
                x_DebugMsg( "Draw: Scene depth provider failed, disabling scene depth for this draw\n" );
                ASSERT( FALSE );                
                m_UseGBufferDepth = FALSE;
            }
        }
        else
        {
            x_DebugMsg( "Draw: Scene depth requested but no provider registered\n" );
            ASSERT( FALSE );            
            m_UseGBufferDepth = FALSE;
        }
    }

    // Setup all render states using centralized state system
    draw_SetupRenderStates( Flags, m_IsTextured );
    
    if( m_IsUI )
    {
        if( !draw_rt_BeginUI() )
        {
            x_DebugMsg( "Draw: Failed to validate UI target, disabling UI rendering\n" );
            ASSERT( FALSE );
            m_IsUI = FALSE;
        }
    }
    else if( m_IsPrimitiveTarget )
    {
        if( !draw_rt_BeginPrimitive() )
        {
            x_DebugMsg( "Draw: Failed to validate primitive target, disabling primitive rendering\n" );
            ASSERT( FALSE );
            m_IsPrimitiveTarget = FALSE;
        }
    }
}

//==============================================================================

void draw_End( void )
{
    ASSERT( m_bBegin );

    // Clear in begin state
    m_bBegin = FALSE;

    if( !g_pd3dContext )
        return; 

    // Flush any drawing we have queued up
    m_pfnDispatch();
    
    if( m_IsUI )
    {
        draw_rt_EndUI();
    }
    else if( m_IsPrimitiveTarget )
    {
        draw_rt_EndPrimitive();
    }
    
    // Restore depth state
    state_SetDepth( STATE_DEPTH_NORMAL );
    
    // Restore standard matrices
    const view* pView = eng_GetView();
    if( pView )
    {
        cb_matrices cbData;
        cbData.World.Identity();
        cbData.View = pView->GetW2V();
        cbData.Projection = pView->GetV2C();
        
        shader_UpdateConstantBuffer( m_pConstantBuffer, &cbData, sizeof(cbData) );
        g_pd3dContext->VSSetConstantBuffers( 0, 1, &m_pConstantBuffer );
    }
}

//==============================================================================

void draw_ResetAfterException( void )
{
    m_bBegin = FALSE;

    // Exit if we lost the D3D device
    if( !g_pd3dContext )
        return;

    // Reset states to default
    state_SetBlend( STATE_BLEND_ALPHA );
    state_SetDepth( STATE_DEPTH_NORMAL );
    state_SetRasterizer( STATE_RASTER_SOLID );
    state_SetSampler( STATE_SAMPLER_LINEAR_WRAP, 0, STATE_SAMPLER_STAGE_PS );
}

//==============================================================================

void draw_SetL2W( const matrix4& L2W )
{
    m_L2W = L2W;
}

//==============================================================================

void draw_ClearL2W( void )
{
    m_L2W.Identity();
}

//==============================================================================

void draw_SetTexture( const xbitmap& Bitmap )
{
    if( m_bBegin )
    {
        m_pfnDispatch();
    }

    // Activate the texture if Textured mode is set
    vram_Activate( Bitmap );
}

//==============================================================================

void draw_SetTexture( void )
{
    if( m_bBegin )
    {
        m_pfnDispatch();
    }

    vram_Activate();
}

//==============================================================================

void draw_UV( const vector2& UV )
{
    ASSERT( m_bBegin );
    ASSERT( m_Primitive != DRAW_SPRITES );

    m_pUVs = NULL;
    m_UV = UV;
}

//==============================================================================

void draw_UV( f32 U, f32 V )
{
    ASSERT( m_bBegin );
    ASSERT( m_Primitive != DRAW_SPRITES );

    m_pUVs = NULL;
    m_UV.X = U;
    m_UV.Y = V;
}

//==============================================================================

void draw_Color( const xcolor& Color )
{
    ASSERT( m_bBegin );
    ASSERT( m_Primitive != DRAW_SPRITES );

    m_pColors = NULL;
    m_Color = Color;
}

//==============================================================================

void draw_Color( f32 R, f32 G, f32 B, f32 A )
{
    ASSERT( m_bBegin );
    ASSERT( m_Primitive != DRAW_SPRITES );

    m_pColors = NULL;
    m_Color.R = (u8)(R*255.0f);
    m_Color.G = (u8)(G*255.0f);
    m_Color.B = (u8)(B*255.0f);
    m_Color.A = (u8)(A*255.0f);
}

//==============================================================================

void draw_Vertex( const vector3& Vertex )
{
    ASSERT( m_bBegin );
    ASSERT( m_Primitive != DRAW_SPRITES );

    if( !g_pd3dContext )
        return;

    if( m_Is2D && !(m_Flags & DRAW_2D_KEEP_Z) )
    {
        // Setup vertex in buffer
        m_pActiveBuffer2d->UV       = m_UV;
        m_pActiveBuffer2d->Color    = draw_ColorToU32(m_Color);
        m_pActiveBuffer2d->Position = Vertex;
        m_pActiveBuffer2d->RHW      = 1.0f;

        // Advance buffer pointer
        m_pActiveBuffer2d++;
        m_iVertex++;

        // Check if it is time to dispatch this buffer
        if( m_iVertex == m_iTrigger )
            m_pfnDispatch();
    }
    else
    {
        // Setup vertex in buffer
        m_pActiveBuffer3d->UV       = m_UV;
        m_pActiveBuffer3d->Color    = draw_ColorToU32(m_Color);
        m_pActiveBuffer3d->Position = Vertex;

        // Advance buffer pointer
        m_pActiveBuffer3d++;
        m_iVertex++;

        // Check if it is time to dispatch this buffer
        if( m_iVertex == m_iTrigger )
            m_pfnDispatch();
    }
}

//==============================================================================

void draw_Vertex( f32 X, f32 Y, f32 Z )
{
    ASSERT( m_bBegin );
    ASSERT( m_Primitive != DRAW_SPRITES );

    if( !g_pd3dContext )
        return;

    if( m_Is2D && !(m_Flags & DRAW_2D_KEEP_Z) )
    {
        // Setup vertex in buffer
        m_pActiveBuffer2d->UV    = m_UV;
        m_pActiveBuffer2d->Color = draw_ColorToU32(m_Color);
        m_pActiveBuffer2d->Position.Set( X, Y, Z );
        m_pActiveBuffer2d->RHW   = 1.0f;

        // Advance buffer pointer
        m_pActiveBuffer2d++;
        m_iVertex++;

        // Check if it is time to dispatch this buffer
        if( m_iVertex == m_iTrigger )
            m_pfnDispatch();
    }
    else
    {
        // Setup vertex in buffer
        m_pActiveBuffer3d->UV    = m_UV;
        m_pActiveBuffer3d->Color = draw_ColorToU32(m_Color);
        m_pActiveBuffer3d->Position.Set( X, Y, Z );

        // Advance buffer pointer
        m_pActiveBuffer3d++;
        m_iVertex++;

        // Check if it is time to dispatch this buffer
        if( m_iVertex == m_iTrigger )
            m_pfnDispatch();
    }
}

//==============================================================================

void draw_UVs( const vector2* pUVs, s32 Count, s32 Stride )
{
    ASSERT( m_bBegin );
    ASSERT( m_Primitive != DRAW_SPRITES );

    m_pUVs = pUVs;
    m_nUVs = Count;
    m_sUVs = Stride;
}

//==============================================================================

void draw_Colors( const xcolor*  pColors, s32 Count, s32 Stride )
{
    ASSERT( m_bBegin );
    ASSERT( m_Primitive != DRAW_SPRITES );

    m_pColors = pColors;
    m_nColors = Count;
    m_sColors = Stride;
}

//==============================================================================

void draw_Verts( const vector3* pVerts,  s32 Count, s32 Stride )
{
    ASSERT( m_bBegin );
    ASSERT( m_Primitive != DRAW_SPRITES );

    m_pVerts = pVerts;
    m_nVerts = Count;
    m_sVerts = Stride;
}

//==============================================================================

void draw_Index( s32 Index )
{
    ASSERT( m_bBegin );
    ASSERT( m_Primitive != DRAW_SPRITES );

    if( !g_pd3dContext )
        return;

    if( Index == -1 )
    {
        m_pfnDispatch();
    }
    else
    {
        if( m_Is2D && !(m_Flags & DRAW_2D_KEEP_Z) )
        {
            // Setup vertex in buffer
            if( m_pUVs )
            {
                ASSERT( Index < m_nUVs );
                m_pActiveBuffer2d->UV = m_pUVs[Index];
            }
            else
            {
                m_pActiveBuffer2d->UV = m_UV;
            }
            if( m_pColors )
            {
                ASSERT( Index < m_nColors );
                m_pActiveBuffer2d->Color = draw_ColorToU32(m_pColors[Index]);
            }
            else
            {
                m_pActiveBuffer2d->Color = draw_ColorToU32(m_Color);
            }

            ASSERT( Index < m_nVerts );
            m_pActiveBuffer2d->Position = m_pVerts[Index];
            m_pActiveBuffer2d->RHW      = 1.0f;

            // Advance buffer pointer
            m_pActiveBuffer2d++;
            m_iVertex++;

            // Check if it is time to dispatch this buffer
            if( m_iVertex == m_iTrigger )
                m_pfnDispatch();
        }
        else
        {
            // Setup vertex in buffer
            if( m_pUVs )
            {
                ASSERT( Index < m_nUVs );
                m_pActiveBuffer3d->UV = m_pUVs[Index];
            }
            else
            {
                m_pActiveBuffer3d->UV = m_UV;
            }
            if( m_pColors )
            {
                ASSERT( Index < m_nColors );
                m_pActiveBuffer3d->Color = draw_ColorToU32(m_pColors[Index]);
            }
            else
            {
                m_pActiveBuffer3d->Color = draw_ColorToU32(m_Color);
            }

            ASSERT( Index < m_nVerts );
            m_pActiveBuffer3d->Position = m_pVerts[Index];

            // Advance buffer pointer
            m_pActiveBuffer3d++;
            m_iVertex++;

            // Check if it is time to dispatch this buffer
            if( m_iVertex == m_iTrigger )
                m_pfnDispatch();
        }
    }
}

//==============================================================================

void draw_Execute( const s16* pIndices, s32 NIndices )
{
    s32     i;

    ASSERT( m_bBegin );
    ASSERT( m_Primitive != DRAW_SPRITES );

    if( !g_pd3dContext )
        return;

    // Loop through indices supplied
    for( i=0 ; i<NIndices ; i++ )
    {
        // Read Index
        s32 Index = pIndices[i];

        // Kick on -1, otherwise add to buffer
        if( Index == -1 )
            m_pfnDispatch();
        else
            draw_Index( pIndices[i] );
    }
}

//==============================================================================

void    draw_Sprite     ( const vector3& Position,  // Hot spot (2D Left-Top), (3D Center)
                          const vector2& WH,        // (2D pixel W&H), (3D World W&H)
                          const xcolor&  Color )
{
    ASSERT( m_bBegin );
    ASSERT( m_Primitive == DRAW_SPRITES );

    if( !g_pd3dContext )
        return;

    drawsprite* p = &m_pSpriteBuffer[m_iSprite];

    p->IsRotated = FALSE;
    p->Position  = Position;
    p->WH        = WH;
    p->Color     = draw_ColorToU32(Color);
    p->Rotation  = 0.0f;
    p->UV0.Set( 0.0f, 0.0f );
    p->UV1.Set( 1.0f, 1.0f );

    m_iSprite++;
    if( m_iSprite == NUM_SPRITES )
        m_pfnDispatch();
}

//==============================================================================

void    draw_SpriteUV   ( const vector3& Position,  // Hot spot (2D Left-Top), (3D Center)
                          const vector2& WH,        // (2D pixel W&H), (3D World W&H)
                          const vector2& UV0,       // Upper Left   UV  [0.0 - 1.0]
                          const vector2& UV1,       // Bottom Right UV  [0.0 - 1.0]
                          const xcolor&  Color )
{
    ASSERT( m_bBegin );
    ASSERT( m_Primitive == DRAW_SPRITES );

    if( !g_pd3dContext )
        return;

    drawsprite* p = &m_pSpriteBuffer[m_iSprite];

    p->IsRotated = FALSE;
    p->Position  = Position;
    p->WH        = WH;
    p->UV0       = UV0;
    p->UV1       = UV1;
    p->Color     = draw_ColorToU32(Color);
    p->Rotation  = 0.0f;

    m_iSprite++;
    if( m_iSprite == NUM_SPRITES )
        m_pfnDispatch();
}

//==============================================================================

void    draw_SpriteUV   ( const vector3& Position,  // Hot spot (3D Center)
                          const vector2& WH,        // (3D World W&H)
                          const vector2& UV0,       // Upper Left   UV  [0.0 - 1.0]
                          const vector2& UV1,       // Bottom Right UV  [0.0 - 1.0]
                          const xcolor&  Color,     //
                                radian   Rotate )
{
    ASSERT( m_bBegin );
    ASSERT( m_Primitive == DRAW_SPRITES );

    if( !g_pd3dContext )
        return;

    drawsprite* p = &m_pSpriteBuffer[m_iSprite];

    p->IsRotated = TRUE;
    p->Position  = Position;
    p->WH        = WH;
    p->UV0       = UV0;
    p->UV1       = UV1;
    p->Color     = draw_ColorToU32(Color);
    p->Rotation  = Rotate;

    m_iSprite++;
    if( m_iSprite == NUM_SPRITES )
        m_pfnDispatch();
}

//==========================================================================

void    draw_OrientedQuad(const vector3& Pos0,
                          const vector3& Pos1,
                          const vector2& UV0,
                          const vector2& UV1,
                          const xcolor&  Color0,
                          const xcolor&  Color1,
                                f32      Radius )
{
    ASSERT( m_bBegin );
    ASSERT( m_Primitive == DRAW_TRIANGLES );

    if( !g_pd3dContext )
        return;

    vector3 Dir = Pos1 - Pos0;
    if( !Dir.SafeNormalize() )
        return;

    vector3 CrossDir = Dir.Cross( eng_GetView()->GetPosition() - Pos0 );
    if( !CrossDir.SafeNormalize() )
        return;

    CrossDir *= Radius;

    draw_Color( Color1 );
    draw_UV( UV1.X, UV1.Y );    draw_Vertex( Pos1 + CrossDir );
    draw_UV( UV1.X, UV0.Y );    draw_Vertex( Pos1 - CrossDir );
    draw_Color( Color0 );
    draw_UV( UV0.X, UV0.Y );    draw_Vertex( Pos0 - CrossDir );
    draw_UV( UV0.X, UV0.Y );    draw_Vertex( Pos0 - CrossDir );
    draw_UV( UV0.X, UV1.Y );    draw_Vertex( Pos0 + CrossDir );
    draw_Color( Color1 );
    draw_UV( UV1.X, UV1.Y );    draw_Vertex( Pos1 + CrossDir );
}

//==========================================================================

void    draw_OrientedQuad(const vector3& Pos0,
                          const vector3& Pos1,
                          const vector2& UV0,
                          const vector2& UV1,
                          const xcolor&  Color0,
                          const xcolor&  Color1,
                                f32      Radius0,
                                f32      Radius1)
{
    ASSERT( m_bBegin );
    ASSERT( m_Primitive == DRAW_TRIANGLES );

    if( !g_pd3dContext )
        return;

    vector3 Dir = Pos1 - Pos0;
    if( !Dir.SafeNormalize() )
        return;

    vector3 CrossDir = Dir.Cross( eng_GetView()->GetPosition() - Pos0 );
    if( !CrossDir.SafeNormalize() )
        return;

    vector3 Cross0 = CrossDir * Radius0;
    vector3 Cross1 = CrossDir * Radius1;
    
    draw_Color( Color1 );
    draw_UV( UV1.X, UV1.Y );    draw_Vertex( Pos1 + Cross1 );
    draw_UV( UV1.X, UV0.Y );    draw_Vertex( Pos1 - Cross1 );
    draw_Color( Color0 );
    draw_UV( UV0.X, UV0.Y );    draw_Vertex( Pos0 - Cross0 );
    draw_UV( UV0.X, UV0.Y );    draw_Vertex( Pos0 - Cross0 );
    draw_UV( UV0.X, UV1.Y );    draw_Vertex( Pos0 + Cross0 );
    draw_Color( Color1 );
    draw_UV( UV1.X, UV1.Y );    draw_Vertex( Pos1 + Cross1 );
}

//==========================================================================

void    draw_OrientedStrand (const vector3* pPosData,
                                   s32      NumPts,
                             const vector2& UV0,
                             const vector2& UV1,
                             const xcolor&  Color0,
                             const xcolor&  Color1,
                                   f32      Radius )
{
    s32 i;
    vector3 quad[6];        //  storage for a quad, plus an extra edge for averaging
    vector2 uv0, uv1;

    if( !g_pd3dContext )
        return;

    uv0 = UV0;
    uv1 = UV1;

    ASSERT( m_bBegin );
    ASSERT( m_Primitive == DRAW_TRIANGLES );

    for ( i = 0; i < NumPts - 1; i++ )
    {
        vector3 Dir = pPosData[i+1] - pPosData[i];
        if( !Dir.SafeNormalize() )
            Dir(0,1,0);

        vector3 CrossDir = Dir.Cross( eng_GetView()->GetPosition() - pPosData[i] );
        if( !CrossDir.SafeNormalize() )
            CrossDir(1,0,0);

        CrossDir *= Radius;

        if ( i == 0 )
        {
            // first set, no point averaging necessary
            quad[ 0 ] =     pPosData[ i ] + CrossDir;
            quad[ 1 ] =     pPosData[ i ] - CrossDir;
            quad[ 2 ] =     pPosData[ i + 1 ] + CrossDir;
            quad[ 3 ] =     pPosData[ i + 1 ] - CrossDir;
        }
        else
        {
            vector3 tq1, tq2;

            tq1 = pPosData[ i ] + CrossDir;
            tq2 = pPosData[ i ] - CrossDir;

            // second set...average verts
            quad[ 2 ] =     ( quad[2] + tq1 ) / 2.0f;
            quad[ 3 ] =     ( quad[3] + tq2 ) / 2.0f;
            quad[ 4 ] =     pPosData[ i + 1 ] + CrossDir;
            quad[ 5 ] =     pPosData[ i + 1 ] - CrossDir;            
        }

        // render q0, q1, q2, and q3 then shift all of them down
        if ( i > 0 )
        {
            draw_Color( Color1 );
            draw_UV( uv1.X, uv1.Y );    draw_Vertex( quad[2] );
            draw_UV( uv1.X, uv0.Y );    draw_Vertex( quad[3] );
            draw_Color( Color0 );
            draw_UV( uv0.X, uv0.Y );    draw_Vertex( quad[1] );
            draw_UV( uv0.X, uv0.Y );    draw_Vertex( quad[1] );
            draw_UV( uv0.X, uv1.Y );    draw_Vertex( quad[0] );
            draw_Color( Color1 );
            draw_UV( uv1.X, uv1.Y );    draw_Vertex( quad[2] );

            // cycle the UV's
            uv0.X = uv1.X;
            uv1.X += ( UV1.X - UV0.X );
            quad[0] = quad[2];
            quad[1] = quad[3];
            quad[2] = quad[4];
            quad[3] = quad[5];
        }
        
    }

    // last edge...
    draw_Color( Color1 );
    draw_UV( UV1.X, UV1.Y );    draw_Vertex( quad[2] );
    draw_UV( UV1.X, UV0.Y );    draw_Vertex( quad[3] );
    draw_Color( Color0 );
    draw_UV( UV0.X, UV0.Y );    draw_Vertex( quad[1] );
    draw_UV( UV0.X, UV0.Y );    draw_Vertex( quad[1] );
    draw_UV( UV0.X, UV1.Y );    draw_Vertex( quad[0] );
    draw_Color( Color1 );
    draw_UV( UV1.X, UV1.Y );    draw_Vertex( quad[2] );
}

//==============================================================================

// TODO: Push Z states to d3deng_state.

void draw_SetZBuffer( const irect& Rect, f32 Z )
{
    // Make sure Z is valid
    ASSERT(Z >= 0.0f) ;
    ASSERT(Z <= 1.0f) ;

    // Begin
    draw_Begin( DRAW_QUADS, DRAW_2D ) ;

    if( g_pd3dContext )
    {
        // Always write to z buffer
        D3D11_DEPTH_STENCIL_DESC depthDesc;
        ZeroMemory( &depthDesc, sizeof(depthDesc) );
        depthDesc.DepthEnable = TRUE;
        depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        depthDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
        depthDesc.StencilEnable = FALSE;

        ID3D11DepthStencilState* pDepthState = NULL;
        g_pd3dDevice->CreateDepthStencilState( &depthDesc, &pDepthState );
        g_pd3dContext->OMSetDepthStencilState( pDepthState, 0 );

        // Trick card into not changing the frame buffer
        D3D11_BLEND_DESC blendDesc;
        ZeroMemory( &blendDesc, sizeof(blendDesc) );
        blendDesc.RenderTarget[0].BlendEnable = TRUE;
        blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ZERO;
        blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
        blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        ID3D11BlendState* pBlendState = NULL;
        g_pd3dDevice->CreateBlendState( &blendDesc, &pBlendState );
        g_pd3dContext->OMSetBlendState( pBlendState, NULL, 0xffffffff );

        // Invalidate state cache
        state_FlushCache();

        // Draw rect
        draw_Color( XCOLOR_WHITE );
        draw_Vertex( (f32)Rect.l, (f32)Rect.t, Z );
        draw_Vertex( (f32)Rect.l, (f32)Rect.b, Z );
        draw_Vertex( (f32)Rect.r, (f32)Rect.b, Z );
        draw_Vertex( (f32)Rect.r, (f32)Rect.t, Z );
        draw_End() ;

        // Clean up
        pDepthState->Release();
        pBlendState->Release();
    }
    else
    {
        draw_End();
    }
}

//==============================================================================

void draw_ClearZBuffer( const irect& Rect )
{
    draw_SetZBuffer(Rect, 1.0f) ;
}

//==============================================================================

void draw_FillZBuffer( const irect& Rect )
{
    draw_SetZBuffer(Rect, 0.0f) ;
}

//==============================================================================

void draw_DisableBilinear( void )
{
    s_bCurrentBilinear = FALSE;
    
    // Set current state based on current flags
    if( m_bBegin )
        draw_SetupRenderStates( m_Flags, m_IsTextured );
    else
        state_SetSampler( STATE_SAMPLER_POINT_WRAP, 0, STATE_SAMPLER_STAGE_PS );
}

//==============================================================================

void draw_EnableBilinear( void )
{
    s_bCurrentBilinear = TRUE;
    
    // Set current state based on current flags
    if( m_bBegin )
        draw_SetupRenderStates( m_Flags, m_IsTextured );
    else
        state_SetSampler( STATE_SAMPLER_LINEAR_WRAP, 0, STATE_SAMPLER_STAGE_PS );
}