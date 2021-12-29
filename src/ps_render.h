
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

// Rendering Objects

typedef u64 render_buffer_id;

struct camera
{
    v3  position;
    v3  target;
};

enum class RenderImageFormat
{
    RGBA
};

struct render_image
{
    u32 width;
    u32 height;
    RenderImageFormat format;
    u32* pixels;
};

struct render_geometry
{
    render_buffer_id indexBuffer;
    render_buffer_id vertexBuffer;  // For now we only need 1
};

// Resource Descriptions

struct PrIndexDesc
{
    u32 count;
    u32* indices;
};

struct PrVertexDesc
{
    // TODO(james): pass vertex description?
    u32 count;
    void* vertexData;
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

struct render_cmd_draw_object
{
    render_cmd_header header;

    m4 model;
    u32 indexCount;
    render_buffer_id indexBuffer;
    render_buffer_id vertexBuffer;
};

