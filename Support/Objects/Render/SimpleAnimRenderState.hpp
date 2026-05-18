#ifndef SIMPLE_ANIM_RENDER_STATE_HPP
#define SIMPLE_ANIM_RENDER_STATE_HPP

#include "Animation\AnimPlayer.hpp"
#include "e_ScratchMem.hpp"

struct simple_anim_render_state
{
    xbool   Valid;
    s32     NBones;
    matrix4 L2W;
    matrix4 Bones[MAX_ANIM_BONES];
};

inline void InitSimpleAnimRenderState( simple_anim_render_state& State )
{
    State.Valid = FALSE;
    State.NBones = 0;
    State.L2W.Identity();
}

inline radian InterpSimpleAnimRenderAngle( radian A, radian B, f32 T )
{
    return A + (x_MinAngleDiff( B, A ) * T);
}

inline radian3 InterpSimpleAnimRenderRotation( const radian3& A, const radian3& B, f32 T )
{
    return radian3( InterpSimpleAnimRenderAngle( A.Pitch, B.Pitch, T ),
                    InterpSimpleAnimRenderAngle( A.Yaw,   B.Yaw,   T ),
                    InterpSimpleAnimRenderAngle( A.Roll,  B.Roll,  T ) );
}

inline vector3 InterpSimpleAnimRenderVector( const vector3& A, const vector3& B, f32 T )
{
    return A + ((B - A) * T);
}

inline matrix4 BuildSimpleAnimRenderL2W( const vector3& Pos, const radian3& Rot )
{
    matrix4 L2W;
    L2W.Identity();
    L2W.SetRotation( Rot );
    L2W.SetTranslation( Pos );
    return L2W;
}

inline matrix4 InterpSimpleAnimRenderMatrix( const matrix4& A, const matrix4& B, f32 T )
{
    return BuildSimpleAnimRenderL2W( InterpSimpleAnimRenderVector( A.GetTranslation(), B.GetTranslation(), T ),
                                     InterpSimpleAnimRenderRotation( A.GetRotation(), B.GetRotation(), T ) );
}

inline void CaptureSimpleAnimRenderState( simple_anim_render_state& Snapshot,
                                          const matrix4&           L2W,
                                          simple_anim_player&      AnimPlayer )
{
    InitSimpleAnimRenderState( Snapshot );
    Snapshot.Valid = TRUE;
    Snapshot.L2W   = L2W;

    const anim_group* pAnimGroup = AnimPlayer.GetAnimGroup();
    if( !pAnimGroup )
        return;

    if( AnimPlayer.GetAnimIndex() < 0 )
        return;

    const s32 nBones = MIN( pAnimGroup->GetNBones(), MAX_ANIM_BONES );
    if( nBones <= 0 )
        return;

    const matrix4* pBones = AnimPlayer.GetBoneL2Ws( FALSE );
    if( !pBones )
        return;

    Snapshot.NBones = nBones;
    x_memcpy( Snapshot.Bones, pBones, nBones * sizeof( matrix4 ) );
}

inline xbool ShouldSnapSimpleAnimRenderState( const simple_anim_render_state& Prev,
                                              const simple_anim_render_state& Curr )
{
    const vector3 Delta   = Curr.L2W.GetTranslation() - Prev.L2W.GetTranslation();
    const radian3 PrevRot = Prev.L2W.GetRotation();
    const radian3 CurrRot = Curr.L2W.GetRotation();

    return (Delta.LengthSquared() > x_sqr( 250.0f )) ||
           (x_abs( x_MinAngleDiff( CurrRot.Pitch, PrevRot.Pitch ) ) > R_90) ||
           (x_abs( x_MinAngleDiff( CurrRot.Yaw,   PrevRot.Yaw   ) ) > R_90) ||
           (x_abs( x_MinAngleDiff( CurrRot.Roll,  PrevRot.Roll  ) ) > R_90) ||
           (Prev.NBones != Curr.NBones);
}

inline void UpdateSimpleAnimRenderState( const simple_anim_render_state& Prev,
                                         const simple_anim_render_state& Curr,
                                               simple_anim_render_state& Interp,
                                               f32                       Alpha )
{
    Alpha = MAX( 0.0f, MIN( Alpha, 1.0f ) );

    Interp = Curr;
    Interp.L2W = BuildSimpleAnimRenderL2W(
        InterpSimpleAnimRenderVector( Prev.L2W.GetTranslation(), Curr.L2W.GetTranslation(), Alpha ),
        InterpSimpleAnimRenderRotation( Prev.L2W.GetRotation(), Curr.L2W.GetRotation(), Alpha ) );

    if( (Prev.NBones == Curr.NBones) && (Curr.NBones > 0) )
    {
        for( s32 i = 0; i < Curr.NBones; i++ )
            Interp.Bones[i] = InterpSimpleAnimRenderMatrix( Prev.Bones[i], Curr.Bones[i], Alpha );
    }
    else
    {
        for( s32 i = 0; i < Curr.NBones; i++ )
            Interp.Bones[i] = Curr.Bones[i];
    }
}

inline xbool GetSimpleAnimRenderBoneL2W( const simple_anim_render_state& State,
                                         s32                             iBone,
                                         matrix4&                        L2W )
{
    if( !State.Valid || (iBone < 0) || (iBone >= State.NBones) )
        return FALSE;

    L2W = State.Bones[iBone];
    return TRUE;
}

inline const matrix4* BuildSimpleAnimRenderMatrices( const simple_anim_render_state& State,
                                                     const anim_group&               AnimGroup,
                                                     s32                             nBones )
{
    if( !State.Valid || (State.NBones < nBones) || (nBones <= 0) )
        return NULL;

    matrix4* pMatrices = (matrix4*)smem_BufferAlloc( nBones * sizeof( matrix4 ) );
    if( !pMatrices )
        return NULL;

    for( s32 i = 0; i < nBones; i++ )
        pMatrices[i] = State.Bones[i] * AnimGroup.GetBoneBindInvMatrix( i );

    return pMatrices;
}

#endif
