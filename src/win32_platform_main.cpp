

#include "project_super.h"
#include "project_super.cpp"

#include <windows.h>
#include <Xinput.h>
#include <Audioclient.h>
#include <mmdeviceapi.h>

#include <math.h>   // for sqrt()
#define Pi32 3.14159265359f

struct win32_dimensions
{
    uint32 width;
    uint32 height;
};

global_variable bool g_Running = true;
global_variable win32_dimensions g_backbuffer_dimensions = {1280, 720};
global_variable BITMAPINFO g_backbuffer_bmpInfo;
global_variable void* g_backbuffer_memory; 

global_variable int32 g_x_offset;
global_variable int32 g_y_offset;

#define XINPUT_LEFT_THUMB_DEADZONE 7849
#define XINPUT_RIGHT_THUMB_DEADZONE 8689
#define XINPUT_TRIGGER_THRESHOLD 30

#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
    return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// NOTE(casey): XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
    return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_


#define REF_MILLI(milli) ((milli) * 10000)
#define REF_SECONDS(seconds) (REF_MILLI(seconds) * 1000)

#define SAFE_RELEASE(punk)  \
              if ((punk) != NULL)  \
                { (punk)->Release(); (punk) = NULL; }

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);

global_variable IMMDeviceEnumerator* g_pEnumerator = 0;
global_variable IMMDevice* g_pDevice = 0;
global_variable IAudioClient* g_pAudioClient = 0;
global_variable IAudioRenderClient *g_pRenderClient = 0;
global_variable uint32 g_nSamplesPerSec = 44100;

internal void
Win32InitSoundDevice()
{
    // TODO(james): Enumerate available sound devices and/or output formats
    
    
    const uint16 nNumChannels = 2;
    const uint16 nNumBitsPerSample = 16;

    // buffer size is expressed in terms of duration (presumably calced against the sample rate etc..)
    REFERENCE_TIME bufferTime = REF_SECONDS(2);

    HRESULT hr = 0;


    hr = CoCreateInstance(
        CLSID_MMDeviceEnumerator, 0, CLSCTX_ALL, 
        IID_IMMDeviceEnumerator, (void**)&g_pEnumerator
        );
    if(FAILED(hr)) { /* TODO(james): log error */ return; }

    hr = g_pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &g_pDevice);
    if(FAILED(hr)) { /* TODO(james): log error */ return; }

    hr = g_pDevice->Activate(IID_IAudioClient, CLSCTX_ALL, 0, (void**)&g_pAudioClient);
    if(FAILED(hr)) { /* TODO(james): log error */ return; }

    // Negotiate an exclusive mode audio stream with the device
    WAVEFORMATEX wave_format = {};
    wave_format.wFormatTag = WAVE_FORMAT_PCM;
    wave_format.nChannels = nNumChannels;
    wave_format.nSamplesPerSec = g_nSamplesPerSec;
    wave_format.nBlockAlign = nNumChannels * nNumBitsPerSample / 8;
    wave_format.nAvgBytesPerSec = g_nSamplesPerSec * wave_format.nBlockAlign;    
    wave_format.wBitsPerSample = nNumBitsPerSample;

    hr = g_pAudioClient->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, &wave_format, 0);  // can't have a closest format in exclusive mode
    if(FAILED(hr)) { /* TODO(james): log error */ return; }

    hr = g_pAudioClient->Initialize(
            AUDCLNT_SHAREMODE_EXCLUSIVE, 0,
            bufferTime, bufferTime, 
            &wave_format, 0
        );
    if(hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED)
    {
        // align the buffer I guess
        UINT32 nFrames = 0;
        hr = g_pAudioClient->GetBufferSize(&nFrames);
        if(FAILED(hr)) { /* TODO(james): log error */ return; }

        bufferTime = (REFERENCE_TIME)((double)REF_SECONDS(1) / g_nSamplesPerSec * nFrames + 0.5);
        hr = g_pAudioClient->Initialize(
            AUDCLNT_SHAREMODE_EXCLUSIVE, 0,
            bufferTime, bufferTime, 
            &wave_format, 0
        );
    }
    if(FAILED(hr)) { /* TODO(james): log error */ return; }

    // Create an event handle to signal Windows that we've got a
    // new audio buffer available
    // HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);  // probably should be global
    // if(!hEvent) { /* TODO(james): logging */ return; }

    // hr = g_pAudioClient->SetEventHandle(hEvent);
    // if(FAILED(hr)) { /* TODO(james): log error */ return; }

    // Get the actual size of the two allocated buffers
    UINT32 nBufferFrameCount = 0;
    hr = g_pAudioClient->GetBufferSize(&nBufferFrameCount);
    if(FAILED(hr)) { /* TODO(james): log error */ return; }

    hr = g_pAudioClient->GetService(IID_IAudioRenderClient, (void**)&g_pRenderClient);
    if(FAILED(hr)) { /* TODO(james): log error */ return; }
}

internal void
Win32RenderAudioTone(real32 fTimeStep, uint16 toneHz)
{
    if( fTimeStep <= 0.0f)
    {
        return;
    }

    uint32 samplesPerPeriod = g_nSamplesPerSec / toneHz;
    uint16 toneVolume = 1000;

    // convert the timestep to the number of samples we need to render
    //   samples = 2 channels of 16 bit audio
    //   LEFT RIGHT | LEFT RIGHT | LEFT RIGHT | ...
    int numRenderSamples = (uint32)(fTimeStep * g_nSamplesPerSec);
    numRenderSamples += numRenderSamples % 2; 

    local_persist uint32 currentAudioIndex = 0;
    
    HRESULT hr = 0;

    BYTE* pBuffer = 0;
    hr = g_pRenderClient->GetBuffer(numRenderSamples, &pBuffer);
    if(SUCCEEDED(hr))
    {
        local_persist real32 tSine;
        
        // copy data to buffer
        int16 *SampleOut = (int16*)pBuffer;
        for(int sampleIndex = 0;
            sampleIndex < numRenderSamples;
            ++sampleIndex)
        {
            real32 SineValue = sinf(tSine);
            int16 SampleValue = (int16)(SineValue * toneVolume);
            *SampleOut++ = SampleValue;
            *SampleOut++ = SampleValue;

            tSine += 2.0f*Pi32*1.0f/(real32)samplesPerPeriod;
        }

        hr = g_pRenderClient->ReleaseBuffer(numRenderSamples, 0);
        if(FAILED(hr))
        {
            // TODO(james): logging
        }
    }

}

internal void
Win32LoadXinput()
{
    HMODULE hXinputLibrary = LoadLibraryA("xinput1_4.dll");

    if(hXinputLibrary)
    {
        XInputGetState = (x_input_get_state *)GetProcAddress(hXinputLibrary, "XInputGetState");
        if(!XInputGetState) {XInputGetState = XInputGetStateStub;}

        XInputSetState = (x_input_set_state *)GetProcAddress(hXinputLibrary, "XInputSetState");
        if(!XInputSetState) {XInputSetState = XInputSetStateStub;}
    }
    else
    {
        // TODO(james): Diagnostic
    }
}


internal win32_dimensions
Win32GetWindowDimensions(HWND hWnd)
{
    win32_dimensions result;

    RECT clientRect;
    GetClientRect(hWnd, &clientRect);
    result.width = clientRect.right - clientRect.left;
    result.height = clientRect.bottom - clientRect.top;

    return result;
}

internal void 
Win32ResizeBackBuffer(uint32 width, uint32 height)
{
    if(g_backbuffer_memory)
    {
        VirtualFree(&g_backbuffer_memory, 0, MEM_RELEASE);
    }

    g_backbuffer_dimensions.width = width;
    g_backbuffer_dimensions.height = height;

    g_backbuffer_bmpInfo.bmiHeader.biSize = sizeof(g_backbuffer_bmpInfo.bmiHeader);
    g_backbuffer_bmpInfo.bmiHeader.biWidth = width;
    g_backbuffer_bmpInfo.bmiHeader.biHeight = height;
    g_backbuffer_bmpInfo.bmiHeader.biPlanes = 1;
    g_backbuffer_bmpInfo.bmiHeader.biBitCount = 32;
    g_backbuffer_bmpInfo.bmiHeader.biCompression = BI_RGB;

    g_backbuffer_memory = VirtualAlloc(0, width*height*4, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE) ;
    // TODO(james): verify that we could allocate the memory...
}

internal void
Win32RenderGradient(int blueOffset, int greenOffset)
{
    uint8* row = (uint8*)g_backbuffer_memory;
    for(uint32 y = 0; y < g_backbuffer_dimensions.height; ++y)
    {
        uint32* pixel = (uint32*)row;
        for(uint32 x = 0; x < g_backbuffer_dimensions.width; ++x)
        {
            uint8 green = (uint8)(y + greenOffset);
            uint8 blue = (uint8)(x + blueOffset);

            *pixel++ = (green << 8) | blue;
        }

        row += g_backbuffer_dimensions.width * 4;   // TODO(james): track the pitch
    }
}

internal void
Win32DisplayBufferInWindow(HDC deviceContext, uint32 windowWidth, uint32 windowHeight)
{
    StretchDIBits(deviceContext,
     0, 0, windowWidth, windowHeight,
     0, 0, g_backbuffer_dimensions.width, g_backbuffer_dimensions.height,
     g_backbuffer_memory,
     &g_backbuffer_bmpInfo,
     DIB_RGB_COLORS, SRCCOPY);
}

internal
LRESULT CALLBACK
Win32WindowProc(
  HWND   hwnd,
  UINT   uMsg,
  WPARAM wParam,
  LPARAM lParam)
{
    LRESULT retValue = 0;

    switch(uMsg)
    {
        case WM_ACTIVATEAPP:
            // TODO(james): handle being set to foreground/background
            break;
        case WM_CLOSE:
            // TODO(james): verify that we should actually close here
            g_Running = false;
            break;
        case WM_SIZE:
            {
                Win32ResizeBackBuffer(LOWORD(lParam), HIWORD(lParam));
            } break;
        case WM_QUIT:
            g_Running = false;
            break;
        case WM_DESTROY:
            g_Running = false;
            break;
        case WM_PAINT:
            {
                PAINTSTRUCT ps;
                HDC deviceContext = BeginPaint(hwnd, &ps);
                
                win32_dimensions dimensions = Win32GetWindowDimensions(hwnd);
                Win32DisplayBufferInWindow(deviceContext, dimensions.width, dimensions.height);

                EndPaint(hwnd, &ps);
                
            } break;
        default:
            retValue = DefWindowProcA(hwnd, uMsg, wParam, lParam);
    }

    return retValue;
}

int CALLBACK
WinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nShowCmd)
{
    HRESULT hr = 0;

    WNDCLASSA wndClass = {};
    wndClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wndClass.lpfnWndProc = Win32WindowProc;
    wndClass.hInstance = hInstance;
    wndClass.lpszClassName = "ProjectSuperWindow";

    RegisterClassA(&wndClass);

    HWND hMainWindow = CreateWindowExA(
            0,
            "ProjectSuperWindow",
            "Project Super",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            1280,
            720,
            0,
            0,
            hInstance,
            0           // TODO(james): pass a pointer to a struct containing the state variables we might want to use in the message handling
        );

    HDC windowDeviceContext = GetDC(hMainWindow);   // CS_OWNDC allows us to get this just once...

    Win32LoadXinput();
    win32_dimensions startDim = Win32GetWindowDimensions(hMainWindow);
    Win32ResizeBackBuffer(startDim.width, startDim.height);

    uint16 toneHz = 262;
    Win32InitSoundDevice();
    hr = g_pAudioClient->Reset();
    Win32RenderAudioTone(1.0f / 60.0f, toneHz); // 4 frames worth of 60 fps timing...
    hr = g_pAudioClient->Start();

    // pre-load the audio buffer with some sound data

    LARGE_INTEGER frequency;
    LARGE_INTEGER currentTicks = {};
    uint64 elapsedMilliseconds = 0;

    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&currentTicks);

    MSG msg;
    while(g_Running)
    {
        if(PeekMessageA(&msg, 0, 0, 0, PM_REMOVE))
        {
            switch(msg.message)
            {
                case WM_QUIT:
                {
                    g_Running = false;
                } break;
                case WM_SYSKEYDOWN:
                case WM_SYSKEYUP:
                case WM_KEYDOWN:
                case WM_KEYUP:
                    {
                        WORD vkCode = LOWORD(msg.wParam);

                        //bool upFlag = (HIWORD(msg.lParam) & KF_UP) == KF_UP;        // transition-state flag, 1 on keyup
                        //bool repeated = (HIWORD(msg.lParam) & KF_REPEAT) == KF_REPEAT;

                        bool altDownFlag = (HIWORD(msg.lParam) & KF_ALTDOWN) == KF_ALTDOWN;

                        switch(vkCode)
                        {
                            case 'W':
                            {
                                g_y_offset += 10;
                            } break;
                            case 'A':
                            {
                                g_x_offset -= 10;
                            } break;
                            case 'S':
                            {
                                g_y_offset -= 10;
                            } break;
                            case 'D':
                            {
                                g_x_offset += 10;
                            } break;
                            case VK_ESCAPE:
                            {
                                g_Running = false;
                            } break;
                            case VK_F4:
                            {
                                if(altDownFlag)
                                {
                                    g_Running = false;
                                }
                            } break;
                        }
                    } break;
                default:
                {
                    TranslateMessage(&msg);
                    DispatchMessageA(&msg);
                } break;
            }
        }

        // TODO(james): Listen to Windows HID events to detect when controllers are active...
        //              Will cause performance stall for non-connected controllers like this...
        for(DWORD dwControllerIndex = 0; dwControllerIndex < XUSER_MAX_COUNT; ++dwControllerIndex)
        {
            XINPUT_STATE state = {};
            DWORD dwResult = XInputGetState(dwControllerIndex, &state);

            if(dwResult == ERROR_SUCCESS)
            {
                // controller is connected
                real32 LX = state.Gamepad.sThumbLX;
                real32 LY = state.Gamepad.sThumbLY;
                real32 RX = state.Gamepad.sThumbRX;
                real32 RY = state.Gamepad.sThumbRY;

                // determine magnitude
                real32 magnitude = sqrtf(LX*LX + LY*LY);
                real32 magR = sqrtf(RX*RX + RY*RY);

                // determine direction
                real32 normalizedLX = LX / magnitude;
                real32 normalizedLY = LY / magnitude;
                //real32 normalizedRX = RX / magR;
                real32 normalizedRY = RY / magR;

                if(magnitude > XINPUT_LEFT_THUMB_DEADZONE)
                {
                    g_x_offset += (int32)(normalizedLX * 5.0f);
                    g_y_offset += (int32)(normalizedLY * 5.0f);

                    // // clip the magnitude
                    // if(magnitude > 32767) magnitude = 32767;

                    // // adjust for deadzone
                    // magnitude -= XINPUT_LEFT_THUMB_DEADZONE;

                    // // normalize the magnitude with respect for the deadzone
                    // real32 normalizedMagnitude = magnitude / (32767 - XINPUT_LEFT_THUMB_DEADZONE);
                }

                if(magR > XINPUT_RIGHT_THUMB_DEADZONE)
                {
                    toneHz += (uint16)(normalizedRY * 2.0f);
                    
                    if(toneHz > 1000)
                    {
                        toneHz = 0;
                    }
                }

                // for now we'll set the vibration of the left and right motors equal to the trigger magnitude
                uint8 leftTrigger = state.Gamepad.bLeftTrigger;
                uint8 rightTrigger = state.Gamepad.bRightTrigger;

                // clamp threshold
                if(leftTrigger < XINPUT_TRIGGER_THRESHOLD) leftTrigger = 0;
                if(rightTrigger < XINPUT_TRIGGER_THRESHOLD) rightTrigger = 0;

                XINPUT_VIBRATION vibration;
                vibration.wLeftMotorSpeed = (WORD)((leftTrigger / 255.0f) * 65535);
                vibration.wRightMotorSpeed = (WORD)((rightTrigger / 255.0f) * 65535);
                XInputSetState(dwControllerIndex, &vibration);
            }
            else
            {
                // controller not connected
            }
        }

        // TODO(james): Use pRenderClient GetBuffer/ReleaseBuffer to write audio data out for playback

        Win32RenderAudioTone(elapsedMilliseconds / 600.0f, toneHz);
        Win32RenderGradient(g_x_offset, g_y_offset);
        
        win32_dimensions dimensions = Win32GetWindowDimensions(hMainWindow);
        Win32DisplayBufferInWindow(windowDeviceContext, dimensions.width, dimensions.height);

        uint64 startTicks = currentTicks.QuadPart;
        QueryPerformanceCounter(&currentTicks);
        elapsedMilliseconds = ((currentTicks.QuadPart - startTicks) * 1000) / frequency.QuadPart;
    }

    SAFE_RELEASE(g_pEnumerator);
    SAFE_RELEASE(g_pDevice);
    SAFE_RELEASE(g_pAudioClient);
    SAFE_RELEASE(g_pRenderClient);   

    return 0;
}
