#if !defined(PONE_ASSERT_H)
#define PONE_ASSERT_H

#include "pone_platform.h"

#if defined(PONE_PLATFORM_WIN64)

#include <windows.h>
#define PONE_ASSERT(expr) do { if (!(expr)) { DebugBreak(); } } while (0)
#define pone_assert(expr) if (!(expr)) { DebugBreak(); }
#define assert(expr) do { if (!(expr)) { DebugBreak(); } } while (0)

#elif defined(PONE_PLATFORM_LINUX)

#define PONE_ASSERT(expr) do { if (!(expr)) { __builtin_trap(); } } while (0)
#define pone_assert(expr) if (!(expr)) { __builtin_trap(); }
#define assert(expr) do { if (!(expr)) { __builtin_trap(); } } while (0)


#endif

#endif
