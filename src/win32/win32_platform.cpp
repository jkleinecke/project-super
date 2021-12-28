

#include "ps_platform.h"
#include "ps_shared.h"
#include "ps_intrinsics.h"
#include "ps_math.h"
#include "ps_memory.h"

#include <windows.h>
#include <stdio.h>
#include <math.h>   // for sqrt()

#include "win32_platform.h"

platform_api Platform;
win32_file_location FileLocationsTable[(u32)FileLocation::LocationsCount];

#include "win32_log.cpp"

#include "win32_audio.cpp"
#include "win32_xinput.cpp"
#include "win32_file.cpp"

// TODO(james): make this work as a loaded dll
//#include "win32_opengl.cpp"
//#include "win32_d3d12.cpp"
//#include "win32_vulkan.cpp"
#include "../vulkan/vk_platform.cpp"    

global const int FIXED_RENDER_WIDTH = 1920;
global const int FIXED_RENDER_HEIGHT = 1080;

/*******************************************************************************

    Things to do:
    * Threading + Work Queue
    * File API
       - Streaming API
    * WM_ACTIVEAPP
    * Harden sound latency with framerate
    * Hardware Capabilities
    * Finish logging system
    * Base framerate on monitor refresh rate
    * Toggle fullscreen window

********************************************************************************/

GAME_UPDATE_AND_RENDER(GameUpdateAndRenderStub) {}

global_variable bool GlobalRunning = true;
global_variable int64 GlobalFrequency;

internal void
Win32SetupFileLocationsTable(win32_state& state)
{
    // TODO(james): Put user folder into the user's app data folder..

    FileLocationsTable[(u32)FileLocation::Content].location = FileLocation::Content;
    FormatString(FileLocationsTable[(u32)FileLocation::Content].szFolder, WIN32_STATE_FILE_NAME_COUNT, "%s..\\data\\", state.EXEFolder);
    
    FileLocationsTable[(u32)FileLocation::User].location = FileLocation::User;
    FormatString(FileLocationsTable[(u32)FileLocation::User].szFolder, WIN32_STATE_FILE_NAME_COUNT, "%s", state.EXEFolder);
    
    FileLocationsTable[(u32)FileLocation::Diagnostic].location = FileLocation::Diagnostic;
    FormatString(FileLocationsTable[(u32)FileLocation::Diagnostic].szFolder, WIN32_STATE_FILE_NAME_COUNT, "%s", state.EXEFolder);
}

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

internal bool32
Win32BeginRecordingInput(win32_state& state, const game_memory& memory)
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
Win32BeginInputPlayback(win32_state& state, game_memory& memory)
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

internal void
Win32AddWorkQueueEntry(platform_work_queue* queue, platform_work_queue_callback* callback, void* data)
{
    // TODO(james): Switch to InterlockedCompareExchange eventually
    // so that any thread can add?
    uint32 nextWriteIndex = (queue->writeIndex + 1) % ARRAY_COUNT(queue->entries);
    ASSERT(nextWriteIndex != queue->readIndex);
    platform_work_queue_entry *entry = queue->entries + queue->writeIndex;
    entry->callback = callback;
    entry->data = data;
    ++queue->countToComplete;
    _WriteBarrier();
    queue->writeIndex = nextWriteIndex;
    ReleaseSemaphore(queue->semaphore, 1, 0);
}

internal bool32
Win32DoNextWorkQueueEntry(platform_work_queue *queue)
{
    bool32 didNoWork = false;
    
    uint32 readIndex = queue->readIndex;
    uint32 nextReadIndex = (readIndex + 1) % ARRAY_COUNT(queue->entries);
    if(readIndex != queue->writeIndex)
    {
        uint32 index = InterlockedCompareExchange((LONG volatile *)&queue->readIndex,
                                                  nextReadIndex,
                                                  readIndex);
        if(index == readIndex)
        {
            platform_work_queue_entry entry = queue->entries[index];
            // TODO(james): allocate some thread specific scratch memory
            // for each task entry to use during execution
            entry.callback(queue, entry.data);
            InterlockedIncrement((LONG volatile *)&queue->countCompleted);
        }
    }
    else
    {
        didNoWork = true;
    }
    
    return didNoWork;
}

internal void
Win32CompleteAllWorkQueueWork(platform_work_queue* queue)
{
    while(queue->countToComplete != queue->countCompleted)
    {
        Win32DoNextWorkQueueEntry(queue);
    }
    
    queue->countToComplete = 0;
    queue->countCompleted = 0;
}

DWORD WINAPI
Win32ThreadProc(LPVOID lpParameter)
{
    win32_thread_info *thread = (win32_thread_info *)lpParameter;
    platform_work_queue *queue = thread->queue;
        
    for(;;)
    {
        if(Win32DoNextWorkQueueEntry(queue))
        {
            WaitForSingleObjectEx(queue->semaphore, INFINITE, FALSE);
        }
    }
    
    //    return(0);
}

internal void
Win32InitWorkQueue(platform_work_queue* queue, u32 numThreads, win32_thread_info* threads, u32 threadIndexOffset = 0)
{
    queue->countToComplete = 0;
    queue->countCompleted = 0;
    queue->writeIndex = 0;
    queue->readIndex = 0;

    u32 initCount = 0;
    // TODO(james): Figure out if we really need the SEMAPHORE_ALL_ACCESS right...
    queue->semaphore = CreateSemaphoreEx(0, initCount, numThreads, 0, 0, SEMAPHORE_ALL_ACCESS);

    for(u32 threadIndex = 0; threadIndex < numThreads; ++threadIndex)
    {
        win32_thread_info* pThreadInfo = threads + threadIndex;
        pThreadInfo->thread_index = threadIndex + threadIndexOffset;
        pThreadInfo->queue = queue;

        DWORD dwThreadID = 0;
        HANDLE hThread = CreateThread(0, Megabytes(1), Win32ThreadProc, pThreadInfo, 0, &dwThreadID);
        CloseHandle(hThread);
    }
}

enum RunLoopMode
{
    RLM_NORMAL,
    RLM_RECORDINPUT,
    RLM_PLAYBACKINPUT
};

internal
PLATFORM_WORK_QUEUE_CALLBACK(ThreadPrintTest)
{
    char szOut[256];
    wsprintf(szOut, "Thread %u: %s\n", GetCurrentThreadId(), (char*)data);
    OutputDebugStringA(szOut);
}

#if 1
int CALLBACK
WinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nShowCmd)
{
#else 

extern "C" int __stdcall WinMainCRTStartup()
{
    SetDefaultFPBehavior();

    HINSTANCE hInstance = GetModuleHandle(0);
#endif
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    win32_thread_info highPriorityThreadInfos[8];
    win32_thread_info lowPriorityThreadInfos[2];
    platform_work_queue highPriorityQueue;
    platform_work_queue lowPriorityQueue;
    LOG_DEBUG("Main Thread ID: %u", GetCurrentThreadId());

    Win32InitWorkQueue(&highPriorityQueue, ARRAY_COUNT(highPriorityThreadInfos), highPriorityThreadInfos);
    Win32InitWorkQueue(&lowPriorityQueue, ARRAY_COUNT(lowPriorityThreadInfos), lowPriorityThreadInfos);

    WNDCLASSEXA wndClass = {};
    wndClass.cbSize = sizeof(wndClass);
    wndClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wndClass.lpfnWndProc = Win32WindowProc;
    wndClass.hInstance = hInstance;
    wndClass.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wndClass.lpszClassName = "ProjectSuperWindow";
    RegisterClassExA(&wndClass);

    int desiredWidth = FIXED_RENDER_WIDTH;
    int desiredHeight = FIXED_RENDER_HEIGHT;

    int screenWidth = ::GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = ::GetSystemMetrics(SM_CYSCREEN);
    
    int desiredLeft = screenWidth - desiredWidth;
    int desiredTop = screenHeight/2 - desiredHeight/2;    

    RECT rc = { desiredLeft, desiredTop, desiredLeft + desiredWidth, desiredTop + desiredHeight };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, 0);

    HWND mainWindow = CreateWindowExA(0,
                                       "ProjectSuperWindow", "Project Super",
                                       WS_OVERLAPPEDWINDOW,
                                       rc.left, rc.top,
                                       rc.right-rc.left, rc.bottom-rc.top,
                                       0, 0,
                                       hInstance, 0   
                                       );
                
    Win32InitClockFrequency();

    win32_state win32State = {};
    Win32GetExecutablePath(win32State);
    Win32SetupFileLocationsTable(win32State);
  
    game_memory gameMemory = {};
    render_context gameRender = {};
    gameRender.width = FIXED_RENDER_WIDTH;  // TODO(james): make these dynamic at runtime..
    gameRender.height = FIXED_RENDER_HEIGHT;
    gameMemory.persistantMemory.size = Megabytes(64);
    gameMemory.transientMemory.size = Gigabytes(1);
    gameRender.commands.cmd_arena.size = Megabytes(16); // Is this enough?
    LPVOID baseAddress = 0;
#ifdef PROJECTSUPER_INTERNAL
    baseAddress = (LPVOID)Terabytes(2);
#endif
    // TODO(james): include some extra memory for some fences to check for memory overwrites
    umm memorySize = gameMemory.persistantMemory.size + gameMemory.transientMemory.size + gameRender.commands.cmd_arena.size;
    uint8* memory = (uint8*)VirtualAlloc(baseAddress, memorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

    if(!memory)
    {
        // failed to allocate enough memory for the game... might as well quit now
        // TODO(james): Log the reason...
        return 0x80000000;
    }

    gameMemory.persistantMemory.basePointer = memory;
    gameMemory.persistantMemory.freePointer = memory;
    memory += gameMemory.persistantMemory.size;
    gameMemory.transientMemory.basePointer = memory;
    gameMemory.transientMemory.freePointer = memory;
    memory += gameMemory.transientMemory.size;
    gameRender.commands.cmd_arena.basePointer = memory;
    gameRender.commands.cmd_arena.freePointer = memory;

    gameMemory.highPriorityQueue = &highPriorityQueue;
    gameMemory.lowPriorityQueue = &lowPriorityQueue;

    gameMemory.platformApi.Log = &Win32Log;
    gameMemory.platformApi.AddWorkEntry = &Win32AddWorkQueueEntry;
    gameMemory.platformApi.CompleteAllWork = &Win32CompleteAllWorkQueueWork;
    gameMemory.platformApi.OpenFile = &Win32OpenFile;
    gameMemory.platformApi.ReadFile = &Win32ReadFile;
    gameMemory.platformApi.WriteFile = &Win32WriteFile;
    gameMemory.platformApi.CloseFile = &Win32CloseFile;

#if defined(PROJECTSUPER_INTERNAL)
    gameMemory.platformApi.DEBUG_Log = &Win32DebugLog;
#endif

    Platform = gameMemory.platformApi;
    
    // NOTE(james): Set the windows scheduler granularity to 1ms so that our sleep can be more granular
    bool32 bSleepIsMs = timeBeginPeriod(1) == TIMERR_NOERROR;

    Win32Dimensions startDim = Win32GetWindowDimensions(mainWindow);

    // TODO(james): Is this the best way to get the monitor refresh rate?? Maybe leave this up to the
    //   renderer implementation...
    HDC dc = GetDC(mainWindow);
    int nMonitorRefreshRate = GetDeviceCaps(dc, VREFRESH);
    real32 targetFrameRateSeconds = 1.0f / nMonitorRefreshRate;
    ReleaseDC(mainWindow, dc);

    HRESULT hr = 0;

    ps_graphics_backend graphicsDriver = platform_load_graphics_backend(hInstance, mainWindow);
    ps_graphics_backend_api graphicsApi = graphicsDriver.api; 
    

    ShowWindow(mainWindow, SW_SHOW);
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
    gameCode.pszDLLName = (char*)"ps_game.dll";
    gameCode.pszTransientDLLName = (char*)"ps_game_temp.dll";
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
                                            Win32BeginRecordingInput(win32State, gameMemory);
                                            break;
                                        case RLM_RECORDINPUT:
                                            runMode = RLM_NORMAL;
                                            Win32StopRecordingInput(win32State);
                                            break;
                                        case RLM_PLAYBACKINPUT:
                                            runMode = RLM_RECORDINPUT;
                                            Win32StopInputPlayback(win32State);
                                            Win32BeginRecordingInput(win32State, gameMemory);
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
                                            Win32BeginInputPlayback(win32State, gameMemory);
                                            break;
                                        case RLM_RECORDINPUT:
                                            runMode = RLM_PLAYBACKINPUT;
                                            Win32StopRecordingInput(win32State);
                                            Win32BeginInputPlayback(win32State, gameMemory);
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
                    Win32BeginInputPlayback(win32State, gameMemory);
                }
                break;
        }

        graphicsApi.BeginFrame(graphicsDriver.instance);

        if(gameFunctions.GameUpdateAndRender)
        {
            gameFunctions.GameUpdateAndRender(gameMemory, gameRender, input, audio.gameAudioBuffer);
        }

        Win32Clock gameSimTime = Win32GetWallClock();
        real32 elapsedFrameTime = Win32GetElapsedTime(lastFrameStartTime, gameSimTime);

        input.clock.elapsedFrameTime = elapsedFrameTime;

        if(elapsedFrameTime < targetFrameRateSeconds)
        {
            real32 frameTime = elapsedFrameTime;
            // while(elapsedFrameTime < (targetFrameRateSeconds))
            // {
            //     if(bSleepIsMs)
            //     {
            //         // truncate to whole milliseconds
            //         DWORD dwSleepTimeMS = (DWORD)((targetFrameRateSeconds - elapsedFrameTime) * 1000.0f);
            //         Sleep(dwSleepTimeMS);
            //     }
            //     elapsedFrameTime = Win32GetElapsedTime(lastFrameStartTime);
            // }
            #if 1
            LOG_INFO("Frame Time: %.2f ms, Total Time: %.2f ms", frameTime * 1000.0f, elapsedFrameTime * 1000.0f);
            #endif
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

        graphicsApi.EndFrame(graphicsDriver.instance, &gameRender.commands);

        ++input.clock.frameCounter;
    }

    COM_RELEASE(audio.pEnumerator);
    COM_RELEASE(audio.pDevice);
    COM_RELEASE(audio.pClient);
    COM_RELEASE(audio.pRenderClient);   

    DestroyWindow(mainWindow);
    platform_unload_graphics_backend(&graphicsDriver);

    ExitProcess(0);
}
