
internal void
push_cmd_UpdateViewProjection(render_commands& cmds, const m4& view, const m4& projection)
{
    render_cmd_update_viewprojection& cmd = *(render_cmd_update_viewprojection*)PushStruct(cmds.cmd_arena, sizeof(render_cmd_update_viewprojection));
    cmd.header = { RenderCommandType::UpdateViewProjection, sizeof(cmd) };
    
    cmd.view = view;
    cmd.projection = projection;
}

internal void
push_cmd_DrawObject(render_commands& cmds, const render_geometry& geometry, const m4& modelTransform)
{
    render_cmd_draw_object& cmd = *(render_cmd_draw_object*)PushStruct(cmds.cmd_arena, sizeof(render_cmd_draw_object));
    cmd.header = { RenderCommandType::DrawObject, sizeof(cmd) };

    cmd.model = modelTransform;
    cmd.indexBuffer = geometry.indexBuffer;
    cmd.vertexBuffer = geometry.vertexBuffer;
}

internal void
BeginRenderCommands(render_commands& cmds)
{
    // reset the memory pointer
    cmds.cmd_arena.freePointer = cmds.cmd_arena.basePointer;
}

internal void
EndRenderCommands(render_commands& cmds)
{
    render_cmd_header& cmd = *(render_cmd_header*)PushStruct(cmds.cmd_arena, sizeof(render_cmd_header));
    cmd = { RenderCommandType::Done, sizeof(render_cmd_header) };
}