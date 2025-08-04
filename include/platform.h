#if !defined(PONE_PLATFORM_H)
#define PONE_PLATFORM_H

#include "types.h"

void *ponePlatformReadFile(char *filename);
b8 ponePlatformWriteFile(char *filename, void *memory, usize memorySize);
void ponePlatformFreeMemory(void *memory);

#endif
