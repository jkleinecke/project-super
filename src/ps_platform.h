
// Main header file for the game and platform interface

#include "ps_types.h"

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

enum class LogLevel
{
    Debug,
    Info,
    Error
};

#define API_FUNCTION(ret, name, ...)   \
    typedef PS_API ret(PS_APICALL* PFN_##name)(__VA_ARGS__); \
    PFN_##name    name

struct platform_api
{
    API_FUNCTION(void, Log, LogLevel level, const char* format, ...);
    
    API_FUNCTION(platform_file, OpenFile, FileLocation location, const char* filename, FileUsage usage);
    API_FUNCTION(u64, ReadFile, platform_file& file, void* buffer, u64 size);
    API_FUNCTION(u64, WriteFile, platform_file& file, const void* buffer, u64 size);
    API_FUNCTION(void, CloseFile, platform_file& file);
    // TODO(james): Add list files API
    // TODO(james): Add Memory APIs
    // TODO(james): Add Threading APIs or just plain async APIs
    // TODO(james): Add window creation APIs? (Editor??)
    
#ifdef PROJECTSUPER_INTERNAL
    API_FUNCTION(void, DEBUG_Log, LogLevel level, const char* file, int lineno, const char* format, ...);
#endif
};
extern platform_api Platform;

//===================== GAME API =====================================

struct GameClock
{
    uint64 frameCounter;
    real32 elapsedFrameTime;  // seconds since the last frame
};


struct MemoryArena
{
    void* basePointer;
    void* freePointer;
    umm size;
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

struct render_commands
{
    MemoryArena cmd_arena;
};

struct render_context
{
    u32 width;
    u32 height;

    render_commands commands;
};

struct game_state;
struct game_memory
{
    game_state* state;
    
    MemoryArena persistantMemory;
    MemoryArena transientMemory;

    platform_api platformApi;
};

#define GAME_UPDATE_AND_RENDER(name) void name(game_memory& gameMemory, render_context& render, InputContext& input, AudioContext& audio)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

inline internal
void* PushStruct(MemoryArena& memory, umm size)
{
    // TODO(james): handle overrun more gracefully
    ASSERT(PtrToUMM(memory.freePointer)+size <= PtrToUMM(memory.basePointer) + memory.size); // already overrun
    ASSERT(memory.size > (umm)((u8*)memory.freePointer - (u8*)memory.basePointer));    // buffer overrun
    void* ret = memory.freePointer;
    memory.freePointer = ((u8*)memory.freePointer) + size;
    return ret;
}

inline internal
void* PushArray(MemoryArena& memory, umm size, u32 count)
{
    // TODO(james): handle overrun more gracefully
    ASSERT(PtrToUMM(memory.freePointer)+(size*count) <= PtrToUMM(memory.basePointer) + memory.size); // already overrun
    ASSERT(memory.size > (umm)((u8*)memory.freePointer - (u8*)memory.basePointer));    // buffer overrun
    void* ret = memory.freePointer;
    memory.freePointer = ((u8*)memory.freePointer) + (size*count);
    return ret;
}
