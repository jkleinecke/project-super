
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


global vg_backend g_VulkanBackend = {};

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
        vb.device.frameArena = BootstrapScratchArena("VkDeviceFrameArena", NonRestoredArena(Megabytes(1)));
        vb.device.frameTemp = BeginTemporaryMemory(*vb.device.frameArena);
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

        // initially there a no objects in the freelist
        vb.device.freelist_descriptorPool = 0;
        vb.device.freelist_renderpass = 0;
        vb.device.freelist_framebuffer = 0;
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

    ps_graphics_backend backend = {};
    backend.instance = &g_VulkanBackend;
    backend.width = vb.device.extent.width;
    backend.height = vb.device.extent.height;
    // ----------------
    backend.gfx.device = DeviceObject::To(g_VulkanBackend.device);
    backend.gfx.CreateResourceHeap = CreateResourceHeap;
    backend.gfx.DestroyResourceHeap = DestroyResourceHeap;
    backend.gfx.CreateBuffer = CreateBuffer;
    backend.gfx.DestroyBuffer = DestroyBuffer;
    backend.gfx.GetBufferData = GetBufferData;
    backend.gfx.CreateTexture = CreateTexture;
    backend.gfx.DestroyTexture = DestroyTexture;
    backend.gfx.CreateSampler = CreateSampler;
    backend.gfx.DestroySampler = DestroySampler;
    backend.gfx.CreateProgram = CreateProgram;
    backend.gfx.DestroyProgram = DestroyProgram;
    backend.gfx.CreateRenderTarget = CreateRenderTarget;
    backend.gfx.DestroyRenderTarget = DestroyRenderTarget;
    backend.gfx.GetDeviceBackBufferFormat = GetDeviceBackBufferFormat;
    backend.gfx.CreateComputeKernel = CreateComputeKernel;
    backend.gfx.CreateGraphicsKernel = CreateGraphicsKernel;
    backend.gfx.DestroyKernel = DestroyKernel;
    backend.gfx.CreateEncoderPool = CreateEncoderPool;
    backend.gfx.DestroyCmdEncoderPool = DestroyCmdEncoderPool;
    backend.gfx.CreateEncoderContext = CreateEncoderContext;
    backend.gfx.CreateEncoderContexts = CreateEncoderContexts;
    backend.gfx.ResetCmdEncoderPool = ResetCmdEncoderPool;
    backend.gfx.BeginEncodingCmds = BeginEncodingCmds;
    backend.gfx.EndEncodingCmds = EndEncodingCmds;
    backend.gfx.CmdResourceBarrier = CmdResourceBarrier;
    backend.gfx.CmdCopyBuffer = CmdCopyBuffer;
    backend.gfx.CmdCopyBufferRange = CmdCopyBufferRange;
    backend.gfx.CmdClearBuffer = CmdClearBuffer;
    backend.gfx.CmdUpdateBuffer = CmdUpdateBuffer;
    backend.gfx.CmdClearTexture = CmdClearTexture;
    backend.gfx.CmdCopyTexture = CmdCopyTexture;
    backend.gfx.CmdClearImage = CmdClearImage;
    backend.gfx.CmdClearRenderTarget = CmdClearRenderTarget;
    backend.gfx.CmdCopyBufferToTexture = CmdCopyBufferToTexture;
    backend.gfx.CmdGenerateMips = CmdGenerateMips;
    backend.gfx.CmdBindRenderTargets = CmdBindRenderTargets;
    backend.gfx.CmdBindKernel = CmdBindKernel;
    backend.gfx.CmdBindIndexBuffer = CmdBindIndexBuffer;
    backend.gfx.CmdBindVertexBuffer = CmdBindVertexBuffer;
    backend.gfx.CmdBindDescriptorSet = CmdBindDescriptorSet;
    backend.gfx.CmdBindPushConstant = CmdBindPushConstant;
    backend.gfx.CmdSetViewport = CmdSetViewport;
    backend.gfx.CmdSetScissorRect = CmdSetScissorRect;
    backend.gfx.CmdDraw = CmdDraw;
    backend.gfx.CmdDrawIndexed = CmdDrawIndexed;
    backend.gfx.CmdDispatch = CmdDispatch;
    backend.gfx.CmdDispatchIndirect = CmdDispatchIndirect;
    backend.gfx.AcquireNextSwapChainTarget = AcquireNextSwapChainTarget;
    backend.gfx.SubmitCommands = SubmitCommands;
    backend.gfx.Frame = Frame;
    backend.gfx.Finish = Finish;
    backend.gfx.CleanupUnusedRenderingResources = CleanupUnusedRenderingResources;
    backend.gfx.CreateTimestampQuery = CreateTimestampQuery;
    backend.gfx.DestroyTimestampQuery = DestroyTimestampQuery;
    backend.gfx.GetTimestampQueryDuration = GetTimestampQueryDuration;
    backend.gfx.BeginTimestampQuery = BeginTimestampQuery;
    backend.gfx.EndTimestampQuery = EndTimestampQuery;
    backend.gfx.BeginEvent = BeginEvent;
    backend.gfx.BeginColorEvent = BeginColorEvent;
    backend.gfx.EndEvent = EndEvent;

    return backend;
}

extern "C"
UNLOAD_GRAPHICS_BACKEND(platform_unload_graphics_backend)
{
    vkDeviceWaitIdle(backend->instance->device.handle);

    vgDestroy(*backend->instance);

    // release all the graphics memory arenas since it's possible that
    // the graphics driver is just being unload and a new one will
    // be loaded soon
    EndTemporaryMemory(backend->instance->device.frameTemp);
    Clear(*(backend->instance->device.frameArena));
    Clear(backend->instance->device.arena);

    ZeroStruct(*backend->instance);
}
