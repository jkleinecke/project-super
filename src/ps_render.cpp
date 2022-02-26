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
    rc.meshProgram = LoadProgram(*rc.frameArena, "box.vert.spv", "box.frag.spv");
    rc.meshKernel = gfx.CreateGraphicsKernel(gfx.device, rc.meshProgram, DefaultPipeline(true));

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
    meshMaterial.ambient = Vec3(1.0f, 0.5f, 0.31f);
    meshMaterial.diffuse = Vec3(1.0f, 0.5f, 0.31f);
    meshMaterial.specular = Vec3(0.5f, 0.5f, 0.5f);
    meshMaterial.shininess = 32.0f;

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
    sbo.light.ambient = Vec3(0.2f, 0.2f, 0.2f);
    sbo.light.diffuse = Vec3(0.5f, 0.5f, 0.5f);
    sbo.light.specular = Vec3(1.0f, 1.0f, 1.0f);

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

    if(rc.numMeshes > 0)
    {
        matrix = Translate(game.position) 
            * Rotate(game.rotationAngle, Vec3i(0,1,0))
            * Scale(Vec3(game.scaleFactor, game.scaleFactor, game.scaleFactor));

        gfx.CmdBindPushConstant(cmds, "constants", &matrix);

        gfx.CmdBindIndexBuffer(cmds, rc.meshes[0].indexBuffer);
        gfx.CmdBindVertexBuffer(cmds, rc.meshes[0].vertexBuffer);
        gfx.CmdDrawIndexed(cmds, rc.meshes[0].indexCount, 1, 0, 0, 0);
    }

    // Now we're done, prep for presenting

    gfx.CmdBindRenderTargets(cmds, 0, nullptr, nullptr);    // Unbind the render targets so we can move the RTV to the Present Mode

    GfxRenderTargetBarrier barrierPresent = { screenRTV, GfxResourceState::RenderTarget, GfxResourceState::Present };
    gfx.CmdResourceBarrier(cmds, 0, nullptr, 0, nullptr, 1, &barrierPresent);

    gfx.EndEncodingCmds(cmds);

    gfx.Frame(gfx.device, 1, &cmds);        
}