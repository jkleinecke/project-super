#pragma once
#ifndef WIN32_PLATFORM_MAIN_H_
#define WIN32_PLATFORM_MAIN_H_

#include <windows.h>
#include <Audioclient.h>
#include <mmdeviceapi.h>

#include "../ps_platform.h"

struct Win32WindowContext
{
    HWND hWindow;
    GraphicsContext graphics;
};

struct Win32AudioContext
{
    IMMDeviceEnumerator* pEnumerator;
    IMMDevice* pDevice;
    IAudioClient* pClient;
    IAudioRenderClient* pRenderClient;
    AudioContext gameAudioBuffer;

    uint32 targetBufferFill;
};

struct Win32Clock
{
    int64 counter;
};

struct Win32Dimensions
{
    uint32 width;
    uint32 height;
};

struct Win32GameCode
{
    HMODULE hLibrary;
    FILETIME ftLastFileWriteTime;

    bool isValid;
    game_update_and_render* GameUpdateAndRender;
};

#endif