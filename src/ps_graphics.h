

/* 
psuedo-code 

TODO(james): Account for layering in multiple levels of shader data (global, per-frame, per-object)

init:
    upload buffer data (vertex)
    upload buffer data (index)
    upload buffer data (uniform) push-constant?
    upload image data
    declare material 
    declare renderpass

    update scene global data

begin frame:

    rough object cull? cpu-side?

    TODO(james): implement gpu driven rendering

    <simple pattern>
    update frame global data
    for render pass
        update render pass global data
        bind render pass
        for each material
            update material global data
            bind material
            for each mesh
                update mesh material data
                draw 

end frame:

    present -- platform layer??

 */

//#include "libs/tiny_imageformat/include/tiny_imageformat/tinyimageformat.h"



#define DECLARE_GRAPHICS_FUNCTION(ret, name, ...)   \
    typedef PS_API ret(PS_APICALL* name##Fn)(__VA_ARGS__); \
    name##Fn    name

struct ps_graphics_api
{
    // Platform layer is responsible for managing the swap chain entirely

    // TODO(james): Extend in the following ways:
    //  - Creation/Usage of multiple queue types
    //  - Background resource uploading
    //  - Creation/Usage of multiple command buffers and/or command pools
    //  - Compute shaders
    //  - Rich resource management interface

    // Rendering Objects
    DECLARE_GRAPHICS_FUNCTION(VkCommandBuffer, GetCmdBuffer, vg_device* device);   
    DECLARE_GRAPHICS_FUNCTION(VkFramebuffer, GetFramebuffer, vg_device* device);
    DECLARE_GRAPHICS_FUNCTION(VkDescriptorSet, CreateDescriptor, vg_device* device, VkDescriptorSetLayout layout);
    
    DECLARE_GRAPHICS_FUNCTION(void, UpdateBufferData, vg_device* device, vg_buffer& buffer, memory_index size, const void* data);
    DECLARE_GRAPHICS_FUNCTION(void, UpdateDescriptorSets, vg_device* device, u32 count, VkWriteDescriptorSet* pWrites);

    DECLARE_GRAPHICS_FUNCTION(void, BeginRecordingCmds, VkCommandBuffer cmds);
    DECLARE_GRAPHICS_FUNCTION(void, EndRecordingCmds, VkCommandBuffer cmds);

    DECLARE_GRAPHICS_FUNCTION(void, BeginRenderPass, VkCommandBuffer cmds, VkRenderPass renderpass, u32 numClearValues, VkClearValue* pClearValues, VkExtent2D extent, VkFramebuffer framebuffer); // TODO(james): pass render targets too
    DECLARE_GRAPHICS_FUNCTION(void, EndRenderPass, VkCommandBuffer cmds);
    DECLARE_GRAPHICS_FUNCTION(void, BindPipeline, VkCommandBuffer cmds, VkPipeline pipeline); // Allow for different bind points
    DECLARE_GRAPHICS_FUNCTION(void, BindDescriptorSets, VkCommandBuffer cmds, VkPipelineLayout layout, u32 count, VkDescriptorSet* pDescriptorSets);
    DECLARE_GRAPHICS_FUNCTION(void, BindVertexBuffers, VkCommandBuffer cmds, u32 count, VkBuffer* pBuffers, memory_index* pOffsets);
    DECLARE_GRAPHICS_FUNCTION(void, BindIndexBuffer, VkCommandBuffer cmds, VkBuffer buffer);

    DECLARE_GRAPHICS_FUNCTION(void, DrawIndexed, VkCommandBuffer cmds, u32 indexCount, u32 instanceCount);
};
