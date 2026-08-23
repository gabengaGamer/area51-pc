//=========================================================================
//
//  LightMgr.cpp
//
//=========================================================================

//=========================================================================
//  INCLUDES
//=========================================================================

#include "LightMgr.hpp"
#include "e_ScratchMem.hpp"

//=========================================================================
//  GLOBAL INSTANCE
//=========================================================================

light_mgr g_LightMgr;

//=========================================================================
//  HELPER FUNCTIONS
//=========================================================================

static 
f32 ComputeLightScore( xcolor const& Color, f32 Intensity )
{
    f32 const R = static_cast<f32>( Color.R ) * Intensity;
    f32 const G = static_cast<f32>( Color.G ) * Intensity;
    f32 const B = static_cast<f32>( Color.B ) * Intensity;

    return R * R + G * G + B * B;
}

//=========================================================================

static 
void ScaleLightColor( xcolor& Destination, xcolor const& Source, f32 Intensity )
{
    Destination.R = static_cast<u8>( MIN( 255.0f, static_cast<f32>( Source.R ) * Intensity ) );
    Destination.G = static_cast<u8>( MIN( 255.0f, static_cast<f32>( Source.G ) * Intensity ) );
    Destination.B = static_cast<u8>( MIN( 255.0f, static_cast<f32>( Source.B ) * Intensity ) );
    Destination.A = static_cast<u8>( MIN( 255.0f, static_cast<f32>( Source.A ) * Intensity ) );
}

//=========================================================================

static 
f32 ComputeSpotConeAttenuation( vector3 const& LightPosition, vector3 const& LightDirection, f32 OuterConeCos,
                                f32 ConeCosRangeInv, vector3 const& TargetPosition )
{
    vector3 ToTarget = TargetPosition - LightPosition;
    if( !ToTarget.SafeNormalize() )
    {
        return 1.0f;
    }

    f32 const SpotCos = LightDirection.Dot( ToTarget );

    return MINMAX( 0.0f, ( SpotCos - OuterConeCos ) * ConeCosRangeInv, 1.0f );
}

//=========================================================================

static 
xbool SphereIntersectsFiniteCone( vector3 const& SphereCenter, f32 SphereRadius, vector3 const& ConeApex,
                                  vector3 const& ConeAxis, f32 ConeOuterCos, f32 ConeOuterSin, f32 ConeOuterTan,
                                  f32 ConeLength )
{
    ASSERT( ConeOuterCos > 0.0f );

    vector3 const ToCenter = SphereCenter - ConeApex;
    f32 const     CenterDistSq = ToCenter.Dot( ToCenter );
    f32 const     AxialDistance = ConeAxis.Dot( ToCenter );
    f32 const     SphereRadiusSq = SphereRadius * SphereRadius;

    if( AxialDistance < -SphereRadius )
    {
        return FALSE;
    }

    if( AxialDistance > ( ConeLength + SphereRadius ) )
    {
        return FALSE;
    }

    if( AxialDistance <= 0.0f )
    {
        return ( CenterDistSq <= SphereRadiusSq );
    }

    f32 const RadialDistanceSq = MAX( CenterDistSq - ( AxialDistance * AxialDistance ), 0.0f );
    f32 const RadialDistance = x_sqrt( RadialDistanceSq );

    if( AxialDistance < ConeLength )
    {
        f32 const SideDistance = ( RadialDistance * ConeOuterCos ) - ( AxialDistance * ConeOuterSin );
        if( SideDistance <= SphereRadius )
        {
            return TRUE;
        }
    }

    f32 const ConeCapRadius = ConeLength * ConeOuterTan;
    f32 const CapAxialDelta = MAX( AxialDistance - ConeLength, 0.0f );
    f32 const CapRadialDelta = MAX( RadialDistance - ConeCapRadius, 0.0f );
    return ( ( CapAxialDelta * CapAxialDelta ) + ( CapRadialDelta * CapRadialDelta ) ) <= SphereRadiusSq;
}

//=========================================================================

static 
xbool SpotLightAffectsSphere( vector3 const& LightPosition, vector3 const& LightDirection, f32 OuterConeCos,
                              f32 OuterConeSin, f32 OuterConeTan, f32 LightRadius, vector3 const& SphereCenter,
                              f32 SphereRadius )
{
    if( OuterConeCos <= 0.0f )
    {
        return TRUE;
    }

    return SphereIntersectsFiniteCone( SphereCenter, SphereRadius, LightPosition, LightDirection,
                                       OuterConeCos, OuterConeSin, OuterConeTan, LightRadius );
}

//=========================================================================

static 
void SetupLightCone( f32 InnerAngle, f32 OuterAngle, f32& InnerConeCos, f32& OuterConeCos, f32& OuterConeSin,
                     f32& OuterConeTan, f32& ConeCosRangeInv )
{
    InnerConeCos = x_cos( DEG_TO_RAD( InnerAngle ) * 0.5f );
    OuterConeCos = x_cos( DEG_TO_RAD( OuterAngle ) * 0.5f );

    f32 const ConeCosRange = MAX( InnerConeCos - OuterConeCos, 0.0001f );
    ConeCosRangeInv = 1.0f / ConeCosRange;

    if( OuterConeCos > 0.0f )
    {
        OuterConeSin = x_sqrt( MAX( 1.0f - ( OuterConeCos * OuterConeCos ), 0.0f ) );
        OuterConeTan = OuterConeSin / OuterConeCos;
    }
    else
    {
        OuterConeSin = 0.0f;
        OuterConeTan = 0.0f;
    }
}

//=========================================================================

static 
void SetupOmniLightCone( f32& InnerConeCos, f32& OuterConeCos, f32& OuterConeSin, f32& OuterConeTan,
                         f32& ConeCosRangeInv )
{
    InnerConeCos = 1.0f;
    OuterConeCos = 1.0f;
    OuterConeSin = 0.0f;
    OuterConeTan = 0.0f;
    ConeCosRangeInv = 0.0f;
}

//=========================================================================

static 
void BuildLightCookieUV( vector3 const& Direction, vector3& CookieU, vector3& CookieV )
{
    vector3 Forward = Direction;
    if( !Forward.SafeNormalize() )
    {
        Forward.Set( 0.0f, 0.0f, 1.0f );
    }

    CookieU = vector3( 0.0f, 1.0f, 0.0f ).Cross( Forward );
    if( !CookieU.SafeNormalize() )
    {
        CookieU = vector3( 1.0f, 0.0f, 0.0f ).Cross( Forward );
        if( !CookieU.SafeNormalize() )
        {
            CookieU.Set( 1.0f, 0.0f, 0.0f );
        }
    }

    CookieV = Forward.Cross( CookieU );
    if( !CookieV.SafeNormalize() )
    {
        CookieV.Set( 0.0f, 1.0f, 0.0f );
    }
}

//=========================================================================
//  FUNCTIONS
//=========================================================================

s32 light_mgr::SpadLightSortFn( void const* pA, void const* pB )
{
    light_mgr::spad_light const* pLightA = static_cast<light_mgr::spad_light const*>( pA );
    light_mgr::spad_light const* pLightB = static_cast<light_mgr::spad_light const*>( pB );

    if( pLightA->CharOnly > pLightB->CharOnly )
    {
        return 1;
    }
    if( pLightA->CharOnly < pLightB->CharOnly )
    {
        return -1;
    }
    if( pLightA->Score > pLightB->Score )
    {
        return -1;
    }
    if( pLightA->Score < pLightB->Score )
    {
        return 1;
    }

    return 0;
}

//=========================================================================

xbool light_mgr::CalcDirLight( dir_light* pDestination, matrix4 const& LocalToWorld,
                               bbox const& LocalBBox, bbox const& WorldBBox,
                               vector3 const& WorldBBoxCenter, vector3 const& Position,
                               f32 Radius, f32 Intensity, xcolor const& Color, s32 Shape,
                               vector3 const& Direction, f32 OuterConeCos, f32 ConeCosRangeInv )
{
    if( WorldBBox.Intersect( Position, Radius ) )
    {
        // Move the light into local space so the bounding box remains accurate.
        vector3 Temp = Position - LocalToWorld.GetTranslation();
        vector3 LocalPosition( Temp.GetX() * LocalToWorld( 0, 0 ) +
                               Temp.GetY() * LocalToWorld( 0, 1 ) +
                               Temp.GetZ() * LocalToWorld( 0, 2 ),
                               Temp.GetX() * LocalToWorld( 1, 0 ) +
                               Temp.GetY() * LocalToWorld( 1, 1 ) +
                               Temp.GetZ() * LocalToWorld( 1, 2 ),
                               Temp.GetX() * LocalToWorld( 2, 0 ) +
                               Temp.GetY() * LocalToWorld( 2, 1 ) +
                               Temp.GetZ() * LocalToWorld( 2, 2 ) );

        // Find the squared distance from the light to the local bounding box.
        f32 DistanceDelta;
        f32 DistanceSquared = 0.0f;
        if( LocalPosition.GetX() > LocalBBox.Max.GetX() )
        {
            DistanceDelta = LocalPosition.GetX() - LocalBBox.Max.GetX();
            DistanceSquared += DistanceDelta * DistanceDelta;
        }

        if( LocalPosition.GetX() < LocalBBox.Min.GetX() )
        {
            DistanceDelta = LocalBBox.Min.GetX() - LocalPosition.GetX();
            DistanceSquared += DistanceDelta * DistanceDelta;
        }

        if( LocalPosition.GetY() > LocalBBox.Max.GetY() )
        {
            DistanceDelta = LocalPosition.GetY() - LocalBBox.Max.GetY();
            DistanceSquared += DistanceDelta * DistanceDelta;
        }

        if( LocalPosition.GetY() < LocalBBox.Min.GetY() )
        {
            DistanceDelta = LocalBBox.Min.GetY() - LocalPosition.GetY();
            DistanceSquared += DistanceDelta * DistanceDelta;
        }

        if( LocalPosition.GetZ() > LocalBBox.Max.GetZ() )
        {
            DistanceDelta = LocalPosition.GetZ() - LocalBBox.Max.GetZ();
            DistanceSquared += DistanceDelta * DistanceDelta;
        }

        if( LocalPosition.GetZ() < LocalBBox.Min.GetZ() )
        {
            DistanceDelta = LocalBBox.Min.GetZ() - LocalPosition.GetZ();
            DistanceSquared += DistanceDelta * DistanceDelta;
        }

        // Convert distance attenuation into light intensity.
        f32 RadiusSquared = Radius * Radius;

        if( RadiusSquared == 0.0f )
        {
            RadiusSquared = 1.0f;
        }

        f32 LightIntensity = 1.0f - DistanceSquared / RadiusSquared;
        LightIntensity = MAX( LightIntensity, 0.0f );
        LightIntensity *= Intensity;

        if( LightIntensity > 0.0f )
        {
            // Point the derived directional light toward the object's center.
            if( Shape == LIGHT_SHAPE_SPOT )
            {
                LightIntensity *= ComputeSpotConeAttenuation( Position, Direction, OuterConeCos,
                                                               ConeCosRangeInv, WorldBBoxCenter );
            }

            if( LightIntensity <= 0.0f )
            {
                return FALSE;
            }

            vector3 LightDirection = WorldBBoxCenter - Position;
            if( !LightDirection.SafeNormalize() )
            {
                // The light may be placed exactly at the object's center.
                LightDirection.Set( 0.7071f, -0.7071f, 0.0f );
            }

            pDestination->Col.R = static_cast<u8>( MIN( LightIntensity * static_cast<f32>( Color.R ), 255.0f ) );
            pDestination->Col.G = static_cast<u8>( MIN( LightIntensity * static_cast<f32>( Color.G ), 255.0f ) );
            pDestination->Col.B = static_cast<u8>( MIN( LightIntensity * static_cast<f32>( Color.B ), 255.0f ) );
            pDestination->Col.A = 0;
            pDestination->Dir = LightDirection;

            return TRUE;
        }
    }

    return FALSE;
}

//=========================================================================

light_mgr::light_mgr( void )
    : m_firstLink( -1 )
    , m_nFadingLights( 0 )
    , m_nDynamicLights( 0 )
    , m_nCharLights( 0 )
    , m_isInCollection( FALSE )
    , m_nSpadLights( 0 )
    , m_nNonCharLightsInSpad( 0 )
    , m_pSpadLights( nullptr )
    , m_pLightBvhNodes( nullptr )
    , m_pLightBvhLightIndices( nullptr )
    , m_nLightBvhNodes( 0 )
    , m_nCollectedLights( 0 )
{
    x_memset( m_fadingLights, 0, sizeof( fading_light ) * MAX_FADING_LIGHTS );
    x_memset( &m_collectionStats, 0, sizeof( m_collectionStats ) );
}

//=========================================================================

light_mgr::~light_mgr( void )
{
}

//=========================================================================

s32 light_mgr::AddLight( void )
{
    // Find a free slot and track the weakest light in case replacement is required.
    s32 LightIndex;
    f32 WeakestScore = F32_MAX;
    s32 LightToReplace = -1;

    for( LightIndex = 0; LightIndex < MAX_FADING_LIGHTS; LightIndex++ )
    {
        if( !m_fadingLights[LightIndex].Valid )
        {
            break;
        }

        f32 Score = ComputeLightScore( m_fadingLights[LightIndex].CurrentColor, m_fadingLights[LightIndex].Intensity );

        if( ( LightToReplace == -1 ) || ( Score < WeakestScore ) )
        {
            LightToReplace = LightIndex;
            WeakestScore = Score;
        }
    }

    if( LightIndex == MAX_FADING_LIGHTS )
    {
        // The replaced light is already linked, so no link repair is required.
        // NOTE: We should rarely hit this, but if we do and it starts to kick out
        // any vital lights, we could start merging lights together (sounds like fun!)
        ASSERT( LightToReplace != -1 );
        LightIndex = LightToReplace;
    }
    else
    {
        // TODO: Consider cache-friendly insertion if this list becomes a hotspot.
        if( m_firstLink != -1 )
        {
            m_fadingLights[m_firstLink].PrevLink = LightIndex;
        }

        m_fadingLights[LightIndex].NextLink = m_firstLink;
        m_fadingLights[LightIndex].PrevLink = -1;
        m_firstLink = LightIndex;
        m_nFadingLights++;
    }

    ASSERT( ( LightIndex >= 0 ) && ( LightIndex < MAX_FADING_LIGHTS ) );
    return LightIndex;
}

//=========================================================================

void light_mgr::RemoveLight( s32 LightIndex )
{
    ASSERT( ( LightIndex >= 0 ) && ( LightIndex < MAX_FADING_LIGHTS ) );
    ASSERT( m_fadingLights[LightIndex].Valid );

    m_fadingLights[LightIndex].Valid = FALSE;

    // Repair the linked list.
    s32 Previous = m_fadingLights[LightIndex].PrevLink;
    s32 Next = m_fadingLights[LightIndex].NextLink;
    if( Previous != -1 )
    {
        m_fadingLights[Previous].NextLink = Next;
    }
    if( Next != -1 )
    {
        m_fadingLights[Next].PrevLink = Previous;
    }
    if( LightIndex == m_firstLink )
    {
        ASSERT( Previous == -1 );
        m_firstLink = Next;
    }

    m_nFadingLights--;
}

//=========================================================================

void light_mgr::AddFadingLight( vector3 const& Position, xcolor const& Color, f32 Radius, f32 Intensity, f32 FadeTime )
{
    if( FadeTime <= 0.0f )
    {
        return;
    }

    s32 LightIndex = AddLight();

    m_fadingLights[LightIndex].Pos = Position;
    m_fadingLights[LightIndex].Radius = Radius;
    m_fadingLights[LightIndex].StartColor = Color;
    m_fadingLights[LightIndex].CurrentColor = Color;
    m_fadingLights[LightIndex].FadeTime = FadeTime;
    m_fadingLights[LightIndex].ElapsedTime = 0.0f;
    m_fadingLights[LightIndex].Valid = TRUE;
    m_fadingLights[LightIndex].InterpolationT = 0.0f;
    m_fadingLights[LightIndex].Intensity = Intensity;
}

//=========================================================================

void light_mgr::OnUpdate( f32 DeltaTime )
{
    for( s32 LightIndex = 0; LightIndex < MAX_FADING_LIGHTS; LightIndex++ )
    {
        if( m_fadingLights[LightIndex].Valid )
        {
            m_fadingLights[LightIndex].ElapsedTime += DeltaTime;

            if( m_fadingLights[LightIndex].ElapsedTime >= m_fadingLights[LightIndex].FadeTime )
            {
                RemoveLight( LightIndex );
            }
            else
            {
                f32 T = m_fadingLights[LightIndex].ElapsedTime / m_fadingLights[LightIndex].FadeTime;
                f32 R = static_cast<f32>( m_fadingLights[LightIndex].StartColor.R );
                f32 G = static_cast<f32>( m_fadingLights[LightIndex].StartColor.G );
                f32 B = static_cast<f32>( m_fadingLights[LightIndex].StartColor.B );

                m_fadingLights[LightIndex].InterpolationT = T;
                m_fadingLights[LightIndex].CurrentColor.R = static_cast<u8>( R * ( 1.0f - T ) );
                m_fadingLights[LightIndex].CurrentColor.G = static_cast<u8>( G * ( 1.0f - T ) );
                m_fadingLights[LightIndex].CurrentColor.B = static_cast<u8>( B * ( 1.0f - T ) );
            }
        }
    }
}

//=========================================================================

s32 light_mgr::RegisterSpotLightCookie( texture::handle const& Cookie )
{
    texture* pTexture = Cookie.GetPointer();
    if( pTexture == nullptr )
    {
        return -1;
    }

    for( s32 Index = 0; Index < m_lightCookieFaces.GetCount(); Index++ )
    {
        if( m_lightCookieFaces[Index].GetPointer() == pTexture )
        {
            return Index;
        }
    }

    m_lightCookieFaces.Append() = Cookie;
    return m_lightCookieFaces.GetCount() - 1;
}

//=========================================================================

void light_mgr::AddDynamicLight( vector3 const& Position, xcolor const& Color, f32 Radius,
                                 f32 Intensity, xbool CharOnly, s32 Shape, xbool CastShadows,
                                 f32 InnerRadius, vector3 const& Direction, f32 Falloff,
                                 f32 InnerAngle, f32 OuterAngle, s32 ShadowMapResolution, s32 ShadowPriority,
                                 texture::handle const& Cookie )
{
    vector3 LightDirection = Direction;

    if( Shape != LIGHT_SHAPE_SPOT )
    {
        Shape = LIGHT_SHAPE_OMNI;
    }

    InnerRadius = MAX( 0.0f, MIN( InnerRadius, Radius ) );
    Falloff = MINMAX( 0.0f, Falloff, 1.0f );
    if( ( Radius > 0.0001f ) && ( InnerRadius > 0.0f ) )
    {
        Falloff = MINMAX( 0.0f, 1.0f - ( InnerRadius / Radius ), 1.0f );
    }
    InnerAngle = MAX( 0.0f, InnerAngle );
    OuterAngle = MAX( InnerAngle, OuterAngle );
    if( !LightDirection.SafeNormalize() )
    {
        LightDirection.Set( 0.0f, 0.0f, 1.0f );
    }

    if( CharOnly )
    {
        if( m_nCharLights >= MAX_CHAR_LIGHTS )
        {
            return;
        }
        x_memset( &m_charLights[m_nCharLights], 0, sizeof( dynamic_light ) );
        m_charLights[m_nCharLights].Pos = Position;
        m_charLights[m_nCharLights].Radius = Radius;
        m_charLights[m_nCharLights].Intensity = Intensity;
        m_charLights[m_nCharLights].Color = Color;
        m_charLights[m_nCharLights].Shape = LIGHT_SHAPE_OMNI;
        m_charLights[m_nCharLights].InnerConeCos = 1.0f;
        m_charLights[m_nCharLights].OuterConeCos = 1.0f;
        m_charLights[m_nCharLights].OuterConeSin = 0.0f;
        m_charLights[m_nCharLights].OuterConeTan = 0.0f;
        m_charLights[m_nCharLights].ConeCosRangeInv = 0.0f;
        m_charLights[m_nCharLights].CookieIndex = -1;
        m_nCharLights++;
    }
    else
    {
        if( m_nDynamicLights >= MAX_DYNAMIC_LIGHTS )
        {
            return;
        }

        f32 InnerConeCos;
        f32 OuterConeCos;
        f32 OuterConeSin;
        f32 OuterConeTan;
        f32 ConeCosRangeInv;
        if( Shape == LIGHT_SHAPE_SPOT )
        {
            SetupLightCone( InnerAngle, OuterAngle, InnerConeCos, OuterConeCos, OuterConeSin, OuterConeTan,
                            ConeCosRangeInv );
        }
        else
        {
            SetupOmniLightCone( InnerConeCos, OuterConeCos, OuterConeSin, OuterConeTan, ConeCosRangeInv );
        }

        vector3   CookieU( 1.0f, 0.0f, 0.0f );
        vector3   CookieV( 0.0f, 1.0f, 0.0f );
        s32 const CookieIndex = ( Shape == LIGHT_SHAPE_SPOT ) ? RegisterSpotLightCookie( Cookie ) : -1;
        if( CookieIndex >= 0 )
        {
            BuildLightCookieUV( LightDirection, CookieU, CookieV );
        }

        m_dynamicLights[m_nDynamicLights].Pos = Position;
        m_dynamicLights[m_nDynamicLights].Radius = Radius;
        m_dynamicLights[m_nDynamicLights].Intensity = Intensity;
        m_dynamicLights[m_nDynamicLights].Color = Color;
        m_dynamicLights[m_nDynamicLights].Direction = LightDirection;
        m_dynamicLights[m_nDynamicLights].Falloff = Falloff;
        m_dynamicLights[m_nDynamicLights].InnerAngle = InnerAngle;
        m_dynamicLights[m_nDynamicLights].OuterAngle = OuterAngle;
        m_dynamicLights[m_nDynamicLights].InnerConeCos = InnerConeCos;
        m_dynamicLights[m_nDynamicLights].OuterConeCos = OuterConeCos;
        m_dynamicLights[m_nDynamicLights].OuterConeSin = OuterConeSin;
        m_dynamicLights[m_nDynamicLights].OuterConeTan = OuterConeTan;
        m_dynamicLights[m_nDynamicLights].ConeCosRangeInv = ConeCosRangeInv;
        m_dynamicLights[m_nDynamicLights].Shape = Shape;
        m_dynamicLights[m_nDynamicLights].ShadowMapResolution = ShadowMapResolution;
        m_dynamicLights[m_nDynamicLights].ShadowPriority = ShadowPriority;
        m_dynamicLights[m_nDynamicLights].CastShadows = CastShadows;
        m_dynamicLights[m_nDynamicLights].CookieIndex = CookieIndex;
        m_dynamicLights[m_nDynamicLights].CookieU = CookieU;
        m_dynamicLights[m_nDynamicLights].CookieV = CookieV;
        m_nDynamicLights++;
    }
}

//=========================================================================

void light_mgr::GetDynamicLight( s32 Index, vector3& Position, f32& Radius, xcolor& Color,
                                 s32& Shape, xbool& CastShadows, f32& InnerRadius,
                                 vector3& Direction, f32& Falloff, f32& InnerAngle, f32& OuterAngle,
                                 s32& ShadowMapResolution, s32& ShadowPriority ) const
{
    ASSERT( ( Index >= 0 ) && ( Index < m_nDynamicLights ) );

    dynamic_light const& Light = m_dynamicLights[Index];
    Position = Light.Pos;
    Radius = Light.Radius;
    InnerRadius = MAX( 0.0f, Light.Radius * ( 1.0f - Light.Falloff ) );
    Direction = Light.Direction;
    Falloff = Light.Falloff;
    InnerAngle = Light.InnerAngle;
    OuterAngle = Light.OuterAngle;
    Shape = Light.Shape;
    ShadowMapResolution = Light.ShadowMapResolution;
    ShadowPriority = Light.ShadowPriority;
    CastShadows = Light.CastShadows;
    ScaleLightColor( Color, Light.Color, Light.Intensity );
}

//=========================================================================

void light_mgr::BeginLightCollection( void )
{
    ASSERT( !m_isInCollection );

    x_memset( &m_collectionStats, 0, sizeof( m_collectionStats ) );

    // Allocate frame-local light data.
    s32 LightCount = m_nFadingLights + m_nDynamicLights + m_nCharLights;
    smem_StackPushMarker();
    if( LightCount > 0 )
    {
        m_pSpadLights = reinterpret_cast<spad_light*>( smem_StackAlloc( sizeof( spad_light ) * LightCount ) );
    }
    else
    {
        m_pSpadLights = nullptr;
    }
    ASSERT( ( LightCount == 0 ) || m_pSpadLights );

    LightCount = 0;

    // Add dynamic lights.
    s32 Index;
    for( Index = 0; Index < m_nDynamicLights; Index++ )
    {
        dynamic_light& Light = m_dynamicLights[Index];
        m_pSpadLights[LightCount].Pos = Light.Pos;
        m_pSpadLights[LightCount].Radius = Light.Radius;
        m_pSpadLights[LightCount].Intensity = Light.Intensity;
        m_pSpadLights[LightCount].Color = Light.Color;
        m_pSpadLights[LightCount].Falloff = Light.Falloff;
        m_pSpadLights[LightCount].Score = ComputeLightScore( Light.Color, Light.Intensity );
        m_pSpadLights[LightCount].CharOnly = FALSE;
        m_pSpadLights[LightCount].Shape = Light.Shape;
        m_pSpadLights[LightCount].Direction = Light.Direction;
        m_pSpadLights[LightCount].InnerConeCos = Light.InnerConeCos;
        m_pSpadLights[LightCount].OuterConeCos = Light.OuterConeCos;
        m_pSpadLights[LightCount].OuterConeSin = Light.OuterConeSin;
        m_pSpadLights[LightCount].OuterConeTan = Light.OuterConeTan;
        m_pSpadLights[LightCount].ConeCosRangeInv = Light.ConeCosRangeInv;
        m_pSpadLights[LightCount].CookieIndex = Light.CookieIndex;
        m_pSpadLights[LightCount].CookieU = Light.CookieU;
        m_pSpadLights[LightCount].CookieV = Light.CookieV;
        m_pSpadLights[LightCount].DynamicLightIndex = Index;
        LightCount++;
    }

    // Add fading lights.
    s32 CurrentLink = m_firstLink;
    while( CurrentLink != -1 )
    {
        fading_light& Light = m_fadingLights[CurrentLink];
        m_pSpadLights[LightCount].Pos = Light.Pos;
        m_pSpadLights[LightCount].Radius = Light.Radius;
        m_pSpadLights[LightCount].Intensity = Light.Intensity;
        m_pSpadLights[LightCount].Color = Light.CurrentColor;
        m_pSpadLights[LightCount].Falloff = 1.0f;
        m_pSpadLights[LightCount].Score = ComputeLightScore( Light.CurrentColor, Light.Intensity );
        m_pSpadLights[LightCount].CharOnly = FALSE;
        m_pSpadLights[LightCount].Shape = LIGHT_SHAPE_OMNI;
        m_pSpadLights[LightCount].Direction.Set( 0.0f, 0.0f, 1.0f );
        m_pSpadLights[LightCount].InnerConeCos = 1.0f;
        m_pSpadLights[LightCount].OuterConeCos = 1.0f;
        m_pSpadLights[LightCount].OuterConeSin = 0.0f;
        m_pSpadLights[LightCount].OuterConeTan = 0.0f;
        m_pSpadLights[LightCount].ConeCosRangeInv = 0.0f;
        m_pSpadLights[LightCount].CookieIndex = -1;
        m_pSpadLights[LightCount].CookieU.Set( 1.0f, 0.0f, 0.0f );
        m_pSpadLights[LightCount].CookieV.Set( 0.0f, 1.0f, 0.0f );
        m_pSpadLights[LightCount].DynamicLightIndex = -1;
        LightCount++;

        CurrentLink = Light.NextLink;
    }

    // Non-character geometry only sees lights collected up to this point.
    m_nNonCharLightsInSpad = LightCount;

    // Add character-only lights.
    for( Index = 0; Index < m_nCharLights; Index++ )
    {
        dynamic_light& Light = m_charLights[Index];
        m_pSpadLights[LightCount].Pos = Light.Pos;
        m_pSpadLights[LightCount].Radius = Light.Radius;
        m_pSpadLights[LightCount].Intensity = Light.Intensity;
        m_pSpadLights[LightCount].Color = Light.Color;
        m_pSpadLights[LightCount].Falloff = 1.0f;
        m_pSpadLights[LightCount].Score = ComputeLightScore( Light.Color, Light.Intensity );
        m_pSpadLights[LightCount].CharOnly = TRUE;
        m_pSpadLights[LightCount].Shape = LIGHT_SHAPE_OMNI;
        m_pSpadLights[LightCount].Direction.Set( 0.0f, 0.0f, 1.0f );
        m_pSpadLights[LightCount].InnerConeCos = 1.0f;
        m_pSpadLights[LightCount].OuterConeCos = 1.0f;
        m_pSpadLights[LightCount].OuterConeSin = 0.0f;
        m_pSpadLights[LightCount].OuterConeTan = 0.0f;
        m_pSpadLights[LightCount].ConeCosRangeInv = 0.0f;
        m_pSpadLights[LightCount].CookieIndex = -1;
        m_pSpadLights[LightCount].CookieU.Set( 1.0f, 0.0f, 0.0f );
        m_pSpadLights[LightCount].CookieV.Set( 0.0f, 1.0f, 0.0f );
        m_pSpadLights[LightCount].DynamicLightIndex = -1;
        LightCount++;
    }

    // Higher-scoring lights take precedence during later collection.
    if( LightCount > 1 )
    {
        x_qsort( m_pSpadLights, LightCount, sizeof( spad_light ), SpadLightSortFn );
    }

    m_nSpadLights = LightCount;
    BuildLightBvh();
    m_isInCollection = TRUE;
}

//=========================================================================

void light_mgr::EndLightCollection( void )
{
    ASSERT( m_isInCollection );

    smem_StackPopToMarker();

    m_nSpadLights = 0;
    m_nNonCharLightsInSpad = 0;
    m_pSpadLights = nullptr;
    m_pLightBvhNodes = nullptr;
    m_pLightBvhLightIndices = nullptr;
    m_nLightBvhNodes = 0;
    m_isInCollection = FALSE;
}

//=========================================================================

void light_mgr::ResetAfterException( void )
{
    m_pLightBvhNodes = nullptr;
    m_pLightBvhLightIndices = nullptr;
    m_nLightBvhNodes = 0;
    m_isInCollection = FALSE;
}

//=========================================================================

s32 light_mgr::BuildLightBvhNode( s32 FirstLight, s32 LightCount )
{
    ASSERT( LightCount > 0 );
    ASSERT( m_nLightBvhNodes < MAX_LIGHT_BVH_NODES );

    light_bvh_node& Node = m_pLightBvhNodes[m_nLightBvhNodes++];
    Node.Bounds = bbox( m_pSpadLights[m_pLightBvhLightIndices[FirstLight]].Pos,
                        m_pSpadLights[m_pLightBvhLightIndices[FirstLight]].Radius );
    for( s32 LightOffset = 1; LightOffset < LightCount; LightOffset++ )
    {
        spad_light const& Light = m_pSpadLights[m_pLightBvhLightIndices[FirstLight + LightOffset]];
        Node.Bounds += bbox( Light.Pos, Light.Radius );
    }

    Node.LeftChild = -1;
    Node.RightChild = -1;
    Node.FirstLight = FirstLight;
    Node.LightCount = LightCount;
    if( LightCount <= 8 )
    {
        return m_nLightBvhNodes - 1;
    }

    vector3 const Extent = Node.Bounds.GetSize();
    s32 const Axis = ( Extent.GetX() >= Extent.GetY() && Extent.GetX() >= Extent.GetZ() )
                         ? 0
                         : ( Extent.GetY() >= Extent.GetZ() ) ? 1 : 2;

    // The light count is bounded, so insertion sort avoids comparator context storage.
    for( s32 LightOffset = FirstLight + 1; LightOffset < FirstLight + LightCount; LightOffset++ )
    {
        s32 const LightIndex = m_pLightBvhLightIndices[LightOffset];
        f32 const Coordinate = ( Axis == 0 ) ? m_pSpadLights[LightIndex].Pos.GetX()
                              : ( Axis == 1 ) ? m_pSpadLights[LightIndex].Pos.GetY()
                                            : m_pSpadLights[LightIndex].Pos.GetZ();
        s32 InsertAt = LightOffset;
        while( InsertAt > FirstLight )
        {
            spad_light const& Previous = m_pSpadLights[m_pLightBvhLightIndices[InsertAt - 1]];
            f32 const PreviousCoordinate = ( Axis == 0 ) ? Previous.Pos.GetX()
                                         : ( Axis == 1 ) ? Previous.Pos.GetY() : Previous.Pos.GetZ();
            if( PreviousCoordinate <= Coordinate )
            {
                break;
            }

            m_pLightBvhLightIndices[InsertAt] = m_pLightBvhLightIndices[InsertAt - 1];
            InsertAt--;
        }
        m_pLightBvhLightIndices[InsertAt] = LightIndex;
    }

    s32 const NodeIndex = m_nLightBvhNodes - 1;
    s32 const LeftLightCount = LightCount / 2;
    Node.LeftChild = BuildLightBvhNode( FirstLight, LeftLightCount );
    Node.RightChild = BuildLightBvhNode( FirstLight + LeftLightCount, LightCount - LeftLightCount );
    Node.FirstLight = 0;
    Node.LightCount = 0;
    return NodeIndex;
}

//=========================================================================

void light_mgr::BuildLightBvh( void )
{
    m_nLightBvhNodes = 0;
    m_pLightBvhNodes = nullptr;
    m_pLightBvhLightIndices = nullptr;
    if( m_nSpadLights <= 0 )
    {
        return;
    }

    m_pLightBvhNodes = reinterpret_cast<light_bvh_node*>(
        smem_StackAlloc( sizeof( light_bvh_node ) * MAX_LIGHT_BVH_NODES ) );
    m_pLightBvhLightIndices = reinterpret_cast<s32*>( smem_StackAlloc( sizeof( s32 ) * m_nSpadLights ) );
    ASSERT( m_pLightBvhNodes && m_pLightBvhLightIndices );

    for( s32 LightIndex = 0; LightIndex < m_nSpadLights; LightIndex++ )
    {
        m_pLightBvhLightIndices[LightIndex] = LightIndex;
    }

    BuildLightBvhNode( 0, m_nSpadLights );
}

//=========================================================================

void light_mgr::InsertCollectedSceneLight( s32 LightIndex, s32 MaxLightCount )
{
    f32 const NewScore = m_pSpadLights[LightIndex].Score;
    s32       InsertAt = m_nCollectedLights;

    if( m_nCollectedLights == MaxLightCount )
    {
        if( NewScore <= m_pSpadLights[m_collectedLights[m_nCollectedLights - 1]].Score )
        {
            return;
        }

        InsertAt = m_nCollectedLights - 1;
    }
    else
    {
        m_nCollectedLights++;
    }

    while( InsertAt > 0 )
    {
        if( NewScore <= m_pSpadLights[m_collectedLights[InsertAt - 1]].Score )
        {
            break;
        }

        m_collectedLights[InsertAt] = m_collectedLights[InsertAt - 1];
        InsertAt--;
    }

    m_collectedLights[InsertAt] = LightIndex;
}

//=========================================================================

s32 light_mgr::CollectLights( bbox const& WorldBBox, s32 MaxLightCount )
{
    ASSERT( m_isInCollection );
    ASSERT( m_pSpadLights || ( m_nSpadLights == 0 ) );

    m_nCollectedLights = 0;
    m_collectionStats.SceneQueries++;
    MaxLightCount = MIN( MAX( 0, MaxLightCount ), MAX_COLLECTED_LIGHTS );
    if( ( MaxLightCount <= 0 ) || ( m_nNonCharLightsInSpad <= 0 ) || ( m_nLightBvhNodes <= 0 ) )
    {
        return m_nCollectedLights;
    }

    vector3 const WorldBBoxCenter = WorldBBox.GetCenter();
    f32 const     WorldBBoxRadius = WorldBBox.GetRadius();
    s32           NodeStack[MAX_LIGHT_BVH_NODES];
    s32           StackCount = 0;
    NodeStack[StackCount++] = 0;

    while( StackCount > 0 )
    {
        light_bvh_node const& Node = m_pLightBvhNodes[NodeStack[--StackCount]];
        if( !Node.Bounds.Intersect( WorldBBox ) )
        {
            continue;
        }

        if( Node.LightCount <= 0 )
        {
            ASSERT( ( Node.LeftChild >= 0 ) && ( Node.RightChild >= 0 ) );
            NodeStack[StackCount++] = Node.LeftChild;
            NodeStack[StackCount++] = Node.RightChild;
            continue;
        }

        for( s32 LightOffset = 0; LightOffset < Node.LightCount; LightOffset++ )
        {
            s32 const LightIndex = m_pLightBvhLightIndices[Node.FirstLight + LightOffset];
            spad_light const& Light = m_pSpadLights[LightIndex];
            if( Light.CharOnly )
            {
                continue;
            }

            m_collectionStats.SceneCandidates++;
            xbool Intersects = WorldBBox.Intersect( Light.Pos, Light.Radius );
            if( Intersects && ( Light.Shape == LIGHT_SHAPE_SPOT ) )
            {
                Intersects = SpotLightAffectsSphere( Light.Pos, Light.Direction, Light.OuterConeCos,
                                                     Light.OuterConeSin, Light.OuterConeTan, Light.Radius,
                                                     WorldBBoxCenter, WorldBBoxRadius );
            }

            if( Intersects )
            {
                InsertCollectedSceneLight( LightIndex, MaxLightCount );
                m_collectionStats.SceneHits++;
            }
        }
    }

    return m_nCollectedLights;
}

//=========================================================================

void light_mgr::GetCollectedLight( s32 Index, vector3& Position, f32& Radius, xcolor& Color )
{
    ASSERT( ( Index >= 0 ) && ( Index < m_nCollectedLights ) );
    ASSERT( m_isInCollection );
    ASSERT( m_pSpadLights );

    spad_light& Light = m_pSpadLights[m_collectedLights[Index]];
    Position = Light.Pos;
    Radius = Light.Radius;
    ScaleLightColor( Color, Light.Color, Light.Intensity );
}

//=========================================================================

void light_mgr::GetCollectedLightInfo( s32 Index, vector3& Position, f32& Radius, xcolor& Color, f32& Falloff )
{
    ASSERT( ( Index >= 0 ) && ( Index < m_nCollectedLights ) );
    ASSERT( m_isInCollection );
    ASSERT( m_pSpadLights );

    spad_light& Light = m_pSpadLights[m_collectedLights[Index]];
    Position = Light.Pos;
    Radius = Light.Radius;
    Falloff = Light.Falloff;
    ScaleLightColor( Color, Light.Color, Light.Intensity );
}

//=========================================================================

void light_mgr::GetCollectedLightInfo( s32 Index, vector3& Position, f32& Radius, xcolor& Color,
                                       f32& Falloff, s32& Shape, vector3& Direction,
                                       f32& InnerConeCos, f32& OuterConeCos )
{
    ASSERT( ( Index >= 0 ) && ( Index < m_nCollectedLights ) );
    ASSERT( m_isInCollection );
    ASSERT( m_pSpadLights );

    spad_light& Light = m_pSpadLights[m_collectedLights[Index]];
    Position = Light.Pos;
    Radius = Light.Radius;
    Falloff = Light.Falloff;
    Shape = Light.Shape;
    Direction = Light.Direction;
    InnerConeCos = Light.InnerConeCos;
    OuterConeCos = Light.OuterConeCos;
    ScaleLightColor( Color, Light.Color, Light.Intensity );
}

//=========================================================================

void light_mgr::GetCollectedLightCookie( s32 Index, s32& CookieIndex, vector3& CookieU, vector3& CookieV )
{
    ASSERT( ( Index >= 0 ) && ( Index < m_nCollectedLights ) );
    ASSERT( m_isInCollection );
    ASSERT( m_pSpadLights );

    spad_light& Light = m_pSpadLights[m_collectedLights[Index]];
    CookieIndex = Light.CookieIndex;
    CookieU = Light.CookieU;
    CookieV = Light.CookieV;
}

//=========================================================================

s32 light_mgr::GetCollectedDynamicLightIndex( s32 Index ) const
{
    ASSERT( ( Index >= 0 ) && ( Index < m_nCollectedLights ) );
    ASSERT( m_isInCollection );
    ASSERT( m_pSpadLights );

    return m_pSpadLights[m_collectedLights[Index]].DynamicLightIndex;
}

//=========================================================================

void light_mgr::GetLight( s32 Index, vector3& Position, f32& Radius, xcolor& Color ) const
{
    ASSERT( m_isInCollection );
    ASSERT( ( Index >= 0 ) && ( Index < m_nSpadLights ) );
    ASSERT( m_pSpadLights );

    spad_light& Light = m_pSpadLights[Index];
    Position = Light.Pos;
    Radius = Light.Radius;
    ScaleLightColor( Color, Light.Color, Light.Intensity );
}

//=========================================================================

void light_mgr::InsertCollectedCharLight( dir_light const& Light, s32 MaxLightCount )
{
    ASSERT( MaxLightCount <= MAX_COLLECTED_LIGHTS );

    if( MaxLightCount <= 0 )
    {
        return;
    }

    f32 const NewScore = ComputeLightScore( Light.Col, 1.0f );
    s32       InsertAt = m_nCollectedLights;

    if( m_nCollectedLights == MaxLightCount )
    {
        f32 const LowestScore = ComputeLightScore( m_collectedCharLights[m_nCollectedLights - 1].Col, 1.0f );
        if( NewScore <= LowestScore )
        {
            return;
        }

        InsertAt = m_nCollectedLights - 1;
    }
    else
    {
        m_nCollectedLights++;
    }

    while( InsertAt > 0 )
    {
        f32 const PreviousScore = ComputeLightScore( m_collectedCharLights[InsertAt - 1].Col, 1.0f );
        if( NewScore <= PreviousScore )
        {
            break;
        }

        m_collectedCharLights[InsertAt] = m_collectedCharLights[InsertAt - 1];
        InsertAt--;
    }

    m_collectedCharLights[InsertAt] = Light;
}

//=========================================================================

s32 light_mgr::CollectCharLights( matrix4 const& LocalToWorld, bbox const& LocalBBox, s32 MaxLightCount )
{
    ASSERT( m_isInCollection );

    m_nCollectedLights = 0;
    m_collectionStats.CharQueries++;
    MaxLightCount = MIN( MAX( 0, MaxLightCount ), MAX_COLLECTED_LIGHTS );
    if( MaxLightCount <= 0 )
    {
        return m_nCollectedLights;
    }

    bbox WorldBBox = LocalBBox;
    WorldBBox.Transform( LocalToWorld );
    vector3 const WorldBBoxCenter = LocalToWorld * LocalBBox.GetCenter();
    dir_light     Light;

    if( m_nLightBvhNodes <= 0 )
    {
        return m_nCollectedLights;
    }

    s32 NodeStack[MAX_LIGHT_BVH_NODES];
    s32 StackCount = 0;
    NodeStack[StackCount++] = 0;
    while( StackCount > 0 )
    {
        light_bvh_node const& Node = m_pLightBvhNodes[NodeStack[--StackCount]];
        if( !Node.Bounds.Intersect( WorldBBox ) )
        {
            continue;
        }

        if( Node.LightCount <= 0 )
        {
            NodeStack[StackCount++] = Node.LeftChild;
            NodeStack[StackCount++] = Node.RightChild;
            continue;
        }

        for( s32 LightOffset = 0; LightOffset < Node.LightCount; LightOffset++ )
        {
            spad_light const& Candidate = m_pSpadLights[m_pLightBvhLightIndices[Node.FirstLight + LightOffset]];
            m_collectionStats.CharCandidates++;
            if( CalcDirLight( &Light, LocalToWorld, LocalBBox, WorldBBox, WorldBBoxCenter,
                              Candidate.Pos, Candidate.Radius, Candidate.Intensity, Candidate.Color,
                              Candidate.Shape, Candidate.Direction, Candidate.OuterConeCos,
                              Candidate.ConeCosRangeInv ) )
            {
                InsertCollectedCharLight( Light, MaxLightCount );
                m_collectionStats.CharHits++;
            }
        }
    }

    return m_nCollectedLights;
}

//=========================================================================

s32 light_mgr::CollectCharLightsOnly( matrix4 const& LocalToWorld, bbox const& LocalBBox, s32 MaxLightCount )
{
    ASSERT( m_isInCollection );

    m_nCollectedLights = 0;
    m_collectionStats.CharQueries++;
    MaxLightCount = MIN( MAX( 0, MaxLightCount ), MAX_COLLECTED_LIGHTS );
    if( MaxLightCount <= 0 )
    {
        return m_nCollectedLights;
    }

    bbox WorldBBox = LocalBBox;
    WorldBBox.Transform( LocalToWorld );
    vector3 const WorldBBoxCenter = LocalToWorld * LocalBBox.GetCenter();
    dir_light     Light;

    if( m_nLightBvhNodes <= 0 )
    {
        return m_nCollectedLights;
    }

    s32 NodeStack[MAX_LIGHT_BVH_NODES];
    s32 StackCount = 0;
    NodeStack[StackCount++] = 0;
    while( StackCount > 0 )
    {
        light_bvh_node const& Node = m_pLightBvhNodes[NodeStack[--StackCount]];
        if( !Node.Bounds.Intersect( WorldBBox ) )
        {
            continue;
        }

        if( Node.LightCount <= 0 )
        {
            NodeStack[StackCount++] = Node.LeftChild;
            NodeStack[StackCount++] = Node.RightChild;
            continue;
        }

        for( s32 LightOffset = 0; LightOffset < Node.LightCount; LightOffset++ )
        {
            spad_light const& Candidate = m_pSpadLights[m_pLightBvhLightIndices[Node.FirstLight + LightOffset]];
            // Fading lights apply to characters; regular dynamic lights do not here.
            if( !Candidate.CharOnly && ( Candidate.DynamicLightIndex >= 0 ) )
            {
                continue;
            }

            m_collectionStats.CharCandidates++;
            if( CalcDirLight( &Light, LocalToWorld, LocalBBox, WorldBBox, WorldBBoxCenter,
                              Candidate.Pos, Candidate.Radius, Candidate.Intensity, Candidate.Color,
                              Candidate.Shape, Candidate.Direction, Candidate.OuterConeCos,
                              Candidate.ConeCosRangeInv ) )
            {
                InsertCollectedCharLight( Light, MaxLightCount );
                m_collectionStats.CharHits++;
            }
        }
    }

    return m_nCollectedLights;
}

//=========================================================================

void light_mgr::GetCollectedCharLight( s32 Index, vector3& Direction, xcolor& Color )
{
    ASSERT( ( Index >= 0 ) && ( Index < m_nCollectedLights ) );
    Direction = m_collectedCharLights[Index].Dir;
    Color = m_collectedCharLights[Index].Col;
}