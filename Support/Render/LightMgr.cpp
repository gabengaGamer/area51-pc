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
// GLOBAL INSTANCE
//=========================================================================

light_mgr g_LightMgr;

//=========================================================================
// FUNCTIONS
//=========================================================================

static f32 ComputeLightScore( xcolor const& color, f32 intensity )
{
    f32 const r = static_cast<f32>( color.R ) * intensity;
    f32 const g = static_cast<f32>( color.G ) * intensity;
    f32 const b = static_cast<f32>( color.B ) * intensity;

    return r * r + g * g + b * b;
}

static void ScaleLightColor( xcolor& dst, xcolor const& src, f32 intensity )
{
    dst.R = static_cast<u8>( MIN( 255.0f, static_cast<f32>( src.R ) * intensity ) );
    dst.G = static_cast<u8>( MIN( 255.0f, static_cast<f32>( src.G ) * intensity ) );
    dst.B = static_cast<u8>( MIN( 255.0f, static_cast<f32>( src.B ) * intensity ) );
    dst.A = static_cast<u8>( MIN( 255.0f, static_cast<f32>( src.A ) * intensity ) );
}

static f32 ComputeSpotConeAttenuation( vector3 const& lightPos, vector3 const& lightDirection, f32 outerConeCos,
                                       f32 coneCosRangeInv, vector3 const& targetPos )
{
    vector3 toTarget = targetPos - lightPos;
    if ( !toTarget.SafeNormalize() )
    {
        return 1.0f;
    }

    f32 const spotCos = lightDirection.Dot( toTarget );

    return MINMAX( 0.0f, ( spotCos - outerConeCos ) * coneCosRangeInv, 1.0f );
}

static xbool SphereIntersectsFiniteCone( vector3 const& sphereCenter, f32 sphereRadius, vector3 const& coneApex,
                                         vector3 const& coneAxis, f32 coneOuterCos, f32 coneOuterSin, f32 coneOuterTan,
                                         f32 coneLength )
{
    ASSERT( coneOuterCos > 0.0f );

    vector3 const toCenter = sphereCenter - coneApex;
    f32 const     centerDistSq = toCenter.Dot( toCenter );
    f32 const     axialDistance = coneAxis.Dot( toCenter );
    f32 const     sphereRadiusSq = sphereRadius * sphereRadius;

    if ( axialDistance < -sphereRadius )
    {
        return FALSE;
    }

    if ( axialDistance > ( coneLength + sphereRadius ) )
    {
        return FALSE;
    }

    if ( axialDistance <= 0.0f )
    {
        return ( centerDistSq <= sphereRadiusSq );
    }

    f32 const radialDistanceSq = MAX( centerDistSq - ( axialDistance * axialDistance ), 0.0f );
    f32 const radialDistance = x_sqrt( radialDistanceSq );

    if ( axialDistance < coneLength )
    {
        f32 const sideDistance = ( radialDistance * coneOuterCos ) - ( axialDistance * coneOuterSin );
        if ( sideDistance <= sphereRadius )
        {
            return TRUE;
        }
    }

    f32 const coneCapRadius = coneLength * coneOuterTan;
    f32 const capAxialDelta = MAX( axialDistance - coneLength, 0.0f );
    f32 const capRadialDelta = MAX( radialDistance - coneCapRadius, 0.0f );
    return ( ( capAxialDelta * capAxialDelta ) + ( capRadialDelta * capRadialDelta ) ) <= sphereRadiusSq;
}

static xbool SpotLightAffectsSphere( vector3 const& lightPos, vector3 const& lightDirection, f32 outerConeCos,
                                     f32 outerConeSin, f32 outerConeTan, f32 lightRadius, vector3 const& sphereCenter,
                                     f32 sphereRadius )
{
    if ( outerConeCos <= 0.0f )
    {
        return TRUE;
    }

    return SphereIntersectsFiniteCone( sphereCenter, sphereRadius, lightPos, lightDirection, outerConeCos, outerConeSin,
                                       outerConeTan, lightRadius );
}

static void SetupLightCone( f32 innerAngle, f32 outerAngle, f32& innerConeCos, f32& outerConeCos, f32& outerConeSin,
                            f32& outerConeTan, f32& coneCosRangeInv )
{
    innerConeCos = x_cos( DEG_TO_RAD( innerAngle ) * 0.5f );
    outerConeCos = x_cos( DEG_TO_RAD( outerAngle ) * 0.5f );

    f32 const coneCosRange = MAX( innerConeCos - outerConeCos, 0.0001f );
    coneCosRangeInv = 1.0f / coneCosRange;

    if ( outerConeCos > 0.0f )
    {
        outerConeSin = x_sqrt( MAX( 1.0f - ( outerConeCos * outerConeCos ), 0.0f ) );
        outerConeTan = outerConeSin / outerConeCos;
    }
    else
    {
        outerConeSin = 0.0f;
        outerConeTan = 0.0f;
    }
}

static void SetupOmniLightCone( f32& innerConeCos, f32& outerConeCos, f32& outerConeSin, f32& outerConeTan,
                                f32& coneCosRangeInv )
{
    innerConeCos = 1.0f;
    outerConeCos = 1.0f;
    outerConeSin = 0.0f;
    outerConeTan = 0.0f;
    coneCosRangeInv = 0.0f;
}

light_mgr::light_mgr( void )
    : m_firstLink( -1 ), m_nFadingLights( 0 ), m_nDynamicLights( 0 ), m_nCharLights( 0 ), m_isInCollection( FALSE ),
      m_nSpadLights( 0 ), m_nNonCharLightsInSpad( 0 ), m_pSpadLights( NULL ), m_nCollectedLights( 0 )

{
    x_memset( m_fadingLights, 0, sizeof( fading_light ) * MAX_FADING_LIGHTS );
    x_memset( &m_collectionStats, 0, sizeof( m_collectionStats ) );
}

//=========================================================================

light_mgr::~light_mgr( void )
{
}

//=========================================================================

void light_mgr::AddFadingLight( vector3 const& pos, xcolor const& c, f32 radius, f32 intensity, f32 fadeTime )
{
    // early return
    if ( fadeTime <= 0.0f )
    {
        return;
    }

    // add a light to the linked list of lights
    s32 iLight = AddLight();

    // now we can fill in the light data
    m_fadingLights[iLight].Pos = pos;
    m_fadingLights[iLight].Radius = radius;
    m_fadingLights[iLight].StartColor = c;
    m_fadingLights[iLight].CurrentColor = c;
    m_fadingLights[iLight].FadeTime = fadeTime;
    m_fadingLights[iLight].ElapsedTime = 0.0f;
    m_fadingLights[iLight].Valid = TRUE;
    m_fadingLights[iLight].InterpolationT = 0.0f;
    m_fadingLights[iLight].Intensity = intensity;
}

//=========================================================================

s32 light_mgr::RegisterSpotLightCookie( texture::handle const& cookie )
{
    texture* pTexture = cookie.GetPointer();
    if ( !pTexture )
    {
        return -1;
    }

    for ( s32 i = 0; i < m_lightCookieFaces.GetCount(); i++ )
    {
        if ( m_lightCookieFaces[i].GetPointer() == pTexture )
        {
            return i;
        }
    }

    m_lightCookieFaces.Append() = cookie;
    return m_lightCookieFaces.GetCount() - 1;
}

//=========================================================================

static void BuildLightCookieUV( vector3 const& direction, vector3& cookieU, vector3& cookieV )
{
    vector3 forward = direction;
    if ( !forward.SafeNormalize() )
    {
        forward.Set( 0.0f, 0.0f, 1.0f );
    }

    cookieU = vector3( 0.0f, 1.0f, 0.0f ).Cross( forward );
    if ( !cookieU.SafeNormalize() )
    {
        cookieU = vector3( 1.0f, 0.0f, 0.0f ).Cross( forward );
        if ( !cookieU.SafeNormalize() )
        {
            cookieU.Set( 1.0f, 0.0f, 0.0f );
        }
    }

    cookieV = forward.Cross( cookieU );
    if ( !cookieV.SafeNormalize() )
    {
        cookieV.Set( 0.0f, 1.0f, 0.0f );
    }
}

//=========================================================================

void light_mgr::AddDynamicLight( vector3 const& pos, xcolor const& c, f32 radius, f32 intensity, xbool charOnly,
                                 s32 shape, xbool castShadows, f32 innerRadius, vector3 const& direction, f32 falloff,
                                 f32 innerAngle, f32 outerAngle, s32 shadowMapResolution, s32 shadowPriority,
                                 texture::handle const& cookie )
{
    vector3 lightDirection = direction;

    if ( shape != LIGHT_SHAPE_SPOT )
    {
        shape = LIGHT_SHAPE_OMNI;
    }

    innerRadius = MAX( 0.0f, MIN( innerRadius, radius ) );
    falloff = MINMAX( 0.0f, falloff, 1.0f );
    if ( ( radius > 0.0001f ) && ( innerRadius > 0.0f ) )
    {
        falloff = MINMAX( 0.0f, 1.0f - ( innerRadius / radius ), 1.0f );
    }
    innerAngle = MAX( 0.0f, innerAngle );
    outerAngle = MAX( innerAngle, outerAngle );
    if ( !lightDirection.SafeNormalize() )
    {
        lightDirection.Set( 0.0f, 0.0f, 1.0f );
    }

    if ( charOnly )
    {
        if ( m_nCharLights >= MAX_CHAR_LIGHTS )
        {
            // ASSERT( FALSE );
            return;
        }
        x_memset( &m_charLights[m_nCharLights], 0, sizeof( dynamic_light ) );
        m_charLights[m_nCharLights].Pos = pos;
        m_charLights[m_nCharLights].Radius = radius;
        m_charLights[m_nCharLights].Intensity = intensity;
        m_charLights[m_nCharLights].Color = c;
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
        if ( m_nDynamicLights >= MAX_DYNAMIC_LIGHTS )
        {
            // ASSERT( FALSE );
            return;
        }

        f32 innerConeCos;
        f32 outerConeCos;
        f32 outerConeSin;
        f32 outerConeTan;
        f32 coneCosRangeInv;
        if ( shape == LIGHT_SHAPE_SPOT )
        {
            SetupLightCone( innerAngle, outerAngle, innerConeCos, outerConeCos, outerConeSin, outerConeTan,
                            coneCosRangeInv );
        }
        else
        {
            SetupOmniLightCone( innerConeCos, outerConeCos, outerConeSin, outerConeTan, coneCosRangeInv );
        }

        vector3   cookieU( 1.0f, 0.0f, 0.0f );
        vector3   cookieV( 0.0f, 1.0f, 0.0f );
        s32 const cookieIndex = ( shape == LIGHT_SHAPE_SPOT ) ? RegisterSpotLightCookie( cookie ) : -1;
        if ( cookieIndex >= 0 )
        {
            BuildLightCookieUV( lightDirection, cookieU, cookieV );
        }

        m_dynamicLights[m_nDynamicLights].Pos = pos;
        m_dynamicLights[m_nDynamicLights].Radius = radius;
        m_dynamicLights[m_nDynamicLights].Intensity = intensity;
        m_dynamicLights[m_nDynamicLights].Color = c;
        m_dynamicLights[m_nDynamicLights].Direction = lightDirection;
        m_dynamicLights[m_nDynamicLights].Falloff = falloff;
        m_dynamicLights[m_nDynamicLights].InnerAngle = innerAngle;
        m_dynamicLights[m_nDynamicLights].OuterAngle = outerAngle;
        m_dynamicLights[m_nDynamicLights].InnerConeCos = innerConeCos;
        m_dynamicLights[m_nDynamicLights].OuterConeCos = outerConeCos;
        m_dynamicLights[m_nDynamicLights].OuterConeSin = outerConeSin;
        m_dynamicLights[m_nDynamicLights].OuterConeTan = outerConeTan;
        m_dynamicLights[m_nDynamicLights].ConeCosRangeInv = coneCosRangeInv;
        m_dynamicLights[m_nDynamicLights].Shape = shape;
        m_dynamicLights[m_nDynamicLights].ShadowMapResolution = shadowMapResolution;
        m_dynamicLights[m_nDynamicLights].ShadowPriority = shadowPriority;
        m_dynamicLights[m_nDynamicLights].CastShadows = castShadows;
        m_dynamicLights[m_nDynamicLights].CookieIndex = cookieIndex;
        m_dynamicLights[m_nDynamicLights].CookieU = cookieU;
        m_dynamicLights[m_nDynamicLights].CookieV = cookieV;
        m_nDynamicLights++;
    }
}

//=========================================================================

void light_mgr::OnUpdate( f32 deltaTime )
{
    for ( s32 iLight = 0; iLight < MAX_FADING_LIGHTS; iLight++ )
    {
        if ( m_fadingLights[iLight].Valid )
        {
            m_fadingLights[iLight].ElapsedTime += deltaTime;

            if ( m_fadingLights[iLight].ElapsedTime >= m_fadingLights[iLight].FadeTime )
            {
                RemoveLight( iLight );
            }
            else
            {
                f32 t = m_fadingLights[iLight].ElapsedTime / m_fadingLights[iLight].FadeTime;
                f32 r = static_cast<f32>( m_fadingLights[iLight].StartColor.R );
                f32 g = static_cast<f32>( m_fadingLights[iLight].StartColor.G );
                f32 b = static_cast<f32>( m_fadingLights[iLight].StartColor.B );

                m_fadingLights[iLight].InterpolationT = t;
                m_fadingLights[iLight].CurrentColor.R = static_cast<u8>( r * ( 1.0f - t ) );
                m_fadingLights[iLight].CurrentColor.G = static_cast<u8>( g * ( 1.0f - t ) );
                m_fadingLights[iLight].CurrentColor.B = static_cast<u8>( b * ( 1.0f - t ) );
            }
        }
    }
}

//=========================================================================

s32 light_mgr::AddLight( void )
{
    // find the first available light, remembering the light with the least
    // amount of time left in case we need to kick one out.
    s32 iLight;
    f32 weakestI = F32_MAX;
    s32 lightToReplace = -1;

    for ( iLight = 0; iLight < MAX_FADING_LIGHTS; iLight++ )
    {
        if ( !m_fadingLights[iLight].Valid )
        {
            break;
        }

        f32 i = ComputeLightScore( m_fadingLights[iLight].CurrentColor, m_fadingLights[iLight].Intensity );

        if ( ( lightToReplace == -1 ) || ( i < weakestI ) )
        {
            lightToReplace = iLight;
            weakestI = i;
        }
    }

    // was there room in the array?
    if ( iLight == MAX_FADING_LIGHTS )
    {
        // replace an old light, the old one was already in the linked list,
        // so no need to fix up any links

        // NOTE: We should rarely hit this, but if we do and it starts to kick out
        // any vital lights, we could start merging lights together (sounds like fun!)
        ASSERT( lightToReplace != -1 );
        iLight = lightToReplace;
    }
    else
    {
        // #### TODO: If this linked-list business starts hitting the cache pretty
        //  hard, insert into the middle of the list so that we at least move
        //  forward through the cache.

        // add the new light to the start of the linked list
        if ( m_firstLink != -1 )
        {
            m_fadingLights[m_firstLink].PrevLink = iLight;
        }

        m_fadingLights[iLight].NextLink = m_firstLink;
        m_fadingLights[iLight].PrevLink = -1;
        m_firstLink = iLight;
        m_nFadingLights++;
    }

    ASSERT( ( iLight >= 0 ) && ( iLight < MAX_FADING_LIGHTS ) );
    return iLight;
}

//=========================================================================

void light_mgr::RemoveLight( s32 lightIndex )
{
    ASSERT( ( lightIndex >= 0 ) && ( lightIndex < MAX_FADING_LIGHTS ) );
    ASSERT( m_fadingLights[lightIndex].Valid );

    // invalidate the light
    m_fadingLights[lightIndex].Valid = FALSE;

    // patch up the linked list indices
    s32 prev = m_fadingLights[lightIndex].PrevLink;
    s32 next = m_fadingLights[lightIndex].NextLink;
    if ( prev != -1 )
    {
        m_fadingLights[prev].NextLink = next;
    }
    if ( next != -1 )
    {
        m_fadingLights[next].PrevLink = prev;
    }
    if ( lightIndex == m_firstLink )
    {
        ASSERT( prev == -1 );
        m_firstLink = next;
    }

    m_nFadingLights--;
}

//=========================================================================

void light_mgr::InsertCollectedCharLight( dir_light const& light, s32 maxLightCount )
{
    ASSERT( maxLightCount <= MAX_COLLECTED_LIGHTS );

    if ( maxLightCount <= 0 )
    {
        return;
    }

    f32 const newScore = ComputeLightScore( light.Col, 1.0f );
    s32       insertAt = m_nCollectedLights;

    if ( m_nCollectedLights == maxLightCount )
    {
        f32 const lowestScore = ComputeLightScore( m_collectedCharLights[m_nCollectedLights - 1].Col, 1.0f );
        if ( newScore <= lowestScore )
        {
            return;
        }

        insertAt = m_nCollectedLights - 1;
    }
    else
    {
        m_nCollectedLights++;
    }

    while ( insertAt > 0 )
    {
        f32 const prevScore = ComputeLightScore( m_collectedCharLights[insertAt - 1].Col, 1.0f );
        if ( newScore <= prevScore )
        {
            break;
        }

        m_collectedCharLights[insertAt] = m_collectedCharLights[insertAt - 1];
        insertAt--;
    }

    m_collectedCharLights[insertAt] = light;
}

//=========================================================================

s32 SpadLightSortFn( void const* pA, void const* pB )
{
    light_mgr::spad_light const* pLightA = static_cast<light_mgr::spad_light const*>( pA );
    light_mgr::spad_light const* pLightB = static_cast<light_mgr::spad_light const*>( pB );

    if ( pLightA->CharOnly > pLightB->CharOnly )
    {
        return 1;
    }
    if ( pLightA->CharOnly < pLightB->CharOnly )
    {
        return -1;
    }
    if ( pLightA->Score > pLightB->Score )
    {
        return -1;
    }
    if ( pLightA->Score < pLightB->Score )
    {
        return 1;
    }

    return 0;
}

//=========================================================================

void light_mgr::BeginLightCollection( void )
{
    ASSERT( !m_isInCollection );

    x_memset( &m_collectionStats, 0, sizeof( m_collectionStats ) );

    // Allocate space for the spad lights.
    s32 nLights = m_nFadingLights + m_nDynamicLights + m_nCharLights;
    smem_StackPushMarker();
    if ( nLights > 0 )
    {
        m_pSpadLights = reinterpret_cast<spad_light*>( smem_StackAlloc( sizeof( spad_light ) * nLights ) );
    }
    else
    {
        m_pSpadLights = NULL;
    }
    ASSERT( ( nLights == 0 ) || m_pSpadLights );

    // Add the optimized lights to scratchpad
    nLights = 0;

    // add the dynamic lights
    s32 i;
    for ( i = 0; i < m_nDynamicLights; i++ )
    {
        dynamic_light& light = m_dynamicLights[i];
        m_pSpadLights[nLights].Pos = light.Pos;
        m_pSpadLights[nLights].Radius = light.Radius;
        m_pSpadLights[nLights].Intensity = light.Intensity;
        m_pSpadLights[nLights].Color = light.Color;
        m_pSpadLights[nLights].Falloff = light.Falloff;
        m_pSpadLights[nLights].Score = ComputeLightScore( light.Color, light.Intensity );
        m_pSpadLights[nLights].CharOnly = FALSE;
        m_pSpadLights[nLights].Shape = light.Shape;
        m_pSpadLights[nLights].Direction = light.Direction;
        m_pSpadLights[nLights].InnerConeCos = light.InnerConeCos;
        m_pSpadLights[nLights].OuterConeCos = light.OuterConeCos;
        m_pSpadLights[nLights].OuterConeSin = light.OuterConeSin;
        m_pSpadLights[nLights].OuterConeTan = light.OuterConeTan;
        m_pSpadLights[nLights].ConeCosRangeInv = light.ConeCosRangeInv;
        m_pSpadLights[nLights].CookieIndex = light.CookieIndex;
        m_pSpadLights[nLights].CookieU = light.CookieU;
        m_pSpadLights[nLights].CookieV = light.CookieV;
        m_pSpadLights[nLights].DynamicLightIndex = i;
        nLights++;
    }

    // add the fading lights
    s32 currLink = m_firstLink;
    while ( currLink != -1 )
    {
        fading_light& light = m_fadingLights[currLink];
        m_pSpadLights[nLights].Pos = light.Pos;
        m_pSpadLights[nLights].Radius = light.Radius;
        m_pSpadLights[nLights].Intensity = light.Intensity;
        m_pSpadLights[nLights].Color = light.CurrentColor;
        m_pSpadLights[nLights].Falloff = 1.0f;
        m_pSpadLights[nLights].Score = ComputeLightScore( light.CurrentColor, light.Intensity );
        m_pSpadLights[nLights].CharOnly = FALSE;
        m_pSpadLights[nLights].Shape = LIGHT_SHAPE_OMNI;
        m_pSpadLights[nLights].Direction.Set( 0.0f, 0.0f, 1.0f );
        m_pSpadLights[nLights].InnerConeCos = 1.0f;
        m_pSpadLights[nLights].OuterConeCos = 1.0f;
        m_pSpadLights[nLights].OuterConeSin = 0.0f;
        m_pSpadLights[nLights].OuterConeTan = 0.0f;
        m_pSpadLights[nLights].ConeCosRangeInv = 0.0f;
        m_pSpadLights[nLights].CookieIndex = -1;
        m_pSpadLights[nLights].CookieU.Set( 1.0f, 0.0f, 0.0f );
        m_pSpadLights[nLights].CookieV.Set( 0.0f, 1.0f, 0.0f );
        m_pSpadLights[nLights].DynamicLightIndex = -1;
        nLights++;

        currLink = light.NextLink;
    }

    // We've collected everything but the character lights now. These are the
    // lights that will be used for all non-skinned geometry.
    m_nNonCharLightsInSpad = nLights;

    // add the character lights
    for ( i = 0; i < m_nCharLights; i++ )
    {
        dynamic_light& light = m_charLights[i];
        m_pSpadLights[nLights].Pos = light.Pos;
        m_pSpadLights[nLights].Radius = light.Radius;
        m_pSpadLights[nLights].Intensity = light.Intensity;
        m_pSpadLights[nLights].Color = light.Color;
        m_pSpadLights[nLights].Falloff = 1.0f;
        m_pSpadLights[nLights].Score = ComputeLightScore( light.Color, light.Intensity );
        m_pSpadLights[nLights].CharOnly = TRUE;
        m_pSpadLights[nLights].Shape = LIGHT_SHAPE_OMNI;
        m_pSpadLights[nLights].Direction.Set( 0.0f, 0.0f, 1.0f );
        m_pSpadLights[nLights].InnerConeCos = 1.0f;
        m_pSpadLights[nLights].OuterConeCos = 1.0f;
        m_pSpadLights[nLights].OuterConeSin = 0.0f;
        m_pSpadLights[nLights].OuterConeTan = 0.0f;
        m_pSpadLights[nLights].ConeCosRangeInv = 0.0f;
        m_pSpadLights[nLights].CookieIndex = -1;
        m_pSpadLights[nLights].CookieU.Set( 1.0f, 0.0f, 0.0f );
        m_pSpadLights[nLights].CookieV.Set( 0.0f, 1.0f, 0.0f );
        m_pSpadLights[nLights].DynamicLightIndex = -1;
        nLights++;
    }

    // sort the lights based on their score...this will mean that lights
    // with a higher intensity and color will get precedence when it comes
    // time to whittle them down to a nice number for the hardware
    if ( nLights > 1 )
    {
        x_qsort( m_pSpadLights, nLights, sizeof( spad_light ), SpadLightSortFn );
    }

    m_nSpadLights = nLights;
    m_isInCollection = TRUE;
}

//=========================================================================

void light_mgr::EndLightCollection( void )
{
    ASSERT( m_isInCollection );

    // free up the spad lights
    smem_StackPopToMarker();

    m_nSpadLights = 0;
    m_nNonCharLightsInSpad = 0;
    m_pSpadLights = NULL;
    m_isInCollection = FALSE;
}

//=========================================================================

void light_mgr::ResetAfterException( void )
{
    m_isInCollection = FALSE;
}

//=========================================================================

s32 light_mgr::CollectLights( bbox const& worldBBox, s32 maxLightCount )
{
    ASSERT( m_isInCollection );
    ASSERT( m_pSpadLights || ( m_nSpadLights == 0 ) );

    s32 i;
    m_nCollectedLights = 0;
    m_collectionStats.SceneQueries++;
    maxLightCount = MIN( MAX( 0, maxLightCount ), MAX_COLLECTED_LIGHTS );
    if ( ( maxLightCount <= 0 ) || ( m_nNonCharLightsInSpad <= 0 ) )
    {
        return m_nCollectedLights;
    }

    vector3 const worldBBoxCenter = worldBBox.GetCenter();
    f32 const     worldBBoxRadius = worldBBox.GetRadius();

    for ( i = 0; ( i < m_nNonCharLightsInSpad ) && ( m_nCollectedLights < maxLightCount ); i++ )
    {
        m_collectionStats.SceneCandidates++;
        ASSERT( !m_pSpadLights[i].CharOnly );

        xbool bIntersects;

        bIntersects = worldBBox.Intersect( m_pSpadLights[i].Pos, m_pSpadLights[i].Radius );
        if ( bIntersects && ( m_pSpadLights[i].Shape == LIGHT_SHAPE_SPOT ) )
        {
            bIntersects =
                SpotLightAffectsSphere( m_pSpadLights[i].Pos, m_pSpadLights[i].Direction, m_pSpadLights[i].OuterConeCos,
                                        m_pSpadLights[i].OuterConeSin, m_pSpadLights[i].OuterConeTan,
                                        m_pSpadLights[i].Radius, worldBBoxCenter, worldBBoxRadius );
        }

        if ( bIntersects )
        {
            m_collectedLights[m_nCollectedLights++] = i;
            m_collectionStats.SceneHits++;
        }
    }

    return m_nCollectedLights;
}

//=========================================================================

void light_mgr::GetCollectedLight( s32 index, vector3& pos, f32& radius, xcolor& c )
{
    ASSERT( ( index >= 0 ) && ( index < m_nCollectedLights ) );
    ASSERT( m_isInCollection );
    ASSERT( m_pSpadLights );

    spad_light& light = m_pSpadLights[m_collectedLights[index]];
    pos = light.Pos;
    radius = light.Radius;
    ScaleLightColor( c, light.Color, light.Intensity );
}

//=========================================================================

void light_mgr::GetCollectedLightInfo( s32 index, vector3& pos, f32& radius, xcolor& c, f32& falloff )
{
    ASSERT( ( index >= 0 ) && ( index < m_nCollectedLights ) );
    ASSERT( m_isInCollection );
    ASSERT( m_pSpadLights );

    spad_light& light = m_pSpadLights[m_collectedLights[index]];
    pos = light.Pos;
    radius = light.Radius;
    falloff = light.Falloff;
    ScaleLightColor( c, light.Color, light.Intensity );
}

//=========================================================================

void light_mgr::GetCollectedLightInfo( s32 index, vector3& pos, f32& radius, xcolor& c, f32& falloff, s32& shape,
                                       vector3& direction, f32& innerConeCos, f32& outerConeCos )
{
    ASSERT( ( index >= 0 ) && ( index < m_nCollectedLights ) );
    ASSERT( m_isInCollection );
    ASSERT( m_pSpadLights );

    spad_light& light = m_pSpadLights[m_collectedLights[index]];
    pos = light.Pos;
    radius = light.Radius;
    falloff = light.Falloff;
    shape = light.Shape;
    direction = light.Direction;
    innerConeCos = light.InnerConeCos;
    outerConeCos = light.OuterConeCos;
    ScaleLightColor( c, light.Color, light.Intensity );
}

//=========================================================================

void light_mgr::GetCollectedLightCookie( s32 index, s32& cookieIndex, vector3& cookieU, vector3& cookieV )
{
    ASSERT( ( index >= 0 ) && ( index < m_nCollectedLights ) );
    ASSERT( m_isInCollection );
    ASSERT( m_pSpadLights );

    spad_light& light = m_pSpadLights[m_collectedLights[index]];
    cookieIndex = light.CookieIndex;
    cookieU = light.CookieU;
    cookieV = light.CookieV;
}

//=========================================================================

s32 light_mgr::GetCollectedDynamicLightIndex( s32 index ) const
{
    ASSERT( ( index >= 0 ) && ( index < m_nCollectedLights ) );
    ASSERT( m_isInCollection );
    ASSERT( m_pSpadLights );

    return m_pSpadLights[m_collectedLights[index]].DynamicLightIndex;
}

//=========================================================================

xbool light_mgr::CalcDirLight( dir_light* pDst, matrix4 const& l2W, bbox const& b, bbox const& worldBox,
                               vector3 const& worldBoxCenter, vector3 const& pos, f32 radius, f32 intensity, xcolor& c,
                               s32 shape, vector3 const& direction, f32 outerConeCos, f32 coneCosRangeInv )
{
    if ( worldBox.Intersect( pos, radius ) )
    {
        //
        // convert the point light into a directional light
        //

        // to make things easier and avoid having the bounding box grow to an
        // inaccurate shape, move the light into the bbox's local space.
        vector3 temp = pos - l2W.GetTranslation();
        vector3 lPos( temp.GetX() * l2W( 0, 0 ) + temp.GetY() * l2W( 0, 1 ) + temp.GetZ() * l2W( 0, 2 ),
                      temp.GetX() * l2W( 1, 0 ) + temp.GetY() * l2W( 1, 1 ) + temp.GetZ() * l2W( 1, 2 ),
                      temp.GetX() * l2W( 2, 0 ) + temp.GetY() * l2W( 2, 1 ) + temp.GetZ() * l2W( 2, 2 ) );

        // find the distance from the light to the bounding box, and use that to adjust
        // the directional intensity
        f32 ftemp;
        f32 dist = 0.0f;
        if ( lPos.GetX() > b.Max.GetX() )
        {
            ftemp = lPos.GetX() - b.Max.GetX();
            dist += ftemp * ftemp;
        }

        if ( lPos.GetX() < b.Min.GetX() )
        {
            ftemp = b.Min.GetX() - lPos.GetX();
            dist += ftemp * ftemp;
        }

        if ( lPos.GetY() > b.Max.GetY() )
        {
            ftemp = lPos.GetY() - b.Max.GetY();
            dist += ftemp * ftemp;
        }

        if ( lPos.GetY() < b.Min.GetY() )
        {
            ftemp = b.Min.GetY() - lPos.GetY();
            dist += ftemp * ftemp;
        }

        if ( lPos.GetZ() > b.Max.GetZ() )
        {
            ftemp = lPos.GetZ() - b.Max.GetZ();
            dist += ftemp * ftemp;
        }

        if ( lPos.GetZ() < b.Min.GetZ() )
        {
            ftemp = b.Min.GetZ() - lPos.GetZ();
            dist += ftemp * ftemp;
        }

        // now we can calculate the lights intensity
        f32 radiusSqr = radius * radius;

        if ( radiusSqr == 0.0f )
        {
            radiusSqr = 1.0f;
        }

        f32 i = 1.0f - dist / radiusSqr;
        i = MAX( i, 0.0f );
        i *= intensity;

        if ( i > 0.0f )
        {
            // and the direction of the light should just be based on the
            // bounding box's center point
            if ( shape == LIGHT_SHAPE_SPOT )
            {
                i *= ComputeSpotConeAttenuation( pos, direction, outerConeCos, coneCosRangeInv, worldBoxCenter );
            }

            if ( i <= 0.0f )
            {
                return FALSE;
            }

            vector3 lDir = worldBoxCenter - pos;
            if ( !lDir.SafeNormalize() )
            {
                // this can happen if the light happens to be placed at the center of
                // the object (odd case but still possible)
                lDir.Set( 0.7071f, -0.7071f, 0.0f );
            }

            // now we can set up the light
            pDst->Col.R = static_cast<u8>( MIN( i * static_cast<f32>( c.R ), 255.0f ) );
            pDst->Col.G = static_cast<u8>( MIN( i * static_cast<f32>( c.G ), 255.0f ) );
            pDst->Col.B = static_cast<u8>( MIN( i * static_cast<f32>( c.B ), 255.0f ) );
            pDst->Col.A = 0;
            pDst->Dir = lDir;

            return TRUE;
        }
    }

    return FALSE;
}

//=========================================================================

s32 light_mgr::CollectCharLights( matrix4 const& l2W, bbox const& b, s32 maxLightCount )
{
    m_nCollectedLights = 0;
    m_collectionStats.CharQueries++;
    maxLightCount = MIN( MAX( 0, maxLightCount ), MAX_COLLECTED_LIGHTS );
    if ( maxLightCount <= 0 )
    {
        return m_nCollectedLights;
    }

    bbox worldBox = b;
    worldBox.Transform( l2W );
    vector3 const worldBoxCenter = l2W * b.GetCenter();
    dir_light     light;

    // walk the list collecting fading lights that may intersect
    s32 currLink = m_firstLink;
    while ( currLink != -1 )
    {
        m_collectionStats.CharCandidates++;
        if ( CalcDirLight( &light, l2W, b, worldBox, worldBoxCenter, m_fadingLights[currLink].Pos,
                           m_fadingLights[currLink].Radius, m_fadingLights[currLink].Intensity,
                           m_fadingLights[currLink].CurrentColor ) )
        {
            InsertCollectedCharLight( light, maxLightCount );
            m_collectionStats.CharHits++;
        }
        currLink = m_fadingLights[currLink].NextLink;
    }

    // now go through all of the normal character lights
    for ( s32 iCharLight = 0; iCharLight < m_nCharLights; iCharLight++ )
    {
        m_collectionStats.CharCandidates++;
        if ( CalcDirLight( &light, l2W, b, worldBox, worldBoxCenter, m_charLights[iCharLight].Pos,
                           m_charLights[iCharLight].Radius, m_charLights[iCharLight].Intensity,
                           m_charLights[iCharLight].Color ) )
        {
            InsertCollectedCharLight( light, maxLightCount );
            m_collectionStats.CharHits++;
        }
    }

    // now go through all of the dynamic lights
    for ( s32 iDynamicLight = 0; iDynamicLight < m_nDynamicLights; iDynamicLight++ )
    {
        m_collectionStats.CharCandidates++;
        if ( CalcDirLight( &light, l2W, b, worldBox, worldBoxCenter, m_dynamicLights[iDynamicLight].Pos,
                           m_dynamicLights[iDynamicLight].Radius, m_dynamicLights[iDynamicLight].Intensity,
                           m_dynamicLights[iDynamicLight].Color, m_dynamicLights[iDynamicLight].Shape,
                           m_dynamicLights[iDynamicLight].Direction, m_dynamicLights[iDynamicLight].OuterConeCos,
                           m_dynamicLights[iDynamicLight].ConeCosRangeInv ) )
        {
            InsertCollectedCharLight( light, maxLightCount );
            m_collectionStats.CharHits++;
        }
    }

    return m_nCollectedLights;
}

//=========================================================================

s32 light_mgr::CollectCharLightsOnly( matrix4 const& l2W, bbox const& b, s32 maxLightCount )
{
    m_nCollectedLights = 0;
    m_collectionStats.CharQueries++;
    maxLightCount = MIN( MAX( 0, maxLightCount ), MAX_COLLECTED_LIGHTS );
    if ( maxLightCount <= 0 )
    {
        return m_nCollectedLights;
    }

    bbox worldBox = b;
    worldBox.Transform( l2W );
    vector3 const worldBoxCenter = l2W * b.GetCenter();
    dir_light     light;

    // walk the list collecting fading lights that may intersect
    s32 currLink = m_firstLink;
    while ( currLink != -1 )
    {
        m_collectionStats.CharCandidates++;
        if ( CalcDirLight( &light, l2W, b, worldBox, worldBoxCenter, m_fadingLights[currLink].Pos,
                           m_fadingLights[currLink].Radius, m_fadingLights[currLink].Intensity,
                           m_fadingLights[currLink].CurrentColor ) )
        {
            InsertCollectedCharLight( light, maxLightCount );
            m_collectionStats.CharHits++;
        }
        currLink = m_fadingLights[currLink].NextLink;
    }

    // now go through only the normal character lights
    for ( s32 iCharLight = 0; iCharLight < m_nCharLights; iCharLight++ )
    {
        m_collectionStats.CharCandidates++;
        if ( CalcDirLight( &light, l2W, b, worldBox, worldBoxCenter, m_charLights[iCharLight].Pos,
                           m_charLights[iCharLight].Radius, m_charLights[iCharLight].Intensity,
                           m_charLights[iCharLight].Color ) )
        {
            InsertCollectedCharLight( light, maxLightCount );
            m_collectionStats.CharHits++;
        }
    }

    return m_nCollectedLights;
}

//=========================================================================

void light_mgr::GetCollectedCharLight( s32 index, vector3& dir, xcolor& c )
{
    ASSERT( ( index >= 0 ) && ( index < m_nCollectedLights ) );
    dir = m_collectedCharLights[index].Dir;
    c = m_collectedCharLights[index].Col;
}

//=========================================================================

void light_mgr::GetDynamicLight( s32 index, vector3& pos, f32& radius, xcolor& c, s32& shape, xbool& castShadows,
                                 f32& innerRadius, vector3& direction, f32& falloff, f32& innerAngle, f32& outerAngle,
                                 s32& shadowMapResolution, s32& shadowPriority ) const
{
    ASSERT( ( index >= 0 ) && ( index < m_nDynamicLights ) );

    dynamic_light const& light = m_dynamicLights[index];
    pos = light.Pos;
    radius = light.Radius;
    innerRadius = MAX( 0.0f, light.Radius * ( 1.0f - light.Falloff ) );
    direction = light.Direction;
    falloff = light.Falloff;
    innerAngle = light.InnerAngle;
    outerAngle = light.OuterAngle;
    shape = light.Shape;
    shadowMapResolution = light.ShadowMapResolution;
    shadowPriority = light.ShadowPriority;
    castShadows = light.CastShadows;
    ScaleLightColor( c, light.Color, light.Intensity );
}

//=========================================================================

void light_mgr::GetLight( s32 index, vector3& pos, f32& radius, xcolor& c ) const
{
    ASSERT( m_isInCollection );
    ASSERT( ( index >= 0 ) && ( index < m_nSpadLights ) );
    ASSERT( m_pSpadLights );

    spad_light& light = m_pSpadLights[index];
    pos = light.Pos;
    radius = light.Radius;
    ScaleLightColor( c, light.Color, light.Intensity );
}

//=========================================================================
