
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

struct Window
{
    ALIGNAS(4) f32 x;
    ALIGNAS(4) f32 y;
    ALIGNAS(4) f32 width;
    ALIGNAS(4) f32 height;
};

struct SceneData
{
    ALIGNAS(16) Window window;
};

struct FrameData
{
    ALIGNAS(4) f32 time;
    ALIGNAS(4) f32 timeDelta;
};

struct CameraData
{
    ALIGNAS(16) v3 pos;
    ALIGNAS(16) m4 view;
    ALIGNAS(16) m4 proj;
    ALIGNAS(16) m4 viewProj;
};

struct LightData
{
    ALIGNAS(16) v3 pos;
    ALIGNAS(16) v3 ambient;
    ALIGNAS(16) v3 diffuse;
    ALIGNAS(16) v3 specular;
};  

struct InstanceData
{
    ALIGNAS(16) m4 mvp;
    ALIGNAS(16) m4 world;
    ALIGNAS(16) m4 worldNormal;
    ALIGNAS(4)  u32 materialIndex; 
};

struct SceneBufferObject
{
    ALIGNAS(16) m4 viewProj;
    ALIGNAS(16) v3 pos;
    ALIGNAS(16) LightData light;
};

struct render_material
{
    ALIGNAS(16) v3 ambient;
    ALIGNAS(16) v3 diffuse;
    ALIGNAS(16) v3 specular;
    ALIGNAS(4) f32 shininess;
};

struct camera
{
    v3  position;
    v3  target;
};

struct render_geometry
{
    u32 indexCount;
    GfxBuffer indexBuffer;
    GfxBuffer vertexBuffer;  // For now we only need 1
};

struct renderable
{
    render_material material;
    render_geometry geometry;
};

// Resource Descriptions

struct render_mesh_vertex
{
    v3 pos;
    v3 normal;
    v3 color;
    v2 texCoord;
    
    bool operator==(const render_mesh_vertex& other) const {
        return pos == other.pos && color == other.color && texCoord == other.texCoord;
    }
};

struct render_context
{
    memory_arena arena;
    memory_arena* frameArena;
    graphics_context* gc;

    GfxCmdEncoderPool cmdpool;
    GfxCmdContext cmds;
    GfxRenderTarget depthTarget;

    u64 stagingPos;
    GfxBuffer stagingBuffer;
    GfxCmdEncoderPool stagingCmdPool;
    GfxCmdContext stagingCmds;
    
    GfxBuffer groundMaterial;
    GfxProgram groundProgram;
    GfxKernel groundKernel;
    GfxTexture texture;
    GfxSampler sampler;

    render_geometry ground;
    
    GfxBuffer meshMaterial;
    GfxProgram meshProgram;
    GfxKernel meshKernel;
    u32 numMeshes;
    render_geometry* meshes;
};
