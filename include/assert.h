#if !defined(PONE_ASSERT_H)
#define PONE_ASSERT_H

#include "defines.h"

#if defined(PONE_DEBUG)
#if defined(PONE_PLATFORM_WIN64)
#include <windows.h>
#define assert(expr) if (!(expr)) { DebugBreak(); }
#else
#error "Only WIN64 supported"
#endif
#else
#define assert(expr)
#endif

#endif
