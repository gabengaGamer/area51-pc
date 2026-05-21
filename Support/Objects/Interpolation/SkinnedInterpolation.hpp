//==============================================================================
//
//  SkinnedInterpolation.hpp
//
//==============================================================================

#ifndef SKINNED_INTERPOLATION_HPP
#define SKINNED_INTERPOLATION_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "InterpolationMath.hpp"

#include "Animation\AnimPlayer.hpp"
#include "e_ScratchMem.hpp"

//==============================================================================
//  STRUCTS
//==============================================================================

struct skinned_interp_state
{
    xbool   Valid;
    s32     NBones;
    matrix4 L2W;
    matrix4 Bones[MAX_ANIM_BONES];
};

//------------------------------------------------------------------------------

struct weapon_interp_state
{
    xbool   Active;
    s32     NBones;
    matrix4 L2W;
    matrix4 Bones[MAX_ANIM_BONES];
};

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

inline
void SetInterpStateBoneMatrices( matrix4*       pStateBones,
                                        s32&           StateNBones,
                                        const matrix4* pBones,
                                        s32            NBones )
{
    if( !pBones || (NBones <= 0) )
    {
        StateNBones = 0;
        return;
    }

    NBones      = MIN( NBones, MAX_ANIM_BONES );
    StateNBones = NBones;
    x_memcpy( pStateBones, pBones, NBones * sizeof( matrix4 ) );
}

//==============================================================================
//  SKINNED STATE
//==============================================================================

inline 
void InitSkinnedInterpState( skinned_interp_state& State )
{
    State.Valid = FALSE;
    State.NBones = 0;
    State.L2W.Identity();
}

//==============================================================================

inline 
void CaptureSkinnedInterpState( skinned_interp_state& State, const matrix4& L2W )
{
    InitSkinnedInterpState( State );
    State.Valid = TRUE;
    State.L2W   = L2W;
}

//==============================================================================

inline 
void SetSkinnedInterpStateBones( skinned_interp_state& State,
                                        const matrix4*        pBones,
                                        s32                   NBones )
{
    SetInterpStateBoneMatrices( State.Bones, State.NBones, pBones, NBones );
}

//==============================================================================

inline 
xbool ShouldSnapSkinnedInterpState( const skinned_interp_state& Prev,
                                           const skinned_interp_state& Curr )
{
    return ShouldSnapInterpL2W( Prev.L2W, Curr.L2W ) ||
           (Prev.NBones != Curr.NBones);
}

//==============================================================================

inline 
void UpdateSkinnedInterpState( const skinned_interp_state& Prev,
                                      const skinned_interp_state& Curr,
                                            skinned_interp_state& Interp,
                                            f32                  Alpha )
{
    Alpha = ClampInterpAlpha( Alpha );

    Interp.Valid  = Curr.Valid;
    Interp.NBones = Curr.NBones;
    Interp.L2W = InterpMatrix( Prev.L2W, Curr.L2W, Alpha );
    UpdateInterpMatrices( Interp.Bones, Prev.Bones, Prev.NBones, Curr.Bones, Curr.NBones, Alpha );
}

//==============================================================================

inline 
xbool GetSkinnedInterpBoneL2W( const skinned_interp_state& State,
                                      s32                         iBone,
                                      matrix4&                    L2W )
{
    if( !State.Valid || (iBone < 0) || (iBone >= State.NBones) )
        return FALSE;

    L2W = State.Bones[iBone];
    return TRUE;
}

//==============================================================================

inline 
const matrix4* BuildSkinnedInterpMatrices( const skinned_interp_state& State,
                                                  const anim_group&           AnimGroup,
                                                  s32                         NBones )
{
    if( !State.Valid || (State.NBones < NBones) || (NBones <= 0) )
        return NULL;

    matrix4* pMatrices = (matrix4*)smem_BufferAlloc( NBones * sizeof( matrix4 ) );
    if( !pMatrices )
        return NULL;

    for( s32 i = 0; i < NBones; i++ )
        pMatrices[i] = State.Bones[i] * AnimGroup.GetBoneBindInvMatrix( i );

    return pMatrices;
}

//==============================================================================

inline
void TransformSkinnedInterpState( skinned_interp_state& State, const matrix4& M )
{
    if( !State.Valid )
        return;

    State.L2W = M * State.L2W;

    for( s32 i = 0; i < State.NBones; i++ )
        State.Bones[i] = M * State.Bones[i];
}

//==============================================================================
//  WEAPON STATE
//==============================================================================

inline 
void InitWeaponInterpState( weapon_interp_state& State )
{
    State.Active = FALSE;
    State.NBones = 0;
    State.L2W.Identity();
}

//==============================================================================

inline 
void CaptureWeaponInterpState( weapon_interp_state& State, const matrix4& L2W )
{
    InitWeaponInterpState( State );
    State.Active = TRUE;
    State.L2W    = L2W;
}

//==============================================================================

inline 
void SetWeaponInterpStateBones( weapon_interp_state& State,
                                       const matrix4*       pBones,
                                       s32                  NBones )
{
    SetInterpStateBoneMatrices( State.Bones, State.NBones, pBones, NBones );
}

//==============================================================================

inline 
void UpdateWeaponInterpState( const weapon_interp_state& Prev,
                                     const weapon_interp_state& Curr,
                                           weapon_interp_state& Interp,
                                           f32                  Alpha )
{
    Alpha = ClampInterpAlpha( Alpha );

    Interp.Active = Curr.Active;
    Interp.NBones = Curr.NBones;
    Interp.L2W    = Curr.L2W;
    if( !Curr.Active )
        return;

    Interp.L2W = InterpMatrix( Prev.L2W, Curr.L2W, Alpha );
    UpdateInterpMatrices( Interp.Bones,
                          Prev.Bones,
                          Prev.Active ? Prev.NBones : 0,
                          Curr.Bones,
                          Curr.NBones,
                          Alpha );
}

//==============================================================================

inline
void TransformWeaponInterpState( weapon_interp_state& State, const matrix4& M )
{
    if( !State.Active )
        return;

    State.L2W = M * State.L2W;

    for( s32 i = 0; i < State.NBones; i++ )
        State.Bones[i] = M * State.Bones[i];
}

//==============================================================================

inline 
const matrix4* GetWeaponInterpStateBones( const weapon_interp_state& State, s32& NBones )
{
    NBones = 0;

    if( !State.Active || (State.NBones <= 0) )
        return NULL;

    NBones = State.NBones;
    return State.Bones;
}

//==============================================================================
#endif //SKINNED_INTERPOLATION_HPP
//==============================================================================
