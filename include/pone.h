#if !defined(PONE_ENGINE_H)
#define PONE_ENGINE_H

#include "types.h"

#define KILOBYTES(n) n << 10
#define MEGABYTES(n) KILOBYTES(n) << 10
#define GIGABYTES(n) MEGABYTES(n) << 10
#define TERABYTES(n) GIGABYTES(n) << 10

struct PoneMemory {
    void *permanentStorage;
    usize permanentStorageSize;
    void *transientStorage;
    usize transientStorageSize;
    b8 isInitialized;
};

struct PoneState {
    i32 greenOffset;
    i32 blueOffset;
    u32 toneHz;
};

struct PoneFramebuffer {
    void *memory;
    usize width;
    usize height;
    usize stride;
};

struct PoneSoundBuffer {
    i16 *samples;
    i32 sampleCount;
    i32 samplesPerSecond;
};

struct PoneInputButtonState {
    i32 halfTransitionCount;
    b8 isDown;
};

struct PoneKeyboardInput {
    PoneInputButtonState states[4];
};

struct PoneInput {
    PoneKeyboardInput keyboardInput;
};

void poneUpdateAndRender(PoneMemory *memory,
                         PoneFramebuffer *framebuffer,
                         PoneInput *input,
                         PoneSoundBuffer *soundBuffer);
                        

#endif
