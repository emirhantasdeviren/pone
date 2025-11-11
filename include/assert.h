#if !defined(PONE_ASSERT_H)
#define PONE_ASSERT_H

#if defined(_WIN64)
#include <windows.h>
#define assert(expr) do { if (!(expr)) { DebugBreak(); } } while (0)
#else
#error "Only WIN64 supported"
#endif

#endif
