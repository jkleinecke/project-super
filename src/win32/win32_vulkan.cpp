
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include <vector>
#include <cstdint>
#include <algorithm>

#include "../ps_image.h"            // this may not be the right place for this include
#include "../vulkan/ps_vulkan.cpp"

#include "win32_vulkan.h"

global win32_vulkan_backend g_Win32VulkanBackend = {};
global const int WIN32_MAX_FRAMES_IN_FLIGHT = 2;

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

    g_Win32VulkanBackend.window_handle = hWnd;

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

    result = vbCreateDepthResources(vb);

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

    vb.imageAvailableSemaphores.resize(WIN32_MAX_FRAMES_IN_FLIGHT);
    vb.renderFinishedSemaphores.resize(WIN32_MAX_FRAMES_IN_FLIGHT);
    vb.inFlightFences.resize(WIN32_MAX_FRAMES_IN_FLIGHT);
    vb.imagesInFlight.resize(vb.swap_chain.images.size(), VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for(int i = 0; i < WIN32_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        VkResult imageAvailableResult = vkCreateSemaphore(vb.device, &semaphoreInfo, nullptr, &vb.imageAvailableSemaphores[i]);
        VkResult renderFinishedResult = vkCreateSemaphore(vb.device, &semaphoreInfo, nullptr, &vb.renderFinishedSemaphores[i]);
        VkResult fenceResult = vkCreateFence(vb.device, &fenceInfo, nullptr, &vb.inFlightFences[i]);

        if(imageAvailableResult != VK_SUCCESS || renderFinishedResult != VK_SUCCESS || fenceResult != VK_SUCCESS)
        {
            ASSERT(false);
            vbDestroy(vb);
            return nullptr;
        }
    }

    return &g_Win32VulkanBackend;
}

internal void
Win32RecreateSwapChain(win32_vulkan_backend &graphics)
{
    // TODO(james): Still need to handle window resize events better.. explicit hook to handle resizing?

    ps_vulkan_backend& vb = graphics.vulkan;

    vkDeviceWaitIdle(vb.device);

    vbDestroySwapChain(vb);

     // TODO(james): Should ALL of this be done in the platform agnostic backend??
    {
        VkSurfaceCapabilitiesKHR surfaceCaps{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vb.physicalDevice, vb.swap_chain.platform_surface, &surfaceCaps);

        VkSurfaceFormatKHR surfaceFormat = Win32VbChooseSwapSurfaceFormat(vb);
        VkPresentModeKHR presentMode = Win32VbChooseSwapPresentMode(vb);
        VkExtent2D extent = Win32VbChooseSwapExtent(graphics.window_handle, surfaceCaps);

        vbCreateSwapChain(vb, surfaceCaps, surfaceFormat, presentMode, extent);
    }

    // just temporary here until we have more framework in place
    VkResult result = vbCreateGraphicsPipeline(vb);

    if(result != VK_SUCCESS)
    {
        ASSERT(false);
    }

    result = vbCreateFramebuffers(vb);

    if(result != VK_SUCCESS)
    {
        ASSERT(false);
    }
}

extern "C"
void Win32UnloadGraphicsBackend(void* backend_data)
{
    win32_vulkan_backend& graphics = *(win32_vulkan_backend*)backend_data;
    ps_vulkan_backend& vb = graphics.vulkan;

    vkDeviceWaitIdle(vb.device);

    for(int i = 0; i < WIN32_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        vkDestroySemaphore(vb.device, vb.imageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(vb.device, vb.renderFinishedSemaphores[i], nullptr);
        vkDestroyFence(vb.device, vb.inFlightFences[i], nullptr);
    }

    vbDestroy(graphics.vulkan);

    ZeroStruct(graphics);
}

extern "C"
void Win32GraphicsBeginFrame(void* backend_data)
{
    win32_vulkan_backend& graphics = *(win32_vulkan_backend*)backend_data;
}

extern "C"
void Win32GraphicsEndFrame(void* backend_data, GameClock& clock)
{
    win32_vulkan_backend& graphics = *(win32_vulkan_backend*)backend_data;
    ps_vulkan_backend& vb = graphics.vulkan;

    vkWaitForFences(vb.device, 1, &vb.inFlightFences[vb.currentFrameIndex], VK_TRUE, UINT64_MAX);

    u32 imageIndex;
    VkResult result = vkAcquireNextImageKHR(vb.device, vb.swap_chain.handle, UINT64_MAX, vb.imageAvailableSemaphores[vb.currentFrameIndex], VK_NULL_HANDLE, &imageIndex);

    if(result == VK_ERROR_OUT_OF_DATE_KHR) 
    {
        Win32RecreateSwapChain(graphics);
    }
    else if(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        // welp, can't render anything so just quit
        return;
    }

    // Check if a previous frame is this image (i.e. the fence isn't null and needs to be waited on)
    if(vb.imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(vb.device, 1, &vb.imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }

    // mark the image as now being in use by this frame
    vb.imagesInFlight[imageIndex] = vb.inFlightFences[vb.currentFrameIndex];

    VkSemaphore waitSemaphores[] = { vb.imageAvailableSemaphores[vb.currentFrameIndex] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSemaphore signalSemaphores[] = { vb.renderFinishedSemaphores[vb.currentFrameIndex] };

    // update the g_UBO with the latest matrices
    {
        local_persist f32 accumlated_elapsedFrameTime = 0.0f;

        accumlated_elapsedFrameTime += clock.elapsedFrameTime;

        UniformBufferObject ubo{};
        // Rotates 90 degrees a second
        ubo.model = Rotate(accumlated_elapsedFrameTime * 90.0f, Vec3(0.0f, 0.0f, 1.0f));
        ubo.view = LookAt(Vec3(2.0f, 2.0f, 2.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f));
        ubo.proj = Perspective(45.0f, (f32)vb.swap_chain.extent.width, (f32)vb.swap_chain.extent.height, 0.1f, 10.0f);
        //ubo.proj.Elements[1][1] *= -1;

        void* data;
        vkMapMemory(vb.device, vb.uniform_buffers[imageIndex].memory_handle, 0, sizeof(ubo), 0, &data);
            Copy(sizeof(ubo), &ubo, data);
        vkUnmapMemory(vb.device, vb.uniform_buffers[imageIndex].memory_handle);
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &vb.command_buffers[imageIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    vkResetFences(vb.device, 1, &vb.inFlightFences[vb.currentFrameIndex]);
    result = vkQueueSubmit(vb.q_graphics.handle, 1, &submitInfo, vb.inFlightFences[vb.currentFrameIndex]);
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

    result = vkQueuePresentKHR(vb.q_present.handle, &presentInfo);

    if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        Win32RecreateSwapChain(graphics);
    }
    else if(DIDFAIL(result))
    {
        LOG_ERROR("Vulkan Present Error: %X", result);
        ASSERT(false);
    }

    vb.currentFrameIndex = (vb.currentFrameIndex + 1) % WIN32_MAX_FRAMES_IN_FLIGHT;
}