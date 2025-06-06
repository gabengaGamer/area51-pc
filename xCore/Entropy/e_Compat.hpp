#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef _WIN64
#define POINTER_64
#else
#define POINTER_64 __ptr64
#endif

#include <windows.h>