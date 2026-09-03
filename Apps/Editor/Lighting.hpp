#ifndef LIGHTING_HPP
#define LIGHTING_HPP

#include "Obj_Mgr/Obj_Mgr.hpp"

enum
{
    LIGHTING_WHITE,
    LIGHTING_DIRECTIONAL,
    LIGHTING_DYNAMIC,
    LIGHTING_DISTANCE,
    LIGHTING_RAYCAST,
    LIGHTING_ZONE,
};

void lighting_Initialize                ( void );
void lighting_ExportTo3DMax             ( const xarray<guid>& lGuid, const char* pFileName );

void lighting_LightObject               ( asset_platform            Platform,
                                          guid                Guid,
                                          const matrix4&      L2W,
                                          s32                 Mode );

void lighting_LightObjects              ( asset_platform            Platform,
                                          const xarray<guid>& lGuid,
                                          s32                 Mode );

void lighting_CreateColorTable          ( asset_platform            Platform,
                                          const xarray<guid>& lGuid,
                                          const char*         pFileName );

void lighting_CreatePlaySurfaceColors   ( asset_platform            Platform,
                                          const xarray<guid>& lGuid );

void lighting_KillPlaySurfaceColors     ( asset_platform            Platform,
                                          const xarray<guid>& lGuid );

#endif
