
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

    ASSERT(hInstance);
    ASSERT(hWnd);

    vg_backend& vb = g_Win32VulkanBackend.vulkan;
    VkResult result = vgInitialize(vb, &platform_extensions);

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
            vgDestroy(vb);  // destroy the instance since we failed to create the win32 surface
            return nullptr;
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
            return nullptr;
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

    // just temporary here until we have more framework in place
    result = vgCreateGraphicsPipeline(vb.device);

    if(result != VK_SUCCESS)
    {
        ASSERT(false);
        vgDestroy(vb);  // destroy the instance since we failed to create the win32 surface
        return nullptr;
    }

    result = vgCreateDepthResources(vb.device);

    if(result != VK_SUCCESS)
    {
        ASSERT(false);
        vgDestroy(vb);  // destroy the instance since we failed to create the win32 surface
        return nullptr;
    }
    
    result = vgCreateFramebuffers(vb.device);

    if(result != VK_SUCCESS)
    {
        ASSERT(false);
        vgDestroy(vb);  // destroy the instance since we failed to create the win32 surface
        return nullptr;
    }

    result = vgCreateCommandPool(vb.device);

    if(result != VK_SUCCESS)
    {
        ASSERT(false);
        vgDestroy(vb);  // destroy the instance since we failed to create the win32 surface
        return nullptr;
    }

    return &g_Win32VulkanBackend;
}

internal void
Win32RecreateSwapChain(win32_vulkan_backend &graphics)
{
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
}

extern "C"
void Win32UnloadGraphicsBackend(void* backend_data)
{
    win32_vulkan_backend& graphics = *(win32_vulkan_backend*)backend_data;
    vg_backend& vb = graphics.vulkan;

    vkDeviceWaitIdle(vb.device.handle);

    vgDestroy(graphics.vulkan);

    ZeroStruct(graphics);
}

extern "C"
void Win32GraphicsBeginFrame(void* backend_data)
{
    win32_vulkan_backend& graphics = *(win32_vulkan_backend*)backend_data;
    vg_device& device = graphics.vulkan.device;

    device.pPrevFrame = device.pCurFrame;
    device.pCurFrame = &device.frames[device.currentFrameIndex];
}

extern "C"
void Win32GraphicsEndFrame(void* backend_data, GameClock& clock)
{
    win32_vulkan_backend& graphics = *(win32_vulkan_backend*)backend_data;
    vg_device& device = graphics.vulkan.device;
 
    // wait fence here...
    VkResult result = vkWaitForFences(device.handle, 1, &device.pCurFrame->renderFence, VK_TRUE, UINT64_MAX);
    u32 imageIndex = 0;
    result = vkAcquireNextImageKHR(device.handle, device.swapChain, UINT64_MAX, device.pCurFrame->presentSemaphore, VK_NULL_HANDLE, &imageIndex);

    if(result == VK_ERROR_OUT_OF_DATE_KHR) 
    {
        Win32RecreateSwapChain(graphics);
    }
    else if(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        // welp, can't render anything so just quit
        return;
    }

    
    VkSemaphore waitSemaphores[] = { device.pCurFrame->presentSemaphore };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSemaphore signalSemaphores[] = { device.pCurFrame->renderSemaphore };

    // update the g_UBO with the latest matrices
    {
        local_persist f32 accumlated_elapsedFrameTime = 0.0f;

        //accumlated_elapsedFrameTime += clock.elapsedFrameTime;

        UniformBufferObject ubo{};
        // Rotates 90 degrees a second
        ubo.model = Rotate(accumlated_elapsedFrameTime * 90.0f, Vec3(0.0f, 0.0f, 1.0f));
        ubo.view = LookAt(Vec3(4.0f, 4.0f, 4.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f));
        ubo.proj = Perspective(45.0f, (f32)device.extent.width, (f32)device.extent.height, 0.1f, 10.0f);
        //ubo.proj.Elements[1][1] *= -1;

        void* data;
        vkMapMemory(device.handle, device.pCurFrame->camera_buffer.memory, 0, sizeof(ubo), 0, &data);
            Copy(sizeof(ubo), &ubo, data);
        vkUnmapMemory(device.handle, device.pCurFrame->camera_buffer.memory);
    }

    vkResetCommandBuffer(device.pCurFrame->commandBuffer, 0);
    vgTempBuildRenderCommands(device, imageIndex);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &device.pCurFrame->commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    vkResetFences(device.handle, 1, &device.pCurFrame->renderFence);
    result = vkQueueSubmit(device.q_graphics.handle, 1, &submitInfo, device.pCurFrame->renderFence);
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
    presentInfo.pSwapchains = &device.swapChain;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr;

    result = vkQueuePresentKHR(device.q_present.handle, &presentInfo);

    if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        Win32RecreateSwapChain(graphics);
    }
    else if(DIDFAIL(result))
    {
        LOG_ERROR("Vulkan Present Error: %X", result);
        ASSERT(false);
    }

    device.currentFrameIndex = (device.currentFrameIndex + 1) % FRAME_OVERLAP;
}