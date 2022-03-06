
// Main header file for the game and platform interface

#include "ps_types.h"

#define API_FUNCTION(ret, name, ...)   \
    typedef PS_API ret(PS_APICALL* PFN_##name)(__VA_ARGS__); \
    PFN_##name    name

#include "ps_graphics.h"

//===================== PLATFORM API =================================

enum class FileUsage
{
    Read    = 0x01,
    Write   = 0x02,
    ReadWrite = Read | Write
};
MAKE_ENUM_FLAG(u32, FileUsage)

enum class FileLocation
{
    Content,
    User,
    Diagnostic,
    LocationsCount
};

struct platform_file
{
    b32 error;
    u64 size;
    void* platform;
};

enum class PlatformMemoryFlags : u64
{
    NotRestored     = 0x01,
    UnderflowCheck  = 0x02,
    OverflowCheck   = 0x04,
    MAX             = U64MAX
};
MAKE_ENUM_FLAG(u64, PlatformMemoryFlags);

struct platform_memory_block
{
    PlatformMemoryFlags flags;
    u64 size;
    u8* base;
    umm used;
    platform_memory_block* prev_block;
};

enum class LogLevel
{
    Debug,
    Info,
    Error
};

#if PROJECTSUPER_INTERNAL
struct debug_platform_memory_stats
{
    u64 totalSize;
    u64 totalUsed;
};
#endif

struct platform_work_queue;


#define PLATFORM_WORK_QUEUE_CALLBACK(name) void name(platform_work_queue* queue, void* data)
typedef PLATFORM_WORK_QUEUE_CALLBACK(platform_work_queue_callback);

struct platform_api
{
    API_FUNCTION(void, Log, LogLevel level, const char* format, ...);

    API_FUNCTION(void, AddWorkEntry, platform_work_queue* queue, platform_work_queue_callback *callback, void* data);
    API_FUNCTION(void, CompleteAllWork, platform_work_queue* queue);
    
    API_FUNCTION(platform_memory_block*, AllocateMemoryBlock, memory_index size, PlatformMemoryFlags flags);
    API_FUNCTION(void, DeallocateMemoryBlock, platform_memory_block*);

    API_FUNCTION(platform_file, OpenFile, FileLocation location, const char* filename, FileUsage usage);
    API_FUNCTION(u64, ReadFile, platform_file& file, void* buffer, u64 size);
    API_FUNCTION(u64, WriteFile, platform_file& file, const void* buffer, u64 size);
    API_FUNCTION(void, CloseFile, platform_file& file);
    // TODO(james): Add list files API
    // TODO(james): Add window creation APIs? (Editor??)
    
#if PROJECTSUPER_INTERNAL
    API_FUNCTION(debug_platform_memory_stats, DEBUG_GetMemoryStats);
    API_FUNCTION(void, DEBUG_Log, LogLevel level, const char* file, int lineno, const char* format, ...);
#endif
};
extern platform_api Platform;

//===================== GAME API =====================================

struct GameClock
{
    uint64 frameCounter;
    real32 totalTime;
    real32 elapsedFrameTime;  // seconds since the last frame
};

struct Thumbstick
{
    real32 x;
    real32 y;
};
struct InputTrigger
{
    real32 value;
};
struct InputButton
{
    bool pressed;
    uint8 transitions;
};
struct InputController
{
    bool32  isConnected;
    bool32  isAnalog;

    Thumbstick leftStick;
    Thumbstick rightStick;

    InputTrigger rightTrigger;
    InputTrigger leftTrigger;

    union
    {
        InputButton buttons[14];
        struct 
        {
            InputButton x;
            InputButton a;
            InputButton b;
            InputButton y;

            InputButton leftShoulder;
            InputButton rightShoulder;

            InputButton up;
            InputButton down;
            InputButton left;
            InputButton right;

            InputButton leftStickButton;
            InputButton rightStickButton;

            InputButton start;
            InputButton back;
        };
    };  

    real32 leftFeedbackMotor;
    real32 rightFeedbackMotor;  
};

struct AudioContextDesc
{
    uint32 samplesPerSecond;
    uint16 numChannels;
    uint16 bitsPerSample;
};

struct AudioContext
{
    AudioContextDesc descriptor;

    uint32 samplesWritten;
    uint32 samplesRequested;  

    buffer streamBuffer;
};

struct InputContext
{
    GameClock clock;
    // keyboard, gamepad 1-4
    InputController controllers[5];
};

enum class RenderResourceOpType
{
    Create,
    Update,
    Destroy
};

struct graphics_context
{
    u32 windowWidth;
    u32 windowHeight;

    gfx_api gfx;
};

struct game_state;
struct game_memory
{
    game_state* state;

    platform_work_queue* highPriorityQueue;
    platform_work_queue* lowPriorityQueue;

    platform_api platformApi;
};

#define GAME_UPDATE_AND_RENDER(name) void name(game_memory& gameMemory, graphics_context& graphics, InputContext& input, AudioContext& audio)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

