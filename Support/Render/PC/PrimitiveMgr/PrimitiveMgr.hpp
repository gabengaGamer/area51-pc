//=========================================================================
//
//  Primitive Manager for PC
//
//=========================================================================

#ifndef PRIMITIVE_MANAGER_HPP
#define PRIMITIVE_MANAGER_HPP

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

#include "Entropy.hpp"
#include "..\RuntimeVertexMgr.hpp"

//=========================================================================
// TYPES
//=========================================================================

typedef xbool (*primitive_gdepth_provider)( void );

//=========================================================================
// CLASS
//=========================================================================

class primitive_mgr
{

//=========================================================================

public:

    void        Init                    ( void );
    void        Kill                    ( void );
    void        BeginRender             ( void );
    void        EndRender               ( void );
    void        SetGDepthProvider       ( primitive_gdepth_provider pfnProvider );

    void        SetDiffuseMaterial      ( const xbitmap& Bitmap,
                                          s32            BlendMode,
                                          xbool          ZTestEnabled );
    void        SetGlowMaterial         ( const xbitmap& Bitmap,
                                          s32            BlendMode,
                                          xbool          ZTestEnabled );
    void        SetEnvMapMaterial       ( const xbitmap& Bitmap,
                                          s32            BlendMode,
                                          xbool          ZTestEnabled );
    void        SetDistortionMaterial   ( s32            BlendMode,
                                          xbool          ZTestEnabled );

    void        RenderRawStrips         ( s32               nVerts,
                                          const matrix4&    L2W,
                                          const vector4*    pPos,
                                          const s16*        pUV,
                                          const u32*        pColor );
    void        Render3dSprites         ( s32               nSprites,
                                          f32               UniScale,
                                          const matrix4*    pL2W,
                                          const vector4*    pPositions,
                                          const vector2*    pRotScales,
                                          const u32*        pColors );
    void        RenderVelocitySprites   ( s32               nSprites,
                                          f32               UniScale,
                                          const matrix4*    pL2W,
                                          const matrix4*    pVelMatrix,
                                          const vector4*    pPositions,
                                          const vector4*    pVelocities,
                                          const u32*        pColors );
    void        RenderHeatHazeSprites   ( s32               nSprites,
                                          f32               UniScale,
                                          const matrix4*    pL2W,
                                          const vector4*    pPositions,
                                          const vector2*    pRotScales,
                                          const u32*        pColors );

//=========================================================================

protected:

    struct primitive_vertex
    {
        vector3p    Position;
        u32         Color;
        vector2     UV;
    };

    u32         DrawColorToU32          ( const xcolor& Color );
    u32         SourceColorToU32        ( u32 Color );
    void        ApplyBlendState         ( u32 Flags );
    void        ApplyDepthState         ( u32 Flags );
    void        ApplyRasterizerState    ( u32 Flags );
    void        ApplySamplerState       ( u32 Flags );
    xbool       BeginPrimitiveDraw      ( const matrix4& L2W );

//=========================================================================

protected:

    runtime_vertex_mgr          m_PrimitiveBuffer;
    primitive_gdepth_provider   m_pGDepthProvider;
    const xbitmap*              m_pBitmap;
    u32                         m_DrawFlags;
    s32                         m_MaterialMode;
    ID3D11Buffer*               m_pMatrixBuffer;
    ID3D11Buffer*               m_pFlagsBuffer;
    ID3D11InputLayout*          m_pInputLayout;
    ID3D11VertexShader*         m_pVertexShader;
    ID3D11PixelShader*          m_pPixelShader;
};

//=========================================================================
// GLOBAL INSTANCE
//=========================================================================

extern primitive_mgr g_PrimitiveMgr;

//=========================================================================
// END
//=========================================================================

#endif
