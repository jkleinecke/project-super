#pragma once
#ifndef PROJECT_SUPER_H_
#define PROJECT_SUPER_H_

// Main header file for the game and platform interface

#include <algorithm>
#include <stdarg.h>

#include "ps_types.h"
#include "ps_shared.h"
#include "ps_math.h"

//===================== PLATFORM API =================================

#define LOG_CALLBACK(name) void name(const char* file, int lineno, const char* format, ...)
typedef LOG_CALLBACK(log_callback);

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

struct GameContext
{
    log_callback* logger;
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

struct GraphicsContext
{
    // format??
    uint32 buffer_width;
    uint32 buffer_height;
    uint32 buffer_pitch;
    void* buffer;
};

#define GAME_UPDATE_AND_RENDER(name) void name(GameContext& gameContext, GraphicsContext& graphics, InputContext& input, AudioContext& audio)
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

inline internal
void ResetArena(MemoryArena& memory)
{
    memset(memory.basePointer, 0, (u8*)memory.freePointer - (u8*)memory.basePointer);
    memory.freePointer = memory.basePointer;
}


#endif