

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

#if 0
struct psg_device { void* native; };
struct psg_framecontext { void* native; };




#define DECLARE_GRAPHICS_FUNCTION(ret, name, ...)   \
    typedef PS_API ret(PS_APICALL* name##Fn)(__VA_ARGS__); \
    name##Fn    name

struct ps_graphics_api
{
    // Platform layer is responsible for managing the swap chain entirely
    psg_device device;
    psg_framecontext frame;

    // Rendering Objects

    DECLARE_GRAPHICS_FUNCTION(void, createFence, psg_device& device, psg_fence* fence);
    DECLARE_GRAPHICS_FUNCTION(void, destroyFence, psg_device& device, psg_fence* fence);
    DECLARE_GRAPHICS_FUNCTION(void, createSemaphore, psg_device& device, psg_semaphore* semaphore);
    DECLARE_GRAPHICS_FUNCTION(void, destroySemaphore, psg_device& device, psg_semaphore* semaphore);
    DECLARE_GRAPHICS_FUNCTION(void, findQueue, psg_device& device, PsgQueueDesc& desc, psg_queue* queue);

    DECLARE_GRAPHICS_FUNCTION(void, createCmdPool, psg_device& device, PsgCmdPoolDesc& desc, psg_cmdpool* cmdpool);
    DECLARE_GRAPHICS_FUNCTION(void, destroyCmdPool, psg_device& device, psg_cmdpool* cmdpool);
    DECLARE_GRAPHICS_FUNCTION(void, createCmd, psg_device& device, PsgCmdDesc& desc, psg_cmd* cmd);
    DECLARE_GRAPHICS_FUNCTION(void, destroyCmd, psg_device& device, psg_cmd* cmd);

    DECLARE_GRAPHICS_FUNCTION(void, createRenderTarget, psg_device& device, PsgRenderTargetDesc& desc, psg_rendertarget* rendertarget);
    DECLARE_GRAPHICS_FUNCTION(void, destroyRenderTarget, psg_device& device, psg_rendertarget* rendertarget);
    DECLARE_GRAPHICS_FUNCTION(void, createSampler, psg_device& device, PsgSamplerDesc& desc, psg_sampler* sampler);
    DECLARE_GRAPHICS_FUNCTION(void, destroySampler, psg_device& device, psg_sampler* sampler);

    DECLARE_GRAPHICS_FUNCTION(void, createPipeline, psg_device& device, PsgPipelineDesc& desc, psg_pipeline* pipeline);
    DECLARE_GRAPHICS_FUNCTION(void, removePipeline, psg_device& device, psg_pipeline* pipeline);
    
    DECLARE_GRAPHICS_FUNCTION(void, createDescriptorSet, psg_device& device, PsgDescriptorSetDesc& desc, psg_descriptorset* descriptorset);
    DECLARE_GRAPHICS_FUNCTION(void, destroyDescriptorSet, psg_device& device, psg_descriptorset* descriptorset);
    DECLARE_GRAPHICS_FUNCTION(void, updateDescriptorSet, u32 index, psg_descriptorset* descriptorset, u32 count, PsgDescriptorSetParam* params);

    // Rendering Commands

    DECLARE_GRAPHICS_FUNCTION(void, resetCmdPool, psg_device& device, psg_cmdpool* cmdpool);
    DECLARE_GRAPHICS_FUNCTION(void, beginCmd, psg_cmd* cmd);
    DECLARE_GRAPHICS_FUNCTION(void, endCmd, psg_cmd* cmd);
    DECLARE_GRAPHICS_FUNCTION(void, cmdBindRenderTargets, psg_cmd* cmd, u32 rendertarget_count, psg_rendertarget* rendertargets, psg_rendertarget* depthstencil, PsgLoadActionsDesc& loadActions);
    DECLARE_GRAPHICS_FUNCTION(void, cmdSetViewport);
    DECLARE_GRAPHICS_FUNCTION(void, cmdSetScissor);
    DECLARE_GRAPHICS_FUNCTION(void, cmdSetStencilRefValue);
    DECLARE_GRAPHICS_FUNCTION(void, cmdBindPipeline);
    DECLARE_GRAPHICS_FUNCTION(void, cmdBindDescriptorSet);
    DECLARE_GRAPHICS_FUNCTION(void, cmdBindPushConstants);
    DECLARE_GRAPHICS_FUNCTION(void, cmdBindIndexBuffer);
    DECLARE_GRAPHICS_FUNCTION(void, cmdBindVertexBuffers);
    DECLARE_GRAPHICS_FUNCTION(void, cmdDraw);
    DECLARE_GRAPHICS_FUNCTION(void, cmdDrawInstanced);
    DECLARE_GRAPHICS_FUNCTION(void, cmdDrawIndexed);
    DECLARE_GRAPHICS_FUNCTION(void, cmdDrawIndexedInstanced);
    DECLARE_GRAPHICS_FUNCTION(void, cmdDispatch);
    DECLARE_GRAPHICS_FUNCTION(void, cmdResourceBarrier);
    
    DECLARE_GRAPHICS_FUNCTION(void, acquireNextImage);
    DECLARE_GRAPHICS_FUNCTION(void, queueSubmit);
    DECLARE_GRAPHICS_FUNCTION(void, queuePresent);
    DECLARE_GRAPHICS_FUNCTION(void, waitForQueueIdle);
    DECLARE_GRAPHICS_FUNCTION(void, waitForFences);
    



    // Resource Managament Functions

    DECLARE_GRAPHICS_FUNCTION(void, waitForToken, psg_device& device, const psg_synctoken* token);
    DECLARE_GRAPHICS_FUNCTION(void, addResources, psg_device& device, PsgAddResourcesDesc* desc, psg_synctoken* token);
    DECLARE_GRAPHICS_FUNCTION(void, removeResources, psg_device& device, u32 numResources, psg_resource* pResources); 
    DELCARE_GRAPHICS_FUNCTION(void, updateResource, psg_device& device, PsgUpdateResourceDesc* desc, psg_synctoken* token);
    // TODO(james): add map/unmap resource?

};

#endif