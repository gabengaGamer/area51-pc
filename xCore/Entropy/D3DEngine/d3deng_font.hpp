//=========================================================================
//
// d3deng_font.hpp
//
//=========================================================================

#ifndef D3DENG_FONT_HPP
#define D3DENG_FONT_HPP

//=========================================================================

void font_Init     ( void );
void font_Kill     ( void );

void font_SetColor ( const xcolor& Color );

void font_OnDeviceLost  ( void );
void font_OnDeviceReset ( void );

//=========================================================================

#endif