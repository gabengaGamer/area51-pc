//==============================================================================
//
//  dlg_GraphicsSettings.hpp
//
//==============================================================================

#ifndef DLG_GRAPHICS_SETTINGS_HPP
#define DLG_GRAPHICS_SETTINGS_HPP

//==============================================================================
//  INCLUDES
//==============================================================================

#include "ui/ui_dialog.hpp"
#include "ui/ui_frame.hpp"
#include "ui/ui_text.hpp"
#include "ui/ui_combo.hpp"
#include "ui/ui_slider.hpp"
#include "ui/ui_check.hpp"
#include "ui/ui_button.hpp"

#include "dlg_PopUp.hpp"

//==============================================================================
//  dlg_graphics_settings
//==============================================================================

extern void     dlg_graphics_settings_register  ( ui_manager* pManager );
extern ui_win*  dlg_graphics_settings_factory   ( s32 UserID, ui_manager* pManager, ui_manager::dialog_tem* pDialogTem, const irect& Position, ui_win* pParent, s32 Flags, void* pUserData );

class dlg_graphics_settings : public ui_dialog
{
public:
                        dlg_graphics_settings     ( void );
    virtual            ~dlg_graphics_settings     ( void );

    xbool               Create                  ( s32                       UserID,
                                                  ui_manager*               pManager,
                                                  ui_manager::dialog_tem*   pDialogTem,
                                                  const irect&              Position,
                                                  ui_win*                   pParent,
                                                  s32                       Flags,
                                                  void*                     pUserData );
    virtual void        Destroy                 ( void );

    virtual void        Render                  ( s32 ox=0, s32 oy=0 );

    virtual void        OnNotify                ( ui_win* pWin, ui_win* pSender, s32 Command, void* pData );
    virtual void        OnPadSelect             ( ui_win* pWin );
    virtual void        OnPadBack               ( ui_win* pWin );
    virtual void        OnPadDelete             ( ui_win* pWin );
    virtual void        OnUpdate                ( ui_win* pWin, f32 DeltaTime );

    void                EnableBlackout          ( void )                    { m_bRenderBlackout = TRUE; }

protected:
    ui_combo*           m_pResolution;
    ui_combo*           m_pDisplayMode;
    ui_slider*          m_pGamma;
    ui_slider*          m_pFov;
    ui_check*           m_pVSync;
    ui_button*          m_pButtonApply;

    ui_text*            m_pResolutionText;
    ui_text*            m_pDisplayModeText;
    ui_text*            m_pGammaText;
    ui_text*            m_pFovText;
    ui_text*            m_pVSyncText;
    ui_text*            m_pNavText;

    s32                 m_CurrHL;
    xbool               m_bRenderBlackout;
};

//==============================================================================
#endif // DLG_GRAPHICS_SETTINGS_HPP
//==============================================================================