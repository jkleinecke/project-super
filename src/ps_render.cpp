// TODO(james): Just for testing
#define CGLTF_IMPLEMENTATION
#include <cgltf/cgltf.h>

/*
    RenderScene - API psuedocode

    RenderBackgroundPass(scene)
    {
        cmds

        SetupRenderPass(cmds, scene->backgroundRenderpass)
        SetPerRenderPassVariables(cmds)  

        for tile in scene->tiles
            SetPerDrawVariables(cmds)
            Draw(cmds, tile)

        return cmds
    }

    RenderCharacters(scene)
    {
        cmds

        SetupRenderPass(cmds, scene->characterRenderpass)
        SetPerRenderPassVariables(cmds)

        for character in scene->characters
            SetPerDrawVariables(cmds)
            Draw(cmds, character)

        SetPerDrawVariables(cmds)
        Draw(cmds, player)

        return cmds
    }

    RenderScene(scene)
    {
        SetPerFrameVariables(renderDevice, windowSize, timeDelta)

        lights = FindVisibleLights(scene)
        SetPerSceneVariables(renderDevice, camera, lights)

        // really I want to render multiple passes of the same information
        //  Shadowmap pass
        //  Material pass
  
        backgroundCmds = RenderBackgroundPass(scene)
        characterCmds = RenderCharacters(scene)

        RenderPostProcessPasses(renderDevice)

        RenderUI(renderDevice)
    }
*/

gfx_api gfx;

internal b32
LoadShaderBlob(memory_arena& memory, const char* filename, GfxShaderDesc* pShaderDesc)
{
    platform_file file = Platform.OpenFile(FileLocation::Content, filename, FileUsage::Read);
    ASSERT(file.size <= (u64)U32MAX);
    if(file.error) return false;

    pShaderDesc->size = file.size;
    pShaderDesc->data = PushSize(memory, file.size);
    u64 bytesRead = Platform.ReadFile(file, pShaderDesc->data, file.size);
    ASSERT(bytesRead == file.size);

    Platform.CloseFile(file);

    return true;
}

internal GfxProgram
LoadProgram(memory_arena& memory, const char* vert_file, const char* frag_file)
{
    GfxShaderDesc vertex = {};
    GfxShaderDesc fragment = {};
    LoadShaderBlob(memory, vert_file, &vertex);
    LoadShaderBlob(memory, frag_file, &fragment);
    
    GfxProgramDesc programDesc = {};
    programDesc.vertex = &vertex;
    programDesc.fragment = &fragment;
    GfxProgram program = gfx.CreateProgram(gfx.device, programDesc);
    GFX_ASSERT_VALID(program);
    
    return program;
}

internal GfxRenderTargetBlendState
OpaqueRenderTarget()
{
    GfxRenderTargetBlendState bs = {};
    bs.blendEnable = false;
    bs.colorWriteMask = GfxColorWriteFlags::All;
    return bs;
}

internal GfxRenderTargetBlendState
TransparentRenderTarget()
{
    GfxRenderTargetBlendState bs = {};
    bs.blendEnable = true;
    bs.srcBlend = GfxBlendMode::SrcAlpha;
    bs.destBlend = GfxBlendMode::InvSrcAlpha;
    bs.blendOp = GfxBlendOp::Add;
    bs.srcAlphaBlend = GfxBlendMode::One;
    bs.destAlphaBlend = GfxBlendMode::Zero;
    bs.blendOpAlpha = GfxBlendOp::Add;
    bs.colorWriteMask = GfxColorWriteFlags::All;
    return bs;
}

internal GfxRenderTargetDesc
DepthRenderTarget(u32 width, u32 height)
{
    GfxRenderTargetDesc rtv = {};
    rtv.type = GfxTextureType::Tex2D;
    rtv.access = GfxMemoryAccess::GpuOnly;
    rtv.width = width;
    rtv.height = height;
    rtv.format = TinyImageFormat_D32_SFLOAT;
    rtv.mipLevels = 1;
    rtv.loadOp = GfxLoadAction::Clear;
    rtv.depthValue = 1.0f;
    rtv.initialState = GfxResourceState::DepthWrite;

    return rtv;
}

internal GfxPipelineDesc
DefaultPipeline(b32 depthEnable) 
{
    GfxBlendState bs = {};
    bs.renderTargets[0] = TransparentRenderTarget();

    GfxDepthStencilState ds = {};
    ds.depthEnable = depthEnable;
    ds.depthWriteMask = GfxDepthWriteMask::All;
    ds.depthFunc = GfxComparisonFunc::LessEqual;

    GfxRasterizerState rs = {};
    rs.fillMode = GfxFillMode::Solid;
    rs.cullMode = GfxCullMode::Back;
    rs.frontCCW = true;

    GfxPipelineDesc desc = {};
    desc.blendState = bs;
    desc.depthStencilState = ds;
    desc.rasterizerState = rs;
    desc.primitiveTopology = GfxPrimitiveTopology::TriangleList;
    desc.numColorTargets = 1;
    desc.colorTargets[0] = gfx.GetDeviceBackBufferFormat(gfx.device);
    desc.depthStencilTarget = TinyImageFormat_D32_SFLOAT;
    return desc;
}

internal GfxBufferDesc
StagingBuffer(u64 size)
{
    GfxBufferDesc desc = {};
    desc.usageFlags = GfxBufferUsageFlags::Uniform;
    desc.access = GfxMemoryAccess::CpuOnly;
    desc.size = size;

    return desc;
}

internal GfxBufferDesc
MeshVertexBuffer(u32 count)
{
    GfxBufferDesc desc = {};
    desc.usageFlags = GfxBufferUsageFlags::Vertex;
    desc.access = GfxMemoryAccess::GpuOnly; 
    desc.size = count * sizeof(render_mesh_vertex);

    return desc;
}

internal GfxBufferDesc
VertexBuffer(u32 count, u32 vertexSize)
{
    GfxBufferDesc desc = {};
    desc.usageFlags = GfxBufferUsageFlags::Vertex;
    desc.access = GfxMemoryAccess::GpuOnly;
    desc.size = count * vertexSize;

    return desc;
}

internal GfxBufferDesc
IndexBuffer(u32 count)
{
    GfxBufferDesc desc = {};
    desc.usageFlags = GfxBufferUsageFlags::Index;
    desc.access = GfxMemoryAccess::GpuOnly; 
    desc.size = count * sizeof(u32);

    return desc;
}

internal GfxBufferDesc
UniformBuffer(u64 size, GfxMemoryAccess access = GfxMemoryAccess::GpuOnly)
{
    GfxBufferDesc desc = {};
    desc.usageFlags = GfxBufferUsageFlags::Uniform;
    desc.access = access; 
    desc.size = size;

    return desc;
}

internal GfxSamplerDesc
Sampler()
{
    GfxSamplerDesc desc = {};
    desc.enableAnisotropy = false;
    desc.coordinatesNotNormalized = false;
    desc.minFilter = GfxSamplerFilter::Linear;
    desc.magFilter = GfxSamplerFilter::Linear;
    desc.addressMode_U = GfxSamplerAddressMode::Repeat;
    desc.addressMode_V = GfxSamplerAddressMode::Repeat;
    desc.addressMode_W = GfxSamplerAddressMode::Repeat;
    desc.mipmapMode = GfxSamplerMipMapMode::Nearest;

    return desc;
}

inline GfxDescriptor
BufferDescriptor(u16 bindingLocation, GfxBuffer buffer, u32 offset = 0)
{
    GfxDescriptor desc{};
    desc.type = GfxDescriptorType::Buffer;
    desc.bindingLocation = bindingLocation;
    desc.buffer = buffer;
    desc.offset = offset;
    return desc;
}

inline GfxDescriptor
NamedBufferDescriptor(const char* name, GfxBuffer buffer, u32 offset = 0)
{
    GfxDescriptor desc{};
    desc.type = GfxDescriptorType::Buffer;
    desc.name = (char*)name;
    desc.buffer = buffer;
    desc.offset = offset;
    return desc;
}

internal GfxDescriptor
TextureDescriptor(u16 bindingLocation, GfxTexture texture, GfxSampler sampler)
{
    GfxDescriptor desc{};
    desc.type = GfxDescriptorType::Image;
    desc.bindingLocation = bindingLocation;
    desc.texture = texture;
    desc.sampler = sampler;
    return desc;
}

internal GfxDescriptor
NamedTextureDescriptor(const char* name, GfxTexture texture, GfxSampler sampler)
{
    GfxDescriptor desc{};
    desc.type = GfxDescriptorType::Image;
    desc.name = (char*)name;
    desc.texture = texture;
    desc.sampler = sampler;
    return desc;
}

#define STAGING_BUFFER_SIZE Megabytes(64)

internal void
BeginStagingData(render_context& rc)
{
    rc.stagingPos = 0;
    gfx.ResetCmdEncoderPool(rc.stagingCmdPool);
    gfx.BeginEncodingCmds(rc.stagingCmds);
}

internal void
StageBufferData(render_context& rc, u64 size, const void* data, GfxBuffer buffer)
{
    while(rc.stagingPos + size > STAGING_BUFFER_SIZE)
    {
        // split the data up into multiple copies
        NotImplemented;
    }

    void* stagingData = gfx.GetBufferData(gfx.device, rc.stagingBuffer);
    Copy(size, data, OffsetPtr(stagingData, rc.stagingPos));
    gfx.CmdCopyBufferRange(rc.stagingCmds, rc.stagingBuffer, rc.stagingPos, buffer, 0, size);

    rc.stagingPos += size;
}

internal void
StageTextureData(render_context& rc, u64 size, const void* data, GfxTexture texture)
{
    GfxTextureBarrier texBarrier = { rc.texture, GfxResourceState::Undefined, GfxResourceState::CopyDst };
    gfx.CmdResourceBarrier(rc.stagingCmds, 0, 0, 1, &texBarrier, 0, 0);

    while(rc.stagingPos + size > STAGING_BUFFER_SIZE)
    {
        // split the data up into multiple copies
        NotImplemented;
    }

    void* stagingData = gfx.GetBufferData(gfx.device, rc.stagingBuffer);
    Copy(size, data, OffsetPtr(stagingData, rc.stagingPos));

    gfx.CmdCopyBufferToTexture(rc.stagingCmds, rc.stagingBuffer, rc.stagingPos, rc.texture);
 
    texBarrier = { rc.texture, GfxResourceState::CopyDst, GfxResourceState::PixelShaderResource };
    gfx.CmdResourceBarrier(rc.stagingCmds, 0, 0, 1, &texBarrier, 0, 0);

    rc.stagingPos += size;
}

internal void
EndStagingData(render_context& rc)
{
    gfx.EndEncodingCmds(rc.stagingCmds);
    gfx.SubmitCommands(gfx.device, 1, &rc.stagingCmds);

    // TODO(james): wait on a sync primitive instead of forcing the GPU to go idle...
    gfx.Finish(gfx.device);
    rc.stagingPos = 0;
}

internal void
TempLoadImagePixels(memory_arena& arena, const char* filename, u32* width, u32* height, u32* channels, void*& pixeldata)
{
    temporary_memory temp = BeginTemporaryMemory(arena);

    platform_file file = Platform.OpenFile(FileLocation::Content, filename, FileUsage::Read);
    ASSERT(file.size <= (u64)U32MAX);

    void* fileBytes = PushSize(arena, file.size);
    u64 bytesRead = Platform.ReadFile(file, fileBytes, file.size);
    ASSERT(bytesRead == file.size);

    Platform.CloseFile(file);

    int texWidth, texHeight, texChannels;
    stbi_uc *pixels = stbi_load_from_memory((stbi_uc*)fileBytes, (int)file.size, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    ASSERT(pixels);
    ASSERT(texChannels == 3);   // NOTE(james): all we support right now

    EndTemporaryMemory(temp);

    if(width) *width = (u32)texWidth;
    if(height) *height = (u32)texHeight;
    if(channels) *channels = (u32)texChannels;
    pixeldata = PushSize(arena, texWidth*texHeight*4);
    Copy(texWidth*texHeight*4, pixels, pixeldata);

    stbi_image_free(pixels);
}

internal void
TempLoadGltfGeometry(render_context& rc, const char* filename, u32* pNumMeshes, render_geometry** pOutMeshes)
{
    ASSERT(pNumMeshes);
    ASSERT(pOutMeshes);

    platform_file file = Platform.OpenFile(FileLocation::Content, filename, FileUsage::Read);
    void* bytes = PushSize(*rc.frameArena, file.size); 
    u64 bytesRead = Platform.ReadFile(file, bytes, file.size);
    ASSERT(bytesRead == file.size);
    
    cgltf_options options = {};
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

            *pNumMeshes = (u32)data->meshes_count;
            *pOutMeshes = PushArray(rc.arena, data->meshes_count, render_geometry);

            render_geometry* loadedMesh = *pOutMeshes;
            FOREACH(mesh, data->meshes, data->meshes_count)
            {
                const char* name = mesh->name;

                FOREACH(primitive, mesh->primitives, mesh->primitives_count)
                {
                    cgltf_accessor* indices = primitive->indices;

                    loadedMesh->indexCount = (u32)indices->count;
                    loadedMesh->indexBuffer = gfx.CreateBuffer(gfx.device, IndexBuffer((u32)indices->count), 0);
                    
                    u32* meshIndices = PushArray(*rc.frameArena, loadedMesh->indexCount, u32);

                    // TODO(james): Change this logic to just setup the buffer views and directly load the buffer bytes
                    for(u32 idx = 0; idx < loadedMesh->indexCount; ++idx) {
                        meshIndices[idx] = (u32)cgltf_accessor_read_index(indices, idx);
                    }
                    StageBufferData(rc, sizeof(u32)*loadedMesh->indexCount, meshIndices, loadedMesh->indexBuffer);

                    // NOTE(james): For now we're always going to load these as interleaved.
                    // Since they will be interleaved, we need to loop through all the attributes
                    // to figure out how big the vertex will be, and then we can read the data.
                    // TODO(james): Support primitive types besides just a triangle list
                    
                    u32 vertexSize = 0;
                    u32 vertexCount = 0;

                    FOREACH(attr, primitive->attributes, primitive->attributes_count)
                    {
                        cgltf_accessor* access = attr->data;
                        vertexSize += (u32)cgltf_num_components(access->type) * sizeof(f32);
                        ASSERT(!vertexCount || vertexCount == access->count);   // NOTE(james): verifies that all attributes have the same number of elements
                        vertexCount = (u32)access->count;
                    }

                    gltf_vertex* meshVertices = PushArray(*rc.frameArena, vertexCount, gltf_vertex);
                    loadedMesh->vertexBuffer = gfx.CreateBuffer(gfx.device, VertexBuffer(vertexCount, vertexSize), 0);

                    umm offset = 0;
                    FOREACH(attr, primitive->attributes, primitive->attributes_count)
                    {
                        cgltf_accessor* access = attr->data;
                        umm numComponents = cgltf_num_components(access->type);

                        void* curVertex = OffsetPtr(meshVertices, offset);
                        for(umm index = 0; index < vertexCount; ++index)
                        {
                            cgltf_accessor_read_float(access, index, (cgltf_float*)curVertex, numComponents);
                            curVertex = OffsetPtr(curVertex, vertexSize);
                        }
                        offset += numComponents * sizeof(f32);
                        //cgltf_accessor_unpack_floats(attr->data, (cgltf_float*)model.vertices, model.vertexCount * cgltf_num_components(attr->data->type));
                    }
                    StageBufferData(rc, vertexSize*vertexCount, meshVertices, loadedMesh->vertexBuffer);
                }

                ++loadedMesh;
            }
        }
    }

    if(data)
    {
        cgltf_free(data);
    }

    Platform.CloseFile(file);   
}

#if 0
// doesn't work
inline v3 computeVertexNormal(v3 pos)
{
    float scale = 1.0f / LengthSquaredVec3(pos);
    return pos * scale;
}

inline v3 computeHalfVertex(v3& a, v3& b, f32 length)
{
    v3 r = Vec3(a.X+b.X,a.Y+b.Y,a.Z+b.Z);
    float scale = length / LengthSquaredVec3(r);
    return r * scale;
}

inline v2 computeHalfTexCoord(v2& a, v2& b)
{
    return (a + b) * 0.5f;
}

internal render_geometry
CreateIcosphere(render_context& rc, f32 radius, u32 subdivisions)
{
    // First build an icosahedron
    v3 icosahedronVertices[12] = {};

    {
        const f32 H_ANGLE = Pi32 / 180.0f * 72.0f;
        const f32 V_ANGLE = atanf(1.0f / 2.0f);
        int i1, i2;
        f32 z, xy;
        f32 hAngle1 = -Pi32 / 2.0f - H_ANGLE / 2.0f;
        f32 hAngle2 = -Pi32 / 2.0f;

        // top vertex
        icosahedronVertices[0] = Vec3(0.0f, 0.0f, radius);

        // 10 vertices at 2nd and 3rd rows
        for(int i = 1; i <= 5; ++i)
        {
            i1 = i;
            i2 = (i + 5);

            z = radius * sinf(V_ANGLE);
            xy = radius * cosf(V_ANGLE);

            icosahedronVertices[i1] = Vec3(xy * cosf(hAngle1), xy * sinf(hAngle1), z);
            icosahedronVertices[i2] = Vec3(xy * cosf(hAngle2), xy * sinf(hAngle2), -z);

            hAngle1 += H_ANGLE;
            hAngle2 += H_ANGLE;
        }

        // bottom vertex
        icosahedronVertices[11] = Vec3(0.0f, 0.0f, -radius);
    }

    u32 vertexCount = 22;
    {
        // pre-compute the number of vertices based on the number of requested subdivisions
        u32 triCount = 20;
        for(u32 i = 0; i < subdivisions; ++i)
        {
            vertexCount += triCount * 3;
            triCount = triCount * 4;
        }
    }

    // const float S_STEP = 186 / 2048.0f;     // horizontal texture step
    // const float T_STEP = 322 / 1024.0f;     // vertical texture step

    // smooth icosahedron has 14 non-shared (0 to 13) and
    // 8 shared vertices (14 to 21) (total 22 vertices)
    //  00  01  02  03  04          //
    //  /\  /\  /\  /\  /\          //
    // /  \/  \/  \/  \/  \         //
    //10--14--15--16--17--11        //
    // \  /\  /\  /\  /\  /\        //
    //  \/  \/  \/  \/  \/  \       //
    //  12--18--19--20--21--13      //
    //   \  /\  /\  /\  /\  /       //
    //    \/  \/  \/  \/  \/        //
    //    05  06  07  08  09        //
    // add 14 non-shared vertices first (index from 0 to 13)
    array<gltf_vertex>& vertices = *array_create(*rc.frameArena, gltf_vertex, vertexCount);
    vertices._size = 22;

    vertices[0].pos = icosahedronVertices[0];
    vertices[0].normal = Vec3(0.0f, 0.0f, 1.0f);
    // vertices[0].texCoord = Vec2(S_STEP, 0);
    
    vertices[1].pos = icosahedronVertices[0];
    vertices[1].normal = Vec3(0.0f, 0.0f, 1.0f);
    // vertices[1].texCoord = Vec2(S_STEP * 3, 0);
    
    vertices[2].pos = icosahedronVertices[0];
    vertices[2].normal = Vec3(0.0f, 0.0f, 1.0f);
    // vertices[2].texCoord = Vec2(S_STEP * 5, 0);
    
    vertices[3].pos = icosahedronVertices[0];
    vertices[3].normal = Vec3(0.0f, 0.0f, 1.0f);
    // vertices[3].texCoord = Vec2(S_STEP * 5, 0);
    
    vertices[4].pos = icosahedronVertices[0];
    vertices[4].normal = Vec3(0.0f, 0.0f, 1.0f);
    // vertices[4].texCoord = Vec2(S_STEP * 9, 0);
  
    vertices[5].pos = icosahedronVertices[11];
    vertices[5].normal = Vec3(0.0f, 0.0f, -1.0f);
    // vertices[5].texCoord = Vec2(S_STEP * 2, T_STEP * 3);
    
    vertices[6].pos = icosahedronVertices[11];
    vertices[6].normal = Vec3(0.0f, 0.0f, -1.0f);
    // vertices[6].texCoord = Vec2(S_STEP * 4, T_STEP * 3);
    
    vertices[7].pos = icosahedronVertices[11];
    vertices[7].normal = Vec3(0.0f, 0.0f, -1.0f);
    // vertices[7].texCoord = Vec2(S_STEP * 6, T_STEP * 3);
    
    vertices[8].pos = icosahedronVertices[11];
    vertices[8].normal = Vec3(0.0f, 0.0f, -1.0f);
    // vertices[8].texCoord = Vec2(S_STEP * 8, T_STEP * 3);
    
    vertices[9].pos = icosahedronVertices[11];
    vertices[9].normal = Vec3(0.0f, 0.0f, -1.0f);
    // vertices[9].texCoord = Vec2(S_STEP * 10, T_STEP * 3);

    vertices[10].pos = icosahedronVertices[1];
    vertices[10].normal = computeVertexNormal(icosahedronVertices[1]);
    // vertices[10].texCoord = Vec2(0, T_STEP);
    
    vertices[11].pos = icosahedronVertices[1];
    vertices[11].normal = vertices[10].normal;
    // vertices[11].texCoord = Vec2(S_STEP * 10, T_STEP);

    vertices[12].pos = icosahedronVertices[6];
    vertices[12].normal = computeVertexNormal(icosahedronVertices[6]);
    // vertices[12].texCoord = Vec2(S_STEP, T_STEP * 2);
    
    vertices[13].pos = icosahedronVertices[6];
    vertices[13].normal = vertices[12].normal;
    // vertices[13].texCoord = Vec2(S_STEP * 11, T_STEP * 2);
    
    // setup 8 shared vertices
    vertices[14].pos = icosahedronVertices[2];
    vertices[14].normal = computeVertexNormal(icosahedronVertices[2]);
    // vertices[14].texCoord = Vec2(S_STEP * 2, T_STEP);

    vertices[15].pos = icosahedronVertices[3];
    vertices[15].normal = computeVertexNormal(icosahedronVertices[3]);
    // vertices[15].texCoord = Vec2(S_STEP * 4, T_STEP);

    vertices[16].pos = icosahedronVertices[4];
    vertices[16].normal = computeVertexNormal(icosahedronVertices[4]);
    // vertices[16].texCoord = Vec2(S_STEP * 6, T_STEP);
    
    vertices[17].pos = icosahedronVertices[5];
    vertices[17].normal = computeVertexNormal(icosahedronVertices[5]);
    // vertices[17].texCoord = Vec2(S_STEP * 8, T_STEP);

    vertices[18].pos = icosahedronVertices[7];
    vertices[18].normal = computeVertexNormal(icosahedronVertices[7]);
    // vertices[18].texCoord = Vec2(S_STEP * 3, T_STEP * 2);

    vertices[19].pos = icosahedronVertices[8];
    vertices[19].normal = computeVertexNormal(icosahedronVertices[8]);
    // vertices[19].texCoord = Vec2(S_STEP * 5, T_STEP * 2);
    
    vertices[20].pos = icosahedronVertices[9];
    vertices[20].normal = computeVertexNormal(icosahedronVertices[9]);
    // vertices[20].texCoord = Vec2(S_STEP * 7, T_STEP * 2);
    
    vertices[21].pos = icosahedronVertices[10];
    vertices[21].normal = computeVertexNormal(icosahedronVertices[10]);
    // vertices[21].texCoord = Vec2(S_STEP * 9, T_STEP * 2);

    u32 indices[] = {
         0, 10, 14,
         1, 14, 15,
         2, 15, 16,
         3, 16, 17,
         4, 17, 11,
        10, 12, 14,
        12, 18, 14,
        14, 18, 15,
        18, 19, 15,
        15, 19, 16,
        19, 20, 16,
        16, 20, 17,
        20, 21, 17,
        17, 21, 11,
        21, 13, 11,
         5, 18, 12,
         6, 19, 18,
         7, 20, 19,
         8, 21, 20,
         9, 13, 21,
    };

    u32 indexCount = ARRAY_COUNT(indices);
    u32* subdividedIndices = indices;
    for(u32 i = 0; i < subdivisions; ++i)
    {
        u32* tmpIndices = subdividedIndices;
        u32 tmpIndicesCount = indexCount;

        u32 triCount = indexCount / 3;
        indexCount = triCount * 12;
        subdividedIndices = PushArray(*rc.frameArena, indexCount, u32);
        indexCount = 0;

        for(u32 j = 0; j < tmpIndicesCount; j += 3)
        {
            // get the 3 indices of each triangle
            u32 i1 = tmpIndices[j];
            u32 i2 = tmpIndices[j+1];
            u32 i3 = tmpIndices[j+2];

            // get the 3 vertices
            gltf_vertex& v1 = vertices[i1];
            gltf_vertex& v2 = vertices[i2];
            gltf_vertex& v3 = vertices[i3];

            gltf_vertex new1{};
            gltf_vertex new2{};
            gltf_vertex new3{};

            new1.pos = computeHalfVertex(v1.pos, v2.pos, radius);
            new1.normal = computeVertexNormal(new1.pos);
            // new1.texCoord = computeHalfTexCoord(v1.texCoord, v2.texCoord);
            
            new2.pos = computeHalfVertex(v2.pos, v3.pos, radius);
            new2.normal = computeVertexNormal(new2.pos);
            // new2.texCoord = computeHalfTexCoord(v2.texCoord, v3.texCoord);
            
            new3.pos = computeHalfVertex(v1.pos, v3.pos, radius);
            new3.normal = computeVertexNormal(new3.pos);
            // new3.texCoord = computeHalfTexCoord(v1.texCoord, v3.texCoord);

            u32 newI1 = vertices.size();
            vertices.push_back(new1);

            u32 newI2 = vertices.size();
            vertices.push_back(new2);
            
            u32 newI3 = vertices.size();
            vertices.push_back(new3);

            subdividedIndices[indexCount++] = i1; subdividedIndices[indexCount++] = newI1; subdividedIndices[indexCount++] = newI3;
            subdividedIndices[indexCount++] = newI1; subdividedIndices[indexCount++] = i2; subdividedIndices[indexCount++] = newI2;
            subdividedIndices[indexCount++] = newI1; subdividedIndices[indexCount++] = newI2; subdividedIndices[indexCount++] = newI3;
            subdividedIndices[indexCount++] = newI3; subdividedIndices[indexCount++] = newI2; subdividedIndices[indexCount++] = i3;
        }
    }
    
    render_geometry sphere{};
    sphere.indexCount = indexCount;
    sphere.indexBuffer = gfx.CreateBuffer(gfx.device, IndexBuffer(indexCount), 0);
    sphere.vertexBuffer = gfx.CreateBuffer(gfx.device, VertexBuffer(vertices.size(), sizeof(gltf_vertex)), 0);

    BeginStagingData(rc);
    StageBufferData(rc, sizeof(gltf_vertex) * vertices.size(), vertices.data(), sphere.vertexBuffer);
    StageBufferData(rc, sizeof(u32) * indexCount, subdividedIndices, sphere.indexBuffer);
    EndStagingData(rc);

    return sphere;
}
#endif

internal render_geometry
CreateSphere(render_context& rc, f32 radius, u32 stackCount, u32 sliceCount)
{
    f32 sliceStep = 2 * Pi32 / sliceCount;
    f32 stackStep = Pi32 / stackCount;
    f32 stackAngle = 0.0f;
    f32 sliceAngle = 0.0f;

    u32 numVertices = (stackCount+1) * (sliceCount+1);
    gltf_vertex* vertices = PushArray(*rc.frameArena, numVertices, gltf_vertex);
    
    v3 pos = Vec3(0.0f, 0.0f, 0.0f);
    v3 normal = Vec3(0.0f, 0.0f, 0.0f);
    // v2 texCoords = Vec2(0.0f, 0.0f);
    f32 xy = 0.0f;
    f32 invLength = 1.0f / radius;
        
    u32 curVertexIdx = 0;
    for(u32 stackIdx = 0; stackIdx <= stackCount; ++stackIdx)
    {
        stackAngle = Pi32 / 2.0f - stackIdx * stackStep;
        xy = radius * cosf(stackAngle);
        pos.Z = radius * sinf(stackAngle);
        // texCoords.V = stackIdx / stackCount;

        for(u32 sliceIdx = 0; sliceIdx <= sliceCount; ++sliceIdx)
        {
            sliceAngle = sliceIdx * sliceStep;

            pos.X = xy * cosf(sliceAngle);
            pos.Y = xy * sinf(sliceAngle);

            normal = pos * invLength;
            // texCoords.U = sliceIdx / sliceCount;

            gltf_vertex& vertex = vertices[curVertexIdx++];
            vertex.pos = pos;
            vertex.normal = normal;
        }
    }

    u32 numIndices = 0;
    numIndices += sliceCount * 3;    // first stack
    numIndices += sliceCount * 3;    // last stack
    numIndices += (stackCount - 2) * sliceCount * 6;   // all stacks except first and last
    array<u32>& indices = *array_create(*rc.frameArena, u32, numIndices);

    u32 k1 = 0;
    u32 k2 = 0;
    // now compute the indices
    for(u32 stackIdx = 0; stackIdx < stackCount; ++stackIdx)
    {
        k1 = stackIdx * (sliceCount + 1);   // beginning of current stack
        k2 = k1 + sliceCount + 1;           // beginning of next stack

        for(u32 sliceIdx = 0; sliceIdx < sliceCount; ++sliceIdx, ++k1, ++k2)
        {
            // skip the first stack
            if(stackIdx != 0)
            {
                indices.push_back(k1);
                indices.push_back(k2);
                indices.push_back(k1 + 1);
            }
            // skip the last stack
            if(stackIdx != (stackCount - 1))
            {
                indices.push_back(k1 + 1);
                indices.push_back(k2);
                indices.push_back(k2 + 1);
            }
        }
    }

    render_geometry sphere{};
    sphere.indexCount = numIndices;
    sphere.indexBuffer = gfx.CreateBuffer(gfx.device, IndexBuffer(numIndices), 0);
    sphere.vertexBuffer = gfx.CreateBuffer(gfx.device, VertexBuffer(numVertices, sizeof(gltf_vertex)), 0);

    BeginStagingData(rc);
    StageBufferData(rc, sizeof(gltf_vertex) * numVertices, vertices, sphere.vertexBuffer);
    StageBufferData(rc, sizeof(u32) * indices.size(), indices.data(), sphere.indexBuffer);
    EndStagingData(rc);

    return sphere;
}

internal void
SetupRenderer(game_state& game)
{
    render_context& rc = *game.renderer;
    graphics_context& gc = *rc.gc;

    f32 width = 20.0f;
    f32 depth = 20.0f;
    f32 halfWidth = width/2.0f;
    f32 halfDepth = depth/2.0f;

    umm posOffset = OffsetOf(render_mesh_vertex, pos);
    umm colorOffset = OffsetOf(render_mesh_vertex, color);
    umm uvOffset = OffsetOf(render_mesh_vertex, texCoord);
    umm vsize = sizeof(render_mesh_vertex);

    render_mesh_vertex vertices[] = {
        {{ -halfWidth, 0.0f,  halfDepth }, { 0.0f, 1.0f, 0.0f}, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f }},
        {{ -halfWidth, 0.0f, -halfDepth }, { 0.0f, 1.0f, 0.0f}, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f }},
        {{  halfWidth, 0.0f, -halfDepth }, { 0.0f, 1.0f, 0.0f}, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f }},
        {{  halfWidth, 0.0f,  halfDepth }, { 0.0f, 1.0f, 0.0f}, { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f }},
    };
    u32 indices[] = { 0, 2, 1, 0, 3, 2 };

    GfxColor colors[] = {
        gfxColor(0x065535), // green
        gfxColor(0x701778), // brown
    };

    GfxBufferDesc vb = MeshVertexBuffer(ARRAY_COUNT(vertices));
    GfxBufferDesc ib = IndexBuffer(ARRAY_COUNT(indices));
    GfxBufferDesc mb = UniformBuffer(sizeof(colors));

    GfxBufferDesc sb = StagingBuffer(Megabytes(16));
    rc.stagingBuffer = gfx.CreateBuffer(gfx.device, sb, nullptr);
    
    rc.cmdpool = gfx.CreateEncoderPool(gfx.device, {GfxQueueType::Graphics});
    rc.cmds = gfx.CreateEncoderContext(rc.cmdpool);
    rc.stagingCmdPool = gfx.CreateEncoderPool(gfx.device, {GfxQueueType::Graphics});    // TODO(james): just use transfer queue?
    rc.stagingCmds = gfx.CreateEncoderContext(rc.stagingCmdPool);

    rc.ground.indexCount = ARRAY_COUNT(indices);
    rc.ground.indexBuffer = gfx.CreateBuffer(gfx.device, ib, 0);
    rc.ground.vertexBuffer = gfx.CreateBuffer(gfx.device, vb, 0);
    rc.groundMaterial = gfx.CreateBuffer(gfx.device, mb, 0);
    rc.groundProgram = LoadProgram(*rc.frameArena, "shader.vert.spv", "shader.frag.spv");
    rc.groundKernel = gfx.CreateGraphicsKernel(gfx.device, rc.groundProgram, DefaultPipeline(true));

    rc.meshSceneBuffer = gfx.CreateBuffer(gfx.device, UniformBuffer(sizeof(SceneBufferObject), GfxMemoryAccess::CpuToGpu), 0);
    rc.meshMaterial = gfx.CreateBuffer(gfx.device, UniformBuffer(sizeof(render_material)), 0);
    rc.meshProgram = LoadProgram(*rc.frameArena, "pbrbox.vert.spv", "pbrbox.frag.spv");
    rc.meshKernel = gfx.CreateGraphicsKernel(gfx.device, rc.meshProgram, DefaultPipeline(true));

    rc.lightProgram = LoadProgram(*rc.frameArena, "lightbox.vert.spv", "lightbox.frag.spv");
    rc.lightKernel = gfx.CreateGraphicsKernel(gfx.device, rc.lightProgram, DefaultPipeline(true));

    rc.depthTarget = gfx.CreateRenderTarget(gfx.device, DepthRenderTarget(gc.windowWidth, gc.windowHeight));

    GfxTextureDesc texDesc = {};

    void* pixels = 0;
    TempLoadImagePixels(*rc.frameArena, "texture.jpg", &texDesc.width, &texDesc.height, 0, pixels);
    texDesc.type = GfxTextureType::Tex2D;
    texDesc.format = TinyImageFormat_R8G8B8A8_SRGB;
    texDesc.access = GfxMemoryAccess::GpuOnly;

    rc.texture = gfx.CreateTexture(gfx.device, texDesc);
    rc.sampler = gfx.CreateSampler(gfx.device, Sampler());

    render_material meshMaterial = {};
    meshMaterial.albedo = Vec3(1.0f, 0.0f, 0.0f);
    meshMaterial.metallic = 0.0f;
    meshMaterial.roughness = 0.2f;
    meshMaterial.ao = 0.2f;

    // rc.sphere = CreateIcosphere(rc, 1.0f, 2);
    rc.sphere = CreateSphere(rc, 1.0f, 18, 36);

    BeginStagingData(rc);
    StageBufferData(rc, sizeof(vertices), vertices, rc.ground.vertexBuffer);
    StageBufferData(rc, sizeof(indices), indices, rc.ground.indexBuffer);
    StageBufferData(rc, sizeof(colors), colors, rc.groundMaterial);
    StageBufferData(rc, sizeof(meshMaterial), &meshMaterial, rc.meshMaterial);
    StageTextureData(rc, texDesc.width*texDesc.height*4, pixels, rc.texture);
    // TODO(james): fix the crash in this next call...
    TempLoadGltfGeometry(rc, "box.glb", &rc.numMeshes, &rc.meshes);
    EndStagingData(rc);
}

internal void
RenderFrame(render_context& rc, game_state& game, const GameClock& clock)
{
    graphics_context& gc = *rc.gc;

    m4 cameraView = LookAt(game.camera.position, game.camera.target, V3_Y_UP);
    //m4 cameraView = LookAt(Vec3(5.0f, 5.0f, -5.0f), game.camera.target, V3_Y_UP);
    m4 projection = game.cameraProjection;
    m4 viewProj = projection * cameraView;

    SceneBufferObject sbo{};
    sbo.viewProj = viewProj;
    sbo.pos = game.camera.position;
    sbo.light.pos = game.lightPosition;
    sbo.light.color = Vec3(1.0f, 1.0f, 1.0f);

    void* sceneBufferData = gfx.GetBufferData(gfx.device, rc.meshSceneBuffer);
    Copy(sizeof(sbo), &sbo, sceneBufferData);

    GfxRenderTarget screenRTV = gfx.AcquireNextSwapChainTarget(gfx.device);

    GfxCmdContext& cmds = rc.cmds;

    gfx.ResetCmdEncoderPool(rc.cmdpool);
    gfx.BeginEncodingCmds(cmds);

    GfxRenderTargetBarrier barrierRTVs[] = {
        { screenRTV, GfxResourceState::Present, GfxResourceState::RenderTarget },
    };
    gfx.CmdResourceBarrier(cmds, 0, nullptr, 0, nullptr, ARRAY_COUNT(barrierRTVs), barrierRTVs);

    gfx.CmdSetViewport(cmds, 0, 0, gc.windowWidth, gc.windowHeight);
    gfx.CmdSetScissorRect(cmds, 0, 0, gc.windowWidth, gc.windowHeight);

    gfx.CmdBindRenderTargets(cmds, 1, &screenRTV, &rc.depthTarget);
    
    // Render Ground
    gfx.CmdBindKernel(cmds, rc.groundKernel);

    GfxDescriptor descriptors[] = {
        // BufferDescriptor(0, state.materialBuffer),
        NamedBufferDescriptor("material", rc.groundMaterial),
        //NamedTextureDescriptor("texSampler", state.texture, state.sampler),
    };

    GfxDescriptorSet desc = {};
    desc.setLocation = 0;   
    desc.count = ARRAY_COUNT(descriptors);
    desc.pDescriptors = descriptors;
    gfx.CmdBindDescriptorSet(cmds, desc);

    m4 matrix = viewProj * Translate(Vec3(0.0f, 0.0f, 0.0f));

    gfx.CmdBindPushConstant(cmds, "constants", &matrix);

    gfx.CmdBindIndexBuffer(cmds, rc.ground.indexBuffer);
    gfx.CmdBindVertexBuffer(cmds, rc.ground.vertexBuffer);
    gfx.CmdDrawIndexed(cmds, rc.ground.indexCount, 1, 0, 0, 0);

    // Render Mesh(es)
    gfx.CmdBindKernel(cmds, rc.meshKernel);
    
    GfxDescriptor sceneDescriptors[] = {
        NamedBufferDescriptor("scene", rc.meshSceneBuffer),
    };
    desc.setLocation = 0;
    desc.count = ARRAY_COUNT(sceneDescriptors);
    desc.pDescriptors = sceneDescriptors;
    gfx.CmdBindDescriptorSet(cmds, desc);

    GfxDescriptor meshMaterialDescriptors[] = {
        NamedBufferDescriptor("material", rc.meshMaterial),
    };
    desc.setLocation = 1;
    desc.count = ARRAY_COUNT(meshMaterialDescriptors);
    desc.pDescriptors = meshMaterialDescriptors;
    gfx.CmdBindDescriptorSet(cmds, desc);

    matrix = Translate(game.position) 
        * Rotate(game.rotationAngle, Vec3i(0,1,0))
        * Scale(Vec3(game.scaleFactor, game.scaleFactor, game.scaleFactor));

    gfx.CmdBindPushConstant(cmds, "constants", &matrix);

    gfx.CmdBindIndexBuffer(cmds, rc.sphere.indexBuffer);
    gfx.CmdBindVertexBuffer(cmds, rc.sphere.vertexBuffer);
    gfx.CmdDrawIndexed(cmds, rc.sphere.indexCount, 1, 0, 0, 0);

    // Render the light...
    // TODO(james): find a better way than assuming that mesh[0] is a cube
    gfx.CmdBindKernel(cmds, rc.lightKernel);

    // TODO(james): find a way to re-use an allocated descriptor set
    desc.setLocation = 0;
    desc.count = ARRAY_COUNT(sceneDescriptors);
    desc.pDescriptors = sceneDescriptors;
    gfx.CmdBindDescriptorSet(cmds, desc);
    
    matrix = Translate(game.lightPosition)
        * Scale(Vec3(game.lightScale, game.lightScale, game.lightScale));

    gfx.CmdBindPushConstant(cmds, "constants", &matrix);

    //gfx.CmdBindIndexBuffer(cmds, rc.meshes[0].indexBuffer);
    //gfx.CmdBindVertexBuffer(cmds, rc.meshes[0].vertexBuffer);
    gfx.CmdDrawIndexed(cmds, rc.sphere.indexCount, 1, 0, 0, 0);

    // Now we're done, prep for presenting

    gfx.CmdBindRenderTargets(cmds, 0, nullptr, nullptr);    // Unbind the render targets so we can move the RTV to the Present Mode

    GfxRenderTargetBarrier barrierPresent = { screenRTV, GfxResourceState::RenderTarget, GfxResourceState::Present };
    gfx.CmdResourceBarrier(cmds, 0, nullptr, 0, nullptr, 1, &barrierPresent);

    gfx.EndEncodingCmds(cmds);

    gfx.Frame(gfx.device, 1, &cmds);        
}