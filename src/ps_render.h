
/*******************************************************************************

    2 level renderer, high-level and low-level

    High-Level

        Will determine which parts of each part of the scene to draw:
            - Scene background
            - NPCs
            - Player
            - HUD
            - etc..

        These objects are at a higher conceptual level than the actual rendering
        techniques required.  It may be possible to only roughly determine this
        part and push the low-level part off to the GPU entirely?

    Low-Level

        Record 1 or more rendering passes.  Each rendering pass specifies the render
        targets, Screen or material images, and records draw commands with material
        information.

        Passes are constructed into a rendering graph where each pass in a node
        in the graph. The render driver will walk backwards from the screen target
        and collect all the rendering passes required.  It will then execute the
        passes starting at the leaf nodes, hopefully with an optimal sort order,
        and preserve the order of the rendering passes.

    Resource Management

        Resources will largely be managed in groups.  Since the game will be
        well partitioned, all resources for a single partition will be
        allocated at once. Static resources can stream their data to the GPU
        while dynamic resources will upload during the execution of the
        render graph

        Shaders???

********************************************************************************/

typedef u32 render_manifest_id;
typedef u32 render_material_id;
typedef u32 render_buffer_id;
typedef u32 render_image_id;
typedef u32 render_shader_id;

// Rendering Objects

struct camera
{
    v3  position;
    v3  target;
};

struct render_geometry
{
    u32 indexCount;
    render_buffer_id indexBuffer;
    render_buffer_id vertexBuffer;  // For now we only need 1
};

// Resource Descriptions

struct render_mesh_vertex
{
    v3 pos;
    v3 color;
    v2 texCoord;
    
    bool operator==(const render_mesh_vertex& other) const {
        return pos == other.pos && color == other.color && texCoord == other.texCoord;
    }
};

struct render_shader_desc
{
    render_shader_id    id;
    
    u32                 sizeInBytes;
    void*               bytes;
};

enum class SamplerAddressMode
{
    Repeat              = 0,    // VK_SAMPLER_ADDRESS_MODE_REPEAT
    MirrorRepeat        = 1,    // VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT
    ClampEdge           = 2,    // VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
    ClampBorder         = 3,    // VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER
    MirrorClampEdge     = 4     // VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE
};

enum class SamplerMipMapMode
{
    Nearest             = 0,    // VK_SAMPLER_MIPMAP_MODE_NEAREST
    Linear              = 1     // VK_SAMPLER_MIPMAP_MODE_LINEAR
};

enum class SamplerFilter
{
    Nearest             = 0,            // VK_FILTER_NEAREST
    Linear              = 1,            // VK_FILTER_LINEAR
    Cubic               = 1000015000,   // VK_FILTER_CUBIC_IMG
};

struct render_sampler_desc
{
    b32                 enableAnisotropy;
    b32                 coordinatesNotNormalized;

    SamplerFilter       minFilter;
    SamplerFilter       magFilter;

    SamplerAddressMode  addressMode_U;
    SamplerAddressMode  addressMode_V;
    SamplerAddressMode  addressMode_W;

    f32                 mipLodBias;
    SamplerMipMapMode   mipmapMode;
    f32                 minLod;
    f32                 maxLod;
};

struct render_material_desc
{
    render_material_id id;

    u32 shaderCount;
    render_shader_id shaders[10]; // TODO(james): Tune this
    // render targets
    // blend states
    u32 samplerCount;
    render_sampler_desc samplers[16]; // TODO(james): Tune this
    // etc...
};

enum class RenderBufferType
{
    Vertex,
    Index,
    Uniform
};

enum class RenderUsage
{
    Static,     // Utilize staging buffer to upload from CPU prior to GPU usage
    Dynamic     // No staging buffer during upload, but slower GPU access
};


struct render_buffer_view
{
    umm offset;
    umm count;
    RenderBufferType type;
};

struct render_buffer_desc
{
    render_buffer_id        id;

    RenderBufferType        type;
    RenderUsage             usage;
    u32                     sizeInBytes;
    void*                   bytes;
};

enum class RenderImageFormat
{
    RGBA_32
};

// TODO(james): support 1D & 3D images too...

// NOTE(james): Only intended to support 2D images
struct render_image_desc
{
    render_image_id     id;

    RenderUsage         usage;
    RenderImageFormat   format;
    v2                  dimensions;
    void*               pixels;

    // TODO(james): add support for mip levels here too
};

struct render_manifest
{
    render_manifest_id id;

    u32 shaderCount;
    render_shader_desc shaders[255];        // TODO(james): Tune this
    u32 materialCount;
    render_material_desc materials[255];    // TODO(james): Tune this
    u32 bufferCount;
    render_buffer_desc buffers[255];        // TODO(james): Tune this
    u32 imageCount;
    render_image_desc images[255];          // TODO(james): Tune this
};

// Rendering Commands

enum class RenderCommandType
{
    Unknown,
    UpdateViewProjection,
    DrawObject,
    Done
};

struct render_cmd_header
{
    RenderCommandType   type;
    memory_index        size;
};

struct render_cmd_update_viewprojection
{
    render_cmd_header header;
    
    m4 view;
    m4 projection;
};

enum class RenderMaterialBindingType
{
    Buffer,
    Image
};

struct render_material_binding
{
    RenderMaterialBindingType type;
    u32 layoutIndex;
    u32 bindingIndex;

    union
    {
        // Valid for the buffer binding type
        struct
        {
            render_buffer_id buffer_id;
            u32              buffer_offset;
            u32              buffer_range;
        };

        // valid for the image binding type
        struct
        {
            render_image_id  image_id;
            u32              image_sampler_index;
        };
    };
};

struct render_cmd_draw_object
{
    render_cmd_header header;

    m4 mvp;
    u32 indexCount;
    render_buffer_id indexBuffer;
    render_buffer_id vertexBuffer;
    render_material_id material_id;
    u32 materialBindingCount;
    render_material_binding materialBindings[20];   // TODO(james): tune this
};

