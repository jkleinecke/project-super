

#include "ps_platform.h"
#include "ps_shared.h"
#include "ps_intrinsics.h"
#include "ps_math.h"

#include <windows.h>
#include <stdio.h>
#include <math.h>   // for sqrt()

#include "win32_platform.h"
#include "win32_log.cpp"

#include "win32_audio.cpp"
#include "win32_xinput.cpp"
#include "win32_file.cpp"

// TODO(james): make this work as a loaded dll
//#include "win32_opengl.cpp"
#include "win32_d3d12.cpp"

/*******************************************************************************

    Things to do:
    1) Threading + Work Queue
    2) File API
       - Streaming API
    3) WM_ACTIVEAPP
    4) Harden sound latency with framerate
    5) Hardware Capabilities
    6) Finish logging system

********************************************************************************/

GAME_UPDATE_AND_RENDER(GameUpdateAndRenderStub) {}

global_variable bool GlobalRunning = true;
global_variable int64 GlobalFrequency;


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

inline internal Win32Dimensions
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

internal bool32
Win32BeginRecordingInput(win32_state& state, const GameContext& memory)
{
    // maybe verify that a file isn't open?
    state.hInputRecordHandle = CreateFileA("recorded_input.psi", GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);

    // save out game state
    Win32WriteMemoryArena(state.hInputRecordHandle, memory.persistantMemory);
    Win32WriteMemoryArena(state.hInputRecordHandle, memory.transientMemory);

    return state.hInputRecordHandle != INVALID_HANDLE_VALUE;
}

internal void
Win32RecordInput(win32_state& state, const InputContext& input)
{
    DWORD dwBytesWritten = 0;
    WriteFile(state.hInputRecordHandle, &input, sizeof(input), &dwBytesWritten, 0);
}

internal void
Win32StopRecordingInput(win32_state& state)
{
    CloseHandle(state.hInputRecordHandle);
    state.hInputRecordHandle = 0;
}

internal bool32
Win32BeginInputPlayback(win32_state& state, GameContext& memory)
{
    state.hInputRecordHandle = CreateFileA("recorded_input.psi", GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);

    // read in game state
    Win32ReadMemoryArena(state.hInputRecordHandle, memory.persistantMemory);
    Win32ReadMemoryArena(state.hInputRecordHandle, memory.transientMemory);

    return state.hInputRecordHandle != INVALID_HANDLE_VALUE;
}

internal bool32
Win32PlaybackInput(win32_state& state, InputContext& input)
{
    DWORD dwBytesRead = 0;
    ReadFile(state.hInputRecordHandle, &input, sizeof(input), &dwBytesRead, 0);
    return dwBytesRead == sizeof(input);
}

internal void
Win32StopInputPlayback(win32_state& state)
{
    CloseHandle(state.hInputRecordHandle);
    state.hInputRecordHandle = 0;
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
    //Win32Window* pContext = (Win32Window*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);

    switch(uMsg)
    {
        case WM_ACTIVATEAPP:
            // TODO(james): handle being set to foreground/background
            break;
        case WM_CLOSE:
            // TODO(james): verify that we should actually close here
            GlobalRunning = false;
            break;
        //case WM_SIZE:
            //{
                //Win32ResizeBackBuffer(pContext->graphics, LOWORD(lParam), HIWORD(lParam));
            //} break;
        case WM_QUIT:
            GlobalRunning = false;
            break;
        case WM_DESTROY:
            //GlobalRunning = false;
            break;
        //case WM_PAINT:
            //{
                //PAINTSTRUCT ps;
                //HDC deviceContext = BeginPaint(hwnd, &ps);
                //
                //Win32Dimensions dimensions = Win32GetWindowDimensions(hwnd);
                //Win32DisplayBufferInWindow(deviceContext, dimensions.width, dimensions.height, pContext->graphics);

                //EndPaint(hwnd, &ps);
                
            //} break;
        default:
            retValue = DefWindowProcA(hwnd, uMsg, wParam, lParam);
    }

    return retValue;
}

inline internal void
Win32ProcessKeyboardButton(InputButton& newState, bool pressed)
{
    newState.pressed = pressed;
    newState.transitions = 1; 
}

enum RunLoopMode
{
    RLM_NORMAL,
    RLM_RECORDINPUT,
    RLM_PLAYBACKINPUT
};

extern "C" int __stdcall WinMainCRTStartup()
{
    SetDefaultFPBehavior();

    HINSTANCE hInstance = GetModuleHandle(0);

    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    WNDCLASSEXA wndClass = {};
    wndClass.cbSize = sizeof(wndClass);
    wndClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wndClass.lpfnWndProc = Win32WindowProc;
    wndClass.hInstance = hInstance;
    wndClass.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wndClass.lpszClassName = "ProjectSuperWindow";
    RegisterClassExA(&wndClass);

    RECT rc = { 0, 0, 3440, 1440 };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, 0);
    
    HWND hMainWindow = CreateWindowExA(0,
                                       "ProjectSuperWindow", "Project Super",
                                       WS_OVERLAPPEDWINDOW,
                                       CW_USEDEFAULT, CW_USEDEFAULT,
                                       rc.right-rc.left, rc.bottom-rc.top,
                                       0, 0,
                                       hInstance, 0   
                                       );
    
            
    Win32InitClockFrequency();

    win32_state win32State = {};
    Win32GetExecutablePath(win32State);
  
    
    GameContext gameContext = {};
    gameContext.persistantMemory.size = Megabytes(64);
    gameContext.transientMemory.size = Gigabytes(1);
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
    
    // NOTE: this is temporary
    gameContext.logger = Win32Log;

    // NOTE(james): Set the windows scheduler granularity to 1ms so that our sleep can be more granular
    bool32 bSleepIsMs = timeBeginPeriod(1) == TIMERR_NOERROR;

    Win32Window& mainWindow = win32State.mainWindow;
    mainWindow.hWindow = hMainWindow;
    mainWindow.hDeviceContext = GetDC(hMainWindow);

    Win32Dimensions startDim = Win32GetWindowDimensions(mainWindow.hWindow);
    Win32ResizeBackBuffer(mainWindow.graphics, startDim.width, startDim.height);

    HRESULT hr = 0;
    
    // TODO(james): Generalize this part to support more than one window? 
    //Win32InitOpenGL(mainWindow);
    Win32LoadRenderer(mainWindow);
    
    SetWindowLongPtrA(mainWindow.hWindow, GWLP_USERDATA, (LONG_PTR)&mainWindow);
    ShowWindow(mainWindow.hWindow, SW_SHOW);
    
    // TODO(james): pull the refresh rate from the monitor
    int targetRefreshRateHz = 60;
    real32 targetFrameRateSeconds = 1.0f / targetRefreshRateHz;

    Win32LoadXinput();

    Win32AudioContext audio = {};
    Win32InitSoundDevice(audio);
    hr = audio.pClient->Reset();
    hr = audio.pClient->Start();

    InputContext input = {};
     
    Win32Clock frameCounterTime = Win32GetWallClock();
    Win32Clock lastFrameStartTime = Win32GetWallClock();

    win32_game_function_table gameFunctions = {};
    win32_loaded_code gameCode = {};
    gameCode.pszDLLName = "ps_game.dll";
    gameCode.pszTransientDLLName = "ps_game_temp.dll";
    gameCode.nFunctionCount = ARRAY_COUNT(Win32GameFunctionTableNames);
    gameCode.ppFunctions = (void**)&gameFunctions;
    gameCode.ppszFunctionNames = (char**)&Win32GameFunctionTableNames;
    
    Win32LoadCode(win32State, gameCode);
    ASSERT(gameCode.isValid);

    RunLoopMode runMode = RLM_NORMAL;

    MSG msg;
    while(GlobalRunning)
    {
        FILETIME ftLastWriteTime = Win32GetFileWriteTime(gameCode.pszDLLName);
        if(CompareFileTime(&ftLastWriteTime, &gameCode.ftLastFileWriteTime) != 0)
        {
            Win32UnloadGameCode(gameCode);
            Win32LoadCode(win32State, gameCode);
        }

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
                        bool repeated = (HIWORD(msg.lParam) & KF_REPEAT) == KF_REPEAT;
                        if(repeated && !upFlag)
                        {
                            break;
                        }
                        //WORD repeatCount = LOWORD(msg.lParam);

                        bool altDownFlag = (HIWORD(msg.lParam) & KF_ALTDOWN) == KF_ALTDOWN;

                        switch(vkCode)
                        {
                            case 'W':
                            {
                                Win32ProcessKeyboardButton(keyboard.up, !upFlag);
                            } break;
                            case 'A':
                            {
                                Win32ProcessKeyboardButton(keyboard.left, !upFlag);
                            } break;
                            case 'S':
                            {
                                Win32ProcessKeyboardButton(keyboard.down, !upFlag);
                            } break;
                            case 'D':
                            {
                                Win32ProcessKeyboardButton(keyboard.right, !upFlag);
                            } break;
                            case '5':
                            case 'Q':
                            {
                                Win32ProcessKeyboardButton(keyboard.leftShoulder, !upFlag);
                            } break;
                            case '6':
                            case 'E':
                            {
                                Win32ProcessKeyboardButton(keyboard.rightShoulder, !upFlag);
                            } break;
                            case '1':
                            {
                                Win32ProcessKeyboardButton(keyboard.x, !upFlag);
                            } break;
                            case '2':
                            {
                                Win32ProcessKeyboardButton(keyboard.a, !upFlag);
                            } break;
                            case '3':
                            {
                                Win32ProcessKeyboardButton(keyboard.b, !upFlag);
                            } break;
                            case '4':
                            {
                                Win32ProcessKeyboardButton(keyboard.y, !upFlag);
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
                            case 'R':
                            {
                                if(altDownFlag && !upFlag && !repeated)
                                {
                                    switch(runMode)
                                    {
                                        case RLM_NORMAL:
                                            runMode = RLM_RECORDINPUT;
                                            Win32BeginRecordingInput(win32State, gameContext);
                                            break;
                                        case RLM_RECORDINPUT:
                                            runMode = RLM_NORMAL;
                                            Win32StopRecordingInput(win32State);
                                            break;
                                        case RLM_PLAYBACKINPUT:
                                            runMode = RLM_RECORDINPUT;
                                            Win32StopInputPlayback(win32State);
                                            Win32BeginRecordingInput(win32State, gameContext);
                                            break;
                                    }
                                }
                            } break;
                            case 'P':
                            {
                                if(altDownFlag && !upFlag && !repeated)
                                {
                                    switch(runMode)
                                    {
                                        case RLM_NORMAL:
                                            runMode = RLM_PLAYBACKINPUT;
                                            Win32BeginInputPlayback(win32State, gameContext);
                                            break;
                                        case RLM_RECORDINPUT:
                                            runMode = RLM_PLAYBACKINPUT;
                                            Win32StopRecordingInput(win32State);
                                            Win32BeginInputPlayback(win32State, gameContext);
                                            break;
                                        case RLM_PLAYBACKINPUT:
                                            runMode = RLM_NORMAL;
                                            Win32StopInputPlayback(win32State);
                                            break;
                                    }
                                }
                            }
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
            Win32ReadGamepad(dwControllerIndex, input.controllers[dwControllerIndex+1]);
        }

        switch(runMode)
        {
            case RLM_RECORDINPUT:
                Win32RecordInput(win32State, input);
                break;
            case RLM_PLAYBACKINPUT:
                // read the input from a file
                if(!Win32PlaybackInput(win32State, input))
                {
                    // No more input to read, so let's loop the playback
                    Win32StopInputPlayback(win32State);
                    Win32BeginInputPlayback(win32State, gameContext);
                }
                break;
        }

        Win32BeginFrame();

        if(gameFunctions.GameUpdateAndRender)
        {
            gameFunctions.GameUpdateAndRender(gameContext, mainWindow.graphics, input, audio.gameAudioBuffer);
        }

        Win32Clock gameSimTime = Win32GetWallClock();
        real32 elapsedFrameTime = Win32GetElapsedTime(lastFrameStartTime, gameSimTime);

        if(elapsedFrameTime < targetFrameRateSeconds)
        {
            real32 frameTime = elapsedFrameTime;
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
            LOG_INFO("Frame Time: %.2f, Total Time: %.2f", frameTime * 1000.0f, elapsedFrameTime * 1000.0f);
        }
        else
        {
            // oops... we missed our frametime limit here...
            // TODO(james): log this out
        }
        input.clock.elapsedFrameTime = elapsedFrameTime;
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

        Win32EndFrame();
        //Win32Dimensions dimensions = Win32GetWindowDimensions(mainWindow.hWindow);              
        //Win32Render(dimensions.width, dimensions.height, mainWindow.graphics);
        //SwapBuffers(mainWindow.hDeviceContext);

        ++input.clock.frameCounter;
    }

    COM_RELEASE(audio.pEnumerator);
    COM_RELEASE(audio.pDevice);
    COM_RELEASE(audio.pClient);
    COM_RELEASE(audio.pRenderClient);   

    ExitProcess(0);
}
