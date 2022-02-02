
#if PROJECTSUPER_WIN32
    #define VK_USE_PLATFORM_WIN32_KHR
    #define VG_PLATFORM_SURFACE_EXT_NAME VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#elif PROJECTSUPER_MACOS
    #define VK_USE_PLATFORM_METAL_EXT
    #define VG_PLATFORM_SURFACE_EXT_NAME VK_EXT_METAL_SURFACE_EXTENSION_NAME
#else
    NotImplemented
#endif

#include <vulkan/vulkan.h>
#include <SPIRV-Reflect/spirv_reflect.h>

#include <vector>
#include <cstdint>
#include <algorithm>

#include <VulkanMemoryAllocator/include/vk_mem_alloc.h>

#include "../ps_image.h"            // this may not be the right place for this include
#include "../ps_render.h"
#include "vk_device.cpp"
#include "vk_platform.h"

#include <SPIRV-Reflect/spirv_reflect.c>

/*******************************************************************************

    TODO List:

    - Integrate vulkan memory allocator or build one
        - logical resource memory groups?
    - Use volk to load vulkan functions direct from the driver
    - Asset resource creation
        - buffers
        - textures? maybe just use materials
        - shaders?
    - Material system
        - material description
        - material instance
    - Background upload of texture & buffer memory
        - staging buffer?
    - Integrate Shaderc for live compiling
    - Move to GPU driven rendering

    - Break out into a DLL

********************************************************************************/

global vg_backend g_VulkanBackend = {};

internal render_sync_token
PlatformAddResourceOperation(render_resource_queue* queue, RenderResourceOpType operationType, render_manifest* manifest)
{
    render_sync_token syncToken = 0;

    {    
        // TODO(james): switch to InterlockedCompareExchange() so any thread can add a resource operation
        u32 writeIndex = queue->writeIndex;
        u32 nextWriteIndex = (queue->writeIndex + 1) % ARRAY_COUNT(queue->resourceOps);
        ASSERT(nextWriteIndex != queue->readIndex);

        u32 index = AtomicCompareExchangeUInt32(&queue->writeIndex, nextWriteIndex, writeIndex);
        if(index == writeIndex)
        {
            render_resource_op* op = queue->resourceOps + index;
            op->type = operationType;
            op->manifest = manifest;

            syncToken = AtomicIncrementU64(&queue->requestedSyncToken);
        }
        else
        {   
            // TODO(james): Setup a loop here so that we can ensure that the resource operation is always added
            InvalidCodePath;
        }
    }

    
    // TODO(james): Move this part to a different thread
    {
        // NOTE(james): assumes that this queue works on the main vulkan device
        u32 readIndex = queue->readIndex;
        u32 nextReadIndex = (readIndex + 1) % ARRAY_COUNT(queue->resourceOps);
        if(readIndex != queue->writeIndex)
        {
            u32 index = AtomicCompareExchangeUInt32(&queue->readIndex, nextReadIndex, readIndex);

            if(index == readIndex)
            {
                render_resource_op& operation = queue->resourceOps[index];
                vgPerformResourceOperation(g_VulkanBackend.device, operation.type, operation.manifest);
                AtomicIncrementU64(&queue->currentSyncToken);
            }
        }
    }

    return syncToken;
}

internal b32
PlatformIsResourceOperationComplete(render_resource_queue* queue, render_sync_token syncToken)
{
    // TODO(james): account for a wrapping 64-bit integer <-- how "correct" do we really need to be here?
    return syncToken <= queue->currentSyncToken;
}

extern "C"
LOAD_GRAPHICS_BACKEND(platform_load_graphics_backend)
{
    std::vector<const char*> platform_extensions = {
        VG_PLATFORM_SURFACE_EXT_NAME,
        VK_KHR_SURFACE_EXTENSION_NAME
    } ;

    vg_backend& vb = g_VulkanBackend;
    VkResult result = vgInitialize(vb, &platform_extensions);

    if(result != VK_SUCCESS)
    {
        ASSERT(false);
    }

    // Go ahead and create a window surface buffer      
    {  
        VkSurfaceKHR surface = nullptr;
#if PROJECTSUPER_WIN32
        
        VkWin32SurfaceCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        createInfo.hwnd = hWnd;
        createInfo.hinstance = hInstance;  

        result = vkCreateWin32SurfaceKHR(vb.instance, &createInfo, nullptr, &surface);

        if(result != VK_SUCCESS)
        {
            ASSERT(false);
            vgDestroy(vb);  // destroy the instance since we failed to create the win32 surface
        }
#elif PROJECTSUPER_MACOS
        
        VkMetalSurfaceCreateInfoEXT createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
        createInfo.pLayer = pMetalLayer; // CAMetalLayer

        result = vkCreateMetalSurfaceEXT(vb.instance, &createInfo, nullptr, &surface);
#else
        NotImplemented
#endif

        std::vector<const char*> platformDeviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };

        // TODO(james): Verify that device supports a swap chain?
        result = vgCreateDevice(vb, surface, &platformDeviceExtensions);

        if(result != VK_SUCCESS)
        {
            ASSERT(false);
            vgDestroy(vb);  // destroy the instance since we failed to create the win32 surface
        }

        // TODO(james): Tune these limits
        vb.device.resourceHeaps = hashtable_create(vb.device.arena, vg_resourceheap*, 32); // TODO(james): tune this to the actual application

        // NOTE(james): default resource heap is always at key 0
        vb.device.resourceHeaps->set(0, vgAllocateResourceHeap());
        vb.device.encoderPools = hashtable_create(vb.device.arena, vg_command_encoder_pool*, 32); // TODO(james): tune this to the actual application

        // TODO(james): Change swap chain creation to create the framebuffers and add them to the runtime lookup
        // At runtime the backend will allocate both framebuffers and renderpasses as required, so just
        // passing a valid rendertargetview for each swap chain image will either create them OR lookup one that was
        // already created.
        vb.device.mapRenderpasses = hashtable_create(vb.device.arena, vg_renderpass*, 128); // TODO(james): also tune these...
        vb.device.mapFramebuffers = hashtable_create(vb.device.arena, vg_framebuffer*, 128);

        // TODO(james) figure out how to put these into a freelist...
        // ZeroStruct(vb.device.renderpass_freelist_sentinal);
        // ZeroStruct(vb.device.framebuffer_freelist_sentinal);
    }

    // now setup the swap chain
    {

#if PROJECTSUPER_WIN32
        RECT rc;
        GetClientRect(hWnd, &rc);

        u32 width = rc.right - rc.left;
        u32 height = rc.bottom - rc.top;
#elif PROJECTSUPER_MACOS
        // TODO(james): pull these dynamically
        u32 width = (u32)windowWidth;
        u32 height = (u32)windowHeight;
#else
        NotImplemented
#endif

        VkPresentModeKHR presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
#if PROJECTSUPER_SLOW
        presentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
#endif
        vgCreateSwapChain(vb.device, VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, presentMode, width, height);
    }

    // Setup device caches and allocators
    result = vgInitializeMemory(vb.device);

    // just temporary here until we have more framework in place
    result = vgCreateScreenRenderPass(vb.device);

    if(result != VK_SUCCESS)
    {
        ASSERT(false);
        vgDestroy(vb);  // destroy the instance since we failed to create the win32 surface
    }

    result = vgCreateDepthResources(vb.device);

    if(result != VK_SUCCESS)
    {
        ASSERT(false);
        vgDestroy(vb);  // destroy the instance since we failed to create the win32 surface
    }
    
    result = vgCreateFramebuffers(vb.device);

    if(result != VK_SUCCESS)
    {
        ASSERT(false);
        vgDestroy(vb);  // destroy the instance since we failed to create the win32 surface
    }

    result = vgCreateCommandPool(vb.device);

    if(result != VK_SUCCESS)
    {
        ASSERT(false);
        vgDestroy(vb);  // destroy the instance since we failed to create the win32 surface
    }

    vgCreateStandardBuffers(vb.device);

    ps_graphics_backend backend = {};
    backend.instance = &g_VulkanBackend;

    // WINDOWS SPECIFIC
    // TODO(james): Setup resource operation thread
    //backend.resourceQueue.semaphore = CreateSemaphore(0, 0, 1, 0);    // Only 1 resource operation thread will be active
    // ----------------

    backend.api.BeginFrame = &VulkanGraphicsBeginFrame;
    backend.api.EndFrame = &VulkanGraphicsEndFrame;
    backend.api.AddResourceOperation = &PlatformAddResourceOperation;
    backend.api.IsResourceOperationComplete = &PlatformIsResourceOperationComplete;
    
    //vgLoadApi(g_VulkanBackend, api.graphics);

    return backend;
}

// internal void
// Win32RecreateSwapChain(win32_vulkan_backend &graphics)
// {
    // TODO(james): Still need to handle window resize events better.. explicit hook to handle resizing?

    // vg_backend& vb = graphics.vulkan;

    // vkDeviceWaitIdle(vb.device);

    // vbDestroySwapChain(vb);

    //  // TODO(james): Should ALL of this be done in the platform agnostic backend??
    // {
    //     VkSurfaceCapabilitiesKHR surfaceCaps{};
    //     vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vb.physicalDevice, vb.swap_chain.platform_surface, &surfaceCaps);

    //     VkSurfaceFormatKHR surfaceFormat = Win32VbChooseSwapSurfaceFormat(vb);
    //     VkPresentModeKHR presentMode = Win32VbChooseSwapPresentMode(vb);
    //     VkExtent2D extent = Win32VbChooseSwapExtent(graphics.window_handle, surfaceCaps);

    //     vbCreateSwapChain(vb, surfaceCaps, surfaceFormat, presentMode, extent);
    // }

    // // just temporary here until we have more framework in place
    // VkResult result = vbCreateGraphicsPipeline(vb);

    // if(result != VK_SUCCESS)
    // {
    //     ASSERT(false);
    // }

    // result = vbCreateFramebuffers(vb);

    // if(result != VK_SUCCESS)
    // {
    //     ASSERT(false);
    // }
// }

extern "C"
UNLOAD_GRAPHICS_BACKEND(platform_unload_graphics_backend)
{
    vkDeviceWaitIdle(backend->instance->device.handle);

    vgDestroy(*backend->instance);

    ZeroStruct(*backend->instance);
}
