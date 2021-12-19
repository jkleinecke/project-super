
#if 0
internal 
ps_gfx_render_commands_h VgGetRenderCommandBuffers(ps_gfx_device_h device)
{
    return 0;
}

internal 
void VgBeginRenderRecording(ps_gfx_render_commands_h cmds)
{
    
}

internal 
void VgEndRenderRecording(ps_gfx_render_commands_h cmds)
{

}

internal 
void VgBindRenderPass(ps_gfx_render_commands_h cmds, ps_gfx_renderpass_h renderpass)
{

}

internal 
void VgBindPipeline(ps_gfx_render_commands_h cmds, ps_gfx_pipeline_h pipeline)
{

}

internal 
void VgBindShaderData(ps_gfx_render_commands_h cmds, ps_gfx_shader_data_h shaderdata)
{

}

internal 
void VgBindIndexBuffer(ps_gfx_render_commands_h cmds, ps_gfx_buffer_h indexBuffer)
{

}

internal 
void VgBindVertexBuffers(ps_gfx_render_commands_h cmds, u32 num_buffers, ps_gfx_buffer_h* pVertexBuffers)
{

}

internal 
void VgDrawIndexed(ps_gfx_render_commands_h cmds, u32 index_count, u32 vertex_offset)
{

}

internal 
void VgCopyBufferToBuffer(ps_gfx_render_commands_h cmds, ps_gfx_buffer_h src, umm srcOffset, ps_gfx_buffer_h dest, umm destOffset, mem_size datasize)
{

}

internal 
void VgCopyBufferToImage(ps_gfx_render_commands_h cmds, ps_gfx_buffer_h src, umm srcOffset, ps_gfx_image_h dest, const PsGfxImageOffset& offset, const PsGfxImageExtent& extent)
{

}

internal 
ps_gfx_renderpass_h VgCreateRenderPass(ps_gfx_device_h device)
{
    return 0;
}

internal 
ps_gfx_pipeline_h VgCreatePipeline(ps_gfx_device_h device)
{
    return 0;
}

internal
ps_gfx_sampler_h VgCreateSampler(ps_gfx_device_h device)
{
    return 0;
}

internal 
ps_gfx_buffer_h VgCreateBuffer(ps_gfx_device_h device, mem_size size, PsGfxBufferType type, PsGfxUsage usage)
{
    return 0;
}

internal 
ps_gfx_image_h VgCreateImage(ps_gfx_device_h device, PsGfxImageType type, PsGfxUsage usage, PsGfxImageFormat format, PsGfxImageExtent extent)
{
    return 0;
}

internal 
void VgUploadBufferData(ps_gfx_device_h device, ps_gfx_buffer_h buffer, umm offset, mem_size datasize, void* data)
{

}

internal 
void VgUploadImageData(ps_gfx_device_h device, ps_gfx_image_h image, umm offset, mem_size datasize, void* data)
{

}

internal 
void VgDestroyRenderPass(ps_gfx_device_h device, ps_gfx_renderpass_h renderpass)
{

}

internal 
void VgDestroyPipeline(ps_gfx_device_h device, ps_gfx_pipeline_h pipeline)
{

}

internal
void VgDestroySampler(ps_gfx_device_h device, ps_gfx_sampler_h sampler)
{

}

internal 
void VgDestroyBuffer(ps_gfx_device_h device, ps_gfx_buffer_h buffer)
{

}

internal 
void VgDestroyImage(ps_gfx_device_h device, ps_gfx_image_h image)
{

}

internal
void VgLoadApi(vg_backend& vb, ps_graphics_api& api)
{
    api.device = (void*)&vb.device;

    // Render Commands
    api.GetRenderCommandBuffers = VgGetRenderCommandBuffers;
    api.BeginRenderRecording = VgBeginRenderRecording;
    api.EndRenderRecording = VgEndRenderRecording; 
    api.BindRenderPass = VgBindRenderPass;
    api.BindPipeline = VgBindPipeline;
    api.BindIndexBuffer = VgBindIndexBuffer;
    api.BindVertexBuffers = VgBindVertexBuffers;
    api.DrawIndexed = VgDrawIndexed;
    api.CopyBufferToBuffer = VgCopyBufferToBuffer;
    api.CopyBufferToImage = VgCopyBufferToImage;

    // Resource Management
    api.CreateRenderPass = VgCreateRenderPass;
    api.CreatePipeline = VgCreatePipeline;
    api.CreateSampler = VgCreateSampler;
    api.CreateBuffer = VgCreateBuffer;
    api.CreateImage = VgCreateImage;
    api.UploadBufferData = VgUploadBufferData;
    api.UploadImageData = VgUploadImageData;
    api.DestroyRenderPass = VgDestroyRenderPass;
    api.DestroyPipeline = VgDestroyPipeline;
    api.DestroySampler = VgDestroySampler;
    api.DestroyBuffer = VgDestroyBuffer;
    api.DestroyImage = VgDestroyImage;
}
#endif