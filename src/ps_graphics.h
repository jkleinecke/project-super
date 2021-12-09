

/* 
psuedo-code 

TODO(james): Account for layering in multiple levels of shader data (global, per-frame, per-object)

init:
    upload buffer data (vertex)
    upload buffer data (index)
    upload buffer data (uniform) push-constant?
    upload image data
    declare pipeline & shader data
    declare renderpass

record render commands:

    begin recording
    begin render pass
    bind pipeline
    
    bind vertex buffer(s)
    bind index buffer
    bind shader data
    
    draw - hopefully more than once

    end render pass
    end recording

end frame:

    submit render commands
    present -- platform layer??

 */

// TODO(james): figure out the best way to handle the opaque graphics pointers
typedef void* ps_gfx_device_h;

typedef void* ps_gfx_render_commands_h;

typedef void* ps_gfx_buffer_h;
typedef void* ps_gfx_image_h;
typedef void* ps_gfx_shader_data_h;
typedef void* ps_gfx_pipeline_h;
typedef void* ps_gfx_renderpass_h;

enum PsGfxBufferType
{
    // More will be added as they are needed
    PS_GFX_BUFFER_TYPE_VERTEX,
    PS_GFX_BUFFER_TYPE_INDEX,
    PS_GFX_BUFFER_TYPE_UNIFORM    
};

// TODO(james): do these make sense?
enum PsGfxUsage
{
    PS_GFX_BUFFER_USAGE_STAGING,
    PS_GFX_BUFFER_USAGE_GPU_ONLY,
    PS_GFX_BUFFER_USAGE_CPU_GPU,
    PS_GFX_BUFFER_USAGE_CPU_READBACK
};

enum PsGfxImageType
{
    PS_GFX_IMAGE_TYPE_1D,
    PS_GFX_IMAGE_TYPE_2D,
    PS_GFX_IMAGE_TYPE_3D,
};

enum PsGfxImageFormat
{
    // More will be added as they are needed
    PS_GFX_FORMAT_R8G8B8A8_SRGB
};

struct PsGfxImageOffset
{
    u32 x;  // All Types
    u32 y; // 2D & 3D
    u32 z;  // 3D only
};

struct PsGfxImageExtent
{
    u32 width;  // All Types
    u32 height; // 2D & 3D
    u32 depth;  // 3D only
};

struct ps_graphics_api
{
    // Platform layer is responsible for managing the swap chain entirely
    ps_gfx_device_h device;
    // TODO(james): Expose the device queues, command pools, etc..

    // Render Commands
    // NOTE: for now this will be enough, we'll probably need to extend this to allow for multi-threading command recording later
    typedef ps_gfx_render_commands_h (*get_render_command_buffer)(ps_gfx_device_h device);   
    typedef void (*begin_render_recording)(ps_gfx_render_commands_h cmds);
    typedef void (*end_render_recording)(ps_gfx_render_commands_h cmds);

    typedef void (*bind_renderpass)(ps_gfx_render_commands_h cmds, ps_gfx_renderpass_h renderpass);
    typedef void (*bind_pipeline)(ps_gfx_render_commands_h cmds, ps_gfx_pipeline_h pipeline);
    typedef void (*bind_shader_data)(ps_gfx_render_commands_h cmds, ps_gfx_shader_data_h shaderdata);
    typedef void (*bind_index_buffer)(ps_gfx_render_commands_h cmds, ps_gfx_buffer_h indexBuffer);
    typedef void (*bind_vertex_buffers)(ps_gfx_render_commands_h cmds, u32 num_buffers, ps_gfx_buffer_h* pVertexBuffers);

    typedef void (*draw_indexed)(ps_gfx_render_commands_h cmds, u32 index_count, u32 vertex_offset);    // extend as needed, instances?

    // TODO(james): sync primitives, callbacks? memory barriers? image transitions?
    typedef void (*copy_buffer_to_buffer)(ps_gfx_render_commands_h cmds, ps_gfx_buffer_h src, umm srcOffset, ps_gfx_buffer_h dest, umm destOffset, mem_size datasize);    
    typedef void (*copy_buffer_to_image)(ps_gfx_render_commands_h cmds, ps_gfx_buffer_h src, umm srcOffset, ps_gfx_image_h dest, const PsGfxImageOffset& offset, const PsGfxImageExtent& extent);

    // Resource Management 

    // TODO(james): should resource management be done from a GPU memory allocator?
    typedef ps_gfx_renderpass_h (*create_render_pass)(ps_gfx_device_h device);  // extend with options
    typedef ps_gfx_pipeline_h (*create_pipeline)(ps_gfx_device_h device);       // extend with options
    typedef ps_gfx_shader_data_h (*create_shader_data)(ps_gfx_device_h device); // extend with options
    typedef ps_gfx_buffer_h (*create_buffer)(ps_gfx_device_h device, mem_size size, PsGfxBufferType type, PsGfxUsage usage);
    typedef ps_gfx_image_h (*create_image)(ps_gfx_device_h device, PsGfxImageType type, PsGfxUsage usage, PsGfxImageFormat format, PsGfxImageExtent extent);

    // TODO(james): combine these two since under the hood all that is needed is a memory address
    // just alias a buffer and image to the same location?
    typedef void (*upload_buffer_data)(ps_gfx_device_h device, ps_gfx_buffer_h buffer, umm offset, mem_size datasize, void* data);
    typedef void (*upload_image_data)(ps_gfx_device_h device, ps_gfx_image_h image, umm offset, mem_size datasize, void* data);

    typedef void (*destroy_renderpass)(ps_gfx_device_h device, ps_gfx_renderpass_h renderpass);
    typedef void (*destroy_pipeline)(ps_gfx_device_h device, ps_gfx_pipeline_h pipeline);
    typedef void (*destroy_shaderdata)(ps_gfx_device_h device, ps_gfx_shader_data_h shaderdata);
    typedef void (*destroy_buffer)(ps_gfx_device_h device, ps_gfx_buffer_h buffer);
    typedef void (*destroy_image)(ps_gfx_device_h device, ps_gfx_image_h image);

};
