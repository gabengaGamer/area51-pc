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
//  INCLUDES
//==============================================================================

#include "e_Engine.hpp"
#include "FrameRenderContext.hpp"

//==============================================================================
//  ENUMS / CONSTANTS
//==============================================================================

enum class GBufferTarget
{
    FinalColor = 0,
    NormalDepth,
    Glow,
    Depth,
    Count
};

//------------------------------------------------------------------------------

static constexpr s32 g_gBufferMrtCount = 2;

//==============================================================================
//  G-BUFFER MANAGER CLASS
//==============================================================================

class GBufferMgr
{
public:
    GBufferMgr  ( void );
    ~GBufferMgr ( void );
    
    void Init ( void );
    void Kill ( void );
    
    xbool InitGBuffer    ( u32 width, u32 height );
    void  DestroyGBuffer ( void );
    xbool ResizeGBuffer  ( u32 width, u32 height );
    
    xbool SetGBufferTargets   ( void );
    void  EndPass             ( void );
    void  SetFinalColorTarget ( void );
    void  PresentFinalColor   ( void );
    void  ClearGBuffer        ( void );
    void  BeginFrame          ( void );
    void  SetTargetOverride   ( rtarget const* pColor, rtarget const* pDepth );
    xbool GetFrameTargets     ( frame_render_targets& targets ) const;
    
    rtarget const* GetGBufferTarget          ( GBufferTarget target ) const;
    xbool          IsGBufferEnabled          ( void ) const { return m_isGBufferValid; }
    xbool          WasSceneRenderedThisFrame ( void ) const { return m_isSceneColorRenderedThisFrame; }
    void           GetGBufferSize            ( u32& width, u32& height ) const;
    
private:
    xbool          CreateTarget         ( rtarget& target, rtarget_format format, f32 const* pClearColor, char const* pErrorMsg );
    rtarget const* GetActiveSceneColor  ( void ) const;
    rtarget const* GetActiveDepthTarget ( void ) const;
    
    xbool m_isInitialized;
    xbool m_isGBufferValid;
    xbool m_areGBufferTargetsActive;
    xbool m_isSceneColorRenderedThisFrame;
    xbool m_clearGBufferOnBind;
    u32   m_gBufferWidth;
    u32   m_gBufferHeight;
    
    rtarget m_sceneColorTarget;
    rtarget m_gBufferTarget[g_gBufferMrtCount];
    rtarget m_gBufferDepth;

    rtarget const* m_pOverrideColor;
    rtarget const* m_pOverrideDepth;
};

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

extern GBufferMgr g_GBufferMgr;

//==============================================================================
#endif // GBUFFER_MANAGER_HPP
//==============================================================================
