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
PushCmd_UpdateViewProjection(render_commands& cmds, const m4& view, const m4& projection)
{
    render_cmd_update_viewprojection& cmd = *PushCmd(cmds, render_cmd_update_viewprojection);
    cmd.header = { RenderCommandType::UpdateViewProjection, sizeof(cmd) };
    
    cmd.view = view;
    cmd.projection = projection;
}

internal void
PushCmd_DrawObject(render_commands& cmds, const render_geometry& geometry, const m4& modelTransform)
{
    render_cmd_draw_object& cmd = *PushCmd(cmds, render_cmd_draw_object);
    cmd.header = { RenderCommandType::DrawObject, sizeof(cmd) };

    cmd.model = modelTransform;
    cmd.indexBuffer = geometry.indexBuffer;
    cmd.vertexBuffer = geometry.vertexBuffer;
}

internal void
BeginRenderCommands(render_commands& cmds)
{
    // TODO(james): should I put a begin header here??
    //cmds.pushBufferDataAt = cmds.pushBufferBase;
}

internal void
EndRenderCommands(render_commands& cmds)
{
    render_cmd_header& cmd = *PushCmd(cmds, render_cmd_header);
    cmd = { RenderCommandType::Done, sizeof(render_cmd_header) };
}