// This is the main graphics interface with the backend graphics api drivers
#include <tiny_imageformat/include/tiny_imageformat/tinyimageformat.h>

// a handle with a value of 0 will always be considered invalid
#define GFX_INVALID_HANDLE 0

struct GfxDevice { u64 handle; };
struct GfxBuffer { u64 handle; };
struct GfxImage { u64 handle; };
struct GfxRenderTarget { u64 handle; };
struct GfxPipelineState { u64 handle; };
struct GfxShaderModule { u64 handle; };
struct GfxSyncToken { u64 handle; };

enum class GfxBufferFlags
{
    Vertex,
    Index,
    Uniform,
    Storage
};
MAKE_ENUM_FLAG(u32, GfxBufferFlags);

enum class GfxMemoryFlags
{
    DeviceOnly,     // GPU Only
    Upload,         // CPU  -> GPU Transfer
    Readback,       // GPU -> CPU Transfer
    // TODO(james): determine proper fallback when not available, this is the infamous resizeable BAR section
    Upload_BAR,     // CPU Write-Only -> GPU Fast Read - Uncached & Write-Combined (Not available on all devices) 
};
MAKE_ENUM_FLAG(u32, GfxMemoryFlags);

struct GfxBufferDesc
{
    GfxBufferFlags usageFlags;
    GfxMemoryFlags memoryFlags;
    umm size;
};

struct GfxImageDesc
{
    GfxTextureType type;
    v3 dimensions;
    TinyImageFormat format;     // uses tiny image format to easily convert to graphics backend api format
};

struct GfxRenderTargetDesc
{
    // not really sure what this needs...
};

struct GfxPipelineStateDesc
{
    // and this is the big boy...

    // shader program
    // root signature
    // vertex layout
    // blend state
    // depth state
    // rasterizer state
    
    u32 renderTargetCount;
    TinyImageFormat colorFormats[8];
    // TODO(james): MSAA config
    TinyImageFormat depthStencilFormat;

    // primitive topology
    // indirect command buffer support?

};

enum class GfxShaderType
{
    // Source will require that the backend be able to compile at runtime...
    // Source, // TODO(james): We may need to specify the actual stage type, Vertex/Pixel/Tessalation...
    SPIRV,
};

struct GfxShaderDesc
{
    GfxShaderType type;
    umm size;
    void* data;
};

enum class GfxCommandType
{
    BindRenderTargets,
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

struct GfxBindRenderTargetsCommand
{
    u32 count;
    GfxRenderTarget rts[8];    // TODO(james): is 16 a decent cap here?? I have no idea
};

enum class GfxIndexFormat
{
    U16,
    U32,
};

struct GfxIndexBufferView
{
    GfxBuffer buffer;
    umm offset;
    umm size;
    GfxIndexFormat format;
};

struct GfxVertexBufferView
{
    GfxBuffer buffer;
    umm offset;
    umm size;
    u32 strideInBytes;
};


enum class GfxSamplerAddressMode
{
    Repeat              = 0,    // VK_SAMPLER_ADDRESS_MODE_REPEAT
    MirrorRepeat        = 1,    // VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT
    ClampEdge           = 2,    // VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
    ClampBorder         = 3,    // VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER
    MirrorClampEdge     = 4     // VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE
};

enum class GfxSamplerMipMapMode
{
    Nearest             = 0,    // VK_SAMPLER_MIPMAP_MODE_NEAREST
    Linear              = 1     // VK_SAMPLER_MIPMAP_MODE_LINEAR
};

enum class GfxSamplerFilter
{
    Nearest             = 0,            // VK_FILTER_NEAREST
    Linear              = 1,            // VK_FILTER_LINEAR
    Cubic               = 1000015000,   // VK_FILTER_CUBIC_IMG
};

struct GfxSampler   // TODO(james): should this be aligned??
{
    b32                 enableAnisotropy;
    b32                 coordinatesNotNormalized;

    GfxSamplerFilter       minFilter;
    GfxSamplerFilter       magFilter;

    GfxSamplerAddressMode  addressMode_U;
    GfxSamplerAddressMode  addressMode_V;
    GfxSamplerAddressMode  addressMode_W;

    f32                 mipLodBias;
    GfxSamplerMipMapMode   mipmapMode;
    f32                 minLod;
    f32                 maxLod;
};

struct GfxDrawCommand
{
    GfxIndexBufferView indexView;   // 0 for non-indexed drawing
    u32 vertexBufferCount;
    GfxVertexBufferView vertexBuffers[15];

    // TODO(james): how do I represent per-draw call shader variables/uniforms
};

struct GfxDispatchCommand
{
    u32 groupX;
    u32 groupY;
    u32 groupZ;
};

struct GfxBufferRegion
{
    GfxBuffer buffer;
    umm offset;
    umm size;
};

struct GfxImageRegion
{
    GfxImage image;
    umm bufferOffset;
    umm bufferRowLength;
    umm bufferHeight;
    v3 imageOffset;
    v3 imageExtent;
};

struct GfxCopyCommand
{
    // TODO(james): define src and dest
    union {
        GfxBufferRegion srcRegion;
    };

    union {
        GfxBufferRegion destRegion;
    };
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
    // TODO(james): Schedule work on different queues (Transfer, Compute, Graphics, etc..)
    API_FUNCTION(GfxSyncToken, SubmitWork, GfxDevice device, u32 numCommandLists, GfxCommandList* pCommandLists);

    // Resource Management
    // TODO(james): Track resource creation with __FILE__, __LINE__ trick?
    API_FUNCTION(GfxBuffer, CreateBuffer, GfxDevice device, GfxBufferDesc* pBufferDesc);
    API_FUNCTION(void, DestroyBuffer, GfxDevice device, GfxBuffer buffer);
    API_FUNCTION(GfxImage, CreateImage, GfxDevice device, GfxImageDesc* pImageDesc);
    API_FUNCTION(void, DestroyImage, GfxDevice device, GfxImage image);
    API_FUNCTION(GfxRenderTarget, CreateRenderTarget, GfxDevice device, GfxRenderTargetDesc* pRenderTargetDesc);
    API_FUNCTION(void, DestroyRenderTarget, GfxDevice device, GfxRenderTarget renderTarget);
    API_FUNCTION(GfxPipelineState, CreateStateBlock, GfxDevice device, GfxPipelineStateDesc* pStateBlockDesc);
    API_FUNCTION(void, DestroyStateBlock, GfxDevice device, GfxStateBlock stateBlock);
    API_FUNCTION(GfxShaderModule, CreateShaderModule, GfxDevice device, GfxShaderDesc* pShaderDesc);
    API_FUNCTION(void, DestroyShader, GfxDevice device, GfxShaderModule shaderModule);
};