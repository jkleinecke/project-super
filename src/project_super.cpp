
#include "project_super.h"
#include <math.h>

internal void
RenderGradient(const GraphicsContext& graphics, int blueOffset, int greenOffset)
{
    uint8* row = (uint8*)graphics.buffer;
    for(uint32 y = 0; y < graphics.buffer_height; ++y)
    {
        uint32* pixel = (uint32*)row;
        for(uint32 x = 0; x < graphics.buffer_width; ++x)
        {
            uint8 green = (uint8)(y + greenOffset);
            uint8 blue = (uint8)(x + blueOffset);

            *pixel++ = (green << 8) | blue;
        }

        row += graphics.buffer_pitch;   
    }
}

internal void
FillSoundBuffer(AudioContext& audio, real32 fTimeStep, uint16 toneHz)
{
    //fTimeStep += 1/60.0f;

    uint32 samplesPerPeriod = audio.samplesPerSecond / toneHz;
    uint16 toneVolume = 1000;

    // convert the timestep to the number of samples we need to render
    //   samples = 2 channels of 16 bit audio
    //   LEFT RIGHT | LEFT RIGHT | LEFT RIGHT | ...
    uint32 numRenderSamples = (uint32)(fTimeStep * audio.samplesPerSecond);
    numRenderSamples += (numRenderSamples % 2); 

    ASSERT(audio.bufferSize >= numRenderSamples * 4);
    audio.bufferBytesFilled = numRenderSamples * 4;
    
    local_persist real32 tSine;
    
    // copy data to buffer
    int16 *SampleOut = (int16*)audio.buffer;
    for(uint32 sampleIndex = 0;
        sampleIndex < numRenderSamples;
        ++sampleIndex)
    {
        real32 SineValue = sinf(tSine);
        int16 SampleValue = (int16)(SineValue * toneVolume);
        *SampleOut++ = SampleValue;
        *SampleOut++ = SampleValue;

        tSine += 2.0f*Pi32*1.0f/(real32)samplesPerPeriod;
    }
}

internal void
GameUpdateAndRender(float fTimeStep, GraphicsContext& graphics, InputContext& input, AudioContext& audio)
{
    local_persist int x_offset = 0;
    local_persist int y_offset = 0;
    local_persist uint16 toneHz = 262;

    for(int controllerIndex = 0; controllerIndex < 5; ++controllerIndex)
    {
        InputController& controller = input.controllers[controllerIndex];

        if(controller.isConnected)
        {
            if(controller.isAnalog)
            {
                x_offset += (int)(controller.leftStick.x * 5.0f);
                y_offset += (int)(controller.leftStick.y * 5.0f);

                // frequency range of 80Hz -> 1200Hz
                toneHz = (uint16)(640 + (controller.rightStick.y * 560.0f));
            }

            if(controller.left.pressed)
            {
                x_offset -= 10;
            }
            if(controller.right.pressed)
            {
                x_offset += 10;
            }
            if(controller.up.pressed)
            {
                y_offset += 10;
            }
            if(controller.down.pressed)
            {
                y_offset -= 10;
            }

            controller.leftFeedbackMotor = controller.leftTrigger.value;
            controller.rightFeedbackMotor = controller.rightTrigger.value;
        }
    }

    FillSoundBuffer(audio, fTimeStep, toneHz);
    RenderGradient(graphics, x_offset, y_offset); 
}