#include "pone.h"
#include "math/constants.h"
#include <math.h>

static void
poneRenderWeirdGradient(PoneFramebuffer *buf,
                        i32 xOffset,
                        i32 yOffset) {
    u8 *row = (u8 *)buf->memory;
    for (usize y = 0; y < buf->height; y++) {
        u32 *pixel = (u32 *)row;
        for (usize x = 0; x < buf->width; x++) {
            /*
             *
             * Pixel in memory: RR GG BB xx
             *
             * !LITTLE ENDIAN ARCHITECTURE!
             * 0xXXBBGGRR (Actually)
             *
             * Windows people defined order backwards so in the registers
             * it will show RGB
             *
             * 0xXXRRGGBB
             *
             */
            u8 red = 255;
            u8 blue = x + xOffset;
            u8 green = y + yOffset;

            *pixel++ = red << 16 | green << 8 | blue;
        }
        row += buf->stride;
    }
}

static void
poneSoundOutput(PoneSoundBuffer *soundBuffer, i32 toneHz) {
    static f32 t;
    i16 toneVolume = 5000;
    i32 wavePeriod = soundBuffer->samplesPerSecond / toneHz;
    
    i16 *sampleOut = soundBuffer->samples;
    for (i32 i = 0; i < soundBuffer->sampleCount; i++) {
        f32 sineValue = sinf(t);
        i16 sampleValue = (i16)(toneVolume * sineValue);
        *sampleOut++ = sampleValue;
        *sampleOut++ = sampleValue;

        t += 2.0f * PI_F32 * 1.0f / (f32)wavePeriod;
    }
}

void
poneUpdateAndRender(PoneMemory *memory,
                    PoneFramebuffer *framebuffer,
                    PoneInput *input,
                    PoneSoundBuffer *soundBuffer) {

    PoneState *state = (PoneState *)memory->permanentStorage;
    if (!memory->isInitialized) {
        state->toneHz = 440;
        memory->isInitialized = 1;
    }

    if (input->keyboardInput.states[0].isDown) {
        state->greenOffset += 1;
    }
    if (input->keyboardInput.states[1].isDown) {
        state->toneHz += 1;
    }
    if (input->keyboardInput.states[2].isDown) {
        state->greenOffset -= 1;
    }
    if (input->keyboardInput.states[3].isDown) {
        if (state->toneHz > 1) {
            state->toneHz--;
        }
    }

    poneRenderWeirdGradient(framebuffer, state->blueOffset, state->greenOffset);

    // TODO(emirhan): Allow sample offsets here for more robust platform options
    poneSoundOutput(soundBuffer, state->toneHz);

}
