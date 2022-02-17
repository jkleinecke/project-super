// This is the main graphics interface with the backend graphics api drivers
#include <tiny_imageformat/include/tiny_imageformat/tinyimageformat.h>

// a handle with a value of 0 will always be considered invalid
#define GFX_INVALID_HANDLE 0
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

enum GfxConstants
{
    GFX_MAX_EVENT_NAME_LENGTH = 256,
    GFX_MAX_RENDERTARGETS = 8,
    GFX_MAX_ANISOTROPY = 8,
    GFX_MAX_BINDLESSSLOTS = 1024,
    GFX_MAX_SHADER_IDENTIFIER_NAME_LENGTH = 32,
    GFX_MAX_SHADER_PARAM_COUNT = 128,
};

enum class GfxResult
{
    Ok,
    InvalidParameter,
    InvalidOperation,
    OutOfMemory,
    OutOfHandles,
    InternalError, 
};

#define GFX_ASSERT_VALID(resource) ASSERT(resource.id)

struct GfxDevice { u64 id; };
struct GfxResourceHeap { u64 deviceId; u64 id; };
struct GfxCmdEncoderPool { u64 deviceId; u64 id; };
struct GfxCmdContext { u64 deviceId; u64 poolId; u64 id; };
struct GfxBuffer { u64 heap; u64 id; };
struct GfxTexture { u64 heap; u64 id; };
struct GfxSampler { u64 heap; u64 id; };
struct GfxProgram { u64 heap; u64 id; };
struct GfxKernel { u64 heap; u64 id; };
struct GfxRenderTarget { u64 heap; u64 id; };
struct GfxTimestampQuery { u64 heap; u64 id; };

enum class GfxMemoryAccess
{
    Unknown,    // 
    GpuOnly,     // GPU Only
    CpuOnly,         // CPU  -> GPU Transfer
    CpuToGpu,       // CPU -> GPU Transfer, CPU Write-Only -> GPU Fast Read - Uncached & Write-Combined (Not available on all devices) 
    GpuToCpu,     // GPU -> CPU Readback Transfer
};

enum class GfxBufferUsageFlags
{
    Uniform     = 0x01,
    Vertex      = 0x02,
    Index       = 0x04,
    Storage     = 0x08,
};
MAKE_ENUM_FLAG(u32, GfxBufferUsageFlags);

enum class GfxResourceState
{
	Undefined               = 0,
	VertexAndConstantBuffer = 0x1,
	IndexBuffer             = 0x2,
	RenderTarget            = 0x4,
	UnorderedAccess         = 0x8,
	DepthWrite              = 0x10,
	DepthRead               = 0x20,
	NonPixelShaderResource  = 0x40,
	PixelShaderResource     = 0x80,
	ShaderResource          = 0x40 | 0x80,
	StreamOut               = 0x100,
	IndirectArgument        = 0x200,
	CopyDst                 = 0x400,
	CopySrc                 = 0x800,
	GenericRead             = (((((0x1 | 0x2) | 0x40) | 0x80) | 0x200) | 0x800),
	Present                 = 0x1000,
	Common                  = 0x2000,
	RayTracingAccel         = 0x4000,
	ShadingRateSrc          = 0x8000,
};
MAKE_ENUM_FLAG(u32, GfxResourceState)

struct GfxBufferDesc
{
    GfxBufferUsageFlags usageFlags;
    GfxMemoryAccess access;
    u64 size;
    GfxResourceHeap heap;
};  

enum class GfxTextureType
{
    Tex2D,
    Tex2DArray,
    Tex3D,
    Cube,
};

enum class GfxSampleCount
{
    Undefined = 0,
    MSAA_1  = 1,
    MSAA_2  = 2,
    MSAA_4  = 4,
    MSAA_8  = 8,
    MSAA_16 = 16,
};

struct GfxColor
{
    union {
        struct 
        {
            f32 r;
            f32 g;
            f32 b;
            f32 a;
        };
        f32 elements[4];
    };

    inline float &operator[](const int &Index)
    {
        return elements[Index];
    }
    inline const float &operator[](const int &Index) const
    {
        return elements[Index];
    }
};

inline GfxColor
gfxColor(const v4& vec)
{
    return GfxColor{ vec[0], vec[1], vec[2], vec[3] };
}

inline GfxColor
gfxColor(f32 r, f32 g, f32 b)
{
    return GfxColor{ r, g, b, 1.0f };
}

inline GfxColor
gfxColor(f32 r, f32 g, f32 b, f32 a)
{
    return GfxColor{ r, g, b, a };
}

inline GfxColor
gfxColor(int r, int g, int b, int a)
{
    return GfxColor{ r/255.0f, g/255.0f, b/255.0f, a/255.0f };
}

inline v4
gfxColorToVec4(const GfxColor& c)
{
    return v4{ c[0], c[1], c[2], c[3] };
}

struct GfxTextureDesc
{
    GfxTextureType type;
    GfxMemoryAccess access;
    u32 width;
    u32 height;
    union {
        u32 size;
        u32 depth;
        u32 slice_count;
    };
    TinyImageFormat format;     // uses tiny image format to easily convert to graphics backend api format
    u32 mipLevels;
    GfxSampleCount sampleCount;
    GfxResourceHeap heap;
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

    GfxResourceHeap     heap;
};

struct GfxShaderDesc
{
    umm size;
    void* data;
    char* szEntryPoint;
};

struct GfxProgramDesc
{
    GfxShaderDesc* compute;
    GfxShaderDesc* vertex;
    union {
        GfxShaderDesc* hull;
        GfxShaderDesc* tessellationControl;
    };
    union {
        GfxShaderDesc* domain;
        GfxShaderDesc* tessellationEvaluation;
    };
    GfxShaderDesc* geometry;
    GfxShaderDesc* fragment;

    GfxResourceHeap heap;
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


enum class GfxColorWriteFlags
{
    Disable     = 0,
    Red         = 1 << 0,
    Green       = 1 << 1,
    Blue        = 1 << 2,
    Alpha       = 1 << 3,
    All         = Red|Green|Blue|Alpha,
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

enum class GfxLoadAction
{
    DontCare,
    Load,
    Clear,
};

struct GfxRenderTargetDesc
{
    GfxTextureType type;
    GfxMemoryAccess access;
    u32 width;
    u32 height;
    union {
        u32 size;
        u32 depth;
        u32 slice_count;
    };
    TinyImageFormat format;     // uses tiny image format to easily convert to graphics backend api format
    u32 mipLevels;
    GfxLoadAction loadOp;
    GfxSampleCount sampleCount;
    union {
        GfxColor clearValue;
        struct {
            f32 depthValue;
            u8 stencilValue;
        };
    };
    
    GfxResourceState initialState;
    GfxResourceHeap heap;
};

struct GfxPipelineDesc
{
    GfxBlendState blendState;
    GfxDepthStencilState depthStencilState;
    GfxRasterizerState rasterizerState;
    GfxPrimitiveTopology primitiveTopology;
    
    u32 numColorTargets;
    TinyImageFormat colorTargets[GFX_MAX_RENDERTARGETS];
    TinyImageFormat depthStencilTarget;

    GfxSampleCount sampleCount;
    b32 supportIndirectCommandBuffer;

    GfxResourceHeap heap;
};

enum class GfxQueueType : u8
{
    Graphics,
    Compute,
    Transfer
};

struct GfxCmdEncoderPoolDesc
{
    GfxQueueType queueType;
};


enum class GfxQueueResourceOp : u8
{
    None,       // indicates that the resource is already part of this queue
    Acquire,    // acquires the resource from another queue
    Release,    // releases the resource to another queue
};

struct GfxBufferBarrier
{
    GfxBuffer           buffer;
	GfxResourceState    currentState;
	GfxResourceState    newState;
    GfxQueueResourceOp  resourceOp;
    GfxQueueType        resourceQueue;
};

struct GfxTextureBarrier
{
    GfxTexture          texture;
	GfxResourceState    currentState;
	GfxResourceState    newState;
	GfxQueueResourceOp  resourceOp;
    GfxQueueType        resourceQueue;
	uint8_t subresourceBarrier : 1;
	// ignored if subresourceBarrier is false
	uint8_t  mipLevel : 7;
	uint16_t layer;
};

struct GfxRenderTargetBarrier
{
    GfxRenderTarget rtv;
	GfxResourceState    currentState;
	GfxResourceState    newState;
	GfxQueueResourceOp  resourceOp;
    GfxQueueType        resourceQueue;
	uint8_t subresourceBarrier : 1;
	// ignored if subresourceBarrier is false
	uint8_t  mipLevel : 7;
	uint16_t layer;
};

enum class GfxDescriptorType
{
    Buffer,
    Image,
    Constant,
};

struct GfxDescriptor
{
    GfxDescriptorType type;
    u16 bindingLocation;
    char* name;

    GfxBuffer buffer;
    
    // u32 numTextures;
    // GfxTexture* pTextures;
    // u32* mipLevels;
    GfxTexture texture;
    GfxSampler sampler;
    u32 mipLevel;

    u32 numBytes;
    void* pBytes;

    // TODO(james): Add expected resource state?
};

struct GfxDescriptorSet
{
    u16 setLocation;
    u32 count;
    GfxDescriptor* pDescriptors;
};

struct gfx_api
{
    GfxDevice device;

    // Resource Management
    // TODO(james): Track resource creation with __FILE__, __LINE__ trick?
    API_FUNCTION(GfxResourceHeap, CreateResourceHeap, GfxDevice device);
    API_FUNCTION(GfxResult, DestroyResourceHeap, GfxDevice device, GfxResourceHeap heap);

    API_FUNCTION(GfxBuffer, CreateBuffer, GfxDevice device, const GfxBufferDesc& bufferDesc, void const* data);
    API_FUNCTION(GfxResult, DestroyBuffer, GfxDevice device, GfxBuffer buffer);
    API_FUNCTION(void*, GetBufferData, GfxDevice device, GfxBuffer buffer);
    
    API_FUNCTION(GfxTexture, CreateTexture, GfxDevice device, const GfxTextureDesc& textureDesc);
    API_FUNCTION(GfxResult, DestroyTexture, GfxDevice device, GfxTexture texture);

    API_FUNCTION(GfxSampler, CreateSampler, GfxDevice device, const GfxSamplerDesc& samplerDesc);
    API_FUNCTION(GfxResult, DestroySampler, GfxDevice device, GfxSampler sampler);

    API_FUNCTION(GfxProgram, CreateProgram, GfxDevice device, const GfxProgramDesc& programDesc);
    API_FUNCTION(GfxResult, DestroyProgram, GfxDevice device, GfxProgram program);
    
    // API_FUNCTION(GfxResult, SetProgramBuffer, GfxDevice device, GfxProgram program, const char* param_name, GfxBuffer buffer);
    // API_FUNCTION(GfxResult, SetProgramTexture, GfxDevice device, GfxProgram program, const char* param_name, GfxTexture texture, u32 mipLevel);
    // API_FUNCTION(GfxResult, SetProgramTextures, GfxDevice device, GfxProgram program, const char* param_name, u32 textureCount, GfxTexture* pTextures, const u32* mipLevels);
    // API_FUNCTION(GfxResult, SetProgramSampler, GfxDevice device, GfxProgram program, const char* param_name, GfxSampler sampler);
    // API_FUNCTION(GfxResult, SetProgramConstants, GfxDevice device, GfxProgram program, const char* param_name, const void* data, u32 size);

    API_FUNCTION(GfxRenderTarget, CreateRenderTarget, GfxDevice device, const GfxRenderTargetDesc& rtvDesc);
    API_FUNCTION(GfxResult, DestroyRenderTarget, GfxDevice device, GfxRenderTarget rtv);
    API_FUNCTION(TinyImageFormat, GetDeviceBackBufferFormat, GfxDevice device);

    API_FUNCTION(GfxKernel, CreateComputeKernel, GfxDevice device, GfxProgram program);
    API_FUNCTION(GfxKernel, CreateGraphicsKernel, GfxDevice device, GfxProgram program, const GfxPipelineDesc& pipelineDesc);
    API_FUNCTION(GfxResult, DestroyKernel, GfxDevice device, GfxKernel kernel);

    // Command Encoding
    API_FUNCTION(GfxCmdEncoderPool, CreateEncoderPool, GfxDevice device, const GfxCmdEncoderPoolDesc& poolDesc);
    API_FUNCTION(GfxResult, DestroyCmdEncoderPool, GfxDevice device, GfxCmdEncoderPool pool);

    API_FUNCTION(GfxCmdContext, CreateEncoderContext, GfxCmdEncoderPool pool);
    // API_FUNCTION(GfxResult, DestroyEncoderContext, GfxCmdEncoderPool pool, GfxCmdContext);
    API_FUNCTION(GfxResult, CreateEncoderContexts, GfxCmdEncoderPool pool, u32 numContexts, GfxCmdContext* pContexts);
    // API_FUNCTION(GfxResult, DestroyEncoderContexts, GfxCmdEncoderPool pool, u32 numContexts, GfxCmdContext* pContexts);

    API_FUNCTION(GfxResult, ResetCmdEncoderPool, GfxCmdEncoderPool pool);
    API_FUNCTION(GfxResult, BeginEncodingCmds, GfxCmdContext cmds);
    API_FUNCTION(GfxResult, EndEncodingCmds, GfxCmdContext cmds);

    API_FUNCTION(GfxResult, CmdResourceBarrier, GfxCmdContext cmds, u32 numBufferBarriers, GfxBufferBarrier* pBufferBarriers, u32 numTextureBarriers, GfxTextureBarrier* pTextureBarriers, u32 numRenderTargetBarriers, GfxRenderTargetBarrier* pRenderTargetBarriers);

    API_FUNCTION(GfxResult, CmdCopyBuffer, GfxCmdContext cmds, GfxBuffer src, GfxBuffer dest);
    API_FUNCTION(GfxResult, CmdCopyBufferRange, GfxCmdContext cmds, GfxBuffer src, u64 srcOffset, GfxBuffer dest, u64 destOffset, u64 size);
    API_FUNCTION(GfxResult, CmdClearBuffer, GfxCmdContext cmds, GfxBuffer buffer, u32 clearValue);
    
    API_FUNCTION(GfxResult, CmdClearTexture, GfxCmdContext cmds, GfxTexture texture, GfxColor color);
    API_FUNCTION(GfxResult, CmdCopyTexture, GfxCmdContext cmds, GfxTexture src, GfxTexture dest);
    API_FUNCTION(GfxResult, CmdClearImage, GfxCmdContext cmds, GfxTexture texture, u32 mipLevel, u32 slice);
    API_FUNCTION(GfxResult, CmdClearBackBuffer, GfxCmdContext cmds, GfxColor color);

    API_FUNCTION(GfxResult, CmdCopyBufferToTexture, GfxCmdContext cmds, GfxBuffer src, u64 srcOffset, GfxTexture dest);
    API_FUNCTION(GfxResult, CmdGenerateMips, GfxCmdContext cmds, GfxTexture texture);

    API_FUNCTION(GfxResult, CmdBindRenderTargets, GfxCmdContext cmds, u32 numRenderTargets, GfxRenderTarget* pColorRTVs, GfxRenderTarget* pDepthStencilRTV);
    API_FUNCTION(GfxResult, CmdBindKernel, GfxCmdContext cmds, GfxKernel kernel);
    API_FUNCTION(GfxResult, CmdBindIndexBuffer, GfxCmdContext cmds, GfxBuffer indexBuffer);
    API_FUNCTION(GfxResult, CmdBindVertexBuffer, GfxCmdContext cmds, GfxBuffer vertexBuffer);
    API_FUNCTION(GfxResult, CmdBindDescriptorSet, GfxCmdContext cmds, const GfxDescriptorSet& descriptorSet);

    API_FUNCTION(GfxResult, CmdSetViewport, GfxCmdContext cmds, f32 x, f32 y, f32 width, f32 height);
    API_FUNCTION(GfxResult, CmdSetScissorRect, GfxCmdContext cmds, i32 x, i32 y, u32 width, u32 height);

    API_FUNCTION(GfxResult, CmdDraw, GfxCmdContext cmds, u32 vertexCount, u32 instanceCount, u32 baseVertex, u32 baseInstance);
    API_FUNCTION(GfxResult, CmdDrawIndexed, GfxCmdContext cmds, u32 indexCount, u32 instanceCount, u32 firstIndex, u32 baseVertex, u32 baseInstance);
    API_FUNCTION(GfxResult, CmdDrawIndirect, GfxCmdContext cmds, GfxBuffer argsBuffer, u32 argsCount);
    API_FUNCTION(GfxResult, CmdDrawIndexedIndirect, GfxCmdContext cmds, GfxBuffer argsBuffer, u32 argsCount);
    API_FUNCTION(GfxResult, CmdDispatch, GfxCmdContext cmds, u32 numGroupsX, u32 numGroupsY, u32 numGroupsZ);
    API_FUNCTION(GfxResult, CmdDispatchIndirect, GfxCmdContext cmds, GfxBuffer argsBuffer);

    // Frame Processing
    // TODO(james): Schedule work on different queues (Transfer, Compute, Graphics, etc..)
    API_FUNCTION(GfxRenderTarget, AcquireNextSwapChainTarget, GfxDevice device);
    API_FUNCTION(GfxResult, SubmitCommands, GfxDevice device, u32 count, GfxCmdContext* pContexts);
    API_FUNCTION(GfxResult, Frame, GfxDevice device, u32 contextCount, GfxCmdContext* pContexts);
    API_FUNCTION(GfxResult, Finish, GfxDevice device);
    API_FUNCTION(GfxResult, CleanupUnusedRenderingResources, GfxDevice device);     // cleans up any transient resources that were allocated during rendering but are no longer needed..  Varies based on backend
    
    // Debug Functions
    API_FUNCTION(GfxTimestampQuery, CreateTimestampQuery, GfxDevice device);
    API_FUNCTION(GfxResult, DestroyTimestampQuery, GfxDevice device, GfxTimestampQuery timestampQuery);
    API_FUNCTION(f32, GetTimestampQueryDuration, GfxDevice device, GfxTimestampQuery timestampQuery);
    API_FUNCTION(GfxResult, BeginTimestampQuery, GfxCmdContext cmds, GfxTimestampQuery query);
    API_FUNCTION(GfxResult, EndTimestampQuery, GfxCmdContext cmds, GfxTimestampQuery query);
    API_FUNCTION(GfxResult, BeginEvent, GfxCmdContext cmds, const char* name);
    API_FUNCTION(GfxResult, BeginColorEvent, GfxCmdContext cmds, const char* name);
    API_FUNCTION(GfxResult, EndEvent, GfxCmdContext cmds);
};