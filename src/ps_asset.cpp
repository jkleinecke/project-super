
// TODO(james): Remove this... just for testing
//#define TINYOBJLOADER_IMPLEMENTATION
#include "libs/tinyobjloader/tiny_obj_loader.h"
#include <vector>
#include <algorithm>
#include <unordered_map>
// ----------------------------------------------

internal
inline void ps_hash_combine(size_t &seed, size_t hash)
{
    hash +=  0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= hash;
}

internal
inline size_t ps_hash_v2(v2 const& v)
{
    size_t seed = 0;
    std::hash<f32> hasher;
    ps_hash_combine(seed, hasher(v.X));
    ps_hash_combine(seed, hasher(v.Y));
    return seed;
}

internal
inline size_t ps_hash_v3(v3 const& v)
{
    size_t seed = 0;
    std::hash<f32> hasher;
    ps_hash_combine(seed, hasher(v.X));
    ps_hash_combine(seed, hasher(v.Y));
    ps_hash_combine(seed, hasher(v.Z));
    return seed;
}

namespace std {
    template<> struct hash<render_mesh_vertex> {
        size_t operator()(render_mesh_vertex const& vertex) const {
            // pos ^ color ^ texCoord
            return (ps_hash_v3(vertex.pos) ^ (ps_hash_v3(vertex.color) << 1) >> 1) ^ (ps_hash_v2(vertex.texCoord) << 1);
        }
    };
}

internal image_asset*
LoadImageAsset(game_assets& assets, const char* filename)
{
    image_asset* asset = PushStruct(assets.memory, image_asset);
    asset->id = assets.nextImageId++;

    temporary_memory temp = BeginTemporaryMemory(assets.memory);

    platform_file file = Platform.OpenFile(FileLocation::Content, filename, FileUsage::Read);
    ASSERT(file.size <= (u64)U32MAX);

    void* fileBytes = PushSize(assets.memory, file.size);
    u64 bytesRead = Platform.ReadFile(file, fileBytes, file.size);
    ASSERT(bytesRead == file.size);

    Platform.CloseFile(file);

    int texWidth, texHeight, texChannels;
    stbi_uc *pixels = stbi_load_from_memory((stbi_uc*)fileBytes, (int)file.size, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    ASSERT(pixels);
    ASSERT(texChannels == 3);   // NOTE(james): all we support right now

    EndTemporaryMemory(temp);

    asset->dimensions.Width = (f32)texWidth;
    asset->dimensions.Height = (f32)texHeight;
    asset->format = RenderImageFormat::RGBA_32;
    asset->pixels = PushSize(assets.memory, texWidth*texHeight*4);
    Copy(texWidth*texHeight*4, pixels, asset->pixels);

    stbi_image_free(pixels);

    return asset;
}

internal model_asset*
LoadModelAsset(game_assets& assets, const char* filename)
{
    model_asset* asset = PushStruct(assets.memory, model_asset);
    asset->id = assets.nextModelId++;

    std::vector<render_mesh_vertex> vertices;
    std::vector<u32> indices;

    {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        bool bResult = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename);
        ASSERT(bResult);
        std::unordered_map<render_mesh_vertex, u32> uniqueVertices{};

        for(const auto& shape : shapes)
        {
            for(const auto& index : shape.mesh.indices)
            {
                render_mesh_vertex vertex{};
                vertex.pos = {
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]
                };

                vertex.texCoord = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };

                vertex.color = {1.0f, 1.0f, 1.0f};

                #if 1
                if(uniqueVertices.count(vertex) == 0) {
                    uniqueVertices[vertex] = (u32)vertices.size();
                    vertices.push_back(vertex);
                }

                indices.push_back(uniqueVertices[vertex]);    // TODO(james): fix this awfulness
                #else
                vertices.push_back(vertex);
                indices.push_back((u32)indices.size());
                #endif
            }
        }
    }

    asset->index_id = assets.nextBufferId++;
    asset->indexCount = (u32)indices.size();
    asset->indices = PushArray(assets.memory, asset->indexCount, u32);
    CopyArray(asset->indexCount, indices.data(), asset->indices);

    asset->vertex_id = assets.nextBufferId++;
    asset->vertexCount = (u32)vertices.size();
    asset->vertices = PushArray(assets.memory, asset->vertexCount, render_mesh_vertex);
    CopyArray(asset->vertexCount, vertices.data(), asset->vertices);

    // TODO(james): load material and texture ids?

    return asset;
}

internal shader_asset*
LoadShaderAsset(game_assets& assets, const char* filename)
{
    shader_asset* asset = PushStruct(assets.memory, shader_asset);
    asset->id = assets.nextShaderId++;
    
    platform_file file = Platform.OpenFile(FileLocation::Content, filename, FileUsage::Read);
    ASSERT(file.size <= (u64)U32MAX);

    asset->sizeInBytes = (u32)file.size;
    asset->bytes = PushSize(assets.memory, file.size);
    u64 bytesRead = Platform.ReadFile(file, asset->bytes, file.size);
    ASSERT(bytesRead == file.size);

    Platform.CloseFile(file);

    return asset;
}

internal material_asset*
LoadMaterialAsset(game_assets& assets)
{
    // TODO(james): Actually load a material definition from a file, if it makes sense??
    material_asset* asset = PushStruct(assets.memory, material_asset);
    asset->id = assets.nextMaterialId++;

    render_material_desc& material = asset->desc;
    material.id = asset->id;
    material.shaderCount = 2;
    material.shaders[0] = (render_shader_id)assets.simpleVS->id;
    material.shaders[1] = (render_shader_id)assets.simpleFS->id;
    
    material.samplerCount = 1;
    render_sampler_desc& sampler = material.samplers[0];
    sampler.enableAnisotropy = true;
    sampler.minFilter = SamplerFilter::Linear;
    sampler.magFilter = SamplerFilter::Linear;
    sampler.addressMode_U = SamplerAddressMode::Repeat;
    sampler.addressMode_V = SamplerAddressMode::Repeat;
    sampler.addressMode_W = SamplerAddressMode::Repeat;
    sampler.mipLodBias = 0.0f;
    sampler.mipmapMode = SamplerMipMapMode::Linear;
    sampler.minLod = 0.0f;
    sampler.maxLod = 0.0f;

    return asset;
}

internal void
AddModelToManifest(render_manifest* manifest, model_asset* model)
{
    u32 index = manifest->bufferCount;
    manifest->bufferCount += 2;

    manifest->buffers[index+0].id             = model->index_id;
    manifest->buffers[index+0].type           = RenderBufferType::Index;
    manifest->buffers[index+0].usage          = RenderUsage::Static;
    manifest->buffers[index+0].sizeInBytes    = sizeof(u32) * model->indexCount;
    manifest->buffers[index+0].bytes          = model->indices;

    manifest->buffers[index+1].id             = model->vertex_id;
    manifest->buffers[index+1].type           = RenderBufferType::Vertex;
    manifest->buffers[index+1].usage          = RenderUsage::Static;
    manifest->buffers[index+1].sizeInBytes    = sizeof(render_mesh_vertex) * model->vertexCount;
    manifest->buffers[index+1].bytes          = model->vertices;
}

internal void
AddImageToManifest(render_manifest* manifest, image_asset* image)
{
    u32 index = manifest->imageCount++;

    manifest->images[index].id              = image->id;
    manifest->images[index].usage           = RenderUsage::Static;
    manifest->images[index].format          = image->format;
    manifest->images[index].dimensions      = image->dimensions;
    manifest->images[index].pixels          = image->pixels;
}

internal void
FillOutRenderManifest(game_assets& assets, render_manifest* manifest)
{
    // TODO(james): Build this up dynamically from a streaming world chunk or resource pack

    manifest->id = 1;   

    // shaders
    {
        manifest->shaderCount = 2;

        manifest->shaders[0].id = assets.simpleVS->id;
        manifest->shaders[0].sizeInBytes = assets.simpleVS->sizeInBytes;
        manifest->shaders[0].bytes = assets.simpleVS->bytes;

        manifest->shaders[1].id = assets.simpleFS->id;
        manifest->shaders[1].sizeInBytes = assets.simpleFS->sizeInBytes;
        manifest->shaders[1].bytes = assets.simpleFS->bytes;
    }
    // materials
    {
        manifest->materialCount = 1;

        Copy(sizeof(render_material_desc), &assets.basicTextureMaterial->desc, &manifest->materials[0]);
        manifest->materials[0].id = assets.basicTextureMaterial->id;
    }
    // buffers
    {
        AddModelToManifest(manifest, assets.vikingModel);
        AddModelToManifest(manifest, assets.skullModel);
    }
    // images
    {
        AddImageToManifest(manifest, assets.vikingTexture);
        AddImageToManifest(manifest, assets.skullTexture);
    }
}

internal game_assets*
AllocateGameAssets(game_state& gm_state, render_context& renderer)
{
    game_assets& assets = *BootstrapPushStruct(game_assets, memory, NonRestoredArena());
    
    assets.frameArena = gm_state.frameArena;
    assets.resourceQueue = renderer.resourceQueue;

    // NOTE(james): all ids should start at 1. 0 is reserved as an invalid id
    assets.nextModelId = 1;  
    assets.nextShaderId = 1;
    assets.nextImageId = 1;
    assets.nextBufferId = 1;
    assets.nextMaterialId = 1;

    assets.simpleVS = LoadShaderAsset(assets, "shader.vert.spv");
    assets.simpleFS = LoadShaderAsset(assets, "shader.frag.spv");
    assets.basicTextureMaterial = LoadMaterialAsset(assets);

    assets.vikingTexture = LoadImageAsset(assets, "viking_room.png");
    
    assets.vikingModel = LoadModelAsset(assets, "../data/viking_room.obj");    // TODO(james): data folder reference can be removed once tinyobj loader is gone
    assets.vikingModel->texture_id = assets.vikingTexture->id;
    assets.vikingModel->material_id = assets.basicTextureMaterial->id;

    assets.skullTexture = LoadImageAsset(assets, "skull.jpg");
    assets.skullModel = LoadModelAsset(assets, "../data/skull.obj");
    assets.skullModel->texture_id = assets.skullTexture->id;
    assets.skullModel->material_id = assets.basicTextureMaterial->id;

    // TODO(james): Allow this part to be streamed in as part of a streaming assets system.  For now we'll just make one load at startup time
    render_manifest* manifest = PushStruct(*gm_state.frameArena, render_manifest);
    FillOutRenderManifest(assets, manifest);

    ASSERT(renderer.AddResourceOperation);
    assets.lastResourceSyncToken = renderer.AddResourceOperation(assets.resourceQueue, RenderResourceOpType::Create, manifest);

    // TODO(james): Only works because we're expecting the resource operations to be synchronous right now.  Remove once async loading is implemented
    b32 isComplete = renderer.IsResourceOperationComplete(assets.resourceQueue, assets.lastResourceSyncToken);
    ASSERT(isComplete);

    return &assets;
}