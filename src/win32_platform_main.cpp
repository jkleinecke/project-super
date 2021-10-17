

//#include "project_super.h"
#include "project_super.cpp"

#include <windows.h>
#include <Xinput.h>
#include <Audioclient.h>
#include <mmdeviceapi.h>

#include <math.h>   // for sqrt()

struct Win32WindowContext
{
    HWND hWindow;
    GraphicsContext graphics;
};

struct win32_dimensions
{
    uint32 width;
    uint32 height;
};

global_variable bool g_Running = true;

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
            g_Running = false;
            break;
        case WM_SIZE:
            {
                Win32ResizeBackBuffer(pContext->graphics, LOWORD(lParam), HIWORD(lParam));
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
                Win32DisplayBufferInWindow(deviceContext, dimensions.width, dimensions.height, pContext->graphics);

                EndPaint(hwnd, &ps);
                
            } break;
        default:
            retValue = DefWindowProcA(hwnd, uMsg, wParam, lParam);
    }

    return retValue;
}

internal void
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
    //real32 normalizedRX = RX / magR;
    //real32 normalizedRY = RY / magR;

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

    Win32LoadXinput();
    win32_dimensions startDim = Win32GetWindowDimensions(hMainWindow);
    Win32ResizeBackBuffer(mainWindowContext.graphics, startDim.width, startDim.height);

    uint16 toneHz = 262;
    Win32InitSoundDevice();
    hr = g_pAudioClient->Reset();
    Win32RenderAudioTone(1.0f / 60.0f, toneHz); // 4 frames worth of 60 fps timing...
    hr = g_pAudioClient->Start();

    InputContext input = {};
    
    // pre-load the audio buffer with some sound data

    LARGE_INTEGER frequency;
    LARGE_INTEGER currentTicks = {};
    uint64 elapsedMilliseconds = 0;

    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&currentTicks);

    MSG msg;
    while(g_Running)
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
                    g_Running = false;
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

        // TODO(james): Use pRenderClient GetBuffer/ReleaseBuffer to write audio data out for playback

        Win32RenderAudioTone(elapsedMilliseconds / 600.0f, toneHz);
        //Win32RenderGradient(g_x_offset, g_y_offset);
        GameUpdateAndRender(mainWindowContext.graphics, input);

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
        
        win32_dimensions dimensions = Win32GetWindowDimensions(hMainWindow);
        Win32DisplayBufferInWindow(windowDeviceContext, dimensions.width, dimensions.height, mainWindowContext.graphics);

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
