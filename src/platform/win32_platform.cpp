#include <windows.h>

#include "platform.h"

void *ponePlatformReadFile(char *filename) {
    void *result = 0;

    HANDLE fileHandle = CreateFileA(filename,
                                    GENERIC_READ,
                                    FILE_SHARE_READ,
                                    0,
                                    OPEN_EXISTING,
                                    0,
                                    0);
    if (fileHandle != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER fileSize;
        if (GetFileSizeEx(fileHandle, &fileSize)) {
            u32 fileSize32 = safeTruncateU32(fileSize.QuadPart);
            result = VirtualAlloc(0, fileSize32,
                                  MEM_RESERVE | MEM_COMMIT,
                                  PAGE_READWRITE);
            if (result) {
                DWORD bytesRead;
                if (ReadFile(fileHandle, result, fileSize32, &bytesRead, 0) &&
                    fileSize32 == bytesRead)
                {
                    // NOTE(emirhan): file read successfully
                } else {
                    // TODO(emirhan): logging
                    ponePlatformFreeMemory(result);
                    result = 0;
                }
            } else {
                // TODO(emirhan): logging
            }
        } else {
            // TODO(emirhan): logging
        }
        CloseHandle(fileHandle);
    } else {
        // TODO(emirhan): logging
    }

    return result;
}

b8 ponePlatformWriteFile(char *filename, void *memory, usize memorySize) {
    b8 result = 0;

    HANDLE fileHandle = CreateFileA(filename,
                                    GENERIC_WRITE,
                                    0,
                                    0,
                                    CREATE_ALWAYS,
                                    0,
                                    0);
    if (fileHandle != INVALID_HANDLE_VALUE) {
        DWORD bytesWritten;
        if (WriteFile(fileHandle, memory, memorySize, &bytesWritten, 0)) {
            result = 1;
        } else {
            // TODO(emirhan): logging
        }
        CloseHandle(fileHandle);
    } else {
        // TODO(emirhan): logging
    }

    return result;
}

void ponePlatformFreeMemory(void *memory) {
    if (memory) {
        VirtualFree(memory, 0, MEM_RELEASE);
    }
}
