
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

    render_buffer_id    index_id;
    u32 indexCount;
    u32* indices;

    render_buffer_id    vertex_id;
    u32 vertexCount;
    render_mesh_vertex* vertices;

    render_image_id     texture_id;
    render_material_id  material_id;
};

struct material_asset
{
    render_material_id      id;
    render_material_desc    desc;
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
    render_material_id  nextMaterialId;

    shader_asset* simpleVS;
    shader_asset* simpleFS;
    material_asset* basicTextureMaterial;

    image_asset*    vikingTexture;
    model_asset*    vikingModel;

    image_asset*    skullTexture;
    model_asset*    skullModel;
};