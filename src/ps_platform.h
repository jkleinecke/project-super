
// Main header file for the game and platform interface

#include "ps_types.h"

//===================== PLATFORM API =================================

#if defined(PROJECTSUPER_INTERNAL)
typedef void (platform_logger)(const char* file, int lineno, const char* format, ...);
#else
typedef void (platform_logger)(const char* format, ...);
#endif

struct platform_api
{
    platform_logger* logger;

    // TODO(james): Add File APIs
    // TODO(james): Add Memory APIs
    // TODO(james): Add Threading APIs or just plain async APIs
    // TODO(james): Add window creation APIs? (Editor??)
};


//===================== GAME API =====================================

struct GameClock
{
    uint64 frameCounter;
    real32 elapsedFrameTime;  // seconds since the last frame
};


struct MemoryArena
{
    uint64 size;
    void* basePointer;
    void* freePointer;
};

struct FrameContext
{
    GameClock clock;

    // Should these live here?
    MemoryArena persistantMemory;
    MemoryArena transientMemory;
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

struct vg_device;
struct ps_graphics_api;
struct GraphicsContext
{
    vg_device* device;
    ps_graphics_api* api;

    // TODO(james): remove the raw buffer members
    // format??
    uint32 buffer_width;
    uint32 buffer_height;
    uint32 buffer_pitch;
    void* buffer;
};

#define GAME_UPDATE_AND_RENDER(name) void name(FrameContext& frame, GraphicsContext& graphics, InputContext& input, AudioContext& audio)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

inline internal
void* PushStruct(MemoryArena& memory, u32 size)
{
    void* ret = memory.freePointer;
    memory.freePointer = ((u8*)memory.freePointer) + size;
    // TODO(james): handle overrun more gracefully
    ASSERT(memory.size > (u64)((u8*)memory.freePointer - (u8*)memory.basePointer));    // buffer overrun
    return ret;
}

inline internal
void* PushArray(MemoryArena& memory, u32 size, u32 count)
{
    void* ret = memory.freePointer;
    memory.freePointer = ((u8*)memory.freePointer) + (size*count);
    // TODO(james): handle overrun more gracefully
    ASSERT(memory.size > (u64)((u8*)memory.freePointer - (u8*)memory.basePointer));    // buffer overrun
    return ret;
}
