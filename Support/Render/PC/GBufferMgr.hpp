//==============================================================================
//
//  GBufferMgr.hpp
//
//  Mini G-Buffer Manager for Forward Renderer
//
//==============================================================================

#ifndef GBUFFER_MANAGER_HPP
#define GBUFFER_MANAGER_HPP

//==============================================================================
//  PLATFORM CHECK
//==============================================================================

#include "x_types.hpp"

#if !defined(TARGET_PC)
#error "This is only for the PC target platform. Please check build exclusion rules"
#endif

//==============================================================================
//  INCLUDES
//==============================================================================

#include "Entropy/D3DEngine/d3deng_rtarget.hpp"

//==============================================================================
//  ENUMS / CONSTANTS
//==============================================================================

enum gbuffer_target
{
    GBUFFER_FINAL_COLOR  = 0,
    GBUFFER_ALBEDO,
    GBUFFER_NORMAL,
    GBUFFER_LINEAR_DEPTH,
    GBUFFER_GLOW,
    GBUFFER_DEPTH,
    GBUFFER_TARGET_COUNT
};

//------------------------------------------------------------------------------

#define GBUFFER_MRT_COUNT  4

//==============================================================================
//  G-BUFFER MANAGER CLASS
//==============================================================================

class gbuffer_mgr
{
public:
    gbuffer_mgr ( void );
    ~gbuffer_mgr( void );

    void           Init                ( void );
    void           Kill                ( void );

    xbool          InitGBuffer         ( u32 Width, u32 Height );
    void           DestroyGBuffer      ( void );
    xbool          ResizeGBuffer       ( u32 Width, u32 Height );

    xbool          SetGBufferTargets   ( void );
    void           SetFinalColorTarget ( void );
    void           PresentFinalColor   ( void );
    void           ClearGBuffer        ( void );
    void           BeginFrame          ( void );
    void           SetTargetOverride   ( const rtarget* pColor, const rtarget* pDepth );

    const rtarget* GetGBufferTarget    ( gbuffer_target Target ) const;
    xbool          IsGBufferEnabled    ( void ) const { return m_bGBufferValid; }
    xbool          WasSceneRenderedThisFrame( void ) const { return m_bSceneColorRenderedThisFrame; }
    void           GetGBufferSize      ( u32& Width, u32& Height ) const;

private:
    xbool          CreateTarget        ( rtarget& Target, rtarget_format Format, const char* pErrorMsg );
    const rtarget* GetActiveSceneColor ( void ) const;
    const rtarget* GetActiveDepthTarget( void ) const;

    xbool          m_bInitialized;
    xbool          m_bGBufferValid;
    xbool          m_bGBufferTargetsActive;
    xbool          m_bSceneColorRenderedThisFrame;
    u32            m_GBufferWidth;
    u32            m_GBufferHeight;

    rtarget        m_SceneColorTarget;
    rtarget        m_GBufferTarget[ GBUFFER_MRT_COUNT ];
    rtarget        m_GBufferDepth;

    const rtarget* m_pOverrideColor;
    const rtarget* m_pOverrideDepth;
};

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

extern gbuffer_mgr g_GBufferMgr;

//==============================================================================
#endif // GBUFFER_MANAGER_HPP
//==============================================================================
