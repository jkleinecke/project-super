
#include <Xinput.h>
#include <math.h>

#define XINPUT_LEFT_THUMB_DEADZONE 7849
#define XINPUT_RIGHT_THUMB_DEADZONE 8689
#define XINPUT_TRIGGER_THRESHOLD 30

#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
    return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// NOTE: XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
    return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

internal void
Win32LoadXinput()
{
    HMODULE hXinputLibrary = LoadLibraryA("xinput1_4.dll");

    if(hXinputLibrary)
    {
        XInputGetState = (x_input_get_state *)GetProcAddress(hXinputLibrary, "XInputGetState");
        if(!XInputGetState) {XInputGetState = XInputGetStateStub;}

        XInputSetState = (x_input_set_state *)GetProcAddress(hXinputLibrary, "XInputSetState");
        if(!XInputSetState) {XInputSetState = XInputSetStateStub;}
    }
    else
    {
        LOG_ERROR("Failed to load XInput: xinput1_4.dll");
    }
}

inline internal void
Win32ProcessGamepadButton(InputButton& newState, const InputButton& oldState, bool pressed)
{
    newState.pressed = pressed;
    newState.transitions = oldState.pressed == pressed ? 0 : 1; // only transitioned if the new state doesn't match the old
}

internal void
Win32ProcessGamepadStick(Thumbstick& stick, SHORT x, SHORT y, const SHORT deadzone)
{
    real32 fX = (real32)x;
    real32 fY = (real32)y;
    // determine magnitude
    real32 magnitude = sqrtf(fX*fX + fY*fY);
    
    // determine direction
    real32 normalizedLX = fX / magnitude;
    real32 normalizedLY = fY / magnitude;

    if(magnitude > deadzone)
    {
        // clip the magnitude
        if(magnitude > 32767) magnitude = 32767;
        // adjust for deadzone
        magnitude -= XINPUT_LEFT_THUMB_DEADZONE;
        // normalize the magnitude with respect for the deadzone
        real32 normalizedMagnitude = magnitude / (32767 - XINPUT_LEFT_THUMB_DEADZONE);

        // produces values between 0..1 with respect to the deadzone
        stick.x = normalizedLX * normalizedMagnitude; 
        stick.y = normalizedLY * normalizedMagnitude;
    }
    else
    {
        stick.x = 0;
        stick.y = 0;
    }
}

internal void
Win32ReadGamepad(uint32 nGamepadIndex, InputController& gamepad)
{
    XINPUT_STATE state = {};
    DWORD dwResult = XInputGetState(nGamepadIndex, &state);

    gamepad.isAnalog = true;

    if(dwResult == ERROR_SUCCESS)
    {
        gamepad.isConnected = true;
        // controller is connected
        Win32ProcessGamepadStick(gamepad.leftStick, state.Gamepad.sThumbLX, state.Gamepad.sThumbLY, XINPUT_LEFT_THUMB_DEADZONE);
        Win32ProcessGamepadStick(gamepad.rightStick, state.Gamepad.sThumbRX, state.Gamepad.sThumbRY, XINPUT_RIGHT_THUMB_DEADZONE);

        // for now we'll set the vibration of the left and right motors equal to the trigger magnitude
        uint8 leftTrigger = state.Gamepad.bLeftTrigger;
        uint8 rightTrigger = state.Gamepad.bRightTrigger;

        // clamp threshold
        if(leftTrigger < XINPUT_TRIGGER_THRESHOLD) leftTrigger = 0;
        if(rightTrigger < XINPUT_TRIGGER_THRESHOLD) rightTrigger = 0;

        gamepad.leftTrigger.value = leftTrigger / 255.0f;
        gamepad.rightTrigger.value = rightTrigger / 255.0f;

        WORD dwButtonIds[14] = {
            XINPUT_GAMEPAD_DPAD_UP, XINPUT_GAMEPAD_DPAD_DOWN, XINPUT_GAMEPAD_DPAD_LEFT, XINPUT_GAMEPAD_DPAD_RIGHT,
            XINPUT_GAMEPAD_Y, XINPUT_GAMEPAD_A, XINPUT_GAMEPAD_X, XINPUT_GAMEPAD_B,
            XINPUT_GAMEPAD_LEFT_SHOULDER, XINPUT_GAMEPAD_RIGHT_SHOULDER,
            XINPUT_GAMEPAD_LEFT_THUMB, XINPUT_GAMEPAD_RIGHT_THUMB,
            XINPUT_GAMEPAD_START, XINPUT_GAMEPAD_BACK
        };

        for(int buttonIndex = 0; buttonIndex < 14; ++buttonIndex)
        {
            InputButton& newButton = gamepad.buttons[buttonIndex];
            InputButton oldButton = gamepad.buttons[buttonIndex];
            bool pressed = (state.Gamepad.wButtons & dwButtonIds[buttonIndex]) == dwButtonIds[buttonIndex];
            Win32ProcessGamepadButton(newButton, oldButton, pressed);
        }
    }
    else
    {
        // controller not connected
        gamepad.isConnected = false;
    }
}