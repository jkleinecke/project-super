
#include "project_super.h"

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
GameUpdateAndRender(GraphicsContext& graphics, InputContext& input)
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

    RenderGradient(graphics, x_offset, y_offset); 
}