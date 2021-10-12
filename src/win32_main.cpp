

#include "project_super.h"
#include "project_super.cpp"

#include <windows.h>

global_variable bool g_Running = true;

struct win32_dimensions
{
    uint32 width;
    uint32 height;
};

internal win32_dimensions
Win32GetWindowDimensions(HWND hWnd)
{
    win32_dimensions result;

    RECT clientRect;
    GetClientRect(hWnd, &clientRect);
    result.width = clientRect.right - clientRect.left;
    result.height = clientRect.bottom - clientRect.top;

    return result;
}

internal
LRESULT CALLBACK
Win32WindowProc(
  HWND   hwnd,
  UINT   uMsg,
  WPARAM wParam,
  LPARAM lParam)
{
    LRESULT retValue = 0;

    switch(uMsg)
    {
        case WM_ACTIVATEAPP:
            // TODO(james): handle being set to foreground/background
            break;
        case WM_CLOSE:
            // TODO(james): verify that we should actually close here
            g_Running = false;
            break;
        // case WM_SIZE:
        //     break;
        case WM_QUIT:
            g_Running = false;
            break;
        case WM_DESTROY:
            g_Running = false;
            break;
        case WM_PAINT:
            {
                PAINTSTRUCT ps;
                HDC deviceContext = BeginPaint(hwnd, &ps);
                // win32_dimensions dimensions = Win32GetWindowDimensions(hwnd);

                RECT clientRect;
                GetClientRect(hwnd, &clientRect);
                HBRUSH brush = CreateSolidBrush(RGB(255,0,0));
                FillRect(deviceContext, &clientRect, brush);
                DeleteObject(brush);

                EndPaint(hwnd, &ps);
                
            } break;
        default:
            retValue = DefWindowProcA(hwnd, uMsg, wParam, lParam);
    }

    return retValue;
}

internal
int CALLBACK
WinMain(HINSTANCE Instance,
        HINSTANCE PrevInstance,
        LPSTR CommandLine,
        int ShowCode)
{
    WNDCLASSA wndClass = {};
    wndClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wndClass.lpfnWndProc = Win32WindowProc;
    wndClass.hInstance = Instance;
    wndClass.lpszClassName = "ProjectSuperWindow";

    RegisterClassA(&wndClass);

    HWND hMainWindow = CreateWindowExA(
            0,
            "ProjectSuperWindow",
            "Project Super",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            1280,
            720,
            0,
            0,
            Instance,
            0           // TODO(james): pass a pointer to a struct containing the state variables we might want to use in the message handling
        );

    MSG msg;
    while(g_Running)
    {
        if(PeekMessageA(&msg, 0, 0, 0, PM_REMOVE))
        {
            if(msg.message == WM_QUIT)
            {
                break;
            }

            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }
    

    return 0;
}
