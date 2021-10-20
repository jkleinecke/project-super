#pragma once
#ifndef PROJECT_SUPER_H_
#define PROJECT_SUPER_H_

// Main header file for the game and platform interface

//==================== Standard includes and types ====================
#include <stdint.h>

typedef int8_t  int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef float   real32;
typedef double  real64;

typedef int32 bool32;

//===================== Standard Defines =============================
#define Pi32 3.14159265359f

// Clarify usage of static keyword with these defines
#define internal static
#define local_persist static
#define global_variable static

#ifdef PROJECTSUPER_SLOW
#define ASSERT(cond) if(!(cond)) { *((int*)0) = 0; }
#else
#define ASSERT(cond)
#endif

//===================== PLATFORM API =================================




//===================== GAME API =====================================

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
            InputButton up;
            InputButton down;
            InputButton left;
            InputButton right;

            InputButton y;
            InputButton a;
            InputButton x;
            InputButton b;

            InputButton leftShoulder;
            InputButton rightShoulder;

            InputButton leftStickButton;
            InputButton rightStickButton;

            InputButton start;
            InputButton back;
        };
    };  

    real32 leftFeedbackMotor;
    real32 rightFeedbackMotor;  
};

struct AudioContext
{
    uint32 samplesPerSecond;
    uint32 bufferSize;
    uint32 samplesWritten;
    uint32 samplesRequested;   // 
    // uint16 numChannels;
    // uint16 bitsPerSample;
    // uint16 blockAlignment;
    void* buffer;
};

struct InputContext
{
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

internal void GameUpdateAndRender(real32 fTimeStep, GraphicsContext& graphics, InputContext& input, AudioContext& audio);


#endif