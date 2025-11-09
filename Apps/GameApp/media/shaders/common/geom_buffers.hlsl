//==============================================================================
//
//  geom_buffers.hlsl
//
//  Aggregated include for geometry constant buffers.
//
//==============================================================================

#ifndef GEOM_BUFFERS_HLSL
#define GEOM_BUFFERS_HLSL

#include "common/frame_constants.hlsl"
#include "common/lighting_constants.hlsl"
#include "common/proj_buffers.hlsl"

#if defined(GEOM_INCLUDE_OBJECT_BUFFER)
#include "common/object_constants.hlsl"
#endif

#endif // GEOM_BUFFERS_HLSL