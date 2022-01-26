// This is the main graphics interface with the backend graphics api drivers
#include <tiny_imageformat/include/tiny_imageformat/tinyimageformat.h>

// a handle with a value of 0 will always be considered invalid
#define GFX_INVALID_HANDLE 0

enum GfxConstants
{
    GFX_MAX_EVENT_NAME_LENGTH = 256,
    GFX_MAX_RENDERTARGETS = 8,
    GFX_MAX_ANISOTROPY = 8,
    GFX_MAX_BINDLESSSLOTS = 1024,
};

enum class GfxResult
{
    Ok,
    InvalidParameter,
    InvalidOperation,
    OutOfMemory,
    InternalError
};

struct GfxContext { u64 handle; };
struct GfxBuffer { u64 handle; };
struct GfxTexture { u64 handle; };
struct GfxSampler { u64 handle; };
struct GfxProgram { u64 handle; };
struct GfxKernel { u64 handle; };
struct GfxTimestampQuery { u64 handle; };


enum class GfxMemoryAccess
{
    Unknown,    // 
    GpuOnly,     // GPU Only
    CpuOnly,         // CPU  -> GPU Transfer
    CpuToGpu,       // CPU -> GPU Transfer, CPU Write-Only -> GPU Fast Read - Uncached & Write-Combined (Not available on all devices) 
    // TODO(james): determine proper fallback when not available, this is the infamous resizeable BAR section
    GpuToCpu,     // GPU -> CPU Readback Transfer
};

struct GfxBufferDesc
{
    u64 size;
    GfxMemoryAccess access;
    u32 stride;
    u32 numElements;
};  

struct GfxBufferRangeDesc
{
    GfxBuffer buffer;
    umm elementOffset;
    umm numElements;
    umm stride;
};

enum class GfxTextureType
{
    Tex2D,
    Tex2DArray,
    Tex3D,
    Cube,
};

struct GfxImageDesc
{
    GfxTextureType type;
    u32 width;
    u32 height;
    union {
        u32 size;
        u32 depth;
        u32 slice_count;
    };
    TinyImageFormat format;     // uses tiny image format to easily convert to graphics backend api format
    u32 mipLevels;
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

struct GfxSamplerDesc
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

struct GfxShaderDesc
{
    umm size;
    void* data;
    char* szEntryPoint;
};

struct GfxProgramDesc
{
    GfxShaderDesc* pCompute;
    GfxShaderDesc* pVertex;
    union {
        GfxShaderDesc* pHull;
        GfxShaderDesc* pTessellationControl;
    };
    union {
        GfxShaderDesc* pDomain;
        GfxShaderDesc* pTessellationEvaluation;
    };
    GfxShaderDesc* pGeometry;
    GfxShaderDesc* pFragment;
};

enum class GfxPrimitiveTopology
{
    Undefined,
    TriangleList,
    TriangleStrip,
    PointList,
    LineList,
    LineStrip,
};

enum class GfxComparisonFunc
{
    Never,
    Less,
    Equal,
    LessEqual,
    Greater,
    NotEqual,
    GreaterEqual,
    Always,
};

enum class GfxDepthWriteMask
{
    Zero,   // Disables depth write
    All,    // enables depth write
};

enum class GfxStencilOp
{
    Keep,
    Zero,
    Replace,
    Inc,
    Dec,
    IncSat,
    DecSat,
    Invert,
};

enum class GfxBlendMode
{   
    Zero,
    One,
    SrcColor,
    InvSrcColor,
    SrcAlpha,
    InvSrcAlpha,
    DestAlpha,
    InvDestAlpha,
    DestColor,
    InvDestColor,
    SrcAlphaSat,
    BlendFactor,
    InvBlendFactor,
    Src1Color,
    InvSrc1Color,
    Src1Alpha,
    InvSrc1Alpha,
};

enum class GfxBlendOp
{
    Add,
    Subtract,
    RevSubtract,
    Min,
    Max,
};

enum class GfxFillMode
{
    Wireframe,
    Solid,
};

enum class GfxCullMode
{
    None,
    Front,
    Back,
};

enum class GfxSampleCount
{
    MSAA_1,
    MSAA_2,
    MSAA_4,
    MSAA_8,
    MSAA_16,
};

enum class GfxColorWriteFlags
{
    Disable     = 0,
    Red         = 1 << 0,
    Green       = 1 << 1,
    Blue        = 1 << 2,
    Alpha       = 1 << 3,
    All         = ~0,
};
MAKE_ENUM_FLAG(u32, GfxColorWriteFlags)

struct GfxRasterizerState
{
    GfxFillMode fillMode;
    GfxCullMode cullMode;
    b32 frontCCW;           // NOTE(james): Front side is counter clock-wise
    i32 depthBias;
    f32 slopeScaledDepthBias;
    b32 multisample;
    b32 scissor;
    b32 depthClampEnable;
};

struct GfxDepthStencilOp
{
    GfxStencilOp stencilFail;
    GfxStencilOp depthFail;
    GfxStencilOp stencilPass;
    GfxComparisonFunc stencilFunc;
};

struct GfxDepthStencilState
{
    b32 depthEnable;
    GfxDepthWriteMask depthWriteMask;
    GfxComparisonFunc depthFunc;
    b32 stencilEnable;
    u8 stencilReadMask;
    u8 stencilWriteMask;
    GfxDepthStencilOp frontFace;
    GfxDepthStencilOp backFace;
    b32 depthBoundsTestEnable;
};

struct GfxRenderTargetBlendState
{
    b32 blendEnable;
    GfxBlendMode srcBlend;
    GfxBlendMode destBlend;
    GfxBlendOp blendOp;
    GfxBlendMode srcAlphaBlend;
    GfxBlendMode destAlphaBlend;
    GfxBlendOp blendOpAlpha;
    GfxColorWriteFlags colorWriteMask;
};

struct GfxBlendState
{
    b32 alphaToCoverageEnable;
    b32 independentBlendMode;
    GfxRenderTargetBlendState renderTargets[GFX_MAX_RENDERTARGETS];
};

// enum class GfxVertexAttribRate
// {
//     Vertex,
//     Instance,
// };

// struct GfxVertexAttrib
// {
//     GfxShaderSemantic semantic;
//     u32 semanticNameLength;
//     char semanticName[128]; // TODO(james): set size as define/enum/constant
//     u32 location;
//     u32 binding;
//     u32 offset;
//     TinyImageFormat format;
//     GfxVertexAttribRate rate;
// };

// struct GfxVertexLayout
// {
//     u32 attribCount;
//     GfxVertexAttrib attribs[15];    // TODO(james): set size as define/enum/constant
// };

struct GfxRenderTargetDesc
{
    GfxTexture texture;
    u32 mipLevel;
    u32 slice;
};

struct GfxPipelineDesc
{
    GfxBlendState blendState;
    GfxDepthStencilState depthStencilState;
    GfxRasterizerState rasterizerState;
    GfxPrimitiveTopology primitiveTopology;
    
    GfxRenderTargetDesc colorTargets[GFX_MAX_RENDERTARGETS];
    GfxRenderTargetDesc depthStencilTarget;

    GfxSampleCount sampleCount;
    b32 supportIndirectCommandBuffer;
};


enum class GfxCmdType
{
    CopyBuffer,
    CopyBufferRange,
    ClearBuffer,
    ClearBackBuffer,
    ClearTexture,
    CopyTexture,
    ClearImage,
    CopyTextureToBackBuffer,
    CopyBufferToTexture,
    GenerateMips,
    BindKernel,
    BindIndexBuffer,
    BindVertexBuffer,
    SetViewport,
    SetScissorRect,
    Draw,
    DrawIndexed,
    MultiDrawIndirect,
    MultiDrawIndexedIndirect,
    Dispatch,
    DispatchIndirect,
    MultiDispatchIndirect,

    // Debug Commands
    BeginTimestampQuery,
    EndTimestampQuery,
    BeginEvent,
    BeginColorEvent,
    EndEvent,
};

struct GfxCmdHeader
{
    u32 type;
    umm size;   // size (in bytes) of the command structure
};

struct GfxCopyBufferCmd
{
    GfxBuffer src;
    GfxBuffer dest;
};

struct GfxCopyBufferRangeCmd
{
    GfxBuffer src;
    u64 srcOffset;
    GfxBuffer dest;
    u64 destOffset;
    u64 size;
};

struct GfxClearBufferCmd
{
    GfxBuffer buffer;
    u32 clearValue;
};

struct GfxClearBackBufferCmd
{
};

struct GfxClearTextureCmd
{
    GfxTexture texture;
};

struct GfxCopyTextureCmd
{
    GfxTexture src;
    GfxTexture dest;
};

struct GfxClearImageCmd
{
    GfxTexture texture;
    u32 mipLevel;
    u32 slice;
};

struct GfxCopyTextureToBackBufferCmd
{
    GfxTexture texture;
};

struct GfxCopyBufferToTextureCmd
{
    GfxBuffer src;
    GfxTexture dest;
};

struct GfxGenerateMipsCmd
{
    GfxTexture texture;
};

struct GfxBindKernelCmd
{
    GfxKernel kernel;
};

struct GfxBindIndexBufferCmd
{
    GfxBuffer buffer;
};

struct GfxBindVertexBufferCmd
{
    GfxBuffer buffer;
};

struct GfxSetViewportCmd
{
    f32 x;
    f32 y;
    f32 width;
    f32 height;
};

struct GfxSetScissorRectCmd
{
    i32 x;
    i32 y;
    i32 width;
    i32 height;
};

struct GfxDrawCmd
{
    u32 vertexCount;
    u32 instanceCount;
    u32 baseVertex;
    u32 baseInstance;
};

struct GfxDrawIndexedCmd
{
    u32 indexCount;
    u32 instanceCount;
    u32 firstIndex;
    u32 baseVertex;
    u32 baseInstance;
};

struct GfxMultiDrawIndirectCmd
{
    GfxBuffer argsBuffer;
    u32 argsCount;
};

struct GfxMultiDrawIndexedIndirectCmd
{
    GfxBuffer argsBuffer;
    u32 argsCount;
};

struct GfxDispatchCmd
{
    u32 numGroupsX;
    u32 numGroupsY;
    u32 numGroupsZ;
};

struct GfxDispatchIndirectCmd
{
    GfxBuffer argsBuffer;
};

struct GfxMultiDispatchIndirectCmd
{
    GfxBuffer argsBuffer;
    u32 argsCount;
};

struct GfxBeginTimestampQueryCmd
{
};

struct GfxEndTimestampQueryCmd
{
};

struct GfxBeginEventCmd
{
    char szName[GFX_MAX_EVENT_NAME_LENGTH];
};

struct GfxBeginColorEventCmd
{
    u64 color;
    char szName[GFX_MAX_EVENT_NAME_LENGTH];
};

struct GfxEndEventCmd
{
};

struct GfxCmdList
{
    u32 numCommands;
    GfxCmdHeader* cmds;
};

// TODO(james): Platform has to pass a valid GfxDevice and GfxImage swapchain handle...
struct gfx_api
{
    // Resource Management
    // TODO(james): Track resource creation with __FILE__, __LINE__ trick?
    API_FUNCTION(GfxBuffer, CreateBuffer, GfxContext context, GfxBufferDesc* pBufferDesc, void const* data);
    API_FUNCTION(GfxBuffer, CreateBufferRange, GfxContext context, GfxBufferRangeDesc* pBufferRangeDesc);
    API_FUNCTION(void*, GetBufferData, GfxContext context, GfxBuffer buffer);
    API_FUNCTION(GfxResult, DestroyBuffer, GfxContext context, GfxBuffer buffer);
    
    API_FUNCTION(GfxImage, CreateImage, GfxContext context, GfxImageDesc* pImageDesc);
    API_FUNCTION(GfxResult, DestroyImage, GfxContext context, GfxImage image);

    API_FUNCTION(GfxSampler, CreateSampler, GfxContext context, GfxSamplerDesc* pSamplerDesc);
    API_FUNCTION(GfxResult, DestroySampler, GfxContext context, GfxSampler sampler);

    API_FUNCTION(GfxProgram, CreateProgram, GfxContext context, GfxProgramDesc* pProgramDesc);
    API_FUNCTION(GfxResult, DestroyProgram, GfxContext context, GfxProgram program);
    API_FUNCTION(GfxResult, SetProgramBuffer, GfxContext context, GfxProgram program, const char* param_name, GfxBuffer buffer);
    API_FUNCTION(GfxResult, SetProgramTexture, GfxContext context, GfxProgram program, const char* param_name, GfxTexture texture, u32 mipLevel);
    API_FUNCTION(GfxResult, SetProgramTextures, GfxContext context, GfxProgram program, const char* param_name, u32 textureCount, GfxTexture* pTextures, const u32* mipLevels);
    API_FUNCTION(GfxResult, SetProgramSampler, GfxContext context, GfxProgram program, const char* param_name, GfxSampler sampler);
    API_FUNCTION(GfxResult, SetProgramConstants, GfxContext context, GfxProgram program, const char* param_name, const void* data, u32 size);

    API_FUNCTION(GfxKernel, CreateComputeKernel, GfxContext context, GfxProgram program);
    API_FUNCTION(GfxKernel, CreateGraphicsKernel, GfxContext context, GfxProgram program, GfxPipelineDesc* pPipelineDesc);
    API_FUNCTION(GfxResult, DestroyKernel, GfxContext context, GfxKernel kernel);

    // Debug Functions
    API_FUNCTION(GfxTimestampQuery, CreateTimestampQuery, GfxContext context);
    API_FUNCTION(GfxResult, DestroyTimestampQuery, GfxContext context, GfxTimestampQuery timestampQuery);
    API_FUNCTION(f32, GetTimestampQueryDuration, GfxContext context, GfxTimestampQuery timestampQuery);

    // Frame Processing
    // TODO(james): Schedule work on different queues (Transfer, Compute, Graphics, etc..)
    API_FUNCTION(GfxResult, SubmitCommands, GfxContext context, u32 count, GfxCmdList* pLists);
    API_FUNCTION(GfxResult, Frame, GfxContext context, b32 vsync);
    API_FUNCTION(GfxResult, Finish, GfxContext context);
};