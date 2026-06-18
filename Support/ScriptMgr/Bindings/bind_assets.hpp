//==============================================================================
//
//  bind_assets.hpp
//
//==============================================================================

#ifndef BIND_ASSETS_HPP
#define BIND_ASSETS_HPP

#include "../script_bindings.hpp"

class bind_assets : public script_bindings
{
public:
    const char*     GetModuleName   ( void ) const { return "assets"; }
    void            Register        ( script_backend* pBackend );
};

#endif
