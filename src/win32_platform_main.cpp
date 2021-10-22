

//#include "project_super.h"
#include "project_super.cpp"

#include <windows.h>
#include <Xinput.h>
#include <Audioclient.h>
#include <mmdeviceapi.h>

#include <stdio.h>
#include <math.h>   // for sqrt()

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

global_variable bool GlobalRunning = true;
global_variable int64 GlobalFrequency;

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


#define REF_MILLI(milli) ((REFERENCE_TIME)((milli) * 10000))
#define REF_SECONDS(seconds) (REF_MILLI(seconds) * 1000)

#define SAFE_RELEASE(punk)  \
              if ((punk) != NULL)  \
                { (punk)->Release(); (punk) = NULL; }

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);

inline internal void
Win32InitClockFrequency()
{
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    GlobalFrequency = freq.QuadPart;
}

inline internal Win32Clock
Win32GetWallClock()
{
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return Win32Clock{(int64)counter.QuadPart};
}

inline internal real32
Win32GetElapsedTime(Win32Clock& clock)
{
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    real32 elapsed = (real32)(counter.QuadPart - clock.counter) / (real32)GlobalFrequency;
    return elapsed;
}

inline internal real32
Win32GetElapsedTime(Win32Clock& start, Win32Clock& end)
{
    return (real32)(end.counter - start.counter) / (real32)GlobalFrequency;
}

internal void
Win32InitSoundDevice(Win32AudioContext& audio)
{
    // TODO(james): Enumerate available sound devices and/or output formats
    const uint32 samplesPerSecond = 48000;
    const uint16 nNumChannels = 2;
    const uint16 nNumBitsPerSample = 16;

    // now acquire access to the audio hardware

    // buffer size is expressed in terms of duration (presumably calced against the sample rate etc..)
    REFERENCE_TIME bufferTime = REF_SECONDS(1.5/60.0);

    HRESULT hr = 0;

    hr = CoCreateInstance(
        CLSID_MMDeviceEnumerator, 0, CLSCTX_ALL, 
        IID_IMMDeviceEnumerator, (void**)&audio.pEnumerator
        );
    if(FAILED(hr)) { /* TODO(james): log error */ return; }

    hr = audio.pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &audio.pDevice);
    if(FAILED(hr)) { /* TODO(james): log error */ return; }

    hr = audio.pDevice->Activate(IID_IAudioClient, CLSCTX_ALL, 0, (void**)&audio.pClient);
    if(FAILED(hr)) { /* TODO(james): log error */ return; }

    // Negotiate an exclusive mode audio stream with the device
    WAVEFORMATEX wave_format = {};
    wave_format.wFormatTag = WAVE_FORMAT_PCM;
    wave_format.nChannels = nNumChannels;
    wave_format.nSamplesPerSec = samplesPerSecond;
    wave_format.nBlockAlign = nNumChannels * nNumBitsPerSample / 8;
    wave_format.nAvgBytesPerSec = samplesPerSecond * wave_format.nBlockAlign;    
    wave_format.wBitsPerSample = nNumBitsPerSample;

    REFERENCE_TIME defLatency = 0;
    REFERENCE_TIME minLatency = 0;
    hr = audio.pClient->GetDevicePeriod(&defLatency, &minLatency);
    if(FAILED(hr)) { /* TODO(james): log error */ return; }

    char szOut[256];
    _snprintf_s(szOut, 256, "Audio Caps\n\tDefault Period: %.02f ms\n\tMinimum Period: %.02f ms\n\tRequested Period: %.02f ms\n", defLatency/10000.0f, minLatency/10000.0f, bufferTime/10000.0f);
    OutputDebugStringA(szOut);

    hr = audio.pClient->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, &wave_format, 0);  // can't have a closest format in exclusive mode
    if(FAILED(hr)) { /* TODO(james): log error */ return; }
    

    hr = audio.pClient->Initialize(
            AUDCLNT_SHAREMODE_EXCLUSIVE, 0,
            bufferTime, bufferTime, 
            &wave_format, 0
        );
    if(hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED)
    {
        // align the buffer I guess
        UINT32 nFrames = 0;
        hr = audio.pClient->GetBufferSize(&nFrames);
        if(FAILED(hr)) { /* TODO(james): log error */ return; }

        bufferTime = (REFERENCE_TIME)((double)REF_SECONDS(1) / samplesPerSecond * nFrames + 0.5);
        hr = audio.pClient->Initialize(
            AUDCLNT_SHAREMODE_EXCLUSIVE, 0,
            bufferTime, bufferTime, 
            &wave_format, 0
        );
    }
    if(FAILED(hr)) { /* TODO(james): log error */ return; }

    REFERENCE_TIME maxLatency = 0;
    hr = audio.pClient->GetStreamLatency(&maxLatency);
    if(FAILED(hr)) { /* TODO(james): log error */ return; }
    char szMaxLatency[256];
    _snprintf_s(szMaxLatency, 256, "\tMax Latency: %.02f ms\n", maxLatency/10000.0f);
    OutputDebugStringA(szMaxLatency);
    // Create an event handle to signal Windows that we've got a
    // new audio buffer available
    // HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);  // probably should be global
    // if(!hEvent) { /* TODO(james): logging */ return; }

    // hr = g_pAudioClient->SetEventHandle(hEvent);
    // if(FAILED(hr)) { /* TODO(james): log error */ return; }

    // Get the actual size of the two allocated buffers
    UINT32 nBufferFrameCount = 0;
    hr = audio.pClient->GetBufferSize(&nBufferFrameCount);
    if(FAILED(hr)) { /* TODO(james): log error */ return; }

    audio.targetBufferFill = nBufferFrameCount/2;   // since we doubled the frame size, aim to keep the buffer half full

    hr = audio.pClient->GetService(IID_IAudioRenderClient, (void**)&audio.pRenderClient);
    if(FAILED(hr)) { /* TODO(james): log error */ return; }

        // setup the game audio buffer

    audio.gameAudioBuffer.samplesPerSecond = samplesPerSecond;
    audio.gameAudioBuffer.bufferSize = nBufferFrameCount * 4; // buffer size here is only as big as the buffer size of the main audio buffer
    audio.gameAudioBuffer.samplesRequested = audio.targetBufferFill; // request a full buffer initially
    audio.gameAudioBuffer.buffer = VirtualAlloc(0, audio.gameAudioBuffer.bufferSize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

    // now reset the buffer
    {
        uint8* pBuffer = (uint8*)audio.gameAudioBuffer.buffer;
        for(uint32 i = 0; i < audio.gameAudioBuffer.bufferSize; ++i)
        {   
            *pBuffer++ = 0;
        }
    }

    // flush the initial buffer
    // BYTE* pBuffer = 0;
    // hr = audio.pRenderClient->GetBuffer(nBufferFrameCount, &pBuffer);
    // if(SUCCEEDED(hr))
    // {
    //     memset(pBuffer, 0, nBufferFrameCount*4);
    //     audio.pRenderClient->ReleaseBuffer(nBufferFrameCount, 0);
    // }
}

internal void
Win32CopyAudioBuffer(Win32AudioContext& audio, float fFrameTimeStep)
{
    HRESULT hr = 0;

    UINT32 curPadding = 0;
    audio.pClient->GetCurrentPadding(&curPadding);
    real32 curAudioLatencyMS = (real32)curPadding/(real32)audio.gameAudioBuffer.samplesPerSecond * 1000.0f;

    BYTE* pBuffer = 0;
    uint32 numSamples = audio.gameAudioBuffer.samplesWritten;
    int32 maxAvailableBuffer = audio.targetBufferFill - curPadding;

    char szOut[256];
    _snprintf_s(szOut, 256, "Cur Latency: %.02f ms\tAvail. Buffer: %d\tPadding: %d\tWrite Frame Samples: %d\n", curAudioLatencyMS, maxAvailableBuffer, curPadding, numSamples);
    OutputDebugStringA(szOut);

    //ASSERT(numSamples <= maxAvailableBuffer);
    // update the number of samples requested from the next frame
    // based on the amount of available buffer space and the current
    // frame rate
    audio.gameAudioBuffer.samplesRequested = 
        (uint32)(fFrameTimeStep * audio.gameAudioBuffer.samplesPerSecond)
        + (maxAvailableBuffer - numSamples);

    hr = audio.pRenderClient->GetBuffer(numSamples, &pBuffer);
    if(SUCCEEDED(hr))
    {
        // copy data to buffer
        int16 *SampleOut = (int16*)pBuffer;
        int16 *SampleIn = (int16*)audio.gameAudioBuffer.buffer;
        for(uint32 sampleIndex = 0;
            sampleIndex < numSamples;
            ++sampleIndex)
        {
            *SampleOut++ = *SampleIn++;
            *SampleOut++ = *SampleIn++;
        }

        hr = audio.pRenderClient->ReleaseBuffer(numSamples, 0);
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


internal Win32Dimensions
Win32GetWindowDimensions(HWND hWnd)
{
    Win32Dimensions result;

    RECT clientRect;
    GetClientRect(hWnd, &clientRect);
    result.width = clientRect.right - clientRect.left;
    result.height = clientRect.bottom - clientRect.top;

    return result;
}

internal void 
Win32ResizeBackBuffer(GraphicsContext& graphics, uint32 width, uint32 height)
{
    if(graphics.buffer)
    {
        VirtualFree(&graphics.buffer, 0, MEM_RELEASE);
    }

    graphics.buffer_width = width;
    graphics.buffer_height = height;
    graphics.buffer_pitch = width * 4;
    
    graphics.buffer = VirtualAlloc(0, width*height*4, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE) ;
    // TODO(james): verify that we could allocate the memory...
}



internal void
Win32DisplayBufferInWindow(HDC deviceContext, uint32 windowWidth, uint32 windowHeight, GraphicsContext& graphics)
{
    BITMAPINFO bmpInfo = {};
    bmpInfo.bmiHeader.biSize = sizeof(bmpInfo.bmiHeader);
    bmpInfo.bmiHeader.biWidth = graphics.buffer_width;
    bmpInfo.bmiHeader.biHeight = graphics.buffer_height;
    bmpInfo.bmiHeader.biPlanes = 1;
    bmpInfo.bmiHeader.biBitCount = 32;
    bmpInfo.bmiHeader.biCompression = BI_RGB;

    StretchDIBits(deviceContext,
     0, 0, windowWidth, windowHeight,
     0, 0, graphics.buffer_width, graphics.buffer_height,
     graphics.buffer,
     &bmpInfo,
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
    Win32WindowContext* pContext = (Win32WindowContext*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);

    switch(uMsg)
    {
        case WM_ACTIVATEAPP:
            // TODO(james): handle being set to foreground/background
            break;
        case WM_CLOSE:
            // TODO(james): verify that we should actually close here
            GlobalRunning = false;
            break;
        case WM_SIZE:
            {
                Win32ResizeBackBuffer(pContext->graphics, LOWORD(lParam), HIWORD(lParam));
            } break;
        case WM_QUIT:
            GlobalRunning = false;
            break;
        case WM_DESTROY:
            GlobalRunning = false;
            break;
        case WM_PAINT:
            {
                PAINTSTRUCT ps;
                HDC deviceContext = BeginPaint(hwnd, &ps);
                
                Win32Dimensions dimensions = Win32GetWindowDimensions(hwnd);
                Win32DisplayBufferInWindow(deviceContext, dimensions.width, dimensions.height, pContext->graphics);

                EndPaint(hwnd, &ps);
                
            } break;
        default:
            retValue = DefWindowProcA(hwnd, uMsg, wParam, lParam);
    }

    return retValue;
}

inline internal void
Win32ProcessKeyboardButton(InputButton& newState, const InputButton& oldState, bool pressed)
{
    newState.pressed = pressed;
    newState.transitions = oldState.pressed == pressed ? 0 : 1; // only transitioned if the new state doesn't match the old
}

internal void
Win32ProcessGamepadStick(Thumbstick& stick, SHORT x, SHORT y, const SHORT deadzone)
{
    real32 fX = (real32)x;
    real32 fY = (real32)y;
    // determine magnitude
    real32 magnitude = sqrtf(fX*fX + fY*fY);
    
    // determine direction
    real32 normalizedLX = fX / magnitude;
    real32 normalizedLY = fY / magnitude;

    if(magnitude > deadzone)
    {
        // clip the magnitude
        if(magnitude > 32767) magnitude = 32767;
        // adjust for deadzone
        magnitude -= XINPUT_LEFT_THUMB_DEADZONE;
        // normalize the magnitude with respect for the deadzone
        real32 normalizedMagnitude = magnitude / (32767 - XINPUT_LEFT_THUMB_DEADZONE);

        // produces values between 0..1 with respect to the deadzone
        stick.x = normalizedLX * normalizedMagnitude; 
        stick.y = normalizedLY * normalizedMagnitude;
    }
    else
    {
        stick.x = 0;
        stick.y = 0;
    }
}

int CALLBACK
WinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nShowCmd)
{
    Win32InitClockFrequency();

    GameContext gameContext = {};
    gameContext.persistantMemory.size = Megabytes(64);
    gameContext.transientMemory.size = Gigabytes(4);
    LPVOID baseAddress = 0;
#ifdef PROJECTSUPER_INTERNAL
    baseAddress = (LPVOID)Terabytes(2);
#endif
    // TODO(james): include some extra memory for some fences to check for memory overwrites
    uint64 memorySize = gameContext.persistantMemory.size + gameContext.transientMemory.size;
    uint8* memory = (uint8*)VirtualAlloc(baseAddress, memorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

    if(!memory)
    {
        // failed to allocate enough memory for the game... might as well quit now
        // TODO(james): Log the reason...
        return 0x80000000;
    }

    gameContext.persistantMemory.basePointer = memory;
    gameContext.persistantMemory.freePointer = memory;
    memory += gameContext.persistantMemory.size;
    gameContext.transientMemory.basePointer = memory;
    gameContext.transientMemory.freePointer = memory;
    

    // NOTE(james): Set the windows scheduler granularity to 1ms so that our sleep can be more granular
    bool32 bSleepIsMs = timeBeginPeriod(1) == TIMERR_NOERROR;

    Win32WindowContext mainWindowContext = {};

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
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            1280,
            720,
            0,
            0,
            hInstance,
            0          
        );
    mainWindowContext.hWindow = hMainWindow;
    SetWindowLongPtrA(hMainWindow, GWLP_USERDATA, (LONG_PTR)&mainWindowContext);
    ShowWindow(hMainWindow, nShowCmd);

    HDC windowDeviceContext = GetDC(hMainWindow);   // CS_OWNDC allows us to get this just once...

    // TODO(james): pull the refresh rate from the monitor
    int targetRefreshRateHz = 60;
    real32 targetFrameRateSeconds = 1.0f / targetRefreshRateHz;

    Win32LoadXinput();
    Win32Dimensions startDim = Win32GetWindowDimensions(hMainWindow);
    Win32ResizeBackBuffer(mainWindowContext.graphics, startDim.width, startDim.height);

    Win32AudioContext audio = {};
    Win32InitSoundDevice(audio);
    hr = audio.pClient->Reset();
    hr = audio.pClient->Start();

    InputContext input = {};
    
 
    Win32Clock frameCounterTime = Win32GetWallClock();
    Win32Clock lastFrameStartTime = Win32GetWallClock();

    MSG msg;
    while(GlobalRunning)
    {
        

        InputController keyboard = {};
        keyboard.isConnected = true;
        keyboard.isAnalog = false;

        if(PeekMessageA(&msg, 0, 0, 0, PM_REMOVE))
        {
            switch(msg.message)
            {
                case WM_QUIT:
                {
                    GlobalRunning = false;
                } break;
                case WM_SYSKEYDOWN:
                case WM_SYSKEYUP:
                case WM_KEYDOWN:
                case WM_KEYUP:
                    {
                        WORD vkCode = LOWORD(msg.wParam);

                        bool upFlag = (HIWORD(msg.lParam) & KF_UP) == KF_UP;        // transition-state flag, 1 on keyup
                        //bool repeated = (HIWORD(msg.lParam) & KF_REPEAT) == KF_REPEAT;
                        //WORD repeatCount = LOWORD(msg.lParam);

                        bool altDownFlag = (HIWORD(msg.lParam) & KF_ALTDOWN) == KF_ALTDOWN;

                        switch(vkCode)
                        {
                            case 'W':
                            {
                                Win32ProcessKeyboardButton(keyboard.up, input.controllers[0].up, !upFlag);
                            } break;
                            case 'A':
                            {
                                Win32ProcessKeyboardButton(keyboard.left, input.controllers[0].left, !upFlag);
                            } break;
                            case 'S':
                            {
                                Win32ProcessKeyboardButton(keyboard.down, input.controllers[0].down, !upFlag);
                            } break;
                            case 'D':
                            {
                                Win32ProcessKeyboardButton(keyboard.right, input.controllers[0].right, !upFlag);
                            } break;
                            case 'Q':
                            {
                                Win32ProcessKeyboardButton(keyboard.leftShoulder, input.controllers[0].leftShoulder, !upFlag);
                            } break;
                            case 'E':
                            {
                                Win32ProcessKeyboardButton(keyboard.rightShoulder, input.controllers[0].rightShoulder, !upFlag);
                            } break;
                            case VK_NUMPAD1:
                            {
                                Win32ProcessKeyboardButton(keyboard.x, input.controllers[0].x, !upFlag);
                            } break;
                            case VK_NUMPAD2:
                            {
                                Win32ProcessKeyboardButton(keyboard.a, input.controllers[0].a, !upFlag);
                            } break;
                            case VK_NUMPAD3:
                            {
                                Win32ProcessKeyboardButton(keyboard.b, input.controllers[0].b, !upFlag);
                            } break;
                            case VK_NUMPAD4:
                            {
                                Win32ProcessKeyboardButton(keyboard.y, input.controllers[0].y, !upFlag);
                            } break;
                            case VK_ESCAPE:
                            {
                                GlobalRunning = false;
                            } break;
                            case VK_F4:
                            {
                                if(altDownFlag)
                                {
                                    GlobalRunning = false;
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

        input.controllers[0] = keyboard;

        // TODO(james): Listen to Windows HID events to detect when controllers are active...
        //              Will cause performance stall for non-connected controllers like this...
        for(DWORD dwControllerIndex = 0; dwControllerIndex < XUSER_MAX_COUNT; ++dwControllerIndex)
        {
            XINPUT_STATE state = {};
            DWORD dwResult = XInputGetState(dwControllerIndex, &state);

            InputController& gamepad = input.controllers[dwControllerIndex+1];
            gamepad.isAnalog = true;

            if(dwResult == ERROR_SUCCESS)
            {
                gamepad.isConnected = true;
                // controller is connected
                Win32ProcessGamepadStick(gamepad.leftStick, state.Gamepad.sThumbLX, state.Gamepad.sThumbLY, XINPUT_LEFT_THUMB_DEADZONE);
                Win32ProcessGamepadStick(gamepad.rightStick, state.Gamepad.sThumbRX, state.Gamepad.sThumbRY, XINPUT_RIGHT_THUMB_DEADZONE);

                // for now we'll set the vibration of the left and right motors equal to the trigger magnitude
                uint8 leftTrigger = state.Gamepad.bLeftTrigger;
                uint8 rightTrigger = state.Gamepad.bRightTrigger;

                // clamp threshold
                if(leftTrigger < XINPUT_TRIGGER_THRESHOLD) leftTrigger = 0;
                if(rightTrigger < XINPUT_TRIGGER_THRESHOLD) rightTrigger = 0;

                gamepad.leftTrigger.value = leftTrigger / 255.0f;
                gamepad.rightTrigger.value = rightTrigger / 255.0f;

                WORD dwButtonIds[14] = {
                    XINPUT_GAMEPAD_DPAD_UP, XINPUT_GAMEPAD_DPAD_DOWN, XINPUT_GAMEPAD_DPAD_LEFT, XINPUT_GAMEPAD_DPAD_RIGHT,
                    XINPUT_GAMEPAD_Y, XINPUT_GAMEPAD_A, XINPUT_GAMEPAD_X, XINPUT_GAMEPAD_B,
                    XINPUT_GAMEPAD_LEFT_SHOULDER, XINPUT_GAMEPAD_RIGHT_SHOULDER,
                    XINPUT_GAMEPAD_LEFT_THUMB, XINPUT_GAMEPAD_RIGHT_THUMB,
                    XINPUT_GAMEPAD_START, XINPUT_GAMEPAD_BACK
                };

                for(int buttonIndex = 0; buttonIndex < 14; ++buttonIndex)
                {
                    InputButton& newButton = gamepad.buttons[buttonIndex];
                    InputButton oldButton = gamepad.buttons[buttonIndex];
                    bool pressed = (state.Gamepad.wButtons & dwButtonIds[buttonIndex]) == dwButtonIds[buttonIndex];
                    Win32ProcessKeyboardButton(newButton, oldButton, pressed);
                }
            }
            else
            {
                // controller not connected
                gamepad.isConnected = false;
            }
        }

        GameUpdateAndRender(gameContext, mainWindowContext.graphics, input, audio.gameAudioBuffer);
        
        Win32Clock gameSimTime = Win32GetWallClock();
        real32 elapsedFrameTime = Win32GetElapsedTime(lastFrameStartTime, gameSimTime);

        if(elapsedFrameTime < targetFrameRateSeconds)
        {
            while(elapsedFrameTime < (targetFrameRateSeconds))
            {
                if(bSleepIsMs)
                {
                    // truncate to whole milliseconds
                    DWORD dwSleepTimeMS = (DWORD)((targetFrameRateSeconds - elapsedFrameTime) * 1000.0f);
                    Sleep(dwSleepTimeMS);
                }
                elapsedFrameTime = Win32GetElapsedTime(lastFrameStartTime);
            }
        }
        else
        {
            // oops... we missed our frametime limit here...
            // TODO(james): log this out
        }
        gameContext.clock.elapsedFrameTime = elapsedFrameTime;
        lastFrameStartTime = Win32GetWallClock();
        
        for(uint32 gamepadIndex = 1; gamepadIndex < 5; ++gamepadIndex)
        {
            InputController& gamepad = input.controllers[gamepadIndex];
            if(gamepad.isConnected)
            {
                XINPUT_VIBRATION vibration;
                vibration.wLeftMotorSpeed = (WORD)(gamepad.leftFeedbackMotor * 65535);
                vibration.wRightMotorSpeed = (WORD)(gamepad.rightFeedbackMotor * 65535);
                XInputSetState(gamepadIndex-1, &vibration);
            }
        }

        Win32CopyAudioBuffer(audio, targetFrameRateSeconds);
        
        Win32Dimensions dimensions = Win32GetWindowDimensions(hMainWindow);
        Win32DisplayBufferInWindow(windowDeviceContext, dimensions.width, dimensions.height, mainWindowContext.graphics);


        ++gameContext.clock.frameCounter;
    }

    SAFE_RELEASE(audio.pEnumerator);
    SAFE_RELEASE(audio.pDevice);
    SAFE_RELEASE(audio.pClient);
    SAFE_RELEASE(audio.pRenderClient);   

    return 0;
}
