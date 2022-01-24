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
    Unknown,    // 
    GpuOnly,     // GPU Only
    CpuOnly,         // CPU  -> GPU Transfer
    CpuToGpu,       // CPU -> GPU Transfer, CPU Write-Only -> GPU Fast Read - Uncached & Write-Combined (Not available on all devices) 
    // TODO(james): determine proper fallback when not available, this is the infamous resizeable BAR section
    GpuToCpu,     // GPU -> CPU Readback Transfer
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

enum class GfxShaderStages
{
    Unknown     = 0,
    Vertex      = 0x001,
    Hull        = 0x002,
    Domain      = 0x004,
    Geometry    = 0x008,
    Pixel       = 0x010,
    Compute     = 0x100,
};
MAKE_ENUM_FLAG(u32, GfxShaderStages);

enum class GfxShaderFormat
{
    Unknown =   0x000,
    // binary
    SpirV   =   0x011,
    Dxbc    =   0x012,
    Dxil    =   0x013,
    // source code
    Glsl    =   0x101,   
    Hlsl    =   0x102,
    Metal   =   0x104,
};

enum class GfxShaderSemantic
{
    Undefined   = 0,
    Position,
    Normal,
    Color,
    Tangent,
    BiTangent,
    Joints,
    Weights,
    ShadingRate,
    TexCoord0,
    TexCoord1,
    TexCoord2,
    TexCoord3,
    TexCoord4,
    TexCoord5,
    TexCoord6,
    TexCoord7,
    TexCoord8,
    TexCoord9,
};

struct GfxShaderDesc
{
    GfxShaderDataType type;
    GfxShaderStages stage;
    umm size;
    void* data;
    char* szEntryPoint;
};

struct GfxRootSignature
{
    // TODO(james): declare the descriptor layout here somehow...
    GfxShaderModule* pShaders;
    u32 shaderCount;
};

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
    GfxRenderTargetBlendState renderTargets[8];
};

enum class GfxVertexAttribRate
{
    Vertex,
    Instance,
};

struct GfxVertexAttrib
{
    GfxShaderSemantic semantic;
    u32 semanticNameLength;
    char semanticName[128]; // TODO(james): set size as define/enum/constant
    u32 location;
    u32 binding;
    u32 offset;
    TinyImageFormat format;
    GfxVertexAttribRate rate;
};

struct GfxVertexLayout
{
    u32 attribCount;
    GfxVertexAttrib attribs[15];    // TODO(james): set size as define/enum/constant
};

struct GfxPipelineStateDesc
{
    // and this is the big boy...

    // shader program
    GfxRootSignature rootSignature;
    GfxVertexLayout vertexLayout;
    GfxBlendState blendState;
    GfxDepthStencilState depthStencilState;
    GfxRasterizerState rasterizerState;
    
    u32 renderTargetCount;
    TinyImageFormat colorFormats[8];
    GfxSampleCount sampleCount;
    TinyImageFormat depthStencilFormat;

    GfxPrimitiveTopology primitiveTopology;
    b32 supportIndirectCommandBuffer;
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