
#ifndef MESHVIEWER_HPP
#define MESHVIEWER_HPP

//=========================================================================

#include "MeshUtil/RawMesh.hpp"
#include "MeshUtil/RawAnim.hpp"
#include "Render/Texture.hpp"

namespace fx_core
{

//=========================================================================
// 
//=========================================================================
class mesh_viewer
{
public:
            mesh_viewer     ( void );
            mesh_viewer     ( const mesh_viewer& mViewer );
           ~mesh_viewer     ( void );

    vector3 GetObjectFocus  ( void ) { return m_bBox.GetCenter(); }
    bbox    GetBBox         ( void ) { return m_bBox; }
    void    Load            ( const char* pFileName );
    void    Unload          ( void );    
    void    Render          ( xcolor TintColor, const matrix4& LocalToWorld );
    void    PlayAnimation   ( void );
    xbool   IsPause         ( void ) {return !m_bPlayAnim; }
    void    PauseAnimation  ( void );
    void    SetBackFacets   ( xbool bFaceFacets );
    void    SetAnimFrameRate( f32 Rate ){m_AnimFrameRate=Rate;}

protected:

    void    RenderSoftSkin  ( const matrix4& LocalToWorld );
    void    RenderSolid     ( xcolor TintColor, const matrix4& LocalToWorld );
    void    CleanUp         ( void );

protected:

    rawmesh     m_Mesh;
    rawanim     m_Anim;
    f32         m_Frame;
    bbox        m_bBox;
    texture     m_Texture[32];
    vector3     m_LightDir;
    vector3     m_Ambient;
    xbool       m_bPlayAnim;
    xtimer      m_Timer;
    f32         m_AnimFrameRate;
    xbool       m_bBackFacets;
};

//=========================================================================
// END
//=========================================================================

} // namespace fx_core

#endif
