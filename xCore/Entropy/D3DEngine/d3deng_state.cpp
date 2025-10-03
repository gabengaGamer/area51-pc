//==============================================================================
//  
//  d3deng_state.cpp
//  
//  Render state manager for D3DENG.
//
//==============================================================================

//#define STATE_VERBOSE_MODE

// TODO: Learn about CMD state system.

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

#include "d3deng_state.hpp"
#include "d3deng_private.hpp"

#ifndef X_STDIO_HPP
#include "x_stdio.hpp"
#endif

//==============================================================================
//  GLOBAL STATE STORAGE
//==============================================================================

static ID3D11BlendState*        s_pBlendStates[STATE_BLEND_COUNT]     = { NULL };
static ID3D11RasterizerState*   s_pRasterStates[STATE_RASTER_COUNT]   = { NULL };
static ID3D11DepthStencilState* s_pDepthStates[STATE_DEPTH_COUNT]     = { NULL };
static ID3D11SamplerState*      s_pSamplerStates[STATE_SAMPLER_COUNT] = { NULL };

//==============================================================================
//  STATE CACHE
//==============================================================================

static struct state_cache
{
    state_blend_mode        CurrentBlendMode;
    state_raster_mode       CurrentRasterMode; 
    state_depth_mode        CurrentDepthMode;
    state_sampler_mode      CurrentSamplerMode;
    xbool                   bInitialized;
    
    state_cache( void ) : CurrentBlendMode(STATE_BLEND_NONE), 
                          CurrentRasterMode(STATE_RASTER_SOLID),
                          CurrentDepthMode(STATE_DEPTH_NORMAL),
                          CurrentSamplerMode(STATE_SAMPLER_LINEAR_WRAP),
                          bInitialized(FALSE) {}
} s_StateCache;

//==============================================================================
//  HELPER FUNCTIONS
//==============================================================================

static
const char* state_GetModeName( state_type Type, s32 Mode )
{
    switch( Type )
    {
        case STATE_TYPE_BLEND:
        {
            static const char* s_BlendModeNames[STATE_BLEND_COUNT] = 
            {
                "NONE", "ALPHA", "ADD", "SUB", "PREMULT_ALPHA", "PREMULT_ADD", "PREMULT_SUB", "MULTIPLY", "INTENSITY"
            };
            ASSERT(Mode >= 0 && Mode < STATE_BLEND_COUNT);
            return (Mode < STATE_BLEND_COUNT) ? s_BlendModeNames[Mode] : "UNKNOWN";
        }
        
        case STATE_TYPE_RASTERIZER:
        {
            static const char* s_RasterModeNames[STATE_RASTER_COUNT] = 
            {
                "SOLID", "WIRE", "SOLID_NO_CULL", "WIRE_NO_CULL"
            };
            ASSERT(Mode >= 0 && Mode < STATE_RASTER_COUNT);
            return (Mode < STATE_RASTER_COUNT) ? s_RasterModeNames[Mode] : "UNKNOWN";
        }
        
        case STATE_TYPE_DEPTH:
        {
            static const char* s_DepthModeNames[STATE_DEPTH_COUNT] = 
            {
                "NORMAL", "NO_WRITE", "DISABLED", "DISABLED_NO_WRITE"
            };
            ASSERT(Mode >= 0 && Mode < STATE_DEPTH_COUNT);
            return (Mode < STATE_DEPTH_COUNT) ? s_DepthModeNames[Mode] : "UNKNOWN";
        }
        
        case STATE_TYPE_SAMPLER:
        {
            static const char* s_SamplerModeNames[STATE_SAMPLER_COUNT] = 
            {
                "LINEAR_WRAP", "LINEAR_CLAMP", "POINT_WRAP", "POINT_CLAMP"
            };
            ASSERT(Mode >= 0 && Mode < STATE_SAMPLER_COUNT);
            return (Mode < STATE_SAMPLER_COUNT) ? s_SamplerModeNames[Mode] : "UNKNOWN";
        }
        
        default:
            ASSERT(FALSE);
            return "UNKNOWN";
    }
}

//==============================================================================
//  STATE CREATION
//==============================================================================

//------------------------------------------------------------------------------------
// NOTE: GS: Yes, I know that in fact in many places of states i duplicate settings, 
// however I would rather spend a couple of lines, 
// but at least avoid possible state leakage.
//------------------------------------------------------------------------------------

static 
void state_CreateBlendStates( void )
{
	if( !g_pd3dDevice )
        return;

    D3D11_BLEND_DESC bd;
    
    // No blending
    ZeroMemory( &bd, sizeof(bd) );
    bd.AlphaToCoverageEnable = FALSE;
    bd.IndependentBlendEnable = FALSE;
    bd.RenderTarget[0].BlendEnable = FALSE;
    bd.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
    bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    g_pd3dDevice->CreateBlendState( &bd, &s_pBlendStates[STATE_BLEND_NONE] );
    
    // Alpha blending
    ZeroMemory( &bd, sizeof(bd) );
    bd.AlphaToCoverageEnable = FALSE;
    bd.IndependentBlendEnable = FALSE;
    bd.RenderTarget[0].BlendEnable = TRUE;
    bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    g_pd3dDevice->CreateBlendState( &bd, &s_pBlendStates[STATE_BLEND_ALPHA] );
    
    // Additive blending
    ZeroMemory( &bd, sizeof(bd) );
    bd.AlphaToCoverageEnable = FALSE;
    bd.IndependentBlendEnable = FALSE;
    bd.RenderTarget[0].BlendEnable = TRUE;
    bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    bd.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    g_pd3dDevice->CreateBlendState( &bd, &s_pBlendStates[STATE_BLEND_ADD] );
    
    // Subtractive blending  
    ZeroMemory( &bd, sizeof(bd) );
    bd.AlphaToCoverageEnable = FALSE;
    bd.IndependentBlendEnable = FALSE;
    bd.RenderTarget[0].BlendEnable = TRUE;
    bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    bd.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_REV_SUBTRACT;
    bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_REV_SUBTRACT;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    g_pd3dDevice->CreateBlendState( &bd, &s_pBlendStates[STATE_BLEND_SUB] );

    // Premultiplied alpha blending
    ZeroMemory( &bd, sizeof(bd) );
    bd.AlphaToCoverageEnable = FALSE;
    bd.IndependentBlendEnable = FALSE;
    bd.RenderTarget[0].BlendEnable = TRUE;
    bd.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    g_pd3dDevice->CreateBlendState( &bd, &s_pBlendStates[STATE_BLEND_PREMULT_ALPHA] );

    // Premultiplied additive blending
    ZeroMemory( &bd, sizeof(bd) );
    bd.AlphaToCoverageEnable = FALSE;
    bd.IndependentBlendEnable = FALSE;
    bd.RenderTarget[0].BlendEnable = TRUE;
    bd.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    g_pd3dDevice->CreateBlendState( &bd, &s_pBlendStates[STATE_BLEND_PREMULT_ADD] );
    
    // Premultiplied subtractive blending  
    ZeroMemory( &bd, sizeof(bd) );
    bd.AlphaToCoverageEnable = FALSE;
    bd.IndependentBlendEnable = FALSE;
    bd.RenderTarget[0].BlendEnable = TRUE;
    bd.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_REV_SUBTRACT;
    bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_REV_SUBTRACT;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    g_pd3dDevice->CreateBlendState( &bd, &s_pBlendStates[STATE_BLEND_PREMULT_SUB] );

    // Multiplicative blending
    ZeroMemory( &bd, sizeof(bd) );
    bd.AlphaToCoverageEnable = FALSE;
    bd.IndependentBlendEnable = FALSE;
    bd.RenderTarget[0].BlendEnable = TRUE;
    bd.RenderTarget[0].SrcBlend = D3D11_BLEND_DEST_COLOR;
    bd.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
    bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_DEST_ALPHA;
    bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    g_pd3dDevice->CreateBlendState( &bd, &s_pBlendStates[STATE_BLEND_MULTIPLY] );

    // Intensity blending - uses source alpha to modulate destination color
    ZeroMemory( &bd, sizeof(bd) );
    bd.AlphaToCoverageEnable = FALSE;
    bd.IndependentBlendEnable = FALSE;
    bd.RenderTarget[0].BlendEnable = TRUE;
    bd.RenderTarget[0].SrcBlend = D3D11_BLEND_ZERO;
    bd.RenderTarget[0].DestBlend = D3D11_BLEND_SRC_ALPHA;
    bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
    bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_SRC_ALPHA;
    bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    g_pd3dDevice->CreateBlendState( &bd, &s_pBlendStates[STATE_BLEND_INTENSITY] );

    x_DebugMsg( "RStateMgr: Blend states created successfully\n" );
}

//==============================================================================

static 
void state_CreateRasterizerStates( void )
{
	if( !g_pd3dDevice )
        return;

    D3D11_RASTERIZER_DESC rd;

    // Solid + cull
    ZeroMemory( &rd, sizeof(rd) );
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_FRONT;
    rd.FrontCounterClockwise = FALSE;
    rd.DepthBias = 0;
    rd.DepthBiasClamp = 0.0f;
    rd.SlopeScaledDepthBias = 0.0f;
    rd.DepthClipEnable = TRUE;
    rd.ScissorEnable = FALSE;
    rd.MultisampleEnable = FALSE;
    rd.AntialiasedLineEnable = FALSE;
    g_pd3dDevice->CreateRasterizerState( &rd, &s_pRasterStates[STATE_RASTER_SOLID] );

    // Wire + cull
    ZeroMemory( &rd, sizeof(rd) );
    rd.FillMode = D3D11_FILL_WIREFRAME;
    rd.CullMode = D3D11_CULL_FRONT;
    rd.FrontCounterClockwise = FALSE;
    rd.DepthBias = 0;
    rd.DepthBiasClamp = 0.0f;
    rd.SlopeScaledDepthBias = 0.0f;
    rd.DepthClipEnable = TRUE;
    rd.ScissorEnable = FALSE;
    rd.MultisampleEnable = FALSE;
    rd.AntialiasedLineEnable = FALSE;
    g_pd3dDevice->CreateRasterizerState( &rd, &s_pRasterStates[STATE_RASTER_WIRE] );
	
    // Solid + no cull
	ZeroMemory( &rd, sizeof(rd) );
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
	rd.FrontCounterClockwise = FALSE;
    rd.DepthBias = 0;
    rd.DepthBiasClamp = 0.0f;
    rd.SlopeScaledDepthBias = 0.0f;
    rd.DepthClipEnable = TRUE;
    rd.ScissorEnable = FALSE;
    rd.MultisampleEnable = FALSE;
    rd.AntialiasedLineEnable = FALSE;
    g_pd3dDevice->CreateRasterizerState( &rd, &s_pRasterStates[STATE_RASTER_SOLID_NO_CULL] );
    
    // Wire + no cull
	ZeroMemory( &rd, sizeof(rd) );
	rd.FillMode = D3D11_FILL_SOLID;
    rd.FillMode = D3D11_FILL_WIREFRAME;
	rd.FrontCounterClockwise = FALSE;
    rd.DepthBias = 0;
    rd.DepthBiasClamp = 0.0f;
    rd.SlopeScaledDepthBias = 0.0f;
    rd.DepthClipEnable = TRUE;
    rd.ScissorEnable = FALSE;
    rd.MultisampleEnable = FALSE;
    rd.AntialiasedLineEnable = FALSE;
    g_pd3dDevice->CreateRasterizerState( &rd, &s_pRasterStates[STATE_RASTER_WIRE_NO_CULL] );

    x_DebugMsg( "RStateMgr: Rasterizer states created successfully\n" );
}

//==============================================================================

static 
void state_CreateDepthStencilStates( void )
{
	if( !g_pd3dDevice )
        return;

    D3D11_DEPTH_STENCIL_DESC dd;
    
    // Normal depth
    ZeroMemory( &dd, sizeof(dd) );
    dd.DepthEnable = TRUE;
    dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    dd.StencilEnable = FALSE;
    g_pd3dDevice->CreateDepthStencilState( &dd, &s_pDepthStates[STATE_DEPTH_NORMAL] );
    
    // No write  
    ZeroMemory( &dd, sizeof(dd) );
    dd.DepthEnable = TRUE;
    dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    dd.StencilEnable = FALSE;
    g_pd3dDevice->CreateDepthStencilState( &dd, &s_pDepthStates[STATE_DEPTH_NO_WRITE] );
    
    // Disabled + write
	ZeroMemory( &dd, sizeof(dd) );
    dd.DepthEnable = FALSE;
    dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	dd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	dd.StencilEnable = FALSE;
    g_pd3dDevice->CreateDepthStencilState( &dd, &s_pDepthStates[STATE_DEPTH_DISABLED] );
    
    // Disabled + no write
	ZeroMemory( &dd, sizeof(dd) );
	dd.DepthEnable = FALSE;
    dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	dd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	dd.StencilEnable = FALSE;
    g_pd3dDevice->CreateDepthStencilState( &dd, &s_pDepthStates[STATE_DEPTH_DISABLED_NO_WRITE] );

    x_DebugMsg( "RStateMgr: Depth stencil states created successfully\n" );
}

//==============================================================================

static 
void state_CreateSamplerStates( void )
{
	if( !g_pd3dDevice )
        return;

    D3D11_SAMPLER_DESC sd;
    
    // Linear + wrap
	ZeroMemory( &sd, sizeof(sd) );
    sd.MipLODBias = 0.0f;
    sd.MaxAnisotropy = 1;
    sd.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    sd.MinLOD = 0;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    g_pd3dDevice->CreateSamplerState( &sd, &s_pSamplerStates[STATE_SAMPLER_LINEAR_WRAP] );
    
    // Linear + clamp
	ZeroMemory( &sd, sizeof(sd) );
    sd.MipLODBias = 0.0f;
    sd.MaxAnisotropy = 1;
    sd.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    sd.MinLOD = 0;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
	sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    g_pd3dDevice->CreateSamplerState( &sd, &s_pSamplerStates[STATE_SAMPLER_LINEAR_CLAMP] );
    
    // Point + wrap  
	ZeroMemory( &sd, sizeof(sd) );
    sd.MipLODBias = 0.0f;
    sd.MaxAnisotropy = 1;
    sd.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    sd.MinLOD = 0;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    g_pd3dDevice->CreateSamplerState( &sd, &s_pSamplerStates[STATE_SAMPLER_POINT_WRAP] );
    
    // Point + clamp
	ZeroMemory( &sd, sizeof(sd) );
    sd.MipLODBias = 0.0f;
    sd.MaxAnisotropy = 1;
    sd.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    sd.MinLOD = 0;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
	sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    g_pd3dDevice->CreateSamplerState( &sd, &s_pSamplerStates[STATE_SAMPLER_POINT_CLAMP] );

    x_DebugMsg( "RStateMgr: Sampler states created successfully\n" );
}

//==============================================================================
//  SYSTEM FUNCTIONS
//==============================================================================

void state_Init( void )
{
    if( s_StateCache.bInitialized )
    {
        x_DebugMsg( "RStateMgr: Already initialized\n" );
		ASSERT(FALSE);
        return;
    }

    x_DebugMsg( "RStateMgr: Initializing render state management system\n" );

    // Clear all state pointers
    s32 i;
    for( i = 0; i < STATE_BLEND_COUNT; i++ )
        s_pBlendStates[i] = NULL;
    for( i = 0; i < STATE_RASTER_COUNT; i++ )
        s_pRasterStates[i] = NULL;
    for( i = 0; i < STATE_DEPTH_COUNT; i++ )
        s_pDepthStates[i] = NULL;
    for( i = 0; i < STATE_SAMPLER_COUNT; i++ )
        s_pSamplerStates[i] = NULL;

    // Create all states
    state_CreateBlendStates();
    state_CreateRasterizerStates();
    state_CreateDepthStencilStates();
    state_CreateSamplerStates();

    // Reset cache to force initial state setup
    state_FlushCache();
    
    s_StateCache.bInitialized = TRUE;
    x_DebugMsg( "RStateMgr: Initialization complete\n" );
}

//==============================================================================

void state_Kill( void )
{
    if( !s_StateCache.bInitialized )
    {
        x_DebugMsg( "RStateMgr: Not initialized\n" );
		ASSERT(FALSE);
        return;
    }

    x_DebugMsg( "RStateMgr: Shutting down render state management system\n" );

    // Release all blend states
    s32 i;
    for( i = 0; i < STATE_BLEND_COUNT; i++ )
    {
        if( s_pBlendStates[i] )
        {
            s_pBlendStates[i]->Release();
            s_pBlendStates[i] = NULL;
        }
    }

    // Release all rasterizer states
    for( i = 0; i < STATE_RASTER_COUNT; i++ )
    {
        if( s_pRasterStates[i] )
        {
            s_pRasterStates[i]->Release();
            s_pRasterStates[i] = NULL;
        }
    }

    // Release all depth stencil states
    for( i = 0; i < STATE_DEPTH_COUNT; i++ )
    {
        if( s_pDepthStates[i] )
        {
            s_pDepthStates[i]->Release();
            s_pDepthStates[i] = NULL;
        }
    }

    // Release all sampler states
    for( i = 0; i < STATE_SAMPLER_COUNT; i++ )
    {
        if( s_pSamplerStates[i] )
        {
            s_pSamplerStates[i]->Release();
            s_pSamplerStates[i] = NULL;
        }
    }

    s_StateCache.bInitialized = FALSE;
    x_DebugMsg( "RStateMgr: Shutdown complete\n" );
}

//==============================================================================
//  STATE MANAGEMENT FUNCTIONS
//==============================================================================

xbool state_SetState( state_type Type, s32 Mode )
{
    if( !s_StateCache.bInitialized )
    {
        x_DebugMsg( "RStateMgr: Not initialized\n" );
		ASSERT(FALSE);
        return FALSE;
    }

    if( !g_pd3dContext )
        return FALSE;

    switch( Type )
    {
        case STATE_TYPE_BLEND:
        {
            if( Mode >= STATE_BLEND_COUNT )
            {
                x_DebugMsg( "RStateMgr: Invalid blend mode %d\n", Mode );
				ASSERT(FALSE);
                return FALSE;
            }

            state_blend_mode BlendMode = (state_blend_mode)Mode;
            
            // Check cache
            if( s_StateCache.CurrentBlendMode == BlendMode )
            {
				#ifdef STATE_VERBOSE_MODE
                x_DebugMsg( "RStateMgr: Blend %s CACHED\n", state_GetModeName(STATE_TYPE_BLEND, Mode) );
				#endif
                return FALSE;
            }


            if( !s_pBlendStates[Mode] )
            {
                x_DebugMsg( "RStateMgr: Blend state %s not created\n", state_GetModeName(STATE_TYPE_BLEND, Mode) );
				ASSERT(FALSE);
                return FALSE;
            }

            // Set the state
            g_pd3dContext->OMSetBlendState( s_pBlendStates[Mode], NULL, 0xffffffff );
            s_StateCache.CurrentBlendMode = BlendMode;

            #ifdef STATE_VERBOSE_MODE
            x_DebugMsg( "RStateMgr: Blend mode set to %s\n", state_GetModeName(STATE_TYPE_BLEND, Mode) );
			#endif
            return TRUE;
        }

        case STATE_TYPE_RASTERIZER:
        {
            if( Mode >= STATE_RASTER_COUNT )
            {
                x_DebugMsg( "RStateMgr: Invalid rasterizer mode %d\n", Mode );
				ASSERT(FALSE);
                return FALSE;
            }

            state_raster_mode RasterMode = (state_raster_mode)Mode;
            
            // Check cache
            if( s_StateCache.CurrentRasterMode == RasterMode )
            {
				#ifdef STATE_VERBOSE_MODE
                x_DebugMsg( "RStateMgr: Rasterizer %s CACHED\n", state_GetModeName(STATE_TYPE_RASTERIZER, Mode) );
				#endif
                return FALSE;
            }

            if( !s_pRasterStates[Mode] )
            {
                x_DebugMsg( "RStateMgr: Rasterizer state %s not created\n", state_GetModeName(STATE_TYPE_RASTERIZER, Mode) );
				ASSERT(FALSE);
                return FALSE;
            }

            // Set the state
            g_pd3dContext->RSSetState( s_pRasterStates[Mode] );
            s_StateCache.CurrentRasterMode = RasterMode;

            #ifdef STATE_VERBOSE_MODE
            x_DebugMsg( "RStateMgr: Rasterizer mode set to %s\n", state_GetModeName(STATE_TYPE_RASTERIZER, Mode) );
			#endif
            return TRUE;
        }

        case STATE_TYPE_DEPTH:
        {
            if( Mode >= STATE_DEPTH_COUNT )
            {
                x_DebugMsg( "RStateMgr: Invalid depth mode %d\n", Mode );
				ASSERT(FALSE);
                return FALSE;
            }

            state_depth_mode DepthMode = (state_depth_mode)Mode;
            
            // Check cache
            if( s_StateCache.CurrentDepthMode == DepthMode )
            {
				#ifdef STATE_VERBOSE_MODE
                x_DebugMsg( "RStateMgr: Depth %s CACHED\n", state_GetModeName(STATE_TYPE_DEPTH, Mode) );
				#endif
                return FALSE;
            }

            if( !s_pDepthStates[Mode] )
            {
                x_DebugMsg( "RStateMgr: Depth state %s not created\n", state_GetModeName(STATE_TYPE_DEPTH, Mode) );
				ASSERT(FALSE);
                return FALSE;
            }

            // Set the state
            g_pd3dContext->OMSetDepthStencilState( s_pDepthStates[Mode], 0 );
            s_StateCache.CurrentDepthMode = DepthMode;

            #ifdef STATE_VERBOSE_MODE
            x_DebugMsg( "RStateMgr: Depth mode set to %s\n", state_GetModeName(STATE_TYPE_DEPTH, Mode) );
			#endif
            return TRUE;
        }

        case STATE_TYPE_SAMPLER:
        {
            if( Mode >= STATE_SAMPLER_COUNT )
            {
                x_DebugMsg( "RStateMgr: Invalid sampler mode %d\n", Mode );
				ASSERT(FALSE);
                return FALSE;
            }

            state_sampler_mode SamplerMode = (state_sampler_mode)Mode;
            
            // Check cache
            if( s_StateCache.CurrentSamplerMode == SamplerMode )
            {
				#ifdef STATE_VERBOSE_MODE
                x_DebugMsg( "RStateMgr: Sampler %s CACHED\n", state_GetModeName(STATE_TYPE_SAMPLER, Mode) );
				#endif
                return FALSE;
            }

            if( !s_pSamplerStates[Mode] )
            {
                x_DebugMsg( "RStateMgr: Sampler state %s not created\n", state_GetModeName(STATE_TYPE_SAMPLER, Mode) );
				ASSERT(FALSE);
                return FALSE;
            }

            // Set the state
            g_pd3dContext->PSSetSamplers( 0, 1, &s_pSamplerStates[Mode] );
            s_StateCache.CurrentSamplerMode = SamplerMode;

            #ifdef STATE_VERBOSE_MODE
            x_DebugMsg( "RStateMgr: Sampler mode set to %s\n", state_GetModeName(STATE_TYPE_SAMPLER, Mode) );
			#endif
            return TRUE;
        }

        default:
            x_DebugMsg( "RStateMgr: Invalid state type %d\n", Type );
			ASSERT(FALSE);
            return FALSE;
    }
}

//==============================================================================
//  UTILITY FUNCTIONS
//==============================================================================

void state_FlushCache( void )
{
    x_DebugMsg( "RStateMgr: Flushing state cache\n" );
    
    // Force all states to be invalid so they get reapplied
    s_StateCache.CurrentBlendMode   = (state_blend_mode)0xFF;
    s_StateCache.CurrentRasterMode  = (state_raster_mode)0xFF;
    s_StateCache.CurrentDepthMode   = (state_depth_mode)0xFF;
    s_StateCache.CurrentSamplerMode = (state_sampler_mode)0xFF;
}

//==============================================================================

s32 state_GetState( state_type Type )
{
    if( !s_StateCache.bInitialized )
    {
        x_DebugMsg( "RStateMgr: Not initialized\n" );
		ASSERT(FALSE);
        return -1;
    }

    switch( Type )
    {
        case STATE_TYPE_BLEND:      return (s32)s_StateCache.CurrentBlendMode;
        case STATE_TYPE_RASTERIZER: return (s32)s_StateCache.CurrentRasterMode;
        case STATE_TYPE_DEPTH:      return (s32)s_StateCache.CurrentDepthMode;
        case STATE_TYPE_SAMPLER:    return (s32)s_StateCache.CurrentSamplerMode;
        default:
            x_DebugMsg( "RStateMgr: Invalid state type %d\n", Type );
			ASSERT(FALSE);
            return -1;
    }
}

//==============================================================================

xbool state_IsState( state_type Type, s32 Mode )
{
    if( !s_StateCache.bInitialized )
    {
        x_DebugMsg( "RStateMgr: Not initialized\n" );
		ASSERT(FALSE);
        return FALSE;
    }

    switch( Type )
    {
        case STATE_TYPE_BLEND:      return (s_StateCache.CurrentBlendMode   == (state_blend_mode)Mode);
        case STATE_TYPE_RASTERIZER: return (s_StateCache.CurrentRasterMode  == (state_raster_mode)Mode);
        case STATE_TYPE_DEPTH:      return (s_StateCache.CurrentDepthMode   == (state_depth_mode)Mode);
        case STATE_TYPE_SAMPLER:    return (s_StateCache.CurrentSamplerMode == (state_sampler_mode)Mode);
        default:
            x_DebugMsg( "RStateMgr: Invalid state type %d\n", Type );
			ASSERT(FALSE);
            return FALSE;
    }
}

//==============================================================================