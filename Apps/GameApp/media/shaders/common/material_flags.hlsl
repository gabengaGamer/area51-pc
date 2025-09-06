//==============================================================================
//
//  material_flags.hlsl
//
//  Shared material flag definitions.
//
//==============================================================================

#define MATERIAL_FLAG_ALPHA_TEST        (1u<<0)
#define MATERIAL_FLAG_ADDITIVE          (1u<<1)
#define MATERIAL_FLAG_SUBTRACTIVE       (1u<<2)
#define MATERIAL_FLAG_VERTEX_COLOR      (1u<<3)
#define MATERIAL_FLAG_TWO_SIDED         (1u<<4)
#define MATERIAL_FLAG_ENVIRONMENT       (1u<<5)
#define MATERIAL_FLAG_DISTORTION        (1u<<6)
#define MATERIAL_FLAG_PERPIXEL_ILLUM    (1u<<7)
#define MATERIAL_FLAG_PERPOLY_ILLUM     (1u<<8)
#define MATERIAL_FLAG_PERPIXEL_ENV      (1u<<9)
#define MATERIAL_FLAG_PERPOLY_ENV       (1u<<10)
#define MATERIAL_FLAG_HAS_DETAIL        (1u<<11)
#define MATERIAL_FLAG_HAS_ENVIRONMENT   (1u<<12)
#define MATERIAL_FLAG_PROJ_LIGHT        (1u<<13)
#define MATERIAL_FLAG_PROJ_SHADOW       (1u<<14)

#define MAX_PROJ_LIGHTS  10
#define MAX_PROJ_SHADOWS 10
