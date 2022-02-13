

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

internal GfxPipelineDesc
DefaultPipeline() 
{
    GfxBlendState bs = {};
    bs.renderTargets[0] = TransparentRenderTarget();

    GfxDepthStencilState ds = {};
    ds.depthEnable = false;

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
    return desc;
}

internal GfxBufferDesc
MeshVertexBuffer(u32 count)
{
    GfxBufferDesc desc = {};
    desc.usageFlags = GfxBufferUsageFlags::Vertex;
    desc.access = GfxMemoryAccess::CpuToGpu;    // TODO(james): use a staging buffer instead
    desc.size = count * sizeof(render_mesh_vertex);

    return desc;
}

internal GfxBufferDesc
IndexBuffer(u32 count)
{
    GfxBufferDesc desc = {};
    desc.usageFlags = GfxBufferUsageFlags::Index;
    desc.access = GfxMemoryAccess::CpuToGpu;    // TODO(james): use a staging buffer instead
    desc.size = count * sizeof(u32);

    return desc;
}

internal GfxBufferDesc
UniformBuffer(u64 size)
{
    GfxBufferDesc desc = {};
    desc.usageFlags = GfxBufferUsageFlags::Uniform;
    desc.access = GfxMemoryAccess::CpuToGpu;    // TODO(james): use a staging buffer instead
    desc.size = size;

    return desc;
}