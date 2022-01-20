// This is the main graphics interface with the backend graphics api drivers

// a handle with a value of 0 will always be considered invalid
#define GFX_INVALID_HANDLE 0

struct GfxDevice { u64 handle; };
struct GfxBuffer { u64 handle; };
struct GfxImage { u64 handle; };
struct GfxStateBlock { u64 handle; };
struct GfxShaderModule { u64 handle; };

struct GfxBufferDesc
{

};

struct GfxImageDesc
{

};

struct GfxStateBlockDesc
{

};

struct GfxShaderDesc
{

};

enum class GfxCommandType
{
    BeginRenderpass,
    EndRenderpass,
    Draw,
    Dispatch,
    Copy,
    Update,
    Query
};

struct GfxCommandHeader
{
    u32 type;
    umm size;   // size (in bytes) of the command structure
};

struct GfxBeginRenderpassCommand
{

};

struct GfxEndRenderpassCommand
{

};

struct GfxDrawCommand
{

};

struct GfxDispatchCommand
{

};

struct GfxCopyCommand
{
    // TODO(james): define src and dest
    // buffer -> buffer
    // buffer -> image
    // image -> buffer
    // image -> image
};

struct GfxUpdateCommand
{
    // will move data from CPU -> GPU
};

struct GfxQueryCommand
{
    // will move data from GPU -> CPU?  Readback...
    // TODO(james): not 100% sure this is the correct nomenclature for a Graphics API Query
};


struct GfxCommandList
{
    u32 numCommands;
    GfxCommandHeader* cmds;
};

// TODO(james): Platform has to pass a valid GfxDevice and GfxImage swapchain handle...
struct gfx_api
{
    API_FUNCTION(void, SubmitWork, GfxDevice device, u32 numCommandLists, GfxCommandList* pCommandLists);

    // Resource Management
    // TODO(james): Track resource creation with __FILE__, __LINE__ trick?
    API_FUNCTION(GfxBuffer, CreateBuffer, GfxDevice device, GfxBufferDesc* pBufferDesc);
    API_FUNCTION(void, DestroyBuffer, GfxDevice device, GfxBuffer buffer);
    API_FUNCTION(GfxImage, CreateImage, GfxDevice device, GfxImageDesc* pImageDesc);
    API_FUNCTION(void, DestroyImage, GfxDevice device, GfxImage image);
    API_FUNCTION(GfxStateBlock, CreateStateBlock, GfxDevice device, GfxStateBlockDesc* pStateBlockDesc);
    API_FUNCTION(void, DestroyStateBlock, GfxDevice device, GfxStateBlock stateBlock);
    API_FUNCTION(GfxShaderModule, CreateShaderModule, GfxDevice device, GfxShaderDesc* pShaderDesc);
    API_FUNCTION(void, DestroyShader, GfxDevice device, GfxShaderModule shaderModule);
};