

#include "ps_platform.h"
#include "ps_shared.h"
#include "ps_intrinsics.h"
#include "ps_math.h"
#include "ps_memory.h"
#include "ps_collections.h"
// #include "ps_graphics.h"

#include <windows.h>
#include <stdio.h>
#include <math.h>   // for sqrt()

#include "win32_platform.h"

win32_state GlobalWin32State;
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

    if(state.hInputRecordHandle)
    {
        DWORD dwWritten = 0;
        // save out game memory
        BeginTicketMutex(&GlobalWin32State.memoryMutex);
        win32_memory_block* sentinal = &GlobalWin32State.memorySentinal;
        for(win32_memory_block* sourceBlk = sentinal->next; sourceBlk != sentinal; sourceBlk = sourceBlk->next)
        {
            // NOTE(james): only write out blocks that have been flagged NOT to restore
            if(IS_FLAG_BIT_NOT_SET(sourceBlk->block.flags, PlatformMemoryFlags::NotRestored))
            {
                win32_saved_memory_block savedBlock = {};
                // TODO(james): See if we can find a way to not directly depend on the pointer location for the recording
                savedBlock.base_pointer = (u64)sourceBlk->block.base;
                savedBlock.size = sourceBlk->block.size;

                
                BOOL success_size = WriteFile(state.hInputRecordHandle, &savedBlock, sizeof(savedBlock), &dwWritten, 0);
                ASSERT(dwWritten == sizeof(savedBlock));
                BOOL success_data = WriteFile(state.hInputRecordHandle, sourceBlk->block.base, (u32)sourceBlk->block.size, &dwWritten, 0);
                ASSERT(dwWritten == (DWORD)sourceBlk->block.size);
            }
        }
        EndTicketMutex(&GlobalWin32State.memoryMutex);

        win32_saved_memory_block finalBlock = {};
        WriteFile(state.hInputRecordHandle, &finalBlock, sizeof(finalBlock), &dwWritten, 0);
        ASSERT(dwWritten == sizeof(finalBlock));
    }

    return state.hInputRecordHandle != INVALID_HANDLE_VALUE;
}

internal void
Win32RecordInput(win32_state& state, const InputContext& input)
{
    DWORD dwBytesWritten = 0;
    WriteFile(state.hInputRecordHandle, &input, sizeof(input), &dwBytesWritten, 0);
}

inline bool32
Win32IsInLoop(win32_state& state)
{
    return state.runMode == RunLoopMode::Playback;
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

    // read in game memory
    if(state.hInputRecordHandle)
    {
        DWORD dwRead = 0;
        for(;;)
        {
            win32_saved_memory_block savedBlock = {};
            ReadFile(state.hInputRecordHandle, &savedBlock, sizeof(savedBlock), &dwRead, 0);
            if(savedBlock.base_pointer != 0)
            {
                void* basePointer = (void*)savedBlock.base_pointer;
                ReadFile(state.hInputRecordHandle, basePointer, (u32)savedBlock.size, &dwRead, 0);
            }
            else
            {
                break;
            }
        }
    }

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

#if PROJECTSUPER_INTERNAL
internal debug_platform_memory_stats
Win32GetMemoryStats()
{
    debug_platform_memory_stats stats = {};

    BeginTicketMutex(&GlobalWin32State.memoryMutex);
    win32_memory_block* sentinal = &GlobalWin32State.memorySentinal;
    for(win32_memory_block* block = sentinal->next; block != sentinal; block = block->next)
    {
        // make sure we don't have any obviously bad allocations
        ASSERT(block->block.size <= U32MAX);

        stats.totalSize += block->block.size;
        stats.totalUsed += block->block.used;
    } 
    EndTicketMutex(&GlobalWin32State.memoryMutex);

    return stats;
}
#endif

internal platform_memory_block*
Win32AllocateMemoryBlock(memory_index size, PlatformMemoryFlags flags)
{
    // NOTE(james): We require memory block headers not to change the cache
    // line alignment of an allocation
    CompileAssert(sizeof(win32_memory_block) == 64);
    
    const umm pageSize = 4096; // TODO(james): Query from system?
    umm totalSize = size + sizeof(win32_memory_block);
    umm baseOffset = sizeof(win32_memory_block);
    umm protectOffset = 0;
    if(IS_FLAG_BIT_SET(flags, PlatformMemoryFlags::UnderflowCheck))
    {
        totalSize = size + 2*pageSize;
        baseOffset = 2*pageSize;
        protectOffset = pageSize;
    }
    else if(IS_FLAG_BIT_SET(flags,PlatformMemoryFlags::OverflowCheck))
    {
        umm sizeRoundedUp = AlignPow2(size, pageSize);
        totalSize = sizeRoundedUp + 2*pageSize;
        baseOffset = pageSize + sizeRoundedUp - size;
        protectOffset = pageSize + sizeRoundedUp;
    }
    
    win32_memory_block *block = (win32_memory_block *) VirtualAlloc(0, totalSize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    ASSERT(block);
    block->block.base = (u8 *)block + baseOffset;
    ASSERT(block->block.used == 0);
    ASSERT(block->block.prev_block == 0);
    
    if(IS_FLAG_BIT_SET(flags,PlatformMemoryFlags::OverflowCheck) || IS_FLAG_BIT_SET(flags,PlatformMemoryFlags::UnderflowCheck))
    {
        DWORD oldProtect = 0;
        BOOL isprotected = VirtualProtect((u8 *)block + protectOffset, pageSize, PAGE_NOACCESS, &oldProtect);
        ASSERT(isprotected);
    }
    
    win32_memory_block *sentinel = &GlobalWin32State.memorySentinal;
    block->next = sentinel;
    block->block.size = size;
    block->block.flags = flags;
    block->loopingFlags = MemoryLoopingFlags::None;
    if(Win32IsInLoop(GlobalWin32State) && IS_FLAG_BIT_NOT_SET(flags,PlatformMemoryFlags::NotRestored))
    {
        block->loopingFlags = MemoryLoopingFlags::Allocated;
    }
    
    BeginTicketMutex(&GlobalWin32State.memoryMutex);
    block->prev = sentinel->prev;
    block->prev->next = block;
    block->next->prev = block;
    EndTicketMutex(&GlobalWin32State.memoryMutex);
    
    platform_memory_block *platformBlock = &block->block;
    return platformBlock;
}

internal void
Win32FreeMemoryBlock(win32_memory_block *block)
{
    BeginTicketMutex(&GlobalWin32State.memoryMutex);
    block->prev->next = block->next;
    block->next->prev = block->prev;
    EndTicketMutex(&GlobalWin32State.memoryMutex);
    
    // NOTE(james): For porting to other platforms that need the size to unmap
    // pages, you can get it from Block->Block.Size!
    
    BOOL Result = VirtualFree(block, 0, MEM_RELEASE);
    ASSERT(Result);
}

internal void
Win32DeallocateMemoryBlock(platform_memory_block* block)
{
    if(block)
    {
        win32_memory_block *win32Block = ((win32_memory_block *)block);
        if(Win32IsInLoop(GlobalWin32State) && IS_FLAG_BIT_NOT_SET(win32Block->block.flags, PlatformMemoryFlags::NotRestored))
        {
            win32Block->loopingFlags = MemoryLoopingFlags::Deallocated;
        }
        else
        {
            Win32FreeMemoryBlock(win32Block);
        }
    }
}

internal void
Win32ClearMemoryBlocksByMask(win32_state& state, MemoryLoopingFlags mask)
{
    BeginTicketMutex(&state.memoryMutex);
    win32_memory_block* sentinal = &state.memorySentinal;
    for(win32_memory_block* block_iter = sentinal->next; block_iter != sentinal; )
    {
        win32_memory_block* block = block_iter;
        block_iter = block->next;

        if((block->loopingFlags & mask) == mask)
        {
            Win32FreeMemoryBlock(block);
        }
        else
        {
            block->loopingFlags = MemoryLoopingFlags::None;
        }
    }

    EndTicketMutex(&state.memoryMutex);
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

    GlobalWin32State.runMode = RunLoopMode::Normal;
    // NOTE(james): Initialize sentinal to point to itself to establish the memory ring
    win32_memory_block* sentinal = &GlobalWin32State.memorySentinal;
    sentinal->next = sentinal;
    sentinal->prev = sentinal;

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

    Win32GetExecutablePath(GlobalWin32State);
    Win32SetupFileLocationsTable(GlobalWin32State);
  
    game_memory gameMemory = {};
    render_context gameRender = {};
    gameRender.renderDimensions.Width = (f32)FIXED_RENDER_WIDTH;  // TODO(james): make these dynamic at runtime..
    gameRender.renderDimensions.Height = (f32)FIXED_RENDER_HEIGHT;

    gameMemory.highPriorityQueue = &highPriorityQueue;
    gameMemory.lowPriorityQueue = &lowPriorityQueue;

    gameMemory.platformApi.Log = &Win32Log;
    gameMemory.platformApi.AddWorkEntry = &Win32AddWorkQueueEntry;
    gameMemory.platformApi.CompleteAllWork = &Win32CompleteAllWorkQueueWork;
    gameMemory.platformApi.AllocateMemoryBlock = &Win32AllocateMemoryBlock;
    gameMemory.platformApi.DeallocateMemoryBlock = &Win32DeallocateMemoryBlock;
    gameMemory.platformApi.OpenFile = &Win32OpenFile;
    gameMemory.platformApi.ReadFile = &Win32ReadFile;
    gameMemory.platformApi.WriteFile = &Win32WriteFile;
    gameMemory.platformApi.CloseFile = &Win32CloseFile;

#if defined(PROJECTSUPER_INTERNAL)
    gameMemory.platformApi.DEBUG_Log = &Win32DebugLog;
#endif

    Platform = gameMemory.platformApi;

#if TEST_COLLECTIONS
    {
        b32 passed = TestCollections();
        ASSERT(passed);
    }
#endif
    
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
    gfx_api& gfx = graphicsDriver.gfx; 
    gameRender.gfx = gfx;

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
    
    Win32LoadCode(GlobalWin32State, gameCode);
    ASSERT(gameCode.isValid);
    
    MSG msg;
    while(GlobalRunning)
    {
        FILETIME ftLastWriteTime = Win32GetFileWriteTime(gameCode.pszDLLName);
        if(CompareFileTime(&ftLastWriteTime, &gameCode.ftLastFileWriteTime) != 0)
        {
            Win32UnloadGameCode(gameCode);
            Win32LoadCode(GlobalWin32State, gameCode);
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
                                    switch(GlobalWin32State.runMode)
                                    {
                                        case RunLoopMode::Normal:
                                            GlobalWin32State.runMode = RunLoopMode::Record;
                                            Win32BeginRecordingInput(GlobalWin32State, gameMemory);
                                            break;
                                        case RunLoopMode::Record:
                                            GlobalWin32State.runMode = RunLoopMode::Normal;
                                            Win32StopRecordingInput(GlobalWin32State);
                                            break;
                                        case RunLoopMode::Playback:
                                            GlobalWin32State.runMode = RunLoopMode::Record;
                                            Win32StopInputPlayback(GlobalWin32State);
                                            Win32BeginRecordingInput(GlobalWin32State, gameMemory);
                                            break;
                                    }
                                }
                            } break;
                            case 'P':
                            {
                                if(altDownFlag && !upFlag && !repeated)
                                {
                                    switch(GlobalWin32State.runMode)
                                    {
                                        case RunLoopMode::Normal:
                                            GlobalWin32State.runMode = RunLoopMode::Playback;
                                            Win32BeginInputPlayback(GlobalWin32State, gameMemory);
                                            break;
                                        case RunLoopMode::Record:
                                            GlobalWin32State.runMode = RunLoopMode::Playback;
                                            Win32StopRecordingInput(GlobalWin32State);
                                            Win32BeginInputPlayback(GlobalWin32State, gameMemory);
                                            break;
                                        case RunLoopMode::Playback:
                                            GlobalWin32State.runMode = RunLoopMode::Normal;
                                            Win32StopInputPlayback(GlobalWin32State);
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

        switch(GlobalWin32State.runMode)
        {
            case RunLoopMode::Record:
                Win32RecordInput(GlobalWin32State, input);
                break;
            case RunLoopMode::Playback:
                // read the input from a file
                if(!Win32PlaybackInput(GlobalWin32State, input))
                {
                    // No more input to read, so let's loop the playback
                    Win32StopInputPlayback(GlobalWin32State);
                    Win32BeginInputPlayback(GlobalWin32State, gameMemory);
                }
                break;
        }

        // graphicsApi.BeginFrame(graphicsDriver.instance, &gameRender.commands);

        if(gameFunctions.GameUpdateAndRender)
        {
            gameFunctions.GameUpdateAndRender(gameMemory, gameRender, input, audio.gameAudioBuffer);
        }

        Win32Clock gameSimTime = Win32GetWallClock();
        real32 elapsedFrameTime = Win32GetElapsedTime(lastFrameStartTime, gameSimTime);

        input.clock.totalTime += elapsedFrameTime;
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
            LOG_DEBUG("Frame Time: %.2f ms, Total Time: %.2f ms", frameTime * 1000.0f, elapsedFrameTime * 1000.0f);
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
