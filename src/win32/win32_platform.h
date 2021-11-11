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
    HDC hDeviceContext;
    HGLRC hGlContext;
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

#define WIN32_STATE_FILE_NAME_COUNT MAX_PATH
struct win32_state
{
    HANDLE hInputRecordHandle;
    
    char EXEFolder[WIN32_STATE_FILE_NAME_COUNT];
    char EXEFilename[WIN32_STATE_FILE_NAME_COUNT];
    
    HINSTANCE Instance;
    Win32WindowContext mainWindow;    
};

struct win32_loaded_code
{
    b32x isValid;
    u32 nTempDLLNumber;
    
    char *pszTransientDLLName;
    char *pszDLLName;
    //char *pszLockFullPath;
    
    HMODULE hDLL;
    FILETIME ftLastFileWriteTime;
    
    u32 nFunctionCount;
    char **ppszFunctionNames;
    void **ppFunctions;
};

struct win32_game_function_table
{
    game_update_and_render* GameUpdateAndRender;
};
global_variable char *Win32GameFunctionTableNames[] = {
    "GameUpdateAndRender"
};

#endif