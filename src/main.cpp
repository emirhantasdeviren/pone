#include <dsound.h>
#include <windows.h>
// TODO(emirhan): implement sine ourselves;
#include <math.h>
#include <stdio.h>

#include "pone.h"

#define DIRECT_SOUND_CREATE(name)                                              \
    HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS,             \
                        LPUNKNOWN pUnkOuter);
typedef DIRECT_SOUND_CREATE(DirectSoundCreateFn);

#define PI_F 3.14159265358979323846264338327950288f

struct Win32OffscreenBuffer {
    BITMAPINFO info;
    void *memory;
    i32 width;
    i32 height;
    i32 stride;
    i32 bytesPerPixel;
};

struct Win32WindowDimension {
    i32 width;
    i32 height;
};

struct Win32SoundOutput {
    i32 samplesPerSecond;
    u32 runningSampleIndex;
    i32 bytesPerSample;
    DWORD secondaryBufferSize;
    DWORD safetyBytes;
};

struct Win32PerfCounterFreq {
    b8 isInitialized;
    LARGE_INTEGER value;
};

struct Win32DebugTimeMarker {
    DWORD playCursor;
    DWORD writeCursor;
};

static LARGE_INTEGER win32GetPerfCounterFreq(Win32PerfCounterFreq *freq) {
    if (freq->isInitialized) {
        return freq->value;
    } else {
        QueryPerformanceFrequency(&freq->value);
        freq->isInitialized = 1;

        return freq->value;
    }
}

struct Win32Clock {
    Win32PerfCounterFreq freq;
};

static LARGE_INTEGER win32GetWallClock() {
    LARGE_INTEGER result;
    QueryPerformanceCounter(&result);

    return result;
}

static f32 win32GetSecondsElapsed(Win32Clock *clock, LARGE_INTEGER start,
                                  LARGE_INTEGER end) {
    LARGE_INTEGER freq = win32GetPerfCounterFreq(&clock->freq);

    f32 result = (f32)(end.QuadPart - start.QuadPart) / (f32)freq.QuadPart;

    return result;
}

// TODO(emirhan): This is global for now
static b8 globalRunning = 1;
static Win32OffscreenBuffer globalBackbuffer;
static LPDIRECTSOUNDBUFFER globalDirectSoundBuffer;

static void win32ClearSoundBuffer(Win32SoundOutput *soundOutput) {
    void *region1;
    DWORD region1Size;
    void *region2;
    DWORD region2Size;
    HRESULT hr = globalDirectSoundBuffer->Lock(
        0, soundOutput->secondaryBufferSize, &region1, &region1Size, &region2,
        &region2Size, 0);
    if (SUCCEEDED(hr)) {
        u8 *dstSample = (u8 *)region1;
        for (DWORD i = 0; i < region1Size; i++) {
            *dstSample++ = 0;
        }

        dstSample = (u8 *)region2;
        for (DWORD i = 0; i < region2Size; i++) {
            *dstSample++ = 0;
        }

        globalDirectSoundBuffer->Unlock(region1, region1Size, region2,
                                        region2Size);
    }
}

static void win32FillSoundBuffer(Win32SoundOutput *soundOutput,
                                 DWORD byteToLock, DWORD bytesToWrite,
                                 PoneSoundBuffer *sourceBuffer) {
    void *region1;
    DWORD region1Size;
    void *region2;
    DWORD region2Size;
    if (SUCCEEDED(globalDirectSoundBuffer->Lock(byteToLock, bytesToWrite,
                                                &region1, &region1Size,
                                                &region2, &region2Size, 0))) {
        // TODO(emirhan): assert that region1Size/region2Size is valid
        i16 *dstSample = (i16 *)region1;
        i16 *srcSample = sourceBuffer->samples;
        DWORD region1SampleCount = region1Size / soundOutput->bytesPerSample;
        for (DWORD i = 0; i < region1SampleCount; i++) {
            *dstSample++ = *srcSample++;
            *dstSample++ = *srcSample++;
            soundOutput->runningSampleIndex++;
        }

        dstSample = (i16 *)region2;
        DWORD region2SampleCount = region2Size / soundOutput->bytesPerSample;
        for (DWORD i = 0; i < region2SampleCount; i++) {
            *dstSample++ = *srcSample++;
            *dstSample++ = *srcSample++;
            soundOutput->runningSampleIndex++;
        }
    }

    globalDirectSoundBuffer->Unlock(region1, region1Size, region2, region2Size);
}

static void win32InitDSound(HWND window, i32 samplesPerSecond, i32 bufferSize) {
    HMODULE dSoundLib = LoadLibraryA("dsound.dll");

    if (dSoundLib) {
        DirectSoundCreateFn *directSoundCreate =
            (DirectSoundCreateFn *)GetProcAddress(dSoundLib,
                                                  "DirectSoundCreate");
        LPDIRECTSOUND directSound;
        if (directSoundCreate &&
            SUCCEEDED(directSoundCreate(0, &directSound, 0))) {
            WAVEFORMATEX waveFormat = {};
            waveFormat.wFormatTag = WAVE_FORMAT_PCM;
            waveFormat.nChannels = 2;
            waveFormat.nSamplesPerSec = samplesPerSecond;
            waveFormat.wBitsPerSample = 16;
            waveFormat.nBlockAlign =
                (waveFormat.nChannels * waveFormat.wBitsPerSample) / 8;
            waveFormat.nAvgBytesPerSec =
                waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
            waveFormat.cbSize = 0;
            if (SUCCEEDED(
                    directSound->SetCooperativeLevel(window, DSSCL_PRIORITY))) {
                DSBUFFERDESC bufferDescription = {};
                bufferDescription.dwSize = sizeof(bufferDescription);
                bufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

                LPDIRECTSOUNDBUFFER primaryBuffer;
                if (SUCCEEDED(directSound->CreateSoundBuffer(
                        &bufferDescription, &primaryBuffer, 0))) {
                    OutputDebugStringA("Primary buffer created.\n");

                    if (SUCCEEDED(primaryBuffer->SetFormat(&waveFormat))) {
                        OutputDebugStringA("Primary buffer's format is set.\n");
                    } else {
                        // TODO(emirhan): Diagnostic
                    }
                } else {
                    // TODO(emirhan): Diagnostic
                }
            } else {
                // TODO(emirhan): Diagnostic
            }
            DSBUFFERDESC bufferDescription = {};
            bufferDescription.dwSize = sizeof(bufferDescription);
            bufferDescription.dwBufferBytes = bufferSize;
            bufferDescription.lpwfxFormat = &waveFormat;
            if (SUCCEEDED(directSound->CreateSoundBuffer(
                    &bufferDescription, &globalDirectSoundBuffer, 0))) {
                OutputDebugStringA("Secondary buffer created.\n");
            }
        }
    } else {
        // TODO(emirhan): Diagnostic
    }
}

Win32WindowDimension win32GetWindowDimension(HWND window) {
    Win32WindowDimension dimension;
    RECT clientRect;
    GetClientRect(window, &clientRect);
    dimension.width = clientRect.right - clientRect.left;
    dimension.height = clientRect.bottom - clientRect.top;

    return (dimension);
}

static void win32ResizeDibSection(Win32OffscreenBuffer *buffer, i32 width,
                                  i32 height) {
    if (buffer->memory) {
        VirtualFree(buffer->memory, 0, MEM_RELEASE);
    }

    buffer->width = width;
    buffer->height = height;
    buffer->bytesPerPixel = 4;

    buffer->info.bmiHeader.biSize = sizeof(buffer->info.bmiHeader);
    buffer->info.bmiHeader.biWidth = buffer->width,
    buffer->info.bmiHeader.biHeight = -buffer->height,
    buffer->info.bmiHeader.biPlanes = 1;
    buffer->info.bmiHeader.biBitCount = 32;
    buffer->info.bmiHeader.biCompression = BI_RGB;

    i32 bitmapMemorySize =
        (buffer->width * buffer->height) * buffer->bytesPerPixel;
    buffer->memory =
        VirtualAlloc(0, bitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
    buffer->stride = buffer->width * buffer->bytesPerPixel;
}

static void win32DisplayBufferInWindow(Win32OffscreenBuffer *buffer,
                                       HDC deviceContext, i32 windowWidth,
                                       i32 windowHeight) {
    StretchDIBits(deviceContext,
                  /*
                  x, y, width, height,
                  x, y, width, height,
                  */
                  0, 0, windowWidth, windowHeight, 0, 0, buffer->width,
                  buffer->height, buffer->memory, &buffer->info, DIB_RGB_COLORS,
                  SRCCOPY);
}

LRESULT win32MainWindowCallback(HWND window, UINT message, WPARAM wParam,
                                LPARAM lParam) {
    LRESULT result = 0;
    switch (message) {
    case WM_QUIT:
    case WM_DESTROY: {
        globalRunning = 0;
    } break;

    case WM_ACTIVATEAPP: {
        OutputDebugStringA("WM_ACTIVATEAPP\n");
    } break;

    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_KEYUP: {
        DebugBreak();
    } break;

    case WM_PAINT: {
        PAINTSTRUCT paint;
        HDC deviceContext = BeginPaint(window, &paint);

        Win32WindowDimension dimension = win32GetWindowDimension(window);
        win32DisplayBufferInWindow(&globalBackbuffer, deviceContext,
                                   dimension.width, dimension.height);
        EndPaint(window, &paint);
    } break;

    default: {
        result = DefWindowProcA(window, message, wParam, lParam);
    } break;
    }

    return result;
}

static void win32ProcessKeyboardButton(PoneInputButtonState *buttonState,
                                       b8 isDown) {
    assert(buttonState->isDown != isDown);
    buttonState->isDown = isDown;
    ++buttonState->halfTransitionCount;
}

static void win32ProcessPendingMessages(PoneInput *input, b8 *running) {
    MSG message;
    while (PeekMessageA(&message, 0, 0, 0, PM_REMOVE)) {
        switch (message.message) {
        case WM_QUIT:
        case WM_DESTROY:
        case WM_CLOSE: {
            *running = 0;
        } break;

        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP: {
            WPARAM wParam = message.wParam;
            WPARAM lParam = message.lParam;

            usize vkCode = wParam;
            b8 wasDown = (lParam & (1 << 30)) != 0;
            b8 isDown = (lParam & (1 << 31)) == 0;

            PoneKeyboardInput *keyboardInput = &input->keyboardInput;

            if (wasDown ^ isDown) {
                if (vkCode == 'W') {
                    win32ProcessKeyboardButton(&keyboardInput->states[0],
                                               isDown);
                } else if (vkCode == 'A') {
                    win32ProcessKeyboardButton(&keyboardInput->states[1],
                                               isDown);
                } else if (vkCode == 'S') {
                    win32ProcessKeyboardButton(&keyboardInput->states[2],
                                               isDown);
                } else if (vkCode == 'D') {
                    win32ProcessKeyboardButton(&keyboardInput->states[3],
                                               isDown);
                } else if (vkCode == VK_ESCAPE) {
                    *running = 0;
                }
            }
        } break;

        default: {
            TranslateMessage(&message);
            DispatchMessageA(&message);
        } break;
        }
    }
}

static void win32DebugDrawVertical(Win32OffscreenBuffer *framebuffer, i32 x,
                                   i32 top, i32 bottom, u32 color) {
    u8 *pixel = (u8 *)framebuffer->memory + x * framebuffer->bytesPerPixel +
                top * framebuffer->stride;

    for (i32 y = top; y < bottom; y++) {
        *(u32 *)pixel = color;
        pixel += framebuffer->stride;
    }
}

static void win32DrawSoundBufferMarker(Win32OffscreenBuffer *framebuffer, f32 c,
                                       i32 padX, i32 top, i32 bottom,
                                       DWORD value, u32 color) {
    i32 x = padX + (i32)(c * (f32)value);
    win32DebugDrawVertical(framebuffer, x, top, bottom, color);
}

static void win32DebugSyncDisplay(Win32OffscreenBuffer *framebuffer,
                                  Win32DebugTimeMarker *marker,
                                  usize markerSize,
                                  Win32SoundOutput *soundOutput,
                                  f32 targetSecondsPerFrame) {
    i32 padX = 16;
    i32 padY = 16;
    i32 top = padY;
    i32 bottom = framebuffer->height - padY;

    f32 c = (f32)(framebuffer->width - 2 * padX) /
            (f32)soundOutput->secondaryBufferSize;
    for (usize i = 0; i < markerSize; i++) {
        Win32DebugTimeMarker *m = marker + i;
        win32DrawSoundBufferMarker(framebuffer, c, padX, top, bottom,
                                   m->playCursor, 0xffffffff);
        win32DrawSoundBufferMarker(framebuffer, c, padX, top, bottom,
                                   m->writeCursor, 0xff0000ff);
    }
}

i32 CALLBACK WinMain(HINSTANCE instance, HINSTANCE prev_instance,
                     LPSTR cmd_line, int show_code) {
    b8 sleepIsGranular = 0;
    if (timeBeginPeriod(1) == TIMERR_NOERROR) {
        sleepIsGranular = 1;
    }
    Win32Clock clock = {};
    i32 monitorVerticalRefreshRate = 60;
    i32 gameFps = monitorVerticalRefreshRate / 2;
    f32 targetSecondsPerFrame = 1.0f / (f32)gameFps;

    win32ResizeDibSection(&globalBackbuffer, 1280, 720);
    WNDCLASSA windowClass = {};
    windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    windowClass.lpfnWndProc = win32MainWindowCallback;
    windowClass.hInstance = instance;
    // windowClass.hIcon;
    windowClass.lpszClassName = "PoneWindowClass";

    if (RegisterClassA(&windowClass)) {
        HWND window = CreateWindowExA(
            0, windowClass.lpszClassName, "Pone Engine",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
            CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, instance, 0);
        if (window) {
            HDC deviceContext = GetDC(window);

            Win32SoundOutput soundOutput;
            soundOutput.samplesPerSecond = 48000;
            soundOutput.runningSampleIndex = 0;
            soundOutput.bytesPerSample = sizeof(i16) * 2;
            soundOutput.secondaryBufferSize =
                soundOutput.samplesPerSecond * soundOutput.bytesPerSample;
            soundOutput.safetyBytes = (soundOutput.samplesPerSecond *
                                       soundOutput.bytesPerSample / gameFps) /
                                      4;
            DWORD expectedSoundBytesPerFrame =
                (soundOutput.samplesPerSecond * soundOutput.bytesPerSample) /
                gameFps;

            win32InitDSound(window, soundOutput.samplesPerSecond,
                            soundOutput.secondaryBufferSize);
            win32ClearSoundBuffer(&soundOutput);
            globalDirectSoundBuffer->Play(0, 0, DSBPLAY_LOOPING);
            i16 *samples =
                (i16 *)VirtualAlloc(0, soundOutput.secondaryBufferSize,
                                    MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

            PoneMemory memory;
            memory.permanentStorageSize = MEGABYTES(64);
            memory.transientStorageSize = GIGABYTES((usize)2);

            usize totalStorageSize =
                memory.permanentStorageSize + memory.transientStorageSize;

#if PONE_INTERNAL
            LPVOID baseAddress = (LPVOID)(TERABYTES((usize)2));
#else
            LPVOID baseAddress = (LPVOID)0;
#endif

            memory.permanentStorage =
                VirtualAlloc(baseAddress, totalStorageSize,
                             MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            memory.transientStorage =
                (u8 *)memory.permanentStorage + memory.permanentStorageSize;

            if (samples && memory.permanentStorage && memory.transientStorage) {
                usize debugTimeMarkerIndex = 0;
                Win32DebugTimeMarker debugTimeMarker[15];

                b8 soundIsValid = 0;

                PoneInput input = {};
                LARGE_INTEGER lastCounter = win32GetWallClock();

                while (globalRunning) {
                    for (usize i = 0; i < 4; i++) {
                        input.keyboardInput.states[i].halfTransitionCount = 0;
                    }
                    win32ProcessPendingMessages(&input, &globalRunning);

                    PoneFramebuffer framebuffer;
                    framebuffer.memory = globalBackbuffer.memory;
                    framebuffer.width = globalBackbuffer.width;
                    framebuffer.height = globalBackbuffer.height;
                    framebuffer.stride = globalBackbuffer.stride;

                    poneUpdateAndRender(&memory, &framebuffer, &input);

                    DWORD playCursor;
                    DWORD writeCursor;
                    HRESULT hr = globalDirectSoundBuffer->GetCurrentPosition(
                        &playCursor, &writeCursor);
                    if (SUCCEEDED(hr)) {
                        if (!soundIsValid) {
                            soundOutput.runningSampleIndex =
                                writeCursor / soundOutput.bytesPerSample;
                            soundIsValid = 1;
                        }

                        DWORD byteToLock = (soundOutput.runningSampleIndex *
                                            soundOutput.bytesPerSample) %
                                           soundOutput.secondaryBufferSize;
                        DWORD safeWriteCursor = writeCursor;
                        if (safeWriteCursor < playCursor) {
                            safeWriteCursor += soundOutput.secondaryBufferSize;
                        }
                        assert(safeWriteCursor >= playCursor);
                        safeWriteCursor += soundOutput.safetyBytes;
                        DWORD expectedFrameBoundaryByte =
                            playCursor + expectedSoundBytesPerFrame;
                        b8 audioHasHighLatency =
                            safeWriteCursor >= expectedFrameBoundaryByte;

                        DWORD targetCursor = 0;
                        if (audioHasHighLatency) {
                            targetCursor = writeCursor +
                                           expectedSoundBytesPerFrame +
                                           soundOutput.safetyBytes;
                        } else {
                            targetCursor = expectedFrameBoundaryByte +
                                           expectedSoundBytesPerFrame;
                        }
                        targetCursor %= soundOutput.secondaryBufferSize;

                        DWORD bytesToWrite = 0;
                        if (byteToLock > targetCursor) {
                            bytesToWrite =
                                soundOutput.secondaryBufferSize - byteToLock;
                            bytesToWrite += targetCursor;
                        } else {
                            bytesToWrite = targetCursor - byteToLock;
                        }

                        PoneSoundBuffer soundBuffer;
                        soundBuffer.samples = samples;
                        soundBuffer.samplesPerSecond =
                            soundOutput.samplesPerSecond;
                        soundBuffer.sampleCount =
                            bytesToWrite / soundOutput.bytesPerSample;
                        poneGetSoundSamples(&memory, &soundBuffer);

#if PONE_INTERNAL
                        {
                            Win32DebugTimeMarker *m =
                                &debugTimeMarker[debugTimeMarkerIndex++];
                            if (debugTimeMarkerIndex == 15) {
                                debugTimeMarkerIndex = 0;
                            }
                            m->playCursor = playCursor;
                            m->writeCursor = writeCursor;
                        }
                        hr = globalDirectSoundBuffer->GetCurrentPosition(
                            &playCursor, &writeCursor);
                        if (SUCCEEDED(hr)) {
                            DWORD unwrappedWriteCursor = writeCursor;
                            if (unwrappedWriteCursor < playCursor) {
                                unwrappedWriteCursor +=
                                    soundOutput.secondaryBufferSize;
                            }
                            DWORD audioLatencyBytes =
                                unwrappedWriteCursor - playCursor;
                            f32 audioLatencySeconds =
                                ((f32)audioLatencyBytes /
                                 (f32)soundOutput.bytesPerSample) /
                                (f32)soundOutput.samplesPerSecond;

                            PoneState *state =
                                (PoneState *)memory.permanentStorage;
                            char soundCursorBuf[256];
                            _snprintf_s(
                                soundCursorBuf, 256,
                                "BTL: %u, TC: %u, BTW: %u - PC: "
                                "%u, WC: %u DELTA: %u (%.3fs) - Tone Hz: %.2fHz Green Offset: %u\n",
                                byteToLock, targetCursor, bytesToWrite,
                                playCursor, writeCursor, audioLatencyBytes,
                                audioLatencySeconds, (f32)state->toneHz, state->greenOffset);
                            OutputDebugStringA(soundCursorBuf);
                        }
#endif

                        win32FillSoundBuffer(&soundOutput, byteToLock,
                                             bytesToWrite, &soundBuffer);
                    } else {
                        soundIsValid = 0;
                    }

                    LARGE_INTEGER workCounter = win32GetWallClock();
                    f32 workSecondsElapsed = win32GetSecondsElapsed(
                        &clock, lastCounter, workCounter);
                    f32 frameSecondsElapsed = workSecondsElapsed;
                    if (frameSecondsElapsed < targetSecondsPerFrame) {
                        if (sleepIsGranular) {
                            DWORD sleepMs =
                                (DWORD)(1000.0f * (targetSecondsPerFrame -
                                                   frameSecondsElapsed));
                            if (sleepMs > 0) {
                                Sleep(sleepMs);
                            }
                        }
                        while (frameSecondsElapsed < targetSecondsPerFrame) {
                            frameSecondsElapsed = win32GetSecondsElapsed(
                                &clock, lastCounter, win32GetWallClock());
                        }
                    }
                    LARGE_INTEGER endCounter = win32GetWallClock();
                    f32 msPerFrame =
                        1000.0f *
                        win32GetSecondsElapsed(&clock, lastCounter, endCounter);
                    lastCounter = endCounter;

                    Win32WindowDimension dimension =
                        win32GetWindowDimension(window);
#if PONE_INTERNAL
                    win32DebugSyncDisplay(&globalBackbuffer, debugTimeMarker,
                                          15, &soundOutput,
                                          targetSecondsPerFrame);
#endif
                    win32DisplayBufferInWindow(&globalBackbuffer, deviceContext,
                                               dimension.width,
                                               dimension.height);

                    char fpsBuffer[256];
                    _snprintf_s(fpsBuffer, sizeof(fpsBuffer), "%.02fms/f\n",
                                msPerFrame);
                    OutputDebugStringA(fpsBuffer);
                }
            } else {
            }
        } else {
        }
    } else {
    }

    return 0;
}
