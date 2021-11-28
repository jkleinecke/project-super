
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include <vector>
#include <cstdint>
#include <algorithm>

#include "../vulkan/ps_vulkan.cpp"

#include "win32_vulkan.h"

global win32_vulkan_backend g_Win32VulkanBackend = {};

internal
VkSurfaceFormatKHR Win32VbChooseSwapSurfaceFormat(const ps_vulkan_backend& vb) {
    u32 formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(vb.physicalDevice, vb.swap_chain.platform_surface, &formatCount, nullptr);

    std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(vb.physicalDevice, vb.swap_chain.platform_surface, &formatCount, surfaceFormats.data());

    for (const auto& availableFormat : surfaceFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }

    return surfaceFormats[0];
}

internal
VkPresentModeKHR Win32VbChooseSwapPresentMode(const ps_vulkan_backend& vb) {
    u32 presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(vb.physicalDevice, vb.swap_chain.platform_surface, &presentModeCount, nullptr);

    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(vb.physicalDevice, vb.swap_chain.platform_surface, &presentModeCount, presentModes.data());

    // TODO(james): Prefer VK_PRESENT_MODE_FIFO_RELAXED_KHR for the variable refresh rate instead??
    for (const auto& availablePresentMode : presentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {  // similar to triple buffering
            return availablePresentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

internal
VkExtent2D Win32VbChooseSwapExtent(HWND hWnd, const VkSurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    } else {
        RECT rcClient;
        GetClientRect(hWnd, &rcClient);

        VkExtent2D actualExtent = {
            static_cast<uint32_t>(rcClient.right - rcClient.left),
            static_cast<uint32_t>(rcClient.bottom - rcClient.top)
        };

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

extern "C"
void* Win32LoadGraphicsBackend(HINSTANCE hInstance, HWND hWnd)
{
    ASSERT(hInstance);
    ASSERT(hWnd);

    std::vector<const char*> platform_extensions = {
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
        VK_KHR_SURFACE_EXTENSION_NAME
    } ;

    ps_vulkan_backend& vb = g_Win32VulkanBackend.vulkan;
    VkResult result = vbInitialize(vb, &platform_extensions);

    if(result != VK_SUCCESS)
    {
        ASSERT(false);
        return nullptr;
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
            vbDestroy(vb);  // destroy the instance since we failed to create the win32 surface
            return nullptr;
        }

        vb.swap_chain.platform_surface = surface;
    }

    std::vector<const char*> platformDeviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    // TODO(james): Verify that device supports a swap chain?
    result = vbCreateDevice(vb, &platformDeviceExtensions);

    if(result != VK_SUCCESS)
    {
        ASSERT(false);
        vbDestroy(vb);  // destroy the instance since we failed to create the win32 surface
        return nullptr;
    }

    // now setup the swap chain
    // TODO(james): Should ALL of this be done in the platform agnostic backend??
    {
        VkSurfaceCapabilitiesKHR surfaceCaps{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vb.physicalDevice, vb.swap_chain.platform_surface, &surfaceCaps);

        VkSurfaceFormatKHR surfaceFormat = Win32VbChooseSwapSurfaceFormat(vb);
        VkPresentModeKHR presentMode = Win32VbChooseSwapPresentMode(vb);
        VkExtent2D extent = Win32VbChooseSwapExtent(hWnd, surfaceCaps);

        vbCreateSwapChain(vb, surfaceCaps, surfaceFormat, presentMode, extent);
    }

    // just temporary here until we have more framework in place
    result = vbCreateGraphicsPipeline(vb);

    if(result != VK_SUCCESS)
    {
        ASSERT(false);
        vbDestroy(vb);  // destroy the instance since we failed to create the win32 surface
        return nullptr;
    }

    result = vbCreateFramebuffers(vb);

    if(result != VK_SUCCESS)
    {
        ASSERT(false);
        vbDestroy(vb);  // destroy the instance since we failed to create the win32 surface
        return nullptr;
    }

    result = vbCreateCommandPool(vb);

    if(result != VK_SUCCESS)
    {
        ASSERT(false);
        vbDestroy(vb);  // destroy the instance since we failed to create the win32 surface
        return nullptr;
    }

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    result = vkCreateSemaphore(vb.device, &semaphoreInfo, nullptr, &vb.imageAvailableSemaphore);
    if(result != VK_SUCCESS)
    {
        ASSERT(false);
        vbDestroy(vb);  // destroy the instance since we failed to create the win32 surface
        return nullptr;
    }
    result = vkCreateSemaphore(vb.device, &semaphoreInfo, nullptr, &vb.renderFinishedSemaphore);
    if(result != VK_SUCCESS)
    {
        ASSERT(false);
        vbDestroy(vb);  // destroy the instance since we failed to create the win32 surface
        return nullptr;
    }

    return &g_Win32VulkanBackend;
}

extern "C"
void Win32UnloadGraphicsBackend(void* backend_data)
{
    win32_vulkan_backend& graphics = *(win32_vulkan_backend*)backend_data;
    ps_vulkan_backend& vb = graphics.vulkan;

    vkDeviceWaitIdle(vb.device);

    vkDestroySemaphore(vb.device, vb.imageAvailableSemaphore, nullptr);
    vkDestroySemaphore(vb.device, vb.renderFinishedSemaphore, nullptr);

    vbDestroy(graphics.vulkan);

    ZeroStruct(graphics);
}

extern "C"
void Win32GraphicsBeginFrame(void* backend_data)
{
    win32_vulkan_backend& graphics = *(win32_vulkan_backend*)backend_data;
}

extern "C"
void Win32GraphicsEndFrame(void* backend_data)
{
    win32_vulkan_backend& graphics = *(win32_vulkan_backend*)backend_data;
    ps_vulkan_backend& vb = graphics.vulkan;

    u32 imageIndex;
    vkAcquireNextImageKHR(vb.device, vb.swap_chain.handle, UINT64_MAX, vb.imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

    VkSemaphore waitSemaphores[] = { vb.imageAvailableSemaphore };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSemaphore signalSemaphores[] = { vb.renderFinishedSemaphore };

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &vb.command_buffers[imageIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    VkResult result = vkQueueSubmit(vb.q_graphics.handle, 1, &submitInfo, VK_NULL_HANDLE);
    if(DIDFAIL(result))
    {
        LOG_ERROR("Vulkan Submit Error: %X", result);
        ASSERT(false);
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &vb.swap_chain.handle;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr;

    vkQueuePresentKHR(vb.q_present.handle, &presentInfo);

    /*******************************************************************************
    
        BIG TODO: Finish the Rendering and Presentation chapter to run multiple
            graphics frames concurrently and only wait if they are all in flight.
    
    ********************************************************************************/

    vkQueueWaitIdle(vb.q_present.handle);
}