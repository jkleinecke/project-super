
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include <vector>
#include <cstdint>
#include <algorithm>

#include "../ps_image.h"            // this may not be the right place for this include
#include "vk_device.cpp"
#include "vk_platform.h"


global vg_backend g_VulkanBackend = {};
global const int WIN32_MAX_FRAMES_IN_FLIGHT = 2;

extern "C"
LOAD_GRAPHICS_BACKEND(platform_load_graphics_backend)
{
    ASSERT(hInstance);
    ASSERT(hWnd);

    std::vector<const char*> platform_extensions = {
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
        VK_KHR_SURFACE_EXTENSION_NAME
    } ;

    ASSERT(hInstance);
    ASSERT(hWnd);

    vg_backend& vb = g_VulkanBackend;
    VkResult result = vgInitialize(vb, &platform_extensions);

    if(result != VK_SUCCESS)
    {
        ASSERT(false);
    }

    // Go ahead and create a window surface buffer      
    {  
        VkSurfaceKHR surface = nullptr;
        
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
    }

    // now setup the swap chain
    // TODO(james): Should ALL of this be done in the platform agnostic backend??
    {
        RECT rc;
        GetClientRect(hWnd, &rc);

        u32 width = rc.right - rc.left;
        u32 height = rc.bottom - rc.top;

        VkPresentModeKHR presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
#if defined(PROJECTSUPER_SLOW)
        presentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
#endif
        vgCreateSwapChain(vb.device, VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, presentMode, width, height);
    }

    // Setup device caches and allocators
    result = vgInitializeMemory(vb.device);

    // just temporary here until we have more framework in place
    result = vgCreateGraphicsPipeline(vb.device);

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

    ///////////////////////////////////
    // Temporary Model Loading
    vgTempLoadModel(vb.device);
    ///////////////////////////////////


    ///////////////////////////////////
    // Temporary Vertex Buffer Creation
    result = vgTempCreateVertexBuffers(vb.device);
    ///////////////////////////////////

    ///////////////////////////////////
    // Temporarty Index Buffer Creation
    result = vgTempCreateIndexBuffers(vb.device);
    ///////////////////////////////////

    ///////////////////////////////////
    // Temporarty Uniform Buffer Creation
    result = vgTempCreateUniformBuffers(vb.device);
    ///////////////////////////////////

    ///////////////////////////////////
    // Temporary Image Creation
    result = vgTempCreateTextureImages(vb.device);
    ///////////////////////////////////

    ///////////////////////////////////
    // Temporary Image View Creation
    result = vgTempCreateTextureImageViews(vb.device);
    ///////////////////////////////////

    ///////////////////////////////////
    // Temporary Sampler Creation
    result = vgTempCreateTextureSamplers(vb.device);
    ///////////////////////////////////

    //result = vgCreateDescriptorSets(vb.device);
    if(DIDFAIL(result))
    {
        LOG_ERROR("Vulkan Error: %X", (result));
        ASSERT(false);
    }

    ps_graphics_backend_api api = {};
    api.instance = &g_VulkanBackend;
    api.BeginFrame = &VulkanGraphicsBeginFrame;
    api.EndFrame = &VulkanGraphicsEndFrame;

    return api;
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
    vkDeviceWaitIdle(backend->device.handle);

    vgDestroy(*backend);

    ZeroStruct(*backend);
}
