
typedef u64 model_id;

struct shader_asset
{
    render_shader_id    id;
    
    u32                 sizeInBytes;
    void*               bytes;
};

struct image_asset
{
    render_image_id     id;

    RenderImageFormat   format;
    v2                  dimensions;
    void*               pixels;

    // TODO(james): account for mip levels
};


struct model_asset
{
    model_id id;

    u32 bufferCount;
    buffer* buffers;

    render_buffer_id    index_id;
    u32 indexCount;
    u32* indices;

    render_buffer_id    vertex_id;
    u32 vertexCount;
    umm vertexSize;
    void* vertices;

    render_image_id     texture_id; // TODO(james): Move this to an actual material
};

struct pipeline_asset
{
    render_pipeline_id      id;
    render_pipeline_desc    desc;
};

struct material_asset   
{
    render_material_id      id;

    v3                      ambient;
    v3                      diffuse;
    v3                      specular;
    float                   shininess;   
};

struct game_assets
{
    memory_arena memory;
    memory_arena* frameArena;

    render_resource_queue*  resourceQueue;
    render_sync_token       lastResourceSyncToken;

    // TODO: Split into multiple regions/rooms to be able to stream assets chunks from disk
    model_id            nextModelId;
    render_shader_id    nextShaderId;
    render_image_id     nextImageId;
    render_buffer_id    nextBufferId;
    render_pipeline_id  nextPipelineId;
    render_material_id  nextMaterialId;

    shader_asset* simpleVS;
    shader_asset* simpleFS;
    pipeline_asset* basicTexturePipeline;

    image_asset*    vikingTexture;
    model_asset*    vikingModel;

    image_asset*    skullTexture;
    model_asset*    skullModel;

    shader_asset*   boxVS;
    shader_asset*   boxFS;
    pipeline_asset* boxPipeline;

    material_asset* boxMaterial;

    shader_asset*   lightboxFS;
    pipeline_asset* lightboxPipeline;

    u32 numModels;
    model_asset*    models;
};