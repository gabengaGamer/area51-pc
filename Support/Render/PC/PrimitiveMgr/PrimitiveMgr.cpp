//=========================================================================
//
//  Primitive Manager for PC
//
//=========================================================================

//=========================================================================
//  PLATFORM CHECK
//=========================================================================

#include "x_types.hpp"

#if !defined(TARGET_PC)
#error "This is only for the PC target platform. Please check build exclusion rules"
#endif

//=========================================================================
// INCLUDES
//=========================================================================

#include "PrimitiveMgr.hpp"

#include "..\..\Render.hpp"
#include "..\..\..\Decals\DecalMgr.hpp"

#include "Entropy/e_Draw.hpp"
#include "Entropy/e_VRAM.hpp"
#include "Entropy/D3DEngine/d3deng_draw_shaders.hpp"
#include "Entropy/D3DEngine/d3deng_shader.hpp"
#include "Entropy/D3DEngine/d3deng_state.hpp"

//=========================================================================
// GLOBAL INSTANCE
//=========================================================================

primitive_mgr g_PrimitiveMgr;

//=========================================================================
// IMPLEMENTATION
//=========================================================================

namespace
{
    enum primitive_material_mode
    {
        PRIMITIVE_MATERIAL_DIFFUSE = 0,
        PRIMITIVE_MATERIAL_GLOW    = 1,
        PRIMITIVE_MATERIAL_ENV     = 2,
    };

    struct cb_primitive_matrices
    {
        matrix4 World;
        matrix4 View;
        matrix4 Projection;
        f32     NearZ;
        f32     FarZ;
        f32     pad0;
        f32     pad1;
    };

    struct cb_primitive_flags
    {
        s32 UseTexture;
        s32 UseAlpha;
        s32 MaterialMode;
        s32 pad0;
    };

    static const char* s_PrimitiveVertexShader =
    "cbuffer cbMatrices : register(b0)\n"
    "{\n"
    "    float4x4 World;\n"
    "    float4x4 View;\n"
    "    float4x4 Projection;\n"
    "    float NearZ;\n"
    "    float FarZ;\n"
    "    float pad0;\n"
    "    float pad1;\n"
    "};\n"
    "struct VS_INPUT\n"
    "{\n"
    "    float3 Pos   : POSITION;\n"
    "    float4 Color : COLOR;\n"
    "    float2 UV    : TEXCOORD;\n"
    "};\n"
    "struct PS_INPUT\n"
    "{\n"
    "    float4 Pos      : SV_POSITION;\n"
    "    float4 Color    : COLOR0;\n"
    "    float2 UV       : TEXCOORD0;\n"
    "    float3 WorldPos : TEXCOORD1;\n"
    "};\n"
    "PS_INPUT main( VS_INPUT input )\n"
    "{\n"
    "    PS_INPUT output;\n"
    "    float4 worldPos = mul( World, float4( input.Pos, 1.0 ) );\n"
    "    float4 viewPos  = mul( View, worldPos );\n"
    "    output.Pos      = mul( Projection, viewPos );\n"
    "    output.Color    = input.Color;\n"
    "    output.UV       = input.UV;\n"
    "    output.WorldPos = worldPos.xyz;\n"
    "    return output;\n"
    "}\n";

    static const char* s_PrimitivePixelShader =
    "Texture2D txDiffuse : register(t0);\n"
    "SamplerState samLinear : register(s0);\n"
    "cbuffer cbMatrices : register(b0)\n"
    "{\n"
    "    float4x4 World;\n"
    "    float4x4 View;\n"
    "    float4x4 Projection;\n"
    "    float NearZ;\n"
    "    float FarZ;\n"
    "    float pad0;\n"
    "    float pad1;\n"
    "};\n"
    "cbuffer cbFlags : register(b1)\n"
    "{\n"
    "    int UseTexture;\n"
    "    int UseAlpha;\n"
    "    int MaterialMode;\n"
    "    int FlagsPad0;\n"
    "};\n"
    "struct PS_INPUT\n"
    "{\n"
    "    float4 Pos      : SV_POSITION;\n"
    "    float4 Color    : COLOR0;\n"
    "    float2 UV       : TEXCOORD0;\n"
    "    float3 WorldPos : TEXCOORD1;\n"
    "    bool   IsFrontFace : SV_IsFrontFace;\n"
    "};\n"
    "struct PS_OUTPUT\n"
    "{\n"
    "    float4 FinalColor  : SV_Target0;\n"
    "    float4 Albedo      : SV_Target1;\n"
    "    float4 Normal      : SV_Target2;\n"
    "    float4 LinearDepth : SV_Target3;\n"
    "    float4 Glow        : SV_Target4;\n"
    "};\n"
    "PS_OUTPUT main( PS_INPUT input )\n"
    "{\n"
    "    PS_OUTPUT output;\n"
    "    float4 texColor   = (UseTexture > 0) ? txDiffuse.Sample( samLinear, input.UV ) : float4( 1.0, 1.0, 1.0, 1.0 );\n"
    "    float4 finalColor = texColor * input.Color;\n"
    "    float3 dx         = ddx( input.WorldPos );\n"
    "    float3 dy         = ddy( input.WorldPos );\n"
    "    float3 worldN     = cross( dx, dy );\n"
    "    float  lenSq      = dot( worldN, worldN );\n"
    "    if( lenSq > 1e-6 )\n"
    "        worldN *= rsqrt( lenSq );\n"
    "    else\n"
    "        worldN = float3( 0.0, 0.0, 1.0 );\n"
    "    if( !input.IsFrontFace )\n"
    "        worldN = -worldN;\n"
    "    float3 viewN       = normalize( mul( (float3x3)View, worldN ) );\n"
    "    float4 viewPos     = mul( View, float4( input.WorldPos, 1.0 ) );\n"
    "    float  invRange    = rcp( max( FarZ - NearZ, 1e-5 ) );\n"
    "    float  linearDepth = saturate( (viewPos.z - NearZ) * invRange );\n"
    "    output.FinalColor  = finalColor;\n"
    "    output.Albedo      = finalColor;\n"
    "    output.Normal      = float4( viewN * 0.5 + 0.5, finalColor.a );\n"
    "    output.LinearDepth = linearDepth.xxxx;\n"
    "    output.Glow        = (MaterialMode == 1) ? finalColor : float4( 0.0, 0.0, 0.0, 0.0 );\n"
    "    return output;\n"
    "}\n";
}

void primitive_mgr::Init( void )
{
    m_pGDepthProvider = NULL;
    m_pBitmap         = NULL;
    m_DrawFlags       = 0;
    m_MaterialMode    = PRIMITIVE_MATERIAL_DIFFUSE;
    m_pMatrixBuffer   = NULL;
    m_pFlagsBuffer    = NULL;
    m_pInputLayout    = NULL;
    m_pVertexShader   = NULL;
    m_pPixelShader    = NULL;

    m_PrimitiveBuffer.Init( sizeof(primitive_vertex) );

    if( !g_pd3dDevice )
        return;

    m_pMatrixBuffer = shader_CreateConstantBuffer( sizeof(cb_primitive_matrices), CB_TYPE_DYNAMIC );
    m_pFlagsBuffer  = shader_CreateConstantBuffer( sizeof(cb_primitive_flags), CB_TYPE_DYNAMIC );

    m_pVertexShader = shader_CompileVertexWithLayout( s_PrimitiveVertexShader,
                                                      &m_pInputLayout,
                                                      s_InputLayout3D,
                                                      ARRAYSIZE(s_InputLayout3D),
                                                      "main",
                                                      "vs_5_0",
                                                      "PrimitiveMgr" );

    m_pPixelShader = shader_CompilePixel( s_PrimitivePixelShader,
                                          "main",
                                          "ps_5_0",
                                          "PrimitiveMgr" );

    ASSERT( m_pMatrixBuffer );
    ASSERT( m_pFlagsBuffer );
    ASSERT( m_pInputLayout );
    ASSERT( m_pVertexShader );
    ASSERT( m_pPixelShader );
}

//=========================================================================

void primitive_mgr::Kill( void )
{
    if( m_pPixelShader )
    {
        m_pPixelShader->Release();
        m_pPixelShader = NULL;
    }

    if( m_pVertexShader )
    {
        m_pVertexShader->Release();
        m_pVertexShader = NULL;
    }

    if( m_pInputLayout )
    {
        m_pInputLayout->Release();
        m_pInputLayout = NULL;
    }

    if( m_pFlagsBuffer )
    {
        m_pFlagsBuffer->Release();
        m_pFlagsBuffer = NULL;
    }

    if( m_pMatrixBuffer )
    {
        m_pMatrixBuffer->Release();
        m_pMatrixBuffer = NULL;
    }

    m_PrimitiveBuffer.Kill();

    m_pGDepthProvider = NULL;
    m_pBitmap         = NULL;
    m_DrawFlags       = 0;
    m_MaterialMode    = PRIMITIVE_MATERIAL_DIFFUSE;
}

//=========================================================================

void primitive_mgr::BeginRender( void )
{
    m_PrimitiveBuffer.BeginRender();
}

//=========================================================================

void primitive_mgr::EndRender( void )
{
}

//=========================================================================

void primitive_mgr::SetGDepthProvider( primitive_gdepth_provider pfnProvider )
{
    m_pGDepthProvider = pfnProvider;
}

//=========================================================================

void primitive_mgr::SetDiffuseMaterial( const xbitmap& Bitmap, s32 BlendMode, xbool ZTestEnabled )
{
    vram_Activate( Bitmap );

    m_DrawFlags = DRAW_TEXTURED | DRAW_NO_ZWRITE | DRAW_UV_CLAMP | DRAW_CULL_NONE;
    if( !ZTestEnabled )
        m_DrawFlags |= DRAW_NO_ZBUFFER;

    switch( BlendMode )
    {
        case render::BLEND_MODE_ADDITIVE:
            m_DrawFlags |= DRAW_BLEND_ADD;
            break;
        case render::BLEND_MODE_SUBTRACTIVE:
            m_DrawFlags |= DRAW_BLEND_SUB;
            break;
        case render::BLEND_MODE_INTENSITY:
            m_DrawFlags |= DRAW_BLEND_INTENSITY;
            break;
        case render::BLEND_MODE_NORMAL:
            m_DrawFlags |= DRAW_USE_ALPHA;
        default:
            break;
    }

    m_pBitmap = &Bitmap;
    m_MaterialMode = PRIMITIVE_MATERIAL_DIFFUSE;
}

//=========================================================================

void primitive_mgr::SetGlowMaterial( const xbitmap& Bitmap, s32 BlendMode, xbool ZTestEnabled )
{
    SetDiffuseMaterial( Bitmap, BlendMode, ZTestEnabled );
    m_MaterialMode = PRIMITIVE_MATERIAL_GLOW;
}

//=========================================================================

void primitive_mgr::SetEnvMapMaterial( const xbitmap& Bitmap, s32 BlendMode, xbool ZTestEnabled )
{
    SetDiffuseMaterial( Bitmap, BlendMode, ZTestEnabled );
    m_MaterialMode = PRIMITIVE_MATERIAL_ENV;
}

//=========================================================================

void primitive_mgr::SetDistortionMaterial( s32 BlendMode, xbool ZTestEnabled )
{
    ASSERTS( FALSE, "Not implemented yet!" );
    (void)BlendMode;
    (void)ZTestEnabled;
}

//=========================================================================

u32 primitive_mgr::DrawColorToU32( const xcolor& Color )
{
    return ((u32)Color.A << 24) | ((u32)Color.R << 16) | ((u32)Color.G << 8) | ((u32)Color.B);
}

//=========================================================================

u32 primitive_mgr::SourceColorToU32( u32 Color )
{
    return (Color & 0xFF000000) |
           ((Color & 0x000000FF) << 16) |
           (Color & 0x0000FF00) |
           ((Color & 0x00FF0000) >> 16);
}

//=========================================================================

void primitive_mgr::ApplyBlendState( u32 Flags )
{
    if( Flags & DRAW_BLEND_ADD )
        state_SetBlend( STATE_BLEND_ADD );
    else if( Flags & DRAW_BLEND_SUB )
        state_SetBlend( STATE_BLEND_SUB );
    else if( Flags & DRAW_BLEND_INTENSITY )
        state_SetBlend( STATE_BLEND_INTENSITY );
    else if( Flags & DRAW_USE_ALPHA )
        state_SetBlend( STATE_BLEND_ALPHA );
    else
        state_SetBlend( STATE_BLEND_NONE );
}

//=========================================================================

void primitive_mgr::ApplyDepthState( u32 Flags )
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

//=========================================================================

void primitive_mgr::ApplyRasterizerState( u32 Flags )
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

//=========================================================================

void primitive_mgr::ApplySamplerState( u32 Flags )
{
    const xbool bClamp = (Flags & (DRAW_U_CLAMP | DRAW_V_CLAMP)) != 0;

    if( bClamp )
        state_SetSampler( STATE_SAMPLER_ANISOTROPIC_CLAMP, 0, STATE_SAMPLER_STAGE_PS );
    else
        state_SetSampler( STATE_SAMPLER_ANISOTROPIC_WRAP, 0, STATE_SAMPLER_STAGE_PS );
}

//=========================================================================

xbool primitive_mgr::BeginPrimitiveDraw( const matrix4& L2W )
{
    if( !g_pd3dDevice || !g_pd3dContext )
        return FALSE;

    const view* pView = eng_GetView();
    if( !pView )
        return FALSE;

    eng_SetViewport( *pView );

    cb_primitive_matrices Matrices;
    f32 NearZ = 0.0f;
    f32 FarZ  = 0.0f;
    pView->GetZLimits( NearZ, FarZ );
    Matrices.World      = L2W;
    Matrices.View       = pView->GetW2V();
    Matrices.Projection = pView->GetV2C();
    Matrices.NearZ      = NearZ;
    Matrices.FarZ       = FarZ;
    Matrices.pad0       = 0.0f;
    Matrices.pad1       = 0.0f;
    shader_UpdateConstantBuffer( m_pMatrixBuffer, &Matrices, sizeof(Matrices) );
    g_pd3dContext->VSSetConstantBuffers( 0, 1, &m_pMatrixBuffer );
    g_pd3dContext->PSSetConstantBuffers( 0, 1, &m_pMatrixBuffer );

    cb_primitive_flags RenderFlags;
    RenderFlags.UseTexture             = m_pBitmap ? 1 : 0;
    RenderFlags.UseAlpha               = (m_DrawFlags & DRAW_USE_ALPHA) ? 1 : 0;
    RenderFlags.MaterialMode           = m_MaterialMode;
    RenderFlags.pad0                   = 0;
    shader_UpdateConstantBuffer( m_pFlagsBuffer, &RenderFlags, sizeof(RenderFlags) );
    g_pd3dContext->PSSetConstantBuffers( 1, 1, &m_pFlagsBuffer );

    shader_SetInputLayout( m_pInputLayout );
    shader_SetVertexShader( m_pVertexShader );
    shader_SetGeometryShader( NULL );
    shader_SetPixelShader( m_pPixelShader );

    ApplyBlendState( m_DrawFlags );
    ApplyDepthState( m_DrawFlags );
    ApplyRasterizerState( m_DrawFlags );
    ApplySamplerState( m_DrawFlags );

    if( m_pBitmap )
        vram_Activate( *m_pBitmap );
    else
        vram_Activate();

    return TRUE;
}

//=========================================================================

void primitive_mgr::RenderRawStrips( s32 nVerts, const matrix4& L2W, const vector4* pPos, const s16* pUV, const u32* pColor )
{
    static const f32 ItoFScale = 1.0f / 4096.0f;

    ASSERTS( m_pBitmap, "You must set a material first!" );
    if( nVerts < 3 )
        return;

    s32 nTris = 0;
    for( s32 iVert = 2; iVert < nVerts; ++iVert )
    {
        const f32 W = pPos[iVert].GetW();
        if( (*((const u32*)&W)) & decal_mgr::decal_vert::FLAG_SKIP_TRIANGLE )
            continue;

        nTris++;
    }

    if( nTris == 0 )
        return;

    primitive_vertex* pVertex = (primitive_vertex*)smem_BufferAlloc( sizeof(primitive_vertex) * nTris * 3 );
    u16*              pIndex  = (u16*)smem_BufferAlloc( sizeof(u16) * nTris * 3 );

    const xbool bIntensity = (m_DrawFlags & DRAW_BLEND_INTENSITY) != 0;
    const u32   WhiteColor = DrawColorToU32( XCOLOR_WHITE );
    s32         iOut       = 0;

    for( s32 iVert = 2; iVert < nVerts; ++iVert )
    {
        const f32 W = pPos[iVert].GetW();
        if( (*((const u32*)&W)) & decal_mgr::decal_vert::FLAG_SKIP_TRIANGLE )
            continue;

        const vector3 Pos[3] =
        {
            vector3( pPos[iVert-2].GetX(), pPos[iVert-2].GetY(), pPos[iVert-2].GetZ() ),
            vector3( pPos[iVert-1].GetX(), pPos[iVert-1].GetY(), pPos[iVert-1].GetZ() ),
            vector3( pPos[iVert-0].GetX(), pPos[iVert-0].GetY(), pPos[iVert-0].GetZ() )
        };

        const vector2 UV[3] =
        {
            vector2( pUV[(iVert-2)*2+0] * ItoFScale, pUV[(iVert-2)*2+1] * ItoFScale ),
            vector2( pUV[(iVert-1)*2+0] * ItoFScale, pUV[(iVert-1)*2+1] * ItoFScale ),
            vector2( pUV[(iVert-0)*2+0] * ItoFScale, pUV[(iVert-0)*2+1] * ItoFScale )
        };

        const u32 PackedColor[3] =
        {
            bIntensity ? WhiteColor : SourceColorToU32( pColor[iVert-2] ),
            bIntensity ? WhiteColor : SourceColorToU32( pColor[iVert-1] ),
            bIntensity ? WhiteColor : SourceColorToU32( pColor[iVert-0] )
        };

        for( s32 i = 0; i < 3; ++i )
        {
            pVertex[iOut].Position = Pos[i];
            pVertex[iOut].Color    = PackedColor[i];
            pVertex[iOut].UV       = UV[i];
            pIndex [iOut]          = (u16)iOut;
            iOut++;
        }
    }

    ASSERT( iOut == (nTris * 3) );

    if( !BeginPrimitiveDraw( L2W ) )
        return;

    runtime_vertex_mgr::primitive Primitive;
    Primitive.pVertex   = pVertex;
    Primitive.pIndex    = pIndex;
    Primitive.nVertices = iOut;
    Primitive.nIndices  = iOut;
    Primitive.Topology  = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    m_PrimitiveBuffer.DrawPrimitive( Primitive );
}

//=========================================================================

void primitive_mgr::Render3dSprites( s32 nSprites, f32 UniScale, const matrix4* pL2W, const vector4* pPositions, const vector2* pRotScales, const u32* pColors )
{
    ASSERTS( m_pBitmap, "You must set a material first!" );
    if( nSprites == 0 )
        return;

    const matrix4& V2W = eng_GetView()->GetV2W();
    const matrix4& W2V = eng_GetView()->GetW2V();
    matrix4 S2V;
    if( pL2W )
        S2V = W2V * (*pL2W);
    else
        S2V = W2V;

    s32 nActiveSprites = 0;
    for( s32 i = 0; i < nSprites; i++ )
    {
        if( (pPositions[i].GetIW() & 0x8000) != 0x8000 )
            nActiveSprites++;
    }

    if( nActiveSprites == 0 )
        return;

    primitive_vertex* pVertex = (primitive_vertex*)smem_BufferAlloc( sizeof(primitive_vertex) * nActiveSprites * 4 );
    u16*              pIndex  = (u16*)smem_BufferAlloc( sizeof(u16) * nActiveSprites * 6 );

    s32 iVertex = 0;
    s32 iIndex  = 0;
    for( s32 i = 0; i < nSprites; i++ )
    {
        if( (pPositions[i].GetIW() & 0x8000) == 0x8000 )
            continue;

        vector3 Center( pPositions[i].GetX(), pPositions[i].GetY(), pPositions[i].GetZ() );
        Center = S2V * Center;

        vector3 Corners[4];
        f32 Sine, Cosine;
        x_sincos( -pRotScales[i].X, Sine, Cosine );

        vector3 v0( Cosine - Sine, Sine + Cosine, 0.0f );
        vector3 v1( Cosine + Sine, Sine - Cosine, 0.0f );
        Corners[0] = v0;
        Corners[1] = v1;
        Corners[2] = -v0;
        Corners[3] = -v1;

        for( s32 j = 0; j < 4; j++ )
        {
            Corners[j].Scale( pRotScales[i].Y * UniScale );
            Corners[j] += Center;
        }

        const u32 PackedColor = SourceColorToU32( pColors[i] );
        const s32 iBaseVertex = iVertex;

        pVertex[iVertex+0].Position = Corners[0];
        pVertex[iVertex+0].Color    = PackedColor;
        pVertex[iVertex+0].UV.Set( 0.0f, 0.0f );
        pVertex[iVertex+1].Position = Corners[1];
        pVertex[iVertex+1].Color    = PackedColor;
        pVertex[iVertex+1].UV.Set( 0.0f, 1.0f );
        pVertex[iVertex+2].Position = Corners[2];
        pVertex[iVertex+2].Color    = PackedColor;
        pVertex[iVertex+2].UV.Set( 1.0f, 1.0f );
        pVertex[iVertex+3].Position = Corners[3];
        pVertex[iVertex+3].Color    = PackedColor;
        pVertex[iVertex+3].UV.Set( 1.0f, 0.0f );

        pIndex[iIndex+0] = (u16)(iBaseVertex + 0);
        pIndex[iIndex+1] = (u16)(iBaseVertex + 3);
        pIndex[iIndex+2] = (u16)(iBaseVertex + 1);
        pIndex[iIndex+3] = (u16)(iBaseVertex + 3);
        pIndex[iIndex+4] = (u16)(iBaseVertex + 1);
        pIndex[iIndex+5] = (u16)(iBaseVertex + 2);

        iVertex += 4;
        iIndex  += 6;
    }

    ASSERT( iVertex == (nActiveSprites * 4) );
    ASSERT( iIndex  == (nActiveSprites * 6) );

    if( !BeginPrimitiveDraw( V2W ) )
        return;

    runtime_vertex_mgr::primitive Primitive;
    Primitive.pVertex   = pVertex;
    Primitive.pIndex    = pIndex;
    Primitive.nVertices = iVertex;
    Primitive.nIndices  = iIndex;
    Primitive.Topology  = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    m_PrimitiveBuffer.DrawPrimitive( Primitive );
}

//=========================================================================

void primitive_mgr::RenderHeatHazeSprites( s32 nSprites, f32 UniScale, const matrix4* pL2W, const vector4* pPositions, const vector2* pRotScales, const u32* pColors )
{
    (void)nSprites;
    (void)UniScale;
    (void)pL2W;
    (void)pPositions;
    (void)pRotScales;
    (void)pColors;
}

//=========================================================================

void primitive_mgr::RenderVelocitySprites( s32 nSprites, f32 UniScale, const matrix4* pL2W, const matrix4* pVelMatrix, const vector4* pPositions, const vector4* pVelocities, const u32* pColors )
{
    ASSERTS( m_pBitmap, "You must set a material first!" );
    if( nSprites == 0 )
        return;

    matrix4 L2W;
    if( pL2W )
        L2W = *pL2W;
    else
        L2W.Identity();

    matrix4 L2WNoTranslate = L2W;
    L2WNoTranslate.ClearTranslation();
    matrix4 VL2W = L2WNoTranslate * (*pVelMatrix);

    vector3 ViewDir = eng_GetView()->GetViewZ();

    s32 nActiveSprites = 0;
    for( s32 i = 0; i < nSprites; i++ )
    {
        if( (pPositions[i].GetIW() & 0x8000) != 0x8000 )
            nActiveSprites++;
    }

    if( nActiveSprites == 0 )
        return;

    primitive_vertex* pVertex = (primitive_vertex*)smem_BufferAlloc( sizeof(primitive_vertex) * nActiveSprites * 4 );
    u16*              pIndex  = (u16*)smem_BufferAlloc( sizeof(u16) * nActiveSprites * 6 );

    s32 iVertex = 0;
    s32 iIndex  = 0;
    for( s32 i = 0; i < nSprites; i++ )
    {
        if( (pPositions[i].GetIW() & 0x8000) == 0x8000 )
            continue;

        vector3 P = L2W * vector3( pPositions[i].GetX(), pPositions[i].GetY(), pPositions[i].GetZ() );

        vector3 Right( pVelocities[i].GetX(), pVelocities[i].GetY(), pVelocities[i].GetZ() );
        Right = VL2W * Right;
        Right.Normalize();
        vector3 Up   = ViewDir.Cross( Right );
        Right *= pVelocities[i].GetW() * UniScale;
        Up    *= pVelocities[i].GetW() * UniScale;
        vector3 Fore = P + Right;
        vector3 Aft  = P - Right;
        vector3 V0   = Fore - Up;
        vector3 V1   = Aft  - Up;
        vector3 V2   = Aft  + Up;
        vector3 V3   = Fore + Up;

        const u32 PackedColor = SourceColorToU32( pColors[i] );
        const s32 iBaseVertex = iVertex;

        pVertex[iVertex+0].Position = V0;
        pVertex[iVertex+0].Color    = PackedColor;
        pVertex[iVertex+0].UV.Set( 1.0f, 0.0f );
        pVertex[iVertex+1].Position = V1;
        pVertex[iVertex+1].Color    = PackedColor;
        pVertex[iVertex+1].UV.Set( 0.0f, 0.0f );
        pVertex[iVertex+2].Position = V2;
        pVertex[iVertex+2].Color    = PackedColor;
        pVertex[iVertex+2].UV.Set( 0.0f, 1.0f );
        pVertex[iVertex+3].Position = V3;
        pVertex[iVertex+3].Color    = PackedColor;
        pVertex[iVertex+3].UV.Set( 1.0f, 1.0f );

        pIndex[iIndex+0] = (u16)(iBaseVertex + 0);
        pIndex[iIndex+1] = (u16)(iBaseVertex + 1);
        pIndex[iIndex+2] = (u16)(iBaseVertex + 3);
        pIndex[iIndex+3] = (u16)(iBaseVertex + 1);
        pIndex[iIndex+4] = (u16)(iBaseVertex + 3);
        pIndex[iIndex+5] = (u16)(iBaseVertex + 2);

        iVertex += 4;
        iIndex  += 6;
    }

    ASSERT( iVertex == (nActiveSprites * 4) );
    ASSERT( iIndex  == (nActiveSprites * 6) );

    matrix4 Identity;
    Identity.Identity();

    if( !BeginPrimitiveDraw( Identity ) )
        return;

    runtime_vertex_mgr::primitive Primitive;
    Primitive.pVertex   = pVertex;
    Primitive.pIndex    = pIndex;
    Primitive.nVertices = iVertex;
    Primitive.nIndices  = iIndex;
    Primitive.Topology  = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    m_PrimitiveBuffer.DrawPrimitive( Primitive );
}

//=========================================================================
