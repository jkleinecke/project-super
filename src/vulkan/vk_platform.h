
#define DECLARE_GB_FUNCTION(ret, name, ...)   \
    typedef PS_API ret(PS_APICALL* name##Fn)(__VA_ARGS__); \
    name##Fn    name

struct vg_backend;

struct ps_graphics_backend_api
{
    // TODO(james): create,resize,destroy swapchain per platform window/surface
    //ResizeSwapChain(vg_backend &graphics, )

    

    // DECLARE_GB_FUNCTION(void, BeginFrame, vg_backend* vb, render_commands* commands);
    // DECLARE_GB_FUNCTION(void, EndFrame, vg_backend* vb, render_commands* commands);

    //DECLARE_GB_FUNCTION(render_sync_token, AddResourceOperation, render_resource_queue* queue, RenderResourceOpType operationType, render_manifest* manifest);
    //DECLARE_GB_FUNCTION(b32, IsResourceOperationComplete, render_resource_queue* queue, render_sync_token operationToken);
};

// struct render_resource_op
// {
//     RenderResourceOpType type;
//     render_manifest* manifest;
// };

// typedef u64 render_sync_token;

// struct render_resource_queue
// {
//     render_sync_token volatile requestedSyncToken;
//     render_sync_token volatile currentSyncToken;

//     u32 volatile readIndex;
//     u32 volatile writeIndex;
//     //HANDLE semaphore;

//     render_resource_op resourceOps[256];
// };

struct ps_graphics_backend
{
    gfx_api gfx;
    vg_backend* instance;
    f32 width;
    f32 height;
};

#if PROJECTSUPER_WIN32
    #define LOAD_GRAPHICS_BACKEND(name) ps_graphics_backend name(HINSTANCE hInstance, HWND hWnd)
#elif PROJECTSUPER_MACOS
    #define LOAD_GRAPHICS_BACKEND(name) ps_graphics_backend name(CAMetalLayer* pMetalLayer, f32 windowWidth, f32 windowHeight)
#else
    NotImplemented
#endif

typedef LOAD_GRAPHICS_BACKEND(load_graphics_backend);

#define UNLOAD_GRAPHICS_BACKEND(name) void name(ps_graphics_backend* backend)
typedef UNLOAD_GRAPHICS_BACKEND(unload_graphics_backend);

