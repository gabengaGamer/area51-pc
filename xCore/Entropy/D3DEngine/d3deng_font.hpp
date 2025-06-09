//=========================================================================
//
// d3deng_font.hpp
//
//=========================================================================

#ifndef D3DENG_FONT_HPP
#define D3DENG_FONT_HPP

//=========================================================================
// DEFINES
//=========================================================================

#define ENG_FONT_SIZEX      7
#define ENG_FONT_SIZEY      12

//=========================================================================
// FUNCTIONS
//=========================================================================

void font_Init          ( void );
void font_Kill          ( void );

void font_SetColor      ( const xcolor& Color );

void font_OnDeviceLost  ( void );
void font_OnDeviceReset ( void );

#endif // D3DENG_FONT_HPP