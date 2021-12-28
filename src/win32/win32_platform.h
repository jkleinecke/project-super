

struct IMMDeviceEnumerator;
struct IMMDevice;
struct IAudioClient;
struct IAudioRenderClient;

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

struct win32_graphics_backend
{
    MemoryArena memory;

    void* backend;
};

#define WIN32_STATE_FILE_NAME_COUNT MAX_PATH
struct win32_state
{
    HANDLE hInputRecordHandle;
    
    char EXEFolder[WIN32_STATE_FILE_NAME_COUNT];
    char EXEFilename[WIN32_STATE_FILE_NAME_COUNT];
    
    HINSTANCE Instance;
    HWND mainWindow;    
};

struct platform_work_queue_entry
{
    platform_work_queue_callback *callback;
    void *data;
};

struct platform_work_queue
{
    u32 volatile countToComplete;
    u32 volatile countCompleted;
    
    u32 volatile writeIndex;
    u32 volatile readIndex;
    HANDLE semaphore;
    
    platform_work_queue_entry entries[256];
};

struct win32_thread_info
{
    u32 thread_index;
    platform_work_queue* queue;
};

struct win32_file_location {
    FileLocation location;
    char szFolder[WIN32_STATE_FILE_NAME_COUNT];
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
global_variable const char *Win32GameFunctionTableNames[] = {
    "GameUpdateAndRender"
};

// Microsoft COM helpers...

#define COM_RELEASE(punk)  \
              if ((punk) != NULL)  \
                { (punk)->Release(); (punk) = NULL; }

