//==============================================================================
//
//  bind_assets.cpp
//
//==============================================================================

#include "bind_assets.hpp"
#include "x_files.hpp"
#include "x_debug.hpp"
#include "ResourceMgr/ResourceMgr.hpp"
#include "IOManager/io_filesystem.hpp"
#include "TweakMgr/TweakMgr.hpp"

//==============================================================================

static s32 bind_asset_mount( script_context& ctx )
{
    const char* pPath = ctx.ArgString( 0 );
    xbool       Ok    = g_IOFSMgr.MountFileSystem( pPath, 4 );
    if( !Ok )
        x_DebugMsg( "[assets] mount FAILED: '%s'\n", pPath );
    ctx.PushBool( Ok );
    return 1;
}

//==============================================================================

static s32 bind_asset_unmount( script_context& ctx )
{
    ctx.PushBool( g_IOFSMgr.UnmountFileSystem( ctx.ArgString( 0 ) ) );
    return 1;
}

//==============================================================================

static s32 bind_asset_load( script_context& ctx )
{
    const char* pName = ctx.ArgString( 0 );
    g_RscMgr.Load( pName );
    xbool Loaded = g_RscMgr.IsLoaded( pName );
    if( !Loaded )
        x_DebugMsg( "[assets] load FAILED (not found in mounted filesystems): '%s'\n", pName );
    ctx.PushBool( Loaded );
    return 1;
}

//==============================================================================

static s32 bind_asset_is_loaded( script_context& ctx )
{
    ctx.PushBool( g_RscMgr.IsLoaded( ctx.ArgString( 0 ) ) );
    return 1;
}

//==============================================================================

static s32 bind_tweak_set( script_context& ctx )
{
    const char* pName  = ctx.ArgString( 0 );
    f32         Value  = ctx.ArgFloat( 1 );

    xbool Ok = SetTweakF32( pName, Value );
    if( !Ok )
        x_DebugMsg( "[assets] tweak_set: script tweak table full, dropped '%s'\n", pName );
    ctx.PushBool( Ok );
    return 1;
}

//==============================================================================

void bind_assets::Register( script_backend* pBackend )
{
    pBackend->RegisterFunction( "asset_mount",     bind_asset_mount     );
    pBackend->RegisterFunction( "asset_unmount",   bind_asset_unmount   );
    pBackend->RegisterFunction( "asset_load",      bind_asset_load      );
    pBackend->RegisterFunction( "asset_is_loaded", bind_asset_is_loaded );
    pBackend->RegisterFunction( "tweak_set",       bind_tweak_set       );
}
