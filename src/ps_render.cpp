inline void*
RendererInternalPushCmd_(render_commands& cmds, umm size)
{
    void* dataptr = (void*)cmds.pushBufferDataAt;
    u8* maxptr = cmds.pushBufferBase + cmds.maxPushBufferSize;
    u8* nextptr = cmds.pushBufferDataAt + size;
    if(nextptr <= maxptr) 
    {
        cmds.pushBufferDataAt += size;
    }
    else
    // This means we don't have enough room in the push buffer..
    { InvalidCodePath; }

    return dataptr;
}

#define PushCmd(cmds, type) (type*)RendererInternalPushCmd_(cmds, sizeof(type))

internal void
PushCmd_UsePipeline(render_commands& cmds, render_pipeline_id id)
{
    render_cmd_use_pipeline& cmd = *PushCmd(cmds, render_cmd_use_pipeline);
    cmd.header = { RenderCommandType::UsePipeline, sizeof(cmd) };
    
    cmd.pipeline_id = id;
}

internal void
PushCmd_UpdateLight(render_commands& cmds, const v3& position, const v3& ambient, const v3& diffuse, const v3& specular)
{
    render_cmd_update_light& cmd = *PushCmd(cmds, render_cmd_update_light);
    cmd.header = { RenderCommandType::UpdateLight, sizeof(cmd) };

    cmd.position = position;
    cmd.ambient = ambient;
    cmd.diffuse = diffuse;
    cmd.specular = specular;
}

internal void
PushCmd_DrawObject(render_commands& cmds, const render_geometry& geometry, const m4& mvp, const m4& world, const m4& worldNormal, render_material_id material, u32 bindingCount = 0, render_material_binding* bindings = nullptr)
{
    render_cmd_draw_object& cmd = *PushCmd(cmds, render_cmd_draw_object);
    cmd.header = { RenderCommandType::DrawObject, sizeof(cmd) };

    cmd.mvp = mvp;
    cmd.world = world;
    cmd.worldNormal = worldNormal;
    cmd.indexCount = geometry.indexCount;
    cmd.indexBuffer = geometry.indexBuffer;
    cmd.vertexBuffer = geometry.vertexBuffer;
    cmd.material_id = material;
    cmd.materialBindingCount = bindingCount;
    Copy(sizeof(render_material_binding) * bindingCount, bindings, cmd.materialBindings);
}

internal void
BeginRenderCommands(render_commands& cmds, const BeginRenderInfo& renderInfo)
{
    cmds.viewportPosition = renderInfo.viewportPosition;
    cmds.viewportSize = renderInfo.viewportSize;

    cmds.time = renderInfo.time;
    cmds.timeDelta = renderInfo.timeDelta;

    cmds.cameraPos = renderInfo.cameraPos;
    cmds.cameraView = renderInfo.cameraView;
    cmds.cameraProj = renderInfo.cameraProj;
}

internal void
EndRenderCommands(render_commands& cmds)
{
    render_cmd_header& cmd = *PushCmd(cmds, render_cmd_header);
    cmd = { RenderCommandType::Done, sizeof(render_cmd_header) };
}


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