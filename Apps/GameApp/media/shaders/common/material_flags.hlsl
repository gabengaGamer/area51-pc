//==============================================================================
//
//  shader_flags.hlsl
//
//  Shared shader flag definitions.
//
//==============================================================================

#define MATERIAL_FLAG_ALPHA_TEST             (1u<<0)
#define MATERIAL_FLAG_ADDITIVE               (1u<<1)
#define MATERIAL_FLAG_SUBTRACTIVE            (1u<<2)
#define MATERIAL_FLAG_VERTEX_COLOR           (1u<<3)
#define MATERIAL_FLAG_TWO_SIDED              (1u<<4)
#define MATERIAL_FLAG_ENVIRONMENT            (1u<<5)
#define MATERIAL_FLAG_DISTORTION             (1u<<6)
#define MATERIAL_FLAG_DISTORTION_PERPOLY_ENV (1u<<7)
#define MATERIAL_FLAG_DIFF_PERPIXEL_ILLUM    (1u<<8)
#define MATERIAL_FLAG_ALPHA_PERPIXEL_ILLUM   (1u<<9)
#define MATERIAL_FLAG_ALPHA_PERPOLY_ILLUM    (1u<<10)
#define MATERIAL_FLAG_DIFF_PERPIXEL_ENV      (1u<<11)
#define MATERIAL_FLAG_ALPHA_PERPOLY_ENV      (1u<<12)
#define MATERIAL_FLAG_DETAIL                 (1u<<13)
#define MATERIAL_FLAG_ENV_CUBEMAP            (1u<<14)
#define MATERIAL_FLAG_ENV_VIEWSPACE          (1u<<15)
#define MATERIAL_FLAG_ENV_WORLDSPACE         (1u<<16)
#define MATERIAL_FLAG_ALPHA_BLEND            (1u<<17)
#define MATERIAL_FLAG_ILLUM_USE_DIFFUSE      (1u<<18)

//------------------------------------------------------------------------------

#define INSTANCE_FLAG_CLIPPED            (1u<<19)
#define INSTANCE_FLAG_GLOWING            (1u<<20)
#define INSTANCE_FLAG_SHADOW_PASS        (1u<<21)
#define INSTANCE_FLAG_FILTERLIGHT        (1u<<22)
#define INSTANCE_FLAG_PROJ_LIGHT         (1u<<23)
#define INSTANCE_FLAG_FADING_ALPHA       (1u<<24)
#define INSTANCE_FLAG_DYNAMIC_LIGHT      (1u<<25)
#define INSTANCE_FLAG_DETAIL             (1u<<26)
#define INSTANCE_FLAG_PROJ_SHADOW        (1u<<27)

//------------------------------------------------------------------------------

//#define RENDER_FLAG_WIREFRAME            (1u<<29)
//#define RENDER_FLAG_WIREFRAME2           (1u<<30)
//#define RENDER_FLAG_PULSED               (1u<<31)
//#define RENDER_FLAG_SHADOW_PASS          (1u<<32)
//#define RENDER_FLAG_GLOWING              (1u<<33)
//#define RENDER_FLAG_FADING_ALPHA         (1u<<34)
//#define RENDER_FLAG_CLIPPED              (1u<<35)
//#define RENDER_FLAG_FORCE_LAST           (1u<<36)
//#define RENDER_FLAG_DISABLE_SPOTLIGHT    (1u<<37)
//#define RENDER_FLAG_DISABLE_FILTERLIGHT  (1u<<38)
//#define RENDER_FLAG_DISABLE_PROJ_SHADOWS (1u<<39)
//#define RENDER_FLAG_SIMPLE_LIGHTING      (1u<<40)
//#define RENDER_FLAG_PERPIXEL_POINTLIGHT  (1u<<41)

//------------------------------------------------------------------------------

#define MAX_PROJ_LIGHTS  4
#define MAX_PROJ_SHADOWS 8
#define MAX_SKIN_BONES 96
#define MAX_GEOM_LIGHTS 4
//#define MAX_SHADOW_SOURCES 64
//#define MAX_SHADOW_LIGHTS 8
//#define POINT_SHADOW_FACE_COUNT 6