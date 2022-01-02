
// TODO(james): Remove this... just for testing
//#define TINYOBJLOADER_IMPLEMENTATION
#include "libs/tinyobjloader/tiny_obj_loader.h"
#include <vector>
#include <algorithm>
#include <unordered_map>
// ----------------------------------------------

// TODO(james): Just for testing
#define CGLTF_IMPLEMENTATION
#include <cgltf/cgltf.h>
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
    asset->vertexSize = sizeof(render_mesh_vertex);
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

internal material_asset*
LoadBoxMaterial(game_assets& assets)
{
    material_asset* asset = PushStruct(assets.memory, material_asset);
    asset->id = assets.nextMaterialId++;

    render_material_desc& material = asset->desc;
    material.id = asset->id;
    material.shaderCount = 2;
    material.shaders[0] = (render_shader_id)assets.boxVS->id;
    material.shaders[1] = (render_shader_id)assets.boxFS->id;

    return asset;
}

struct vertex
{
    v3 position;
    v3 normal;
};

internal void
LoadCgltfAssets(game_assets& assets, const char* filename)
{
    memory_arena scratch = {};

    platform_file file = Platform.OpenFile(FileLocation::Content, filename, FileUsage::Read);
    void* bytes = PushSize(scratch, file.size); 
    u64 bytesRead = Platform.ReadFile(file, bytes, file.size);
    ASSERT(bytesRead == file.size);

    cgltf_options options = {0};
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse(&options, bytes, file.size, &data);

    if(result == cgltf_result_success)
    {
        // file is parsed, now validate and read the contents
        result = cgltf_validate(data);

        // NOTE(james): This isn't the most efficient way to load these files.  It should
        // be possible to directly load the buffer bytes and then just setup the interpretation
        // properly.  We're not going to do that though because we're going to change the actual
        // data types of some of the properties.  Like indexes from u16->u32...

        if(result == cgltf_result_success)
        {
            // NOTE(james): we only really support the gltf binary files, so load buffers
            // doesn't actually reach back out to the filesystem
            result = cgltf_load_buffers(&options, data, 0);
            ASSERT(result == cgltf_result_success);

            // NOTE(james): We'll treat a single cgltf file as a single manifest...
            //              no matter how many scenes it holds.

            assets.numModels = (u32)data->meshes_count;
            assets.models = PushArray(assets.memory, assets.numModels, model_asset);

            model_asset* model = assets.models;
            FOREACH(mesh, data->meshes, data->meshes_count)
            {
                const char* name = mesh->name;

                FOREACH(primitive, mesh->primitives, mesh->primitives_count)
                {
                    cgltf_accessor* indices = primitive->indices;

                    model->index_id = assets.nextBufferId++;
                    model->indexCount = (u32)indices->count;
                    model->indices = PushArray(assets.memory, model->indexCount, u32);

                    // TODO(james): Change this logic to just setup the buffer views and directly load the buffer bytes

                    cgltf_size i = 0;
                    FOREACH(idx, model->indices, model->indexCount) {
                        *idx = (u32)cgltf_accessor_read_index(indices, i++);
                    }

                    // NOTE(james): For now we're always going to load these as interleaved.
                    // Since they will be interleaved, we need to loop through all the attributes
                    // to figure out how big the vertex will be, and then we can read the data.
                    // TODO(james): Support primitive types besides just a triangle list
                    model->vertex_id = assets.nextBufferId++;
                    model->vertexSize = 0;
                    model->vertexCount = 0;
                    
                    FOREACH(attr, primitive->attributes, primitive->attributes_count)
                    {
                        cgltf_accessor* access = attr->data;
                        model->vertexSize += cgltf_num_components(access->type) * sizeof(f32);
                        ASSERT(!model->vertexCount || model->vertexCount == access->count);   // NOTE(james): verifies that all attributes have the same number of elements
                        model->vertexCount = (u32)access->count;
                    }

                    model->vertices = PushSize(assets.memory, model->vertexSize * model->vertexCount);

                    umm offset = 0;
                    FOREACH(attr, primitive->attributes, primitive->attributes_count)
                    {
                        cgltf_accessor* access = attr->data;
                        umm numComponents = cgltf_num_components(access->type);

                        void* curVertex = OffsetPtr(model->vertices, offset);
                        for(umm index = 0; index < model->vertexCount; ++index)
                        {
                            cgltf_accessor_read_float(access, index, (cgltf_float*)curVertex, numComponents);
                            curVertex = OffsetPtr(curVertex, model->vertexSize);
                        }
                        offset += numComponents * sizeof(f32);
                        //cgltf_accessor_unpack_floats(attr->data, (cgltf_float*)model.vertices, model.vertexCount * cgltf_num_components(attr->data->type));
                    }

                    model->material_id = assets.boxMaterial->id;
                    model->texture_id = 0;
                }

                ++model;
            }
        }
    }

    if(data)
    {
        cgltf_free(data);
    }

    Platform.CloseFile(file);

    Clear(scratch);
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
    manifest->buffers[index+1].sizeInBytes    = (u32)(model->vertexSize * model->vertexCount);
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
        manifest->shaderCount = 4;

        manifest->shaders[0].id = assets.simpleVS->id;
        manifest->shaders[0].sizeInBytes = assets.simpleVS->sizeInBytes;
        manifest->shaders[0].bytes = assets.simpleVS->bytes;

        manifest->shaders[1].id = assets.simpleFS->id;
        manifest->shaders[1].sizeInBytes = assets.simpleFS->sizeInBytes;
        manifest->shaders[1].bytes = assets.simpleFS->bytes;

        manifest->shaders[2].id = assets.boxVS->id;
        manifest->shaders[2].sizeInBytes = assets.boxVS->sizeInBytes;
        manifest->shaders[2].bytes = assets.boxVS->bytes;

        manifest->shaders[3].id = assets.boxFS->id;
        manifest->shaders[3].sizeInBytes = assets.boxFS->sizeInBytes;
        manifest->shaders[3].bytes = assets.boxFS->bytes;
    }
    // materials
    {
        manifest->materialCount = 2;

        manifest->materials[0] = assets.basicTextureMaterial->desc;
        manifest->materials[0].id = assets.basicTextureMaterial->id;
        
        manifest->materials[1] = assets.boxMaterial->desc;
        manifest->materials[1].id = assets.boxMaterial->id;
    }
    // buffers
    {
        AddModelToManifest(manifest, assets.vikingModel);
        AddModelToManifest(manifest, assets.skullModel);

        FOREACH(model, assets.models, assets.numModels)
        {
            AddModelToManifest(manifest, model);
        }
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
    game_assets& assets = *BootstrapPushStructMember(game_assets, memory, NonRestoredArena());
    
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

    assets.boxVS = LoadShaderAsset(assets, "box.vert.spv");
    assets.boxFS = LoadShaderAsset(assets, "box.frag.spv");
    assets.boxMaterial = LoadBoxMaterial(assets);
    LoadCgltfAssets(assets, "box.glb");

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