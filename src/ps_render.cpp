
template<typename T> struct rendercmdtype { };
#define DECLARE_RENDER_CMD_TYPE(typename, typeval) template<> struct rendercmdtype<typename> { internal RenderCommandType cmd_type; }; RenderCommandType rendercmdtype<typename>::cmd_type = typeval;
DECLARE_RENDER_CMD_TYPE(render_cmd_update_viewprojection, RENDER_CMD_UPDATE_VIEWPROJECTION)
DECLARE_RENDER_CMD_TYPE(render_cmd_draw_object, RENDER_CMD_DRAW_OBJECT)

template<typename T>
internal inline
T Cmd()
{
     T ret = { rendercmdtype<T>::cmd_type, sizeof(T) };
     return ret;
} 

#define ADD_CMD(cmds, var) do{ void* pcmd = PushStruct(cmds.cmd_arena, sizeof(var)); Copy(sizeof(var), &var, pcmd); }while(0)

internal void
push_cmd_UpdateViewProjection(render_commands& cmds, const m4& view, const m4& projection)
{
    auto cmd = Cmd<render_cmd_update_viewprojection>();

    cmd.view = view;
    cmd.projection = projection;

    ADD_CMD(cmds, cmd);   
}

internal void
push_cmd_DrawObject(render_commands& cmds, const ps_geometry& geometry, const m4& modelTransform)
{
    auto cmd = Cmd<render_cmd_draw_object>();

    cmd.model = modelTransform;
    cmd.indexBuffer = geometry.indexBuffer;
    cmd.vertexBuffer = geometry.vertexBuffer;

    ADD_CMD(cmds, cmd);
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
    render_cmd_header cmd = { RENDER_CMD_DONE, sizeof(render_cmd_header) };

    ADD_CMD(cmds, cmd);
}