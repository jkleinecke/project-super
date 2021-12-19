
#define DECLARE_GB_FUNCTION(ret, name, ...)   \
    typedef PS_API ret(PS_APICALL* name##Fn)(__VA_ARGS__); \
    name##Fn    name

struct vg_backend;

struct ps_graphics_backend_api
{
    vg_backend* instance;

    // TODO(james): create,resize,destroy swapchain per platform window/surface
    //ResizeSwapChain(vg_backend &graphics, )

    DECLARE_GB_FUNCTION(void, BeginFrame, vg_backend* vb);
    DECLARE_GB_FUNCTION(void, EndFrame, vg_backend* vb, GameClock& clock);
};

#define LOAD_GRAPHICS_BACKEND(name) ps_graphics_backend_api name(HINSTANCE hInstance, HWND hWnd)
typedef LOAD_GRAPHICS_BACKEND(load_graphics_backend);

#define UNLOAD_GRAPHICS_BACKEND(name) void name(vg_backend* backend)
typedef UNLOAD_GRAPHICS_BACKEND(unload_graphics_backend);

