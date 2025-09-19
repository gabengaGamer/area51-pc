//==============================================================================
//  
//  GBufferMgr.hpp
//  
//  Mini G-Buffer Manager for Forward+ Renderer
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
//  ENUMS
//==============================================================================

enum gbuffer_target
{
    GBUFFER_FINAL_COLOR  = 0,  // Main backbuffer output
    GBUFFER_ALBEDO,            // RGB: Base color, A: Metallic
    GBUFFER_NORMAL,            // RGB: World Normal, A: Roughness  
    GBUFFER_DEPTH_INFO,        // R: Linear Depth, G: Material ID
    GBUFFER_DEPTH,             // Hardware depth buffer
    GBUFFER_TARGET_COUNT
};

//==============================================================================
//  G-BUFFER MANAGER CLASS
//==============================================================================

class gbuffer_mgr
{
public:
    void           Init                ( void );
    void           Kill                ( void );
                   
    xbool          InitGBuffer         ( u32 Width, u32 Height );
    void           DestroyGBuffer         ( void );
    xbool          ResizeGBuffer       ( u32 Width, u32 Height );
                   
    xbool          SetGBufferTargets   ( void );
    void           SetBackBufferTarget ( void );
    void           ClearGBuffer        ( void );
    
    const rtarget* GetGBufferTarget    ( gbuffer_target Target ) const;
    xbool          IsGBufferEnabled    ( void ) const { return m_bGBufferValid; }
    void           GetGBufferSize      ( u32& Width, u32& Height ) const;

private:
    xbool       m_bInitialized;
    rtarget     m_GBufferTarget[GBUFFER_TARGET_COUNT-1];
    rtarget     m_GBufferTargetSet[GBUFFER_TARGET_COUNT];
    rtarget     m_GBufferDepth;
    xbool       m_bGBufferValid;
    u32         m_GBufferWidth;
    u32         m_GBufferHeight;
    xbool       m_bGBufferTargetsActive;
};

//==============================================================================
//  GLOBAL INSTANCE
//==============================================================================

extern gbuffer_mgr g_GBufferMgr;

//==============================================================================
//  CONSTANTS
//==============================================================================

#define GBUFFER_FORMAT_FINAL_COLOR  RTARGET_FORMAT_RGBA8
#define GBUFFER_FORMAT_ALBEDO       RTARGET_FORMAT_RGBA8
#define GBUFFER_FORMAT_NORMAL       RTARGET_FORMAT_RGBA8
#define GBUFFER_FORMAT_DEPTH_INFO   RTARGET_FORMAT_RG16F

//==============================================================================
#endif // GBUFFER_MANAGER_HPP
//==============================================================================