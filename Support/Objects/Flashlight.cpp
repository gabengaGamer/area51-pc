#include "Flashlight.hpp"

#include "Player.hpp"
#include "Render\\LightMgr.hpp"

static const f32     s_FlashlightRange          = 2048.0f;
static const radian  s_FlashlightFOV            = R_45;
static const f32     s_FlashlightIntensity      = 2.0f;
static const f32     s_FlashlightFalloff        = 0.8f;
static const f32     s_FlashlightInnerConeScale = 0.75f;
static const xcolor  s_FlashlightColor          ( 255, 255, 255, 255 );
static const vector3 s_FlashlightOffset         ( 0.0f, 0.0f, -100.0f );

xbool flashlight_CalcTransform( player& Player, matrix4& L2W )
{
    new_weapon* pWeapon = Player.GetCurrentWeaponPtr();
    if( !pWeapon || !pWeapon->CheckFlashlightPoint() )
        return FALSE;

    vector3 Offset;
    if( !pWeapon->GetFlashlightTransformInfo( L2W, Offset ) )
        return FALSE;

    L2W.PreTranslate( Offset );
    L2W.PreRotateY( R_180 );
    L2W.PreTranslate( s_FlashlightOffset );
    return TRUE;
}

void flashlight_Register( player& Player )
{
    if( !Player.IsFlashlightOn() )
        return;

    matrix4 L2W;
    if( !flashlight_CalcTransform( Player, L2W ) )
        return;

    vector3 Direction = L2W.RotateVector( vector3( 0.0f, 0.0f, 1.0f ) );
    if( !Direction.SafeNormalize() )
    {
        Direction.Set( 0.0f, 0.0f, 1.0f );
    }

    const f32 OuterAngle  = RAD_TO_DEG( s_FlashlightFOV );
    const f32 InnerAngle  = MAX( 1.0f, OuterAngle * s_FlashlightInnerConeScale );
    const f32 InnerRadius = MAX( 0.0f, s_FlashlightRange * ( 1.0f - s_FlashlightFalloff ) );

    g_LightMgr.AddDynamicLight( L2W.GetTranslation(),
                                s_FlashlightColor,
                                s_FlashlightRange,
                                s_FlashlightIntensity,
                                FALSE,
                                light_mgr::LIGHT_SHAPE_SPOT,
                                TRUE,
                                InnerRadius,
                                Direction,
                                s_FlashlightFalloff,
                                InnerAngle,
                                OuterAngle );
}
