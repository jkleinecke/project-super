
#include "ps_platform.h"
#include <math.h>

struct GameTestState
{
    int x_offset;
    int y_offset;

    real32 toneHz;
    real64 tSine;
};

internal void
RenderGradient(const GraphicsContext& graphics, GameTestState& gameState)
{
    uint8* row = (uint8*)graphics.buffer;
    for(uint32 y = 0; y < graphics.buffer_height; ++y)
    {
        uint32* pixel = (uint32*)row;
        for(uint32 x = 0; x < graphics.buffer_width; ++x)
        {
            uint8 green = (uint8)(y + gameState.y_offset);
            uint8 blue = (uint8)(x + gameState.x_offset);

            *pixel++ = (green << 8) | blue;
        }

        row += graphics.buffer_pitch;   
    }
}


internal void
FillSoundBuffer(AudioContext& audio, GameTestState& gameState)
{
    real32 samplesPerPeriod = audio.samplesPerSecond / gameState.toneHz;
    uint16 toneVolume = 1000;

    // convert the timestep to the number of samples we need to render
    //   samples = 2 channels of 16 bit audio
    //   LEFT RIGHT | LEFT RIGHT | LEFT RIGHT | ...
    uint32 numRenderSamples = audio.samplesRequested;
  
    ASSERT(audio.bufferSize >= numRenderSamples * 4);
    audio.samplesWritten = numRenderSamples;    
        
    // copy data to buffer
    int16 *SampleOut = (int16*)audio.buffer;
    for(uint32 sampleIndex = 0;
        sampleIndex < numRenderSamples;
        ++sampleIndex)
    {
        real64 SineValue = sin(gameState.tSine);
        int16 SampleValue = (int16)(SineValue * toneVolume);
        *SampleOut++ = SampleValue;
        *SampleOut++ = SampleValue;

        gameState.tSine += 2.0*Pi32*1.0/samplesPerPeriod;
    }
}

extern "C"
GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    // TODO(james): needs a better allocation scheme 
    GameTestState& gameState = (GameTestState&)gameContext.persistantMemory.basePointer;
    if(gameContext.persistantMemory.basePointer == gameContext.persistantMemory.freePointer)
    {
        gameContext.persistantMemory.freePointer = ((uint8*)gameContext.persistantMemory.freePointer) + sizeof(gameState);
        // memory isn't initialized
        gameState.x_offset = 0;
        gameState.y_offset = 0;
        gameState.toneHz = 264.0f;
        gameState.tSine = 0.0f;
    }

    for(int controllerIndex = 0; controllerIndex < 5; ++controllerIndex)
    {
        InputController& controller = input.controllers[controllerIndex];

        if(controller.isConnected)
        {
            if(controller.isAnalog)
            {
                gameState.x_offset += (int)(controller.leftStick.x * 5.0f);
                gameState.y_offset += (int)(controller.leftStick.y * 5.0f);

                // frequency range of 80Hz -> 1200Hz
                const real32 min = 250.0f;
                const real32 max = 278.0f;
                const real32 halfMagnitude = (max - min)/2.0f;
                gameState.toneHz = min + halfMagnitude + controller.rightStick.y * halfMagnitude;
            }
            else
            {
                gameState.toneHz = 264.0f;
            }

            if(controller.left.pressed)
            {
                gameState.x_offset -= 10;
            }
            if(controller.right.pressed)
            {
                gameState.x_offset += 10;
            }
            if(controller.up.pressed)
            {
                gameState.y_offset += 10;
            }
            if(controller.down.pressed)
            {
                gameState.y_offset -= 10;
            }

            controller.leftFeedbackMotor = controller.leftTrigger.value;
            controller.rightFeedbackMotor = controller.rightTrigger.value;
        }
    }

    FillSoundBuffer(audio, gameState);
    RenderGradient(graphics, gameState); 
}