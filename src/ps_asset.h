

// enum class AssetType
// {
//     Program,
//     Kernel
//     Image,
//     Model_OBJ,
//     Model_GLTF,
//     Material,
// };

// #define ASSET_MAP(XX)                           \
//     XX(AssetType::Program, "shader.vert.spv", shader_vert_spv)      \
//     XX(AssetType::Program, "shader.frag.spv", shader_frag_spv)      \
//     XX(AssetType::Image, "viking_room.png", viking_room_png)      \
//     XX(AssetType::Model_OBJ, "../data/viking_room.obj", viking_room_obj)      \
//     XX(AssetType::Image, "skull.jpg", skull_jpg)                  \
//     XX(AssetType::Model_OBJ, "../data/skull.obj", skull_obj)                  \
//     XX(AssetType::Program, "box.vert.spv", box_vert_spv)            \
//     XX(AssetType::Program, "box.frag.spv", box_frag_spv)            \
//     XX(AssetType::Model_GLTF, "box.glb", box_glb)                      \
//     XX(AssetType::Program, "lightbox.frag.spv", lightbox_frag_spv)  \
//     XX(AssetType::Material, "box.mat", mat_box) \
//     XX(AssetType::Kernel, "pl_basic", pl_basic)   \
//     XX(AssetType::Kernel, "pl_box", pl_box)   \
//     XX(AssetType::Kernel, "pl_lightbox", pl_lightbox)

// enum assetid : u64
// {
// #define XX(type, filename, name)  name = C_HASH64(name),
//     ASSET_MAP(XX)
// #undef XX
// };

// struct asset_desc
// {
//     AssetType type;
//     assetid id;
//     const char* filename;
// };

// #define XX(type, filename, name) {type, name, filename},
// global asset_desc asset_descriptions[] = {
//     ASSET_MAP(XX)
// };
// #undef XX

// // NOTE(james): This function serves 2 purposes.
// //  1. Translates the asset id into it's filename
// //  2. Compile checks for collisions on the asset id name
// const char* getAssetFilename(assetid id)
// {
// #define XX(type, filename, name) case C_HASH64(name): return filename;
//     switch(id)
//     {
//         ASSET_MAP(XX)
//         InvalidDefaultCase;
//     }
// #undef XX

//     ASSERT(false);
//     return "";
//}


// typedef hashtable<shader_asset*> shader_table;
// typedef hashtable<image_asset*> image_table;
// typedef hashtable<model_asset*> model_table;
// typedef hashtable<pipeline_asset*> pipeline_table;
// typedef hashtable<material_asset*> material_table;

struct game_assets
{
    memory_arena memory;
    memory_arena* frameArena;

    // render_resource_queue*  resourceQueue;
    // render_sync_token       lastResourceSyncToken;

    // TODO: Split into multiple regions/rooms to be able to stream assets chunks from disk
    // model_id            nextModelId;
    // render_shader_id    nextShaderId;
    // render_image_id     nextImageId;
    // render_buffer_id    nextBufferId;
    // render_pipeline_id  nextPipelineId;
    // render_material_id  nextMaterialId;

    hashtable<GfxProgram>* mapPrograms;
    hashtable<GfxKernel>* mapKernels;
    hashtable<render_geometry>* mapGeometry;
    //hashtable<render_material>* mapMaterials;


    // shader_table* mapShaders;
    // image_table* mapImages;
    // model_table* mapModels;
    // pipeline_table* mapPipelines;
    // material_table* mapMaterials;

    // u32 numModels;
    // model_asset*    models;
    
};