//=========================================================================
//
//  Version.cpp
//
//=========================================================================

#include "x_types.hpp"

#define A51_BUILD_CHANGELIST 87525 // GS: NOTE: Change this when game "globaly" updates, like networking and etc.
                                   // GS: TODO: Maybe this stuff should auto-synchronize with git changes ?

s32         g_Changelist  = A51_BUILD_CHANGELIST;
const char* g_pBuildDate  = __DATE__ " " __TIME__;
