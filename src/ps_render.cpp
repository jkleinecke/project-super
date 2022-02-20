

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
    rtv.depthValue = 0.0f;
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
    rs.cullMode = GfxCullMode::None;
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
    desc.access = GfxMemoryAccess::GpuOnly;    // TODO(james): use a staging buffer instead
    desc.size = count * sizeof(render_mesh_vertex);

    return desc;
}

internal GfxBufferDesc
IndexBuffer(u32 count)
{
    GfxBufferDesc desc = {};
    desc.usageFlags = GfxBufferUsageFlags::Index;
    desc.access = GfxMemoryAccess::GpuOnly;    // TODO(james): use a staging buffer instead
    desc.size = count * sizeof(u32);

    return desc;
}

internal GfxBufferDesc
UniformBuffer(u64 size)
{
    GfxBufferDesc desc = {};
    desc.usageFlags = GfxBufferUsageFlags::Uniform;
    desc.access = GfxMemoryAccess::GpuOnly;    // TODO(james): use a staging buffer instead
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
BufferDescriptor(u16 bindingLocation, GfxBuffer buffer)
{
    GfxDescriptor desc{};
    desc.type = GfxDescriptorType::Buffer;
    desc.bindingLocation = bindingLocation;
    desc.buffer = buffer;
    return desc;
}

inline GfxDescriptor
NamedBufferDescriptor(const char* name, GfxBuffer buffer)
{
    GfxDescriptor desc{};
    desc.type = GfxDescriptorType::Buffer;
    desc.name = (char*)name;
    desc.buffer = buffer;
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