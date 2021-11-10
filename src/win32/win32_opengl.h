// GLAD PERMALINK
#pragma once
#ifndef WIN32_OPENGL_H_
#define WIN32_OPENGL_H_

#include "win32_platform.h"
#include <gl/gl.h>
// TODO(james): copy out the defines we need and get rid of these files 
//#include "glext.h"
//#include "wglext.h"

#define WGL_CONTEXT_DEBUG_BIT_ARB         0x00000001
#define WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB 0x00000002
#define WGL_CONTEXT_MAJOR_VERSION_ARB     0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB     0x2092
#define WGL_CONTEXT_FLAGS_ARB             0x2094

#define WGL_CONTEXT_PROFILE_MASK_ARB      0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB  0x00000001

#define WGL_DRAW_TO_WINDOW_ARB            0x2001
#define WGL_DOUBLE_BUFFER_ARB             0x2011
#define WGL_ACCELERATION_ARB              0x2003
#define WGL_FULL_ACCELERATION_ARB         0x2027
#define WGL_SUPPORT_OPENGL_ARB            0x2010
#define WGL_PIXEL_TYPE_ARB                0x2013
#define WGL_COLOR_BITS_ARB                0x2014
#define WGL_ALPHA_BITS_ARB                0x201B
#define WGL_DEPTH_BITS_ARB                0x2022
#define WGL_STENCIL_BITS_ARB              0x2023
#define WGL_TYPE_RGBA_ARB                 0x202B

#define WGL_SAMPLE_BUFFERS_ARB            0x2041
#define WGL_SAMPLES_ARB                   0x2042

// forward declare the winproc for now
internal LRESULT CALLBACK Win32WindowProc(HWND,UINT,WPARAM,LPARAM);

typedef HGLRC WINAPI wgl_create_context_attribs_arb(HDC hDC, HGLRC hShareContext,
                                                    const int *attribList);

typedef BOOL WINAPI wgl_get_pixel_format_attrib_iv_arb(HDC hdc,
                                                       int iPixelFormat,
                                                       int iLayerPlane,
                                                       UINT nAttributes,
                                                       const int *piAttributes,
                                                       int *piValues);

typedef BOOL WINAPI wgl_get_pixel_format_attrib_fv_arb(HDC hdc,
                                                       int iPixelFormat,
                                                       int iLayerPlane,
                                                       UINT nAttributes,
                                                       const int *piAttributes,
                                                       FLOAT *pfValues);

typedef BOOL WINAPI wgl_choose_pixel_format_arb(HDC hdc,
                                                const int *piAttribIList,
                                                const FLOAT *pfAttribFList,
                                                UINT nMaxFormats,
                                                int *piFormats,
                                                UINT *nNumFormats);

typedef BOOL WINAPI wgl_swap_interval_ext(int interval);
typedef const char* WINAPI wgl_get_extensions_string_ext(void);

global wgl_create_context_attribs_arb* wglCreateContextAttribsARB;
global wgl_choose_pixel_format_arb* wglChoosePixelFormatARB;
global wgl_swap_interval_ext* wglSwapIntervalEXT;
global wgl_get_extensions_string_ext* wglGetExtensionsStringEXT;

global int Win32OpenGLAttribs[] =
{
    WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
    WGL_CONTEXT_MINOR_VERSION_ARB, 3,
    WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB
#if HANDMADE_INTERNAL
    |WGL_CONTEXT_DEBUG_BIT_ARB
#endif
    ,
#if 0
    WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
#else
    WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
#endif
    0,
};

internal void
Win32SetPixelFormat(HDC hDeviceContext)
{
    int suggestedPixelFormatIndex = 0;
    GLuint extendedPixelFormatsCount = 0;
    if(wglChoosePixelFormatARB)
    {
        int intAttribsList[] =
        {
            WGL_DRAW_TO_WINDOW_ARB, GL_TRUE, // 0
            WGL_SUPPORT_OPENGL_ARB, GL_TRUE, // 1
            WGL_DOUBLE_BUFFER_ARB, GL_TRUE, // 2
            WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB, // 3
            WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB, // 4
            WGL_COLOR_BITS_ARB, 32, // 5
            WGL_ALPHA_BITS_ARB, 8, // 6
            WGL_DEPTH_BITS_ARB, 24, // 7
            WGL_STENCIL_BITS_ARB, 8, // 8
            WGL_SAMPLE_BUFFERS_ARB, GL_TRUE, // 9
            WGL_SAMPLES_ARB, 4, // 10
            0
        };
        
        wglChoosePixelFormatARB(hDeviceContext, intAttribsList, 0, 1, &suggestedPixelFormatIndex, &extendedPixelFormatsCount);
    }
    
    if(!extendedPixelFormatsCount)
    {
        PIXELFORMATDESCRIPTOR fakePFD = {};
        fakePFD.nSize = sizeof(fakePFD);
        fakePFD.nVersion = 1;
        fakePFD.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        fakePFD.iPixelType = PFD_TYPE_RGBA;
        fakePFD.cColorBits = 32;
        fakePFD.cAlphaBits = 8;
        fakePFD.cDepthBits = 24;
        
        suggestedPixelFormatIndex = ChoosePixelFormat(hDeviceContext, &fakePFD);
    }
    
    PIXELFORMATDESCRIPTOR suggestedPixelFormat;
    DescribePixelFormat(hDeviceContext, suggestedPixelFormatIndex, sizeof(suggestedPixelFormat), &suggestedPixelFormat);
    SetPixelFormat(hDeviceContext, suggestedPixelFormatIndex, &suggestedPixelFormat);
}

internal void
Win32InitOpenGLWindow(win32_state& state)
{
    // Because OpenGL and Windows had a fight once, we have to trick
    // OpenGL into working as well as it possibly can.
    WNDCLASSEXA wndClass = {};
    wndClass.cbSize = sizeof(wndClass);
    wndClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wndClass.lpfnWndProc = Win32WindowProc;
    wndClass.hInstance = state.Instance;
    wndClass.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wndClass.lpszClassName = "ProjectSuperWindow";
    RegisterClassExA(&wndClass);
    
    // We first have to create a fake window and then
    // initialize OpenGL on that window to get the
    // wglChoosePixelFormatARB extension function.
    HWND hFakeWnd = CreateWindowExA(0,
                                    "ProjectSuperWindow", "Fake", 
                                 WS_OVERLAPPEDWINDOW,
                                 0,0,1,1,
                                 0,0,
                                 state.Instance, 0
                                 );
    
    HDC hFakeDC = GetDC(hFakeWnd);
    
    Win32SetPixelFormat(hFakeDC);
    
    HGLRC hFakeRC = wglCreateContext(hFakeDC);
    ASSERT(hFakeRC);  // TODO(james): log out error
    
    if(wglMakeCurrent(hFakeDC, hFakeRC))
    {
        
        // Now we can access the extended pixel formats with the full set of 
        // functionality available.  Petty Windows was trying to hide this stuff
        // from us because it got mad...
        wglChoosePixelFormatARB = (wgl_choose_pixel_format_arb*)wglGetProcAddress("wglChoosePixelFormatARB");
        wglCreateContextAttribsARB = (wgl_create_context_attribs_arb*)wglGetProcAddress("wglCreateContextAttribsARB");
        
        // TODO(james): use wglGetExtensionsStringEXT to gather a basic set of capabilities 
        // TODO(james): Load the rest of extensions here 
        
        wglMakeCurrent(0, 0);
    }
    
    wglDeleteContext(hFakeRC);
    ReleaseDC(hFakeWnd, hFakeDC);
    DestroyWindow(hFakeWnd);
    
    HWND hMainWindow = CreateWindowExA(0,
                                       "ProjectSuperWindow", "Project Super",
                                       WS_OVERLAPPEDWINDOW,
                                       CW_USEDEFAULT, CW_USEDEFAULT,
                                       1280, 720,
                                       0, 0,
                                       state.Instance, 0   
                                       );

   
    state.hMainWindow = hMainWindow;
                                 
    HDC windowDeviceContext = GetDC(hMainWindow);   // CS_OWNDC allows us to get this just once...
    Win32SetPixelFormat(windowDeviceContext);
    
    HGLRC hRC = wglCreateContextAttribsARB(windowDeviceContext, 0, Win32OpenGLAttribs);
    ASSERT(hRC); // TODO(james): more better verification
    
    wglMakeCurrent(windowDeviceContext, hRC);
    
    // testing code
    SetWindowText(hMainWindow, (LPCSTR)glGetString(GL_VERSION));

}

#endif