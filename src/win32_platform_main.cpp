

#include "project_super.h"
#include "project_super.cpp"

#include <windows.h>

struct win32_dimensions
{
    uint32 width;
    uint32 height;
};

global_variable bool g_Running = true;
global_variable win32_dimensions g_backbuffer_dimensions = {1280, 720};
global_variable BITMAPINFO g_backbuffer_bmpInfo;
global_variable void* g_backbuffer_memory; 

global_variable int32 g_x_offset;
global_variable int32 g_y_offset;


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

internal void 
Win32ResizeBackBuffer(uint32 width, uint32 height)
{
    if(g_backbuffer_memory)
    {
        VirtualFree(&g_backbuffer_memory, 0, MEM_RELEASE);
    }

    g_backbuffer_dimensions.width = width;
    g_backbuffer_dimensions.height = height;

    g_backbuffer_bmpInfo.bmiHeader.biSize = sizeof(g_backbuffer_bmpInfo.bmiHeader);
    g_backbuffer_bmpInfo.bmiHeader.biWidth = width;
    g_backbuffer_bmpInfo.bmiHeader.biHeight = height;
    g_backbuffer_bmpInfo.bmiHeader.biPlanes = 1;
    g_backbuffer_bmpInfo.bmiHeader.biBitCount = 32;
    g_backbuffer_bmpInfo.bmiHeader.biCompression = BI_RGB;

    g_backbuffer_memory = VirtualAlloc(0, width*height*4, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE) ;
    // TODO(james): verify that we could allocate the memory...
}

internal void
Win32RenderGradient(int blueOffset, int greenOffset)
{
    uint8* row = (uint8*)g_backbuffer_memory;
    for(int y = 0; y < g_backbuffer_dimensions.height; ++y)
    {
        uint32* pixel = (uint32*)row;
        for(int x = 0; x < g_backbuffer_dimensions.width; ++x)
        {
            uint8 green = y + greenOffset;
            uint8 blue = x + blueOffset;

            *pixel++ = (green << 8) | blue;
        }

        row += g_backbuffer_dimensions.width * 4;   // TODO(james): track the pitch
    }
}

internal void
Win32DisplayBufferInWindow(HDC deviceContext, uint32 windowWidth, uint32 windowHeight)
{
    StretchDIBits(deviceContext,
     0, 0, windowWidth, windowHeight,
     0, 0, g_backbuffer_dimensions.width, g_backbuffer_dimensions.height,
     g_backbuffer_memory,
     &g_backbuffer_bmpInfo,
     DIB_RGB_COLORS, SRCCOPY);
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
        case WM_SIZE:
            {
                Win32ResizeBackBuffer(LOWORD(lParam), HIWORD(lParam));
            } break;
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
                
                win32_dimensions dimensions = Win32GetWindowDimensions(hwnd);
                Win32DisplayBufferInWindow(deviceContext, dimensions.width, dimensions.height);

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

    HDC windowDeviceContext = GetDC(hMainWindow);   // CS_OWNDC allows us to get this just once...

    Win32ResizeBackBuffer(1280, 720);

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


        Win32RenderGradient(g_x_offset++, g_y_offset++);
        
        win32_dimensions dimensions = Win32GetWindowDimensions(hMainWindow);
        Win32DisplayBufferInWindow(windowDeviceContext, dimensions.width, dimensions.height);
    }
    

    return 0;
}
