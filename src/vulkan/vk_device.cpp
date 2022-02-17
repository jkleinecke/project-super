
#include <vulkan/vulkan.h>

#include <SPIRV-Reflect/spirv_reflect.h>
#include "vk_device.h"

#include "vk_extensions.h"
#include "vk_initializers.cpp"
//#include "vk_descriptor.cpp"

#include <vector>
#include <algorithm>
#include <array>
#include <unordered_map>

#if defined(PROJECTSUPER_SLOW)
#define GRAPHICS_DEBUG
#endif

// #define VERIFY_SUCCESS(result) ASSERT(DIDSUCCEED(result))
#define DIDFAIL(result) ((result) != VK_SUCCESS)
#define DIDSUCCEED(result) ((result) == VK_SUCCESS)

global const std::vector<const char*> g_validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};
#if defined(GRAPHICS_DEBUG)
    global const bool g_enableValidationLayers = true;
#else
    global const bool g_enableValidationLayers = false;
#endif

// UBOs have alignment requirements depending on the data being uploaded
// so it is always good to be specific about the alignment for a UBO
// struct FrameObject
// {
//     alignas(16) m4 viewProj;
// };

// struct InstanceObject
// {
//     alignas(16) m4 mvp;
// };



// internal SpecialDescriptorBinding
// vgGetSpecialDescriptorBindingFromName(const std::string& name)
// {
//     if(name == "Camera")        return SpecialDescriptorBinding::Camera;
//     else if(name == "Instance") return SpecialDescriptorBinding::Instance;
//     else if(name == "Scene")    return SpecialDescriptorBinding::Scene;
//     else if(name == "Frame")    return SpecialDescriptorBinding::Frame;
//     else if(name == "Light")    return SpecialDescriptorBinding::Light;
//     else if(name == "Material") return SpecialDescriptorBinding::Material;

//     return SpecialDescriptorBinding::Undefined;
// }


// internal i32 vgFindMemoryType(vg_device& device, u32 typeFilter, VkMemoryPropertyFlags properties)
// {
//     VkPhysicalDeviceMemoryProperties& memProperties = device.device_memory_properties;

//     for(u32 i = 0; i < memProperties.memoryTypeCount; ++i)
//     {
//         b32x bValidMemoryType = typeFilter & (1 << i);
//         bValidMemoryType = bValidMemoryType && (memProperties.memoryTypes[i].propertyFlags & properties) == properties;
//         if(bValidMemoryType)
//         {
//             return i;
//         }
//     }

//     return -1;
// }

// internal VkResult vgCreateBuffer(vg_device& device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, vg_buffer* pBuffer)
// {
//     VkBufferCreateInfo bufferInfo{};
//     bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
//     bufferInfo.size = size;
//     bufferInfo.usage = usage;
//     bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

//     VkResult result = vkCreateBuffer(device.handle, &bufferInfo, nullptr, &pBuffer->handle);
//     if(DIDFAIL(result))
//     {
//         ASSERT(false);
//         return result;
//     }

//     VkMemoryRequirements memRequirements;
//     vkGetBufferMemoryRequirements(device.handle, pBuffer->handle, &memRequirements);

//     i32 memoryIndex = vgFindMemoryType(device, memRequirements.memoryTypeBits, properties);
//     if(memoryIndex < 0)
//     {
//         // failed
//         ASSERT(false);
//         return VK_ERROR_MEMORY_MAP_FAILED;  // seems like a decent fit for what happened
//     }
    
//     // TODO(james): We should really allocate several large chunks and use a custom allocator for this
//     VkMemoryAllocateInfo allocInfo{};
//     allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
//     allocInfo.allocationSize = memRequirements.size;
//     allocInfo.memoryTypeIndex = memoryIndex;

//     // result = vkAllocateMemory(device.handle, &allocInfo, nullptr, &pBuffer->memory);
//     // if(DIDFAIL(result))
//     // {
//     //     ASSERT(false);
//     //     return result;
//     // }

//     // vkBindBufferMemory(device.handle, pBuffer->handle, pBuffer->memory, 0);

//     return VK_SUCCESS;
// }

// internal inline void
// vgMapBuffer(VkDevice device, vg_buffer& buffer, umm offset, umm size)
// {
//     // vkMapMemory(device, buffer.memory, offset, size, 0, &buffer.mapped);  
// }

// internal inline void
// vgUnmapBuffer(VkDevice device, vg_buffer& buffer)
// {
//     if(buffer.mapped)
//     {
//         // vkUnmapMemory(device, buffer.memory);
//         buffer.mapped = 0;
//     }
// }

// internal void
// vgDestroyBuffer(VkDevice device, vg_buffer& buffer)
// {
//     vgUnmapBuffer(device, buffer);
//     IFF(buffer.handle, vkDestroyBuffer(device, buffer.handle, nullptr));
//     // IFF(buffer.memory, vkFreeMemory(device, buffer.memory, nullptr));
// }

// ======================================
// TRANSFER BUFFER
// ======================================

// internal void
// vgCreateTransferBuffer(vg_device& device, vg_transfer_buffer* transfer)
// {
//     transfer->device = device.handle;
//     transfer->queue = device.q_graphics.handle;
//     transfer->stagingBufferSize = Megabytes(256);

//     VkCommandPoolCreateInfo poolInfo = vkInit_command_pool_create_info(device.q_graphics.queue_family_index, 0);
//     VkResult result = vkCreateCommandPool(device.handle, &poolInfo, nullptr, &transfer->cmdPool);
//     ASSERT(result == VK_SUCCESS);

//     VkCommandBufferAllocateInfo allocInfo = vkInit_command_buffer_allocate_info(transfer->cmdPool, 1, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
//     vkAllocateCommandBuffers(device.handle, &allocInfo, &transfer->cmds);

//     VkFenceCreateInfo fenceInfo = vkInit_fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
//     vkCreateFence(device.handle, &fenceInfo, nullptr, &transfer->fence);

//     result = vgCreateBuffer(device, transfer->stagingBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &transfer->staging_buffer);
//     ASSERT(result == VK_SUCCESS);

//     vgMapBuffer(device.handle, transfer->staging_buffer, 0, VK_WHOLE_SIZE);
// }

// internal void
// vgDestroyTransferBuffer(vg_transfer_buffer& transfer)
// {
//     vgDestroyBuffer(transfer.device, transfer.staging_buffer);
//     vkDestroyFence(transfer.device, transfer.fence, nullptr);
//     vkFreeCommandBuffers(transfer.device, transfer.cmdPool, 1, &transfer.cmds);
//     vkDestroyCommandPool(transfer.device, transfer.cmdPool, nullptr);
// }

// internal void
// vgBeginDataTransfer(vg_transfer_buffer& transfer)
// {
//     // must wait for any previous data transfers to finish
//     vkWaitForFences(transfer.device, 1, &transfer.fence, VK_TRUE, UINT64_MAX);
//     vkResetFences(transfer.device, 1, &transfer.fence);

//     transfer.lastWritePosition = 0;
//     vkResetCommandPool(transfer.device, transfer.cmdPool, 0);

//     VkCommandBufferBeginInfo beginInfo = vkInit_command_buffer_begin_info(0);
//     vkBeginCommandBuffer(transfer.cmds, &beginInfo);
// }

// internal b32
// vgTransferDataToBuffer(vg_transfer_buffer& transfer, umm size, void* srcdata, vg_buffer& target, umm offset = 0)
// {
//     ASSERT(size < transfer.stagingBufferSize); // NOTE(james): size of the request cannot exceed the size of the staging buffer!
//     b32 roomInStagingBuffer = transfer.lastWritePosition + size < transfer.stagingBufferSize;

//     if(roomInStagingBuffer)
//     {
//         // first copy to the staging buffer
//         void* dstptr = OffsetPtr(transfer.staging_buffer.mapped, transfer.lastWritePosition);
//         Copy(size, srcdata, dstptr);

//         VkBufferCopy copy{};
//         copy.srcOffset = transfer.lastWritePosition;
//         copy.dstOffset = offset;
//         copy.size = size;

//         vkCmdCopyBuffer(transfer.cmds, transfer.staging_buffer.handle, target.handle, 1, &copy);
//         transfer.lastWritePosition += size;
//     }

//     return roomInStagingBuffer;
// }

// internal b32
// vgTransferImageDataToBuffer(vg_transfer_buffer& transfer, f32 width, f32 height, VkFormat format, void* pixels, vg_image& target)
// {
//     umm pixelSizeInBytes = (umm)(width * height * vkInit_GetFormatSize(format));
//     b32 roomInStagingBuffer = transfer.lastWritePosition + pixelSizeInBytes < transfer.stagingBufferSize;
    
//     if(roomInStagingBuffer)
//     {
//         void* dstptr = OffsetPtr(transfer.staging_buffer.mapped, transfer.lastWritePosition);
//         Copy(pixelSizeInBytes, pixels, dstptr);

//         // NOTE(james): Images are weird in that you have to transfer them to the proper format for each stage you use them
//         VkImageMemoryBarrier copyBarrier = vkInit_image_barrier(
//             target.handle, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT
//         );

//         vkCmdPipelineBarrier(transfer.cmds, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,  VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &copyBarrier);

//         VkBufferImageCopy region{};
//         region.bufferOffset = transfer.lastWritePosition;
//         region.bufferRowLength = 0;
//         region.bufferImageHeight = 0;

//         region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//         region.imageSubresource.mipLevel = 0;
//         region.imageSubresource.baseArrayLayer = 0;
//         region.imageSubresource.layerCount = 1;

//         region.imageOffset = {0,0,0};
//         region.imageExtent = { (u32)width, (u32)height, 1 };

//         vkCmdCopyBufferToImage(transfer.cmds, transfer.staging_buffer.handle, target.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

//         transfer.lastWritePosition += pixelSizeInBytes;

//         VkImageMemoryBarrier useBarrier = vkInit_image_barrier(
//             target.handle, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT
//         );

//         vkCmdPipelineBarrier(transfer.cmds, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &useBarrier);
//     }

//     return roomInStagingBuffer;
// }

// internal void
// vgEndDataTransfer(vg_transfer_buffer& transfer)
// {
//     VkBufferMemoryBarrier barrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
//     barrier.buffer = transfer.staging_buffer.handle;
//     barrier.offset = 0;
//     barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
//     barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
//     barrier.size = transfer.lastWritePosition;
//     barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
//     barrier.dstAccessMask = VK_ACCESS_UNIFORM_READ_BIT;

//     // VK_PIPELINE_STAGE_TRANSFER_BIT
//     // VK_PIPELINE_STAGE_VERTEX_INPUT_BIT
//     vkCmdPipelineBarrier(transfer.cmds, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, 0, 0, NULL, 1, &barrier, 0, NULL);

//     vkEndCommandBuffer(transfer.cmds);

//     VkSubmitInfo submitInfo = vkInit_submit_info(&transfer.cmds);
//     vkQueueSubmit(transfer.queue, 1, &submitInfo, transfer.fence);
// }

// ======================================
// ======================================


VkPipelineStageFlags DeterminePipelineStageFlags(vg_device& device, VkAccessFlags accessFlags, GfxQueueType queueType)
{
	VkPipelineStageFlags flags = 0;

	switch (queueType)
	{
		case GfxQueueType::Graphics:
		{
			if ((accessFlags & (VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT)) != 0)
				flags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;

			if ((accessFlags & (VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)) != 0)
			{
				flags |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
				flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                // TODO(james): detect geometry and tessellation support on device
				// if (pRenderer->pActiveGpuSettings->mGeometryShaderSupported)
				// {
				// 	flags |= VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
				// }
				// if (pRenderer->pActiveGpuSettings->mTessellationSupported)
				// {
				// 	flags |= VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT;
				// 	flags |= VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
				// }
				flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

                // TODO(james): enable this part if ray tracing is supported
				// if (pRenderer->mVulkan.mRaytracingExtension)
				// {
				// 	flags |= VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV;
				// }
			}

			if ((accessFlags & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT) != 0)
				flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

			if ((accessFlags & (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)) != 0)
				flags |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

			if ((accessFlags & (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)) != 0)
				flags |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			break;
		}
		case GfxQueueType::Compute:
		{
			if ((accessFlags & (VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT)) != 0 ||
				(accessFlags & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT) != 0 ||
				(accessFlags & (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)) != 0 ||
				(accessFlags & (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)) != 0)
				return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

			if ((accessFlags & (VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)) != 0)
				flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

			break;
		}
		case GfxQueueType::Transfer: return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		InvalidDefaultCase;
	}

	// Compatible with both compute and graphics queues
	if ((accessFlags & VK_ACCESS_INDIRECT_COMMAND_READ_BIT) != 0)
		flags |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;

	if ((accessFlags & (VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT)) != 0)
		flags |= VK_PIPELINE_STAGE_TRANSFER_BIT;

	if ((accessFlags & (VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT)) != 0)
		flags |= VK_PIPELINE_STAGE_HOST_BIT;

	if (flags == 0)
		flags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

	return flags;
}


internal
VkSurfaceFormatKHR vgChooseSwapSurfaceFormat(const vg_device& device, VkFormat preferredFormat, VkColorSpaceKHR preferredColorSpace)
{
    u32 formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device.physicalDevice, device.platform_surface, &formatCount, nullptr);

    std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device.physicalDevice, device.platform_surface, &formatCount, surfaceFormats.data());

    for (const auto& availableFormat : surfaceFormats) {
        if (availableFormat.format == preferredFormat && availableFormat.colorSpace == preferredColorSpace) {
            return availableFormat;
        }
    }

    return surfaceFormats[0];
}

internal
VkPresentModeKHR vgChooseSwapPresentMode(const vg_device& device, VkPresentModeKHR preferredPresentMode) {
    u32 presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device.physicalDevice, device.platform_surface, &presentModeCount, nullptr);

    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device.physicalDevice, device.platform_surface, &presentModeCount, presentModes.data());

    for (const auto& availablePresentMode : presentModes) {
        if (availablePresentMode == preferredPresentMode) {  
            return availablePresentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

internal
VkExtent2D vgChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, u32 actualWidth, u32 actualHeight) {
    if (capabilities.currentExtent.width != UINT32_MAX) 
    {
        return capabilities.currentExtent;
    }
    else
    {
        VkExtent2D actualExtent;
        actualExtent.width = std::clamp(actualWidth, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualHeight, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

// internal
// void vgDestroyImage(VkDevice device, vg_image& image)
// {
//     IFF(image.view, vkDestroyImageView(device, image.view, nullptr));
//     IFF(image.handle, vkDestroyImage(device, image.handle, nullptr));
//     // IFF(image.memory, vkFreeMemory(device, image.memory, nullptr));
// }

// internal
// VkCommandBuffer vgBeginSingleTimeCommands(vg_device& device)
// {
//     VkCommandBufferAllocateInfo allocInfo = vkInit_command_buffer_allocate_info(
//         device.pCurFrame->commandPool, 1, VK_COMMAND_BUFFER_LEVEL_PRIMARY
//     );

//     VkCommandBuffer commandBuffer;
//     vkAllocateCommandBuffers(device.handle, &allocInfo, &commandBuffer);

//     VkCommandBufferBeginInfo beginInfo = vkInit_command_buffer_begin_info(
//         VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
//     );
//     vkBeginCommandBuffer(commandBuffer, &beginInfo);

//     return commandBuffer;
// }

// internal
// void vgEndSingleTimeCommands(vg_device& device, VkCommandBuffer commandBuffer)
// {
//     vkEndCommandBuffer(commandBuffer);

//     VkSubmitInfo submitInfo = vkInit_submit_info(&commandBuffer);

//     // TODO(james): give it a fence so we can tell when it is done?
//     vkQueueSubmit(device.q_graphics.handle, 1, &submitInfo, VK_NULL_HANDLE);
//     // Have to call QueueWaitIdle unless we're going to go the event route
//     vkQueueWaitIdle(device.q_graphics.handle);

//     vkFreeCommandBuffers(device.handle, device.pCurFrame->commandPool, 1, &commandBuffer);
// }

// internal
// VkFormat vgFindSupportedFormat(vg_device& device, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
// {
//     for(VkFormat format : candidates)
//     {
//         VkFormatProperties props;
//         vkGetPhysicalDeviceFormatProperties(device.physicalDevice, format, &props);

//         if(tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
//         {
//             return format;
//         }
//         else if(tiling == VK_IMAGE_TILING_OPTIMAL  && (props.optimalTilingFeatures & features) == features)
//         {
//             return format;
//         }
//     }

//     return VK_FORMAT_UNDEFINED;
// }

// internal
// VkFormat vgFindDepthFormat(vg_device& device)
// {
//     return vgFindSupportedFormat(
//         device,
//         {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
//         VK_IMAGE_TILING_OPTIMAL,
//         VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
//     );
// }

// internal 
// bool vgHasStencilComponent(VkFormat format)
// {
//     return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
// }

internal VKAPI_ATTR VkBool32 VKAPI_CALL vgDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {


    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) 
    {
        LOG_ERROR("Validation Layer: %s", pCallbackData->pMessage);
        ASSERT(false);
    }
    else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) 
    {
        // Message is important enough to show
        LOG_ERROR("Validation Layer: %s", pCallbackData->pMessage);
    }
    else
    {
        LOG_INFO("Validation Layer: %s", pCallbackData->pMessage);
    }

    return VK_FALSE;
}


internal
bool vgCheckValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : g_validationLayers) {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            return false;
        }
    }

    return true;
}

internal
void vgPopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = vgDebugCallback;
    createInfo.pUserData = nullptr; // Optional
}

internal
void vgGetAvailableExtensions(VkPhysicalDevice device, std::vector<VkExtensionProperties>& extensions)
{
    // first get the number of extensions
    u32 extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    // now get the full list
    extensions.assign(extensionCount, VkExtensionProperties{});
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data());
}

internal
void vgLogAvailableExtensions(VkPhysicalDevice device)
{
    // now get the full list
    std::vector<VkExtensionProperties> extensions;
    vgGetAvailableExtensions(device, extensions);

    if(device)
    {
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        LOG_INFO("Available Vulkan Extensions for Physical Device: %s(%X)", deviceProperties.deviceName, deviceProperties.deviceID);
    }
    else
    {
        LOG_INFO("Available Vulkan Extensions for Instance:");
    }
    for(u32 index = 0; index < (u32)extensions.size(); ++index)
    {
        LOG_INFO("\t%s", extensions[index].extensionName);
    }
    LOG_INFO("");
}

internal
VkResult vgInitialize(vg_backend& vb, const std::vector<const char*>* platformExtensions)
{
#define VERIFY_SUCCESS(result) if(DIDFAIL(result)) { LOG_ERROR("Vulkan Error: %X", (result)); ASSERT(false); return result; }

    VkResult result = VK_ERROR_UNKNOWN;

    // Create Instance
    {
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Project Super";
        appInfo.applicationVersion = VK_MAKE_VERSION(0,0,1);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(0,0,1);
        appInfo.apiVersion = VK_API_VERSION_1_2;

        std::vector<const char*> extensions = {
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME
        };

        if(platformExtensions)
        {
            extensions.insert(extensions.end(), platformExtensions->begin(), platformExtensions->end());
        }

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = (u32)extensions.size();
        createInfo.ppEnabledExtensionNames = extensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if(g_enableValidationLayers)
        {
            createInfo.enabledLayerCount = (u32)g_validationLayers.size();
            createInfo.ppEnabledLayerNames = g_validationLayers.data();

            vgPopulateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
        }
        else
        {
            createInfo.enabledLayerCount = 0;
        }

        // TODO(james): use the custom allocators???
        VkInstance instance = 0;
        result = vkCreateInstance(&createInfo, nullptr, &instance);
        VERIFY_SUCCESS(result);

        // Debug Messenger

        VkDebugUtilsMessengerEXT debugMessenger = 0;

        if(g_enableValidationLayers)
        {
            // TODO(james): Enable passing this to the create/destroy instance calls, ie createInfo.pNext
            result = vbCreateDebugUtilsMessengerEXT(instance, &debugCreateInfo, nullptr, &debugMessenger);
            VERIFY_SUCCESS(result);
        }

        // Assign the values to the instance
        vb.debugMessenger = debugMessenger;
        vb.instance = instance;
    }

#undef VERIFY_SUCCESS

    return VK_SUCCESS;
}

internal bool
vgFindPresentQueueFamily(VkPhysicalDevice device, VkSurfaceKHR platformSurface, u32* familyIndex)
{
    bool bFound = false;
    u32 count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, queueFamilies.data());

    for(u32 index = 0; index < count; ++index)
    {
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, index, platformSurface, &presentSupport);

        if(presentSupport)
        {
            if(familyIndex)
            {
                *familyIndex = index;
            }
            bFound = true;
            break;
        }
    }
    
    return bFound;
}

internal bool
vgFindQueueFamily(VkPhysicalDevice device, VkQueueFlags withFlags, u32* familyIndex)
{
    bool bFound = false;
    u32 count = 0;
    VkQueueFamilyProperties queueFamilies[32];
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    ASSERT(count <= 32);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, queueFamilies);

    for(u32 index = 0; index < count; ++index)
    {
        if((queueFamilies[index].queueFlags & withFlags))
        {
            if(familyIndex)
            {
                *familyIndex = index;
            }
            bFound = true;
            break;
        }
    }
    
    return bFound;
}

internal bool
vgIsDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR platformSurface, const std::vector<const char*>& requiredExtensions)
{
    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

    if(!supportedFeatures.samplerAnisotropy)
    {
        // missing sampler anisotropy?? how old is this gpu?
        return false;   
    }

    u32 graphicsQueueIndex = 0;
    if(!vgFindQueueFamily(device, VK_QUEUE_GRAPHICS_BIT, &graphicsQueueIndex))
    {
        // no graphics queues... which is a minimum requirement
        return false;   // not suitable
    }

    if(!vgFindPresentQueueFamily(device, platformSurface, nullptr))
    {
        // since a platform surface was defined, there has to be at least one present queue family
        return false;
    }

    std::vector<VkExtensionProperties> extensions;
    vgGetAvailableExtensions(device, extensions);

    std::vector<const char*> foundExtensions;
    for(const auto& extension : extensions) {
        for(auto required : requiredExtensions) {
            if(strcmp(required, extension.extensionName) == 0)
            {
                foundExtensions.push_back(required);
                break;
            }
        }
    }

    return requiredExtensions.size() == foundExtensions.size();
}

internal u32
vgGetPhysicalDeviceSuitability(VkPhysicalDevice device, VkSurfaceKHR platformSurface)
{
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);

    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
    
    int nScore = 0;     // not suitable

    // TODO(james): Look at memory capabilities
    
    // Look at Queue Family capabilities
    u32 graphicsQueueIndex = 0;
    if(!vgFindQueueFamily(device, VK_QUEUE_GRAPHICS_BIT, &graphicsQueueIndex))
    {
        // no graphics queues... which is a minimum requirement
        return 0;   // not suitable
    }

    VkBool32 presentSupport = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, graphicsQueueIndex, platformSurface, &presentSupport);

    if(presentSupport)
    {
        // prefer devices with queues that support BOTH graphics and presenting
        nScore += 100;
    }
    else if(!vgFindPresentQueueFamily(device, platformSurface, nullptr))
    {
        // since a platform surface was defined, there has to be at least one present queue family
        return 0;
    }

    // Right now we only care about basic support, but will
    // favor a discrete GPU over an integrated one

    if(deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        nScore += 1000;
    }

    return nScore;
}

// internal void
// vgCreateStandardBuffers(vg_device& device)
// {
//     vgCreateTransferBuffer(device, &device.transferBuffer);

    

//     for(u32 i = 0; i < FRAME_OVERLAP; i++)
//     {
//         {
//             VkDeviceSize size = sizeof(SceneBufferObject);
//             VkResult result = vgCreateBuffer(device, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &device.frames[i].scene_buffer);
//             ASSERT(result == VK_SUCCESS);
//         }

//         {
//             // TODO(james): support multiple lights
//             VkResult result = vgCreateBuffer(device, sizeof(LightData), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &device.frames[i].lighting_buffer);
//             ASSERT(result == VK_SUCCESS);
//         }
        
//         {
//             // TODO(james): support multiple instances
//             VkResult result = vgCreateBuffer(device, sizeof(InstanceData), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &device.frames[i].instance_buffer);
//             // VkResult result = vgCreateBuffer(device, sizeof(InstanceData), VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &device.frames[i].instance_buffer);
//             ASSERT(result == VK_SUCCESS);

//             //vgMapBuffer(device.handle, device.frames[i].instance_buffer, 0, VK_WHOLE_SIZE);
//         }
//     }
// }

// internal void
// vgTransferSceneBufferObject(vg_device& device, SceneBufferObject& sbo)
// {
//     // NOTE(james): Assumes that it will be updated during the frame
//     vgBeginDataTransfer(device.transferBuffer);
//     vgTransferDataToBuffer(device.transferBuffer, sizeof(sbo), &sbo, device.pCurFrame->scene_buffer, 0);
//     vgEndDataTransfer(device.transferBuffer);
// }

// internal void
// vgTransferLightBufferObject(vg_device& device, LightData& lbo)
// {
//     vgBeginDataTransfer(device.transferBuffer);
//     vgTransferDataToBuffer(device.transferBuffer, sizeof(lbo), &lbo, device.pCurFrame->lighting_buffer, 0);
//     vgEndDataTransfer(device.transferBuffer);
// }

// internal void
// vgTransferInstanceBufferObject(vg_device& device, InstanceData& ibo)
// {
//     // TODO: Factor out doing this right smack in the middle of the render loop...
//     vgBeginDataTransfer(device.transferBuffer);
//     vgTransferDataToBuffer(device.transferBuffer, sizeof(ibo), &ibo, device.pCurFrame->instance_buffer, 0);
//     vgEndDataTransfer(device.transferBuffer);
// }

// internal void
// vgCopyBuffer(vg_device& device, VkBuffer src, VkBuffer dest, VkDeviceSize size)
// {
//     VkCommandBuffer commandBuffer = vgBeginSingleTimeCommands(device);

//     VkBufferCopy copyRegion{};
//     copyRegion.size = size;
//     vkCmdCopyBuffer(commandBuffer, src, dest, 1, &copyRegion);

//     vgEndSingleTimeCommands(device, commandBuffer);
// }

// internal VkResult
// vgCreateImage(vg_device& device, u32 width, u32 height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, vg_image* pImage)
// {
//     VkImageCreateInfo imageInfo = vkInit_image_create_info(
//         format, usage, { width, height, 1 }
//     );
//     imageInfo.tiling = tiling;
//     imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
//     imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

//     VkResult result = vkCreateImage(device.handle, &imageInfo, nullptr, &pImage->handle);
//     ASSERT(DIDSUCCEED(result));

//     VkMemoryRequirements memRequirements;
//     vkGetImageMemoryRequirements(device.handle, pImage->handle, &memRequirements);

//     VkMemoryAllocateInfo allocInfo{};
//     allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
//     allocInfo.allocationSize = memRequirements.size;
//     allocInfo.memoryTypeIndex = vgFindMemoryType(device, memRequirements.memoryTypeBits, properties);
//     ASSERT(allocInfo.memoryTypeIndex >= 0);

//     // vkAllocateMemory(device.handle, &allocInfo, nullptr, &pImage->memory);

//     // vkBindImageMemory(device.handle, pImage->handle, pImage->memory, 0);

//     return VK_SUCCESS;
// }

// internal
// void vgTransitionImageLayout(vg_device& device, vg_image& image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
// {
//     VkCommandBuffer commandBuffer = vgBeginSingleTimeCommands(device);

//     VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_NONE_KHR;
//     VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_NONE_KHR;

//     VkImageMemoryBarrier barrier = vkInit_image_barrier(
//         image.handle, 0, 0, oldLayout, newLayout, 0
//     );
//     barrier.subresourceRange.baseMipLevel = 0;
//     barrier.subresourceRange.levelCount = 1;
//     barrier.subresourceRange.baseArrayLayer = 0;
//     barrier.subresourceRange.layerCount = 1;

//     // TODO(james): This feels dirty, think about a simpler more robust solution here
//     if(newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
//     {
//         barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

//         if(vgHasStencilComponent(format)) 
//         {
//             barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
//         }
//     }
//     else
//     {
//         barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//     }

//     if(oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) 
//     {
//         barrier.srcAccessMask = 0;
//         barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

//         sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
//         destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
//     }
//     else if(oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
//     {
//         barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
//         barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

//         sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
//         destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
//     }
//     else if(oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
//     {
//         barrier.srcAccessMask = 0;
//         barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

//         sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
//         destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
//     }
//     else
//     {
//         // invalid layout transition
//         ASSERT(false);
//     }

//     vkCmdPipelineBarrier(commandBuffer, 
//             sourceStage, destinationStage,
//             0,
//             0, nullptr,
//             0, nullptr,
//             1, &barrier
//         );

//     vgEndSingleTimeCommands(device, commandBuffer);
// }

// VkResult vgCreateImageView(vg_device& device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, VkImageView* pView)
// {
//     VkImageViewCreateInfo viewInfo = vkInit_imageview_create_info(
//         format, image, aspectFlags
//     );

//     VkResult result = vkCreateImageView(device.handle, &viewInfo, nullptr, pView);
//     ASSERT(DIDSUCCEED(result));

//     return result;
// }

// internal
// void vgCopyBufferToImage(vg_device& device, vg_buffer& buffer, vg_image& image, u32 width, u32 height)
// {
//     VkCommandBuffer commandBuffer = vgBeginSingleTimeCommands(device);

//     VkBufferImageCopy region{};
//     region.bufferOffset = 0;
//     region.bufferRowLength = 0;
//     region.bufferImageHeight = 0;

//     region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//     region.imageSubresource.mipLevel = 0;
//     region.imageSubresource.baseArrayLayer = 0;
//     region.imageSubresource.layerCount = 1;

//     region.imageOffset = {0,0,0};
//     region.imageExtent = { width, height, 1 };

//     vkCmdCopyBufferToImage(commandBuffer, buffer.handle, image.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

//     vgEndSingleTimeCommands(device, commandBuffer);
// }

internal VkResult
vgCreateDevice(vg_backend& vb, VkSurfaceKHR platformSurface, const std::vector<const char*>* platformDeviceExtensions)
{
    VkResult result = VK_ERROR_UNKNOWN;
    vg_device& device = vb.device;

    // NOTE(james): We are assuming that all extensions are required for now
    std::vector<const char*> deviceExtensions = {
    };

    if(platformDeviceExtensions)
    {
        deviceExtensions.insert(deviceExtensions.end(), platformDeviceExtensions->begin(), platformDeviceExtensions->end());
    }

#define VERIFY_SUCCESS(result) if(DIDFAIL(result)) { LOG_ERROR("Vulkan Error: %X", (result)); ASSERT(false); return result; }
    // Pick Physical Device
    {
        VkPhysicalDevice chosenPhysicalDevice = VK_NULL_HANDLE;

        u32 deviceCount = 0;
        vkEnumeratePhysicalDevices(vb.instance, &deviceCount, nullptr);

        std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
        vkEnumeratePhysicalDevices(vb.instance, &deviceCount, physicalDevices.data());

        u32 physicalDeviceSuitability = 0;
        for (const auto& physicalDevice : physicalDevices)
        {
            if(vgIsDeviceSuitable(physicalDevice, platformSurface, deviceExtensions))
            {
                vgLogAvailableExtensions(physicalDevice);
                // select the device with the highest suitability score
                u32 suitability = vgGetPhysicalDeviceSuitability(physicalDevice, platformSurface);

                if(suitability >= physicalDeviceSuitability)
                {
                    chosenPhysicalDevice = physicalDevice;
                    physicalDeviceSuitability = suitability;
                }
            }
        }

        if(chosenPhysicalDevice == VK_NULL_HANDLE)
        {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        device.physicalDevice = chosenPhysicalDevice;
        device.platform_surface = platformSurface;

        // vulkan spec states that VK_KHR_portability_subset extension has to be included if it comes back as supported
        // TODO(james): look into what this extension truly means, partial API support??
        std::vector<VkExtensionProperties> extensions;
        vgGetAvailableExtensions(device.physicalDevice, extensions);

        std::string portability = "VK_KHR_portability_subset";
        for(auto& ext : extensions)
        {
            if(portability == ext.extensionName)
            {
                deviceExtensions.push_back("VK_KHR_portability_subset");
                break;
            }
        }

        vkGetPhysicalDeviceProperties(device.physicalDevice, &device.device_properties);
        vkGetPhysicalDeviceMemoryProperties(device.physicalDevice, &device.device_memory_properties);
    }

    // TODO(james): This seems over complicated, I bet there's room to simplify, specifically the queue family stuff
    // Create the Logical Device
    {
        device.handle = VK_NULL_HANDLE;

        u32 graphicsQueueIndex = 0;
        u32 presentQueueIndex = 0;
        u32 computeQueueIndex = 0;
        u32 transferQueueIndex = 0;
        vgFindQueueFamily(device.physicalDevice, VK_QUEUE_GRAPHICS_BIT, &graphicsQueueIndex);
        vgFindQueueFamily(device.physicalDevice, VK_QUEUE_COMPUTE_BIT, &computeQueueIndex);
        vgFindQueueFamily(device.physicalDevice, VK_QUEUE_TRANSFER_BIT, &transferQueueIndex);

        u32 numQueueCreates = 0;
        u32 queueIdx = 0;
        VkDeviceQueueCreateInfo queueCreateInfos[4]; 
        // setup graphics queue

        f32 queuePriority = 1.0f;
        queueIdx = numQueueCreates++;
        queueCreateInfos[queueIdx] = VkDeviceQueueCreateInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queueCreateInfos[queueIdx].queueFamilyIndex = graphicsQueueIndex;
        queueCreateInfos[queueIdx].queueCount = 1;
        queueCreateInfos[queueIdx].pQueuePriorities = &queuePriority;

        if(computeQueueIndex != graphicsQueueIndex)
        {
            queueIdx = numQueueCreates++;
            queueCreateInfos[queueIdx] = VkDeviceQueueCreateInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
            queueCreateInfos[queueIdx].queueFamilyIndex = computeQueueIndex;
            queueCreateInfos[queueIdx].queueCount = 1;
            queueCreateInfos[queueIdx].pQueuePriorities = &queuePriority;
        }

        if(transferQueueIndex != computeQueueIndex && transferQueueIndex != graphicsQueueIndex)
        {
            queueIdx = numQueueCreates++;
            queueCreateInfos[queueIdx] = VkDeviceQueueCreateInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
            queueCreateInfos[queueIdx].queueFamilyIndex = transferQueueIndex;
            queueCreateInfos[queueIdx].queueCount = 1;
            queueCreateInfos[queueIdx].pQueuePriorities = &queuePriority;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device.physicalDevice, graphicsQueueIndex, device.platform_surface, &presentSupport);

        if(presentSupport)
        {
            presentQueueIndex = graphicsQueueIndex;
        }
        else
        {
            // Needs a separate queue for presenting the platform surface
            vgFindPresentQueueFamily(device.physicalDevice, device.platform_surface, &presentQueueIndex);

            VkDeviceQueueCreateInfo presentQueueInfo{};
            queueIdx = numQueueCreates++;
            queueCreateInfos[queueIdx] = VkDeviceQueueCreateInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
            queueCreateInfos[queueIdx].queueFamilyIndex = presentQueueIndex;
            queueCreateInfos[queueIdx].queueCount = 1;
            queueCreateInfos[queueIdx].pQueuePriorities = &queuePriority;
        }
        
        VkPhysicalDeviceFeatures deviceFeatures{};
        deviceFeatures.samplerAnisotropy = VK_TRUE;

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = numQueueCreates;
        createInfo.pQueueCreateInfos = queueCreateInfos;
        createInfo.pEnabledFeatures = &deviceFeatures;
        createInfo.enabledExtensionCount = (u32)deviceExtensions.size();
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        if(g_enableValidationLayers)
        {
            createInfo.enabledLayerCount = (u32)g_validationLayers.size();
            createInfo.ppEnabledLayerNames = g_validationLayers.data();
        }
        else
        {
            createInfo.enabledLayerCount = 0;
        }

        result = vkCreateDevice(device.physicalDevice, &createInfo, nullptr, &device.handle);
        VERIFY_SUCCESS(result);

        device.q_graphics.queue_family_index = graphicsQueueIndex;
        device.q_present.queue_family_index = presentQueueIndex;
        device.q_compute.queue_family_index = computeQueueIndex;
        device.q_transfer.queue_family_index = transferQueueIndex;

        vkGetDeviceQueue(device.handle, graphicsQueueIndex, 0, &device.q_graphics.handle);
        vkGetDeviceQueue(device.handle, presentQueueIndex, 0, &device.q_present.handle);
        vkGetDeviceQueue(device.handle, computeQueueIndex, 0, &device.q_compute.handle);
        vkGetDeviceQueue(device.handle, transferQueueIndex, 0, &device.q_transfer.handle);
    }

    // Create the Vulkan Memory Allocator
    {
        VmaAllocatorCreateInfo allocInfo = {};
        allocInfo.vulkanApiVersion = VK_API_VERSION_1_2;
        allocInfo.physicalDevice = device.physicalDevice;
        allocInfo.device = device.handle;
        allocInfo.instance = vb.instance;
        // NOTE(james): since we will handle our own synchronization on allocations, we don't want the allocator to use mutexes internally
        allocInfo.flags = VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT; 

        vmaCreateAllocator(&allocInfo, &device.allocator);
    }

    // create internal command pool here so that we can do an initial transition of the swap chain images for the user
    {
        VkCommandPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        poolInfo.queueFamilyIndex = device.q_graphics.queue_family_index;
        result = vkCreateCommandPool(device.handle, &poolInfo, nullptr, &device.internal_cmd_pool);
        VERIFY_SUCCESS(result);

        VkCommandBufferAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocInfo.commandBufferCount = 1;
        allocInfo.commandPool = device.internal_cmd_pool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

        result = vkAllocateCommandBuffers(device.handle, &allocInfo, &device.internal_cmd_buffer);
        VERIFY_SUCCESS(result);

        VkSemaphoreCreateInfo semaphoreInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkFenceCreateInfo fenceInfo = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fenceInfo.flags = 0; // VK_FENCE_CREATE_SIGNALED_BIT

        VkResult imageAvailableResult = vkCreateSemaphore(device.handle, &semaphoreInfo, nullptr, &device.internal_wait_semaphore);
        VkResult renderFinishedResult = vkCreateSemaphore(device.handle, &semaphoreInfo, nullptr, &device.internal_signal_semaphore);
        VkResult fenceResult = vkCreateFence(device.handle, &fenceInfo, nullptr, &device.internal_cmd_fence);
    }

#undef VERIFY_SUCCESS

    return VK_SUCCESS;
}

internal
VkResult vgCreateSwapChain(vg_device& device, VkFormat preferredFormat, VkColorSpaceKHR preferredColorSpace, VkPresentModeKHR preferredPresentMode, u32 width, u32 height)
{
#define VERIFY_SUCCESS(result) if(DIDFAIL(result)) { LOG_ERROR("Vulkan Error: %X", (result)); ASSERT(false); return result; }

    VkSurfaceCapabilitiesKHR surfaceCaps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.physicalDevice, device.platform_surface, &surfaceCaps);

    VkSurfaceFormatKHR surfaceFormat = vgChooseSwapSurfaceFormat(device, preferredFormat, preferredColorSpace);
    VkPresentModeKHR presentMode = vgChooseSwapPresentMode(device, preferredPresentMode);
    VkExtent2D extent = vgChooseSwapExtent(surfaceCaps, width, height);

    u32 imageCount = surfaceCaps.minImageCount + 1;

    if(surfaceCaps.maxImageCount > 0 && imageCount > surfaceCaps.maxImageCount)
    {
        imageCount = surfaceCaps.maxImageCount;
    }

    device.numSwapChainImages = imageCount;

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = device.platform_surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    
    if(device.q_graphics.handle == device.q_present.handle)
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }
    else
    {
        u32 families[2] = { device.q_graphics.queue_family_index, device.q_present.queue_family_index };
        // slower version
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = families;
    }

    createInfo.preTransform = surfaceCaps.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    VkSwapchainKHR swapChain = VK_NULL_HANDLE;

    VkResult result = vkCreateSwapchainKHR(device.handle, &createInfo, nullptr, &swapChain);
    VERIFY_SUCCESS(result);

    device.swapChain = swapChain;
    device.swapChainFormat = surfaceFormat.format;
    device.extent = extent; 
    
    vkGetSwapchainImagesKHR(device.handle, swapChain, &imageCount, nullptr);
    VkImage* pImages = PushArray(*device.frameArena, imageCount, VkImage);
    vkGetSwapchainImagesKHR(device.handle, swapChain, &imageCount, pImages);

    device.swapChainImages = array_create(device.arena, vg_image*, imageCount);

    u32 numBarriers = 0;
    VkImageMemoryBarrier* pImgBarriers = PushArray(*device.frameArena, imageCount, VkImageMemoryBarrier);

    vg_resourceheap* pHeap = device.resourceHeaps->get(0);
    //device.swapChainImages.resize(imageCount);
    for(u32 index = 0; index < imageCount; ++index)
    {
        vg_image* swapChainImg = PushStruct(device.arena, vg_image); 
        swapChainImg->handle = pImages[index];
        swapChainImg->format = device.swapChainFormat;
        swapChainImg->width = extent.width;
        swapChainImg->height = extent.height;
        swapChainImg->layers = 1;
        swapChainImg->numMipLevels = 1;

        device.swapChainImages->push_back(swapChainImg);    

        // now setup the image view for use in the swap chain
        VkImageViewCreateInfo viewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.image = pImages[index];
        viewInfo.format = device.swapChainFormat;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

        result = vkCreateImageView(device.handle, &viewInfo, nullptr, &swapChainImg->view);
        VERIFY_SUCCESS(result);

        vg_rendertargetview* pRTV = PushStruct(pHeap->arena, vg_rendertargetview);
        VERIFY_SUCCESS(result);
        pRTV->image = swapChainImg;
        pRTV->loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        pRTV->clearValue = VkClearValue{0.2f,0.2f,0.2f,0.0f} ;   // gray
        pRTV->sampleCount = VK_SAMPLE_COUNT_1_BIT;  // TODO(james): This should come from a graphics init config or something
 
        u64 key = pHeap->rtvs->size()+1;
        pHeap->rtvs->set(key, pRTV);

        VkImageMemoryBarrier& imgBarrier = pImgBarriers[numBarriers++];
        imgBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imgBarrier.image = pImages[index];
        imgBarrier.srcAccessMask = 0;
        imgBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        imgBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imgBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        imgBarrier.subresourceRange.baseMipLevel = 0;
        imgBarrier.subresourceRange.levelCount = 1;
        imgBarrier.subresourceRange.baseArrayLayer = 0;
        imgBarrier.subresourceRange.layerCount = 1;
        imgBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imgBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imgBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    }

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(device.internal_cmd_buffer, &beginInfo);
    vkCmdPipelineBarrier(device.internal_cmd_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, numBarriers, pImgBarriers);
    vkEndCommandBuffer(device.internal_cmd_buffer);
    // submit and wait for the device to transition the images
    VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &device.internal_cmd_buffer;
	submitInfo.signalSemaphoreCount = 0;
	submitInfo.pSignalSemaphores = nullptr;//&device.internal_signal_semaphore;

    vkQueueSubmit(device.q_graphics.handle, 1, &submitInfo, device.internal_cmd_fence);
    vkWaitForFences(device.handle, 1, &device.internal_cmd_fence, VK_TRUE, U64MAX);
    vkResetFences(device.handle, 1, &device.internal_cmd_fence);
    vkResetCommandPool(device.handle, device.internal_cmd_pool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);

    VERIFY_SUCCESS(result);

#undef VERIFY_SUCCESS

    return VK_SUCCESS;
}

internal
VkResult vgInitializeMemory(vg_device& device)
{
    // vgInitDescriptorAllocator(device.descriptorAllocator, device.handle);
    // vgInitDescriptorLayoutCache(device.descriptorLayoutCache, device.handle);

    // for(int i = 0; i < FRAME_OVERLAP; ++i)
    // {
    //     vgInitDescriptorAllocator(device.frames[i].dynamicDescriptorAllocator, device.handle);
    // }

    u32 numImages = FRAME_OVERLAP;

    VkSemaphoreCreateInfo semaphoreInfo = vkInit_semaphore_create_info(0);
    VkFenceCreateInfo fenceInfo = vkInit_fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);

    for(u32 i = 0; i < numImages; i++)
    {
        vg_framedata& frame = device.frames[i];

        VkResult imageAvailableResult = vkCreateSemaphore(device.handle, &semaphoreInfo, nullptr, &frame.renderSemaphore);
        VkResult renderFinishedResult = vkCreateSemaphore(device.handle, &semaphoreInfo, nullptr, &frame.presentSemaphore);
        VkResult fenceResult = vkCreateFence(device.handle, &fenceInfo, nullptr, &frame.renderFence);

        if(DIDFAIL(imageAvailableResult)) { 
            LOG_ERROR("Vulkan Error: %X", (imageAvailableResult));
            ASSERT(false);
            return imageAvailableResult; 
        }
        else if(DIDFAIL(renderFinishedResult))
        {
            LOG_ERROR("Vulkan Error: %X", (renderFinishedResult));
            ASSERT(false);
            return renderFinishedResult; 
        } 
        else if(DIDFAIL(fenceResult))
        {
            LOG_ERROR("Vulkan Error: %X", (fenceResult));
            ASSERT(false);
            return fenceResult; 
        }

        device.descriptorPools[i] = 0;
    }

    device.pCurFrame = &device.frames[0];
    device.pPrevFrame = &device.frames[FRAME_OVERLAP - 1];

    return VK_SUCCESS;
}

#define VG_POOLSIZER(x, size) (u32)((x) * (size))
internal VkDescriptorPool
vgCreateDescriptorPool(vg_device& device, u32 poolSize, VkDescriptorPoolCreateFlags createFlags)
{

    // NOTE(james): Tune these numbers for better VRAM efficiency, multiples of the entire descriptor pool count
    VkDescriptorPoolSize poolSizes[] = {
				{ VK_DESCRIPTOR_TYPE_SAMPLER,                  VG_POOLSIZER( 0.5, poolSize ) },
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,   VG_POOLSIZER( 1.0, poolSize ) },
				{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,            VG_POOLSIZER( 4.0, poolSize ) },
				{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,            VG_POOLSIZER( 1.0, poolSize ) },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,     VG_POOLSIZER( 1.0, poolSize ) },
				{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,     VG_POOLSIZER( 1.0, poolSize ) },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,           VG_POOLSIZER( 2.0, poolSize ) },
				{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,           VG_POOLSIZER( 2.0, poolSize ) },
                { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,   VG_POOLSIZER( 1.0, poolSize ) },
				{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,   VG_POOLSIZER( 1.0, poolSize ) },
				{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,         VG_POOLSIZER( 0.5, poolSize ) }
			};

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = ARRAY_COUNT(poolSizes);
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = poolSize;
    poolInfo.flags = createFlags; 

    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkResult result = vkCreateDescriptorPool(device.handle, &poolInfo, nullptr, &pool);
    ASSERT(result == VK_SUCCESS);

    return pool;
}

internal void
vgResetDescriptorPools(vg_device& device)
{
    vg_descriptor_pool* pool = device.descriptorPools[device.currentFrameIndex];
    while(pool)
    {
        vg_descriptor_pool* freepool = pool;
        vkResetDescriptorPool(device.handle, freepool->handle, 0);
        pool = pool->next;
        freepool->next = device.freelist_descriptorPool;
        device.freelist_descriptorPool = freepool;
    }

    device.descriptorPools[device.currentFrameIndex] = 0;
}

internal vg_descriptor_pool*
vgGrabFreeDescriptorPool(vg_device& device)
{
    vg_descriptor_pool* pool = 0;

    if(device.freelist_descriptorPool)
    {
        pool = device.freelist_descriptorPool;
        device.freelist_descriptorPool = pool->next;
        pool->next = 0;
    }
    else
    {
        pool = PushStruct(device.arena, vg_descriptor_pool);
        pool->handle = vgCreateDescriptorPool(device, 1000, 0);
    }

    return pool;
}

internal VkResult
vgAllocateDescriptor(vg_device& device, VkDescriptorSetLayout layout, VkDescriptorSet* pDescriptor)
{
    vg_descriptor_pool* pool = device.descriptorPools[device.currentFrameIndex];
    if(!pool)
    {
        pool = vgGrabFreeDescriptorPool(device);
        device.descriptorPools[device.currentFrameIndex] = pool;
    }

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.pNext = nullptr;
    allocInfo.descriptorPool = pool->handle;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkResult result = vkAllocateDescriptorSets(device.handle, &allocInfo, pDescriptor);
    bool needsRealloc = false;
    
    switch(result)
    {
        case VK_SUCCESS:
            return result;
        
        case VK_ERROR_FRAGMENTED_POOL:
        case VK_ERROR_OUT_OF_POOL_MEMORY:
            {
                vg_descriptor_pool* newPool = vgGrabFreeDescriptorPool(device);
                newPool->next = pool;
                device.descriptorPools[device.currentFrameIndex] = newPool;
                
                // if this fails we have bigger issues than a missing descriptor set
                result = vkAllocateDescriptorSets(device.handle, &allocInfo, pDescriptor);
            }
            break;
        default:
            // do nothing...
            break;
    }

    // uh-oh this is some bad-juju
    ASSERT(result != VK_SUCCESS);
    *pDescriptor = VK_NULL_HANDLE;
    return result;
}

// internal
// VkResult vgCreateShader(VkDevice device, buffer& bytes, vg_shader* shader)
// {
//     {
//         // TODO(james): Only load reflection at asset compile time, build out structures we need at that point
//         SpvReflectShaderModule module = {};
//         SpvReflectResult result = spvReflectCreateShaderModule(bytes.size, bytes.data, &module);

//         shader->shaderStageMask = (VkShaderStageFlagBits)module.shader_stage;
//         uint32_t count = 0;

//         // NOTE(james): Straight from SPIRV-Reflect examples...
//         // Loads the shader's vertex buffer description attributes
//         {
//             result = spvReflectEnumerateInputVariables(&module, &count, NULL);
//             ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);

//             std::vector<SpvReflectInterfaceVariable*> input_vars(count);
//             result = spvReflectEnumerateInputVariables(&module, &count, input_vars.data());
//             ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);

//             count = 0;
//             result = spvReflectEnumerateOutputVariables(&module, &count, NULL);
//             ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);

//             std::vector<SpvReflectInterfaceVariable*> output_vars(count);
//             result = spvReflectEnumerateOutputVariables(&module, &count, output_vars.data());
//             ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);

//             if(module.shader_stage & SPV_REFLECT_SHADER_STAGE_VERTEX_BIT)
//             {
//                 // Demonstrates how to generate all necessary data structures to populate
//                 // a VkPipelineVertexInputStateCreateInfo structure, given the module's
//                 // expected input variables.
//                 //
//                 // Simplifying assumptions:
//                 // - All vertex input attributes are sourced from a single vertex buffer,
//                 //   bound to VB slot 0.
//                 // - Each vertex's attribute are laid out in ascending order by location.
//                 // - The format of each attribute matches its usage in the shader;
//                 //   float4 -> VK_FORMAT_R32G32B32A32_FLOAT, etc. No attribute compression is applied.
//                 // - All attributes are provided per-vertex, not per-instance.
//                 shader->vertexBindingDesc.binding = 0;
//                 shader->vertexBindingDesc.stride = 0;  // computed below
//                 shader->vertexBindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

//                 VkPipelineVertexInputStateCreateInfo vertexInfo = vkInit_vertex_input_state_create_info();
//                 shader->vertexAttributes.assign(input_vars.size(), VkVertexInputAttributeDescription{});
//                 for(size_t index = 0; index < input_vars.size(); ++index)
//                 {
//                     const SpvReflectInterfaceVariable& input = *(input_vars[index]);
//                     VkVertexInputAttributeDescription& attr = shader->vertexAttributes[index];
//                     attr.location = input.location;
//                     attr.binding = shader->vertexBindingDesc.binding;
//                     attr.format = (VkFormat)input.format;
//                     attr.offset = 0;    // final value computed below
//                 }

//                 // sort by location
//                 std::sort(shader->vertexAttributes.begin(), shader->vertexAttributes.end(),
//                 [](const VkVertexInputAttributeDescription& a, const VkVertexInputAttributeDescription& b)
//                 {
//                     return a.location < b.location;
//                 });

//                 for (auto& attribute : shader->vertexAttributes) {
//                     uint32_t format_size = vkInit_GetFormatSize(attribute.format);
//                     attribute.offset = shader->vertexBindingDesc.stride;
//                     shader->vertexBindingDesc.stride += format_size;
//                 }
//             }
//         }

//         // Loads the shader's descriptor sets
//         {
//             result = spvReflectEnumerateDescriptorSets(&module, &count, NULL);
//             ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);

//             std::vector<SpvReflectDescriptorSet*> sets(count);
//             result = spvReflectEnumerateDescriptorSets(&module, &count, sets.data());
//             ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);
            
//             shader->set_layouts.assign(sets.size(), vg_shader_descriptorset_layoutdata{});
//             for(size_t index = 0; index < sets.size(); ++index)
//             {
//                 const SpvReflectDescriptorSet& reflSet = *sets[index];
//                 vg_shader_descriptorset_layoutdata& layoutdata = shader->set_layouts[index];

//                 layoutdata.bindings.resize(reflSet.binding_count);
//                 for(u32 bindingIndex = 0; bindingIndex < reflSet.binding_count; ++bindingIndex)
//                 {
//                     const SpvReflectDescriptorBinding& refl_binding = *(reflSet.bindings[bindingIndex]);
//                     VkDescriptorSetLayoutBinding& layout_binding = layoutdata.bindings[bindingIndex];
//                     SpecialDescriptorBinding specialValue = vgGetSpecialDescriptorBindingFromName(refl_binding.name);
//                     if(specialValue != SpecialDescriptorBinding::Undefined)
//                     {
//                         layoutdata.mapSpecialBindings[specialValue] = bindingIndex;
//                     }

//                     layout_binding.binding = refl_binding.binding;
//                     layout_binding.descriptorType = (VkDescriptorType)refl_binding.descriptor_type;
//                     layout_binding.descriptorCount = 1;
//                     for (u32 i_dim = 0; i_dim < refl_binding.array.dims_count; ++i_dim)
//                     {
//                         layout_binding.descriptorCount *= refl_binding.array.dims[i_dim];
//                     }
//                     layout_binding.stageFlags = (VkShaderStageFlagBits)module.shader_stage;
//                 }
//                 layoutdata.setNumber = reflSet.set;
//             }
//         }

//         // loads the push constant blocks
//         {
//             result = spvReflectEnumeratePushConstantBlocks(&module, &count, NULL);
//             ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);

//             std::vector<SpvReflectBlockVariable*> blocks(count);
//             result = spvReflectEnumeratePushConstantBlocks(&module, &count, blocks.data());
//             ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);

//             shader->pushConstants.assign(count, VkPushConstantRange{});
//             for(size_t index = 0; index < blocks.size(); ++index)
//             {
//                 const SpvReflectBlockVariable& block = *blocks[index];
//                 VkPushConstantRange& pushConstant = shader->pushConstants[index];

//                 pushConstant.offset = block.offset;
//                 pushConstant.size = block.size;
//                 pushConstant.stageFlags = shader->shaderStageMask;
//             }
//         }

//         spvReflectDestroyShaderModule(&module);
//     }

//     VkShaderModuleCreateInfo createInfo{};
//     createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
//     createInfo.codeSize = bytes.size;
//     createInfo.pCode = (u32*)bytes.data; 

//     VkResult vulkanResult = vkCreateShaderModule(device, &createInfo, nullptr, &shader->shaderModule);

//     return vulkanResult;
// }

// internal void
// vgDestroyShader(VkDevice device, vg_shader& shader)
// {
//     vkDestroyShaderModule(device, shader.shaderModule, nullptr);
// }

// internal
// VkResult vgCreateScreenRenderPass(vg_device& device)
// {
//     VkAttachmentDescription depthAttachment{};
//     depthAttachment.format = vgFindDepthFormat(device);
//     depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
//     depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
//     depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
//     depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
//     depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
//     depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
//     depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

//     VkAttachmentReference depthAttachmentRef{};
//     depthAttachmentRef.attachment = 1;
//     depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

//     VkAttachmentDescription colorAttachment{};
//     colorAttachment.format = device.swapChainFormat;
//     colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
//     colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
//     colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
//     colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
//     colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
//     colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
//     colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

//     VkAttachmentReference colorAttachmentRef{};
//     colorAttachmentRef.attachment = 0;
//     colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

//     VkSubpassDescription subpass{};
//     subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
//     subpass.colorAttachmentCount = 1;
//     subpass.pColorAttachments = &colorAttachmentRef;
//     subpass.pDepthStencilAttachment = &depthAttachmentRef;

//     VkSubpassDependency dependency{};
//     dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
//     dependency.dstSubpass = 0;
//     dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
//     dependency.srcAccessMask = 0;
//     dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
//     dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

//     VkAttachmentDescription attachments[] = {colorAttachment, depthAttachment};
//     VkRenderPassCreateInfo renderPassInfo{};
//     renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
//     renderPassInfo.attachmentCount = ARRAY_COUNT(attachments);
//     renderPassInfo.pAttachments = attachments;
//     renderPassInfo.subpassCount = 1;
//     renderPassInfo.pSubpasses = &subpass;
//     renderPassInfo.dependencyCount = 1;
//     renderPassInfo.pDependencies = &dependency;

//     VkResult result = vkCreateRenderPass(device.handle, &renderPassInfo, nullptr, &device.screenRenderPass);

//     return result;
// }

// internal
// VkResult vgCreateFramebuffers(vg_device& device)
// {
//     u32 numImages = FRAME_OVERLAP;

//     VkSemaphoreCreateInfo semaphoreInfo = vkInit_semaphore_create_info(0);
//     VkFenceCreateInfo fenceInfo = vkInit_fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);

//     for(u32 i = 0; i < numImages; i++)
//     {
//         vg_framedata& frame = device.frames[i];

//         VkResult imageAvailableResult = vkCreateSemaphore(device.handle, &semaphoreInfo, nullptr, &frame.renderSemaphore);
//         VkResult renderFinishedResult = vkCreateSemaphore(device.handle, &semaphoreInfo, nullptr, &frame.presentSemaphore);
//         VkResult fenceResult = vkCreateFence(device.handle, &fenceInfo, nullptr, &frame.renderFence);

//         if(DIDFAIL(imageAvailableResult)) { 
//             LOG_ERROR("Vulkan Error: %X", (imageAvailableResult));
//             ASSERT(false);
//             return imageAvailableResult; 
//         }
//         else if(DIDFAIL(renderFinishedResult))
//         {
//             LOG_ERROR("Vulkan Error: %X", (renderFinishedResult));
//             ASSERT(false);
//             return renderFinishedResult; 
//         } 
//         else if(DIDFAIL(fenceResult))
//         {
//             LOG_ERROR("Vulkan Error: %X", (fenceResult));
//             ASSERT(false);
//             return fenceResult; 
//         }
//     }

//     device.pCurFrame = &device.frames[0];
//     device.pPrevFrame = &device.frames[FRAME_OVERLAP - 1];

//     arrsetlen(device.paFramebuffers, device.numSwapChainImages);
//     for(u32 i = 0; i < device.numSwapChainImages; ++i)
//     {
//         VkImageView attachments[] = {
//             device.paSwapChainImages[i].view,
//             device.depth_image.view
//         };

//         VkFramebufferCreateInfo framebufferInfo = vkInit_framebuffer_create_info(
//             device.screenRenderPass, device.extent
//         );
//         framebufferInfo.attachmentCount = ARRAY_COUNT(attachments);
//         framebufferInfo.pAttachments = attachments;

//         VkResult result = vkCreateFramebuffer(device.handle, &framebufferInfo, nullptr, &device.paFramebuffers[i]);
//         ASSERT(DIDSUCCEED(result));
//     }

//     return VK_SUCCESS;
// }


// internal
// VkResult vgCreateDepthResources(vg_device& device)
// {
//     VkFormat depthFormat = vgFindDepthFormat(device);

//     VkResult result = vgCreateImage(device, device.extent.width, device.extent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &device.depth_image);
//     if(DIDFAIL(result))
//     {
//         ASSERT(false);
//         return result;
//     }

//     result = vgCreateImageView(device, device.depth_image.handle, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, &device.depth_image.view);

//     return result;
// }

// internal
// VkResult vgCreateCommandPool(vg_device& device)
// {
//     VkResult result = VK_SUCCESS;

//     for(u32 i = 0; i < FRAME_OVERLAP; ++i)
//     {
//         VkCommandPoolCreateInfo poolInfo = vkInit_command_pool_create_info
//             (device.q_graphics.queue_family_index, 0);
//         result = vkCreateCommandPool(device.handle, &poolInfo, nullptr, &device.frames[i].commandPool);

//         if(DIDFAIL(result)) { 
//             LOG_ERROR("Vulkan Error: %X", (result));
//             ASSERT(false);
//             return result; 
//         }

//         VkCommandBufferAllocateInfo allocInfo = vkInit_command_buffer_allocate_info(
//             device.frames[i].commandPool, 1, VK_COMMAND_BUFFER_LEVEL_PRIMARY
//         );

//         result = vkAllocateCommandBuffers(device.handle, &allocInfo, &device.frames[i].commandBuffer);

//         if(DIDFAIL(result)) { 
//             LOG_ERROR("Vulkan Error: %X", (result));
//             ASSERT(false);
//             return result; 
//         }
//     }


//     // temporary triangle draw that must be done on all the command buffers
 

//     return VK_SUCCESS;
// }


// internal void
// vgCleanupResourcePool(vg_device& device)
// {
//     vg_device_resource_pool& pool = device.resource_pool;

//     FOREACH(renderPipeline, pool.pipelines, pool.pipelineCount)
//     {
//         vkDestroyPipeline(device.handle, renderPipeline->pipeline, nullptr);
//         vkDestroyPipelineLayout(device.handle, renderPipeline->layout, nullptr);

//         if(renderPipeline->renderPass != VK_NULL_HANDLE)
//         {
//             vkDestroyRenderPass(device.handle, renderPipeline->renderPass, nullptr);
//             vkDestroyFramebuffer(device.handle, renderPipeline->framebuffer, nullptr);
//         }

//         FOREACH(sampler, renderPipeline->samplers, renderPipeline->samplerCount)
//         {
//             vkDestroySampler(device.handle, *sampler, nullptr);
//         }
//         renderPipeline->samplerCount = 0;
//     }
    
//     FOREACH(image, pool.images, pool.imageCount)
//     {
//         vgDestroyImage(device.handle, *image);
//     }

//     FOREACH(shader, pool.shaders, pool.shaderCount)
//     {
//         vgDestroyShader(device.handle, *shader);
//     }

//     FOREACH(buff, pool.buffers, pool.bufferCount)
//     {
//         vgDestroyBuffer(device.handle, *buff);
//     }

//     vgDestroyBuffer(device.handle, pool.material_buffer);

//     pool.materialsCount = 0;
//     pool.pipelineCount = 0;
//     pool.imageCount = 0;
//     pool.shaderCount = 0;
//     pool.bufferCount = 0;
// }

internal VkResult
vgDestroyCmdEncoderPool(vg_device& device, vg_command_encoder_pool* pool)
{
    // NOTE(james): Technically we're leaking the command contexts here, but since the application
    //              shouldn't be willy-nilly allocating and destroying pools, we're ok with the
    //              resource leak

    u32 numBuffers = pool->cmdcontexts->size();
    for(u32 i = 0; i < FRAME_OVERLAP; ++i)
    {
        vkDestroyCommandPool(device.handle, pool->cmdPool[i], nullptr);
    }

    return VK_SUCCESS;
}

internal VkResult 
vgDestroyResourceHeap(vg_device& device, vg_resourceheap* pHeap)
{
// buffer
    for(auto entry: *pHeap->buffers)
    {
        vg_buffer& buffer = **entry;
        if(buffer.mapped)
        {
            vmaUnmapMemory(device.allocator, buffer.allocation);
            buffer.mapped = nullptr;
        }
        vmaDestroyBuffer(device.allocator, buffer.handle, buffer.allocation);
    }

    // image
    for(auto entry: *pHeap->textures)
    {
        vg_image& image = **entry;
        vkDestroyImageView(device.handle, image.view, nullptr);
        vmaDestroyImage(device.allocator, image.handle, image.allocation);
    }
    
    // sampler
    for(auto entry: *pHeap->samplers)
    {
        vg_sampler& sampler = **entry;
        vkDestroySampler(device.handle, sampler.handle, nullptr);
    }

    for(auto entry: *pHeap->rtvs)
    {
        vg_rendertargetview& rtv = **entry;
    }

    // kernel
    for(auto entry: *pHeap->kernels)
    {
        vg_kernel& kernel = **entry;
        
        vkDestroyPipeline(device.handle, kernel.pipeline, nullptr);
    }

    // program
    for(auto entry: *pHeap->programs)
    {
        vg_program& program = **entry;
        for(u32 i = 0; i < program.numShaders; ++i)
        {
            spvReflectDestroyShaderModule(program.shaderReflections[i]);
            vkDestroyShaderModule(device.handle, program.shaders[i], nullptr);
        }
        
        if(program.pipelineLayout != nullptr)
        {
            vkDestroyPipelineLayout(device.handle, program.pipelineLayout, nullptr);
        }

        for(auto setLayout: *(program.descriptorSetLayouts))
        {
            vkDestroyDescriptorSetLayout(device.handle, setLayout, nullptr);
        }
    }

    // now clear the heap memory
    Clear(pHeap->arena);

    return VK_SUCCESS;
}

internal
void vgDestroy(vg_backend& vb)
{
    // NOTE(james): Clean up the rest of the backend members prior to destroying the instance
    if(vb.device.handle)
    {
        vg_device& device = vb.device;
        // vgCleanupResourcePool(device);
        for(auto entry: *device.resourceHeaps)
        {
            vgDestroyResourceHeap(device, entry.value);
        }
        device.resourceHeaps->clear();

        for(auto entry: *device.encoderPools)
        {
            vgDestroyCmdEncoderPool(device, entry.value);
        }
        device.encoderPools->clear();

        for(auto entry: *device.mapFramebuffers)
        {
            vg_framebuffer& framebuffer = **entry;
            vkDestroyFramebuffer(device.handle, framebuffer.handle, nullptr);
        }
        device.mapFramebuffers->clear();

        for(auto entry: *device.mapRenderpasses)
        {
            vg_renderpass& renderpass = **entry;
            vkDestroyRenderPass(device.handle, renderpass.handle, nullptr);
        }
        device.mapRenderpasses->clear();

        vg_descriptor_pool* pool = device.freelist_descriptorPool;
        while(pool)
        {
            vkDestroyDescriptorPool(device.handle, pool->handle, nullptr);
            pool = pool->next;
        }

        // Swapchain
        for(u32 i = 0; i < FRAME_OVERLAP; ++i)
        {
            vg_framedata& frame = device.frames[i];

            pool = device.descriptorPools[i];
            while(pool)
            {
                vkDestroyDescriptorPool(device.handle, pool->handle, nullptr);
                pool = pool->next;
            }
            
            // vgCleanupDescriptorAllocator(frame.dynamicDescriptorAllocator);
            vkDestroySemaphore(device.handle, frame.renderSemaphore, nullptr);
            vkDestroySemaphore(device.handle, frame.presentSemaphore, nullptr);
            vkDestroyFence(device.handle, frame.renderFence, nullptr);
        }

        // IFF(device.screenRenderPass, vkDestroyRenderPass(device.handle, device.screenRenderPass, nullptr));
        
        for(auto& swapChainImage: *device.swapChainImages)
        {
            vkDestroyImageView(device.handle, swapChainImage->view, nullptr);
        }

        vkDestroySwapchainKHR(device.handle, device.swapChain, nullptr);
        // end swapchain

        vkDestroySemaphore(device.handle, device.internal_wait_semaphore, nullptr);
        vkDestroySemaphore(device.handle, device.internal_signal_semaphore, nullptr);
        vkDestroyFence(device.handle, device.internal_cmd_fence, nullptr);
        vkDestroyCommandPool(device.handle, device.internal_cmd_pool, nullptr);

        // TODO(james): Account for this in the window resize
        // vgDestroyImage(device.handle, device.depth_image);
        
        // vgDestroyTransferBuffer(device.transferBuffer);

        // vgCleanupDescriptorAllocator(device.descriptorAllocator);
        // vgCleanupDescriptorLayoutCache(device.descriptorLayoutCache);

        vmaDestroyAllocator(device.allocator);

        vkDestroyDevice(device.handle, nullptr);
    }

    vkDestroySurfaceKHR(vb.instance, vb.device.platform_surface, nullptr);

    if(vb.debugMessenger)
    {
        vbDestroyDebugUtilsMessengerEXT(vb.instance, vb.debugMessenger, nullptr);
    }

    vkDestroyInstance(vb.instance, nullptr);
}

// inline vg_shader* 
// vgGetPoolShaderFromId(vg_device_resource_pool& pool, render_shader_id id)
// {
//     vg_shader* shader = 0;

//     for(u32 i = 0; i < pool.shaderCount && !shader; ++i)
//     {
//         if(pool.shaders[i].id == id)
//         {
//             shader = &pool.shaders[i];
//         }
//     }

//     return shader;
// }

// inline vg_image* 
// vgGetPoolImageFromId(vg_device_resource_pool& pool, render_image_id id)
// {
//     vg_image* image = 0;

//     for(u32 i = 0; i < pool.imageCount && !image; ++i)
//     {
//         // if(pool.images[i].id == id)
//         // {
//         //     image = &pool.images[i];
//         // }
//     }

//     return image;
// }

// inline vg_buffer* 
// vgGetPoolBufferFromId(vg_device_resource_pool& pool, render_buffer_id id)
// {
//     vg_buffer* buffer = 0;

//     for(u32 i = 0; i < pool.bufferCount && !buffer; ++i)
//     {
//         // if(pool.buffers[i].id == id)
//         // {
//         //     buffer = &pool.buffers[i];
//         // }
//     }

//     return buffer;
// }

// inline vg_render_pipeline* 
// vgGetPoolPipelineFromId(vg_device_resource_pool& pool, render_material_id id)
// {
//     vg_render_pipeline* pipe = 0;

//     for(u32 i = 0; i < pool.pipelineCount && !pipe; ++i)
//     {
//         if(pool.pipelines[i].id == id)
//         {
//             pipe = &pool.pipelines[i];
//         }
//     }

//     return pipe;
// }

// inline umm
// vgGetMaterialBufferIndexFromId(vg_device_resource_pool& pool, render_material_id id)
// {
//     // TODO(james): Fully implement a translation so that we can support more than 1 render manifest
//     return id;
// }

// internal void
// vgCreateManifestResources(vg_device& device, render_manifest* manifest)
// {
//     VkResult result = VK_SUCCESS;

//     vgBeginDataTransfer(device.transferBuffer);

//     vg_device_resource_pool& pool = device.resource_pool;

//     for(u32 i = 0; i < manifest->shaderCount; ++i)
//     {
//         const render_shader_desc& shaderDesc = manifest->shaders[i];
//         vg_shader& shader = pool.shaders[pool.shaderCount++];

//         buffer shaderBuffer;
//         shaderBuffer.size = shaderDesc.sizeInBytes;
//         shaderBuffer.data = (u8*)shaderDesc.bytes;

//         result = vgCreateShader(device.handle, shaderBuffer, &shader);
//         ASSERT(result == VK_SUCCESS);

//         shader.id = shaderDesc.id;
//     }

//     for(u32 i = 0; i < manifest->bufferCount; ++i)
//     {
//         const render_buffer_desc& bufferDesc = manifest->buffers[i];
//         vg_buffer& buffer = pool.buffers[pool.bufferCount++];

//         VkBufferUsageFlags usageFlags = 0;
//         VkMemoryPropertyFlags memProperties = 0;
//         VkDeviceSize bufferSize = bufferDesc.sizeInBytes;

//         switch(bufferDesc.type)
//         {
//             case RenderBufferType::Vertex:
//                 usageFlags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
//                 break;
//             case RenderBufferType::Index:
//                 usageFlags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
//                 break;
//             case RenderBufferType::Uniform:
//                 usageFlags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
//                 break;

//             InvalidDefaultCase;
//         }

//         b32 useStagingBuffer = false;

//         switch(bufferDesc.usage)
//         {
//             case RenderUsage::Static:
//                 useStagingBuffer = true;
//                 usageFlags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
//                 memProperties |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
//                 break;
//             case RenderUsage::Dynamic:
//                 usageFlags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
//                 memProperties |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;    // TODO(james): confirm these properties are the most efficient for a dynamically changing buffer
//                 break;

//             InvalidDefaultCase;
//         }

//         result = vgCreateBuffer(device, bufferSize, usageFlags, memProperties, &buffer);
//         ASSERT(result == VK_SUCCESS);

//         // now upload the buffer data
//         if(useStagingBuffer)
//         {
//             vgTransferDataToBuffer(device.transferBuffer, bufferDesc.sizeInBytes, bufferDesc.bytes, buffer, 0);
//         }
//         else
//         {
//             // NOTE(james): dynamic buffers need to have a copy for each in-flight frame so that they can be updated
//             // TODO(james): actually support this dynamic update per frame...
//             NotImplemented;
//         }
//     }

    
//     if(manifest->materialCount)
//     {
//         vg_buffer& materialBuffer = pool.material_buffer;
//         result = vgCreateBuffer(device, sizeof(render_material) * manifest->materialCount, VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &materialBuffer);
        
//         u32 materialIndex = 0;
//         render_material materials[255] = {};
//         for(u32 i = 0; i < manifest->materialCount; ++i)
//         {
//             const render_material_desc& materialDesc = manifest->materials[i];
//             materials[materialIndex++] = materialDesc.data;
//         }

//         vgTransferDataToBuffer(device.transferBuffer, sizeof(render_material) * materialIndex, materials, materialBuffer, 0);
//     }

//     for(u32 i = 0; i < manifest->imageCount; ++i)
//     {
//         const render_image_desc& imageDesc = manifest->images[i];
//         vg_image& image = pool.images[pool.imageCount++];

//         VkBufferUsageFlags usageFlags = VK_IMAGE_USAGE_SAMPLED_BIT;
//         VkMemoryPropertyFlags memProperties = 0;
//         VkFormat format = GetVkFormatFromRenderFormat(imageDesc.format);
//         VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;

//         b32 useStagingBuffer = false;

//         switch(imageDesc.usage)
//         {
//             case RenderUsage::Static:
//                 useStagingBuffer = true;
//                 usageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT ;
//                 memProperties |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
//                 break;
//             case RenderUsage::Dynamic:
//                 tiling = VK_IMAGE_TILING_LINEAR;
//                 usageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
//                 memProperties |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;    // TODO(james): confirm these properties are the most efficient for a dynamically changing buffer
//                 break;

//             InvalidDefaultCase;
//         }

//         result = vgCreateImage(device, (u32)imageDesc.dimensions.Width, (u32)imageDesc.dimensions.Height, format, tiling, usageFlags, memProperties, &image);
//         ASSERT(result == VK_SUCCESS);

//         // now setup the image "view"
        
//         VkImageViewCreateInfo viewInfo = vkInit_imageview_create_info(
//             format, image.handle, VK_IMAGE_ASPECT_COLOR_BIT
//         );

//         result = vkCreateImageView(device.handle, &viewInfo, nullptr, &image.view);
//         ASSERT(result == VK_SUCCESS);

//         // and finally copy the image pixels over to VRAM

//         if(useStagingBuffer)
//         {
//             vgTransferImageDataToBuffer(device.transferBuffer, imageDesc.dimensions.Width, imageDesc.dimensions.Height, format, imageDesc.pixels, image);
//         }
//         else
//         {
//             NotImplemented;
//         }
//     }
    
//     for(u32 i = 0; i < manifest->pipelineCount; ++i)
//     {
//         const render_pipeline_desc& pipelineDesc = manifest->pipelines[i];
//         vg_render_pipeline& pipeline = pool.pipelines[pool.pipelineCount++];
//         pipeline.id = pipelineDesc.id;

//         // first gather the shader references
//         for(u32 k = 0; k < pipelineDesc.shaderCount; ++k)
//         {
//             u32 shaderIndex = pipeline.shaderCount++;
//             pipeline.shaderRefs[shaderIndex] = vgGetPoolShaderFromId(pool, pipelineDesc.shaders[k]);

//             for(auto& layout : pipeline.shaderRefs[shaderIndex]->set_layouts)
//             {
//                 b32 merged = false;
//                 // insert or merge with an existing layout
//                 for(auto& existing_layout : pipeline.set_layouts)
//                 {
//                     if(existing_layout.setNumber == layout.setNumber)
//                     {
//                         u32 bindingIndex = 0;
//                         for(auto& binding : layout.bindings)
//                         {
//                             b32 bindingExists = false;

//                             for(auto& existing_binding : existing_layout.bindings)
//                             {
//                                 if(binding.binding == existing_binding.binding)
//                                 {
//                                     existing_binding.stageFlags |= binding.stageFlags;
//                                     bindingExists = true; 
//                                     break;
//                                 }
//                             }

//                             if(!bindingExists)
//                             {
//                                 // need to move any special mappings that exist for the binding as well...
//                                 for(auto& specials : layout.mapSpecialBindings)
//                                 {
//                                     if(specials.second == bindingIndex)
//                                     {
//                                         // it's being pushed onto the back, so just set the index to the size...
//                                         existing_layout.mapSpecialBindings[specials.first] = (u32)existing_layout.bindings.size();
//                                         break;
//                                     }
//                                 }
//                                 existing_layout.bindings.push_back(binding);
//                             }
//                             ++bindingIndex;
//                         }                 

//                         merged = true;
//                         break;       
//                     }
//                 }

//                 if(!merged)
//                 {
//                     pipeline.set_layouts.push_back(layout);
//                 }
//             }
//         }

//         // sort the descriptor sets by the set number
//         std::sort(pipeline.set_layouts.begin(), pipeline.set_layouts.end(), [](const vg_shader_descriptorset_layoutdata& a, const vg_shader_descriptorset_layoutdata& b)
//             { return a.setNumber < b.setNumber; }
//         );

//         for(auto& layout : pipeline.set_layouts)
//         {
//         }

//         // now create the samplers
//         for(u32 k = 0; k < pipelineDesc.samplerCount; ++k)
//         {
//             const render_sampler_desc& samplerDesc = pipelineDesc.samplers[k];
//             VkSampler& sampler = pipeline.samplers[pipeline.samplerCount++];

//             VkSamplerCreateInfo samplerInfo = vkInit_sampler_create_info(
//                 VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT
//             );
//             samplerInfo.anisotropyEnable = samplerDesc.enableAnisotropy ? VK_TRUE : VK_FALSE;
//             samplerInfo.maxAnisotropy = device.device_properties.limits.maxSamplerAnisotropy;
            
//             samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
//             samplerInfo.unnormalizedCoordinates = samplerDesc.coordinatesNotNormalized ? VK_TRUE : VK_FALSE;     // False = [0..1,0..1], [True = 0..Width, 0..Height]
            
//             samplerInfo.compareEnable = VK_FALSE;
//             samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

//             samplerInfo.mipmapMode = (VkSamplerMipmapMode) samplerDesc.mipmapMode;
//             samplerInfo.mipLodBias = samplerDesc.mipLodBias;
//             samplerInfo.minLod = samplerDesc.minLod;
//             samplerInfo.maxLod = samplerDesc.maxLod;

//             samplerInfo.minFilter = (VkFilter) samplerDesc.minFilter;
//             samplerInfo.magFilter = (VkFilter) samplerDesc.magFilter;

//             samplerInfo.addressModeU = (VkSamplerAddressMode) samplerDesc.addressMode_U;
//             samplerInfo.addressModeV = (VkSamplerAddressMode) samplerDesc.addressMode_V;
//             samplerInfo.addressModeW = (VkSamplerAddressMode) samplerDesc.addressMode_W;
            
//             result = vkCreateSampler(device.handle, &samplerInfo, nullptr, &sampler);
//             ASSERT(result == VK_SUCCESS);
//         }

//         // and finally it is time to slog through the render pass and pipeline creation, ugh...

//         // NOTE(james): A render pass of VK_NULL_HANDLE will indicate that the pipeline will use the default screen render pass
//         // TODO(james): implement render target creation logic
//         pipeline.renderPass = VK_NULL_HANDLE;
//         pipeline.framebuffer = VK_NULL_HANDLE;

//         VkRenderPass pipelineRenderPass = device.screenRenderPass;

//         VkVertexInputBindingDescription* vertexBindingDescription = 0;
//         std::vector<VkVertexInputAttributeDescription>* vertexAttributeDescriptions = 0;

//         std::vector<VkPipelineShaderStageCreateInfo> shaderStages(pipeline.shaderCount);
//         for(u32 k = 0; k < pipeline.shaderCount; ++k)
//         {
//             shaderStages[k] = vkInit_pipeline_shader_stage_create_info((VkShaderStageFlagBits)pipeline.shaderRefs[k]->shaderStageMask, pipeline.shaderRefs[k]->shaderModule);

//             if(pipeline.shaderRefs[k]->shaderStageMask == VK_SHADER_STAGE_VERTEX_BIT)
//             {
//                 vertexBindingDescription = &pipeline.shaderRefs[k]->vertexBindingDesc;
//                 vertexAttributeDescriptions = &pipeline.shaderRefs[k]->vertexAttributes;
//             }
//         }
//         // NOTE(james): I'm assuming these have to be valid for now
//         ASSERT(vertexBindingDescription);
//         ASSERT(vertexAttributeDescriptions);

//         VkPipelineVertexInputStateCreateInfo vertexInputInfo = vkInit_vertex_input_state_create_info();
//         vertexInputInfo.vertexBindingDescriptionCount = vertexBindingDescription ? 1 : 0;
//         vertexInputInfo.pVertexBindingDescriptions = vertexBindingDescription;
//         vertexInputInfo.vertexAttributeDescriptionCount = vertexAttributeDescriptions ? (u32)vertexAttributeDescriptions->size() : 0;
//         vertexInputInfo.pVertexAttributeDescriptions = vertexAttributeDescriptions ? vertexAttributeDescriptions->data() : 0;

//         VkPipelineInputAssemblyStateCreateInfo inputAssembly = vkInit_input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

//         VkViewport viewport{};
//         viewport.x = 0.0f;
//         viewport.y = 0.0f;
//         viewport.width = (f32)device.extent.width;
//         viewport.height = (f32)device.extent.height;
//         viewport.minDepth = 0.0f;
//         viewport.maxDepth = 1.0f;

//         VkRect2D scissor{};
//         scissor.offset = {0,0};
//         scissor.extent = device.extent;

//         VkPipelineViewportStateCreateInfo viewportState{};
//         viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
//         viewportState.viewportCount = 1;
//         viewportState.pViewports = &viewport;
//         viewportState.scissorCount = 1;
//         viewportState.pScissors = &scissor;

//         VkPipelineRasterizationStateCreateInfo rasterizer = vkInit_rasterization_state_create_info(
//             VK_POLYGON_MODE_FILL
//         );
//         rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

//         VkPipelineMultisampleStateCreateInfo multisampling = vkInit_multisampling_state_create_info();

//         VkPipelineColorBlendAttachmentState colorBlendAttachment = vkInit_color_blend_attachment_state();
//         colorBlendAttachment.blendEnable = VK_TRUE;
//         colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
//         colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
//         colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
//         colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
//         colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
//         colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

//         VkPipelineColorBlendStateCreateInfo colorBlending{};
//         colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
//         colorBlending.logicOpEnable = VK_FALSE;
//         colorBlending.logicOp = VK_LOGIC_OP_COPY; 
//         colorBlending.attachmentCount = 1;
//         colorBlending.pAttachments = &colorBlendAttachment;
//         colorBlending.blendConstants[0] = 0.0f; 
//         colorBlending.blendConstants[1] = 0.0f; 
//         colorBlending.blendConstants[2] = 0.0f; 
//         colorBlending.blendConstants[3] = 0.0f; 

//         VkPipelineDepthStencilStateCreateInfo depthStencil = vkInit_depth_stencil_create_info(
//             true, true, VK_COMPARE_OP_LESS
//         );

//         std::vector<VkPushConstantRange> pushConstants;
//         for(u32 k = 0; k < pipeline.shaderCount; ++k)
//         {
//             // TODO: merge all the shader push constants into 1 list for the pipeline
//             pushConstants.insert(pushConstants.end(), pipeline.shaderRefs[k]->pushConstants.begin(), pipeline.shaderRefs[k]->pushConstants.end());

//         }

//         for(auto& layout : pipeline.set_layouts)
//         {
//             ASSERT(pipeline.descriptorSetLayoutCount < 20);
//             pipeline.descriptorLayouts[pipeline.descriptorSetLayoutCount++] = vgGetDescriptorLayoutFromCache( device.descriptorLayoutCache, (u32)layout.bindings.size(), layout.bindings.data());
//         }

//         VkPipelineLayoutCreateInfo pipelineLayoutInfo = vkInit_pipeline_layout_create_info();
//         pipelineLayoutInfo.setLayoutCount = pipeline.descriptorSetLayoutCount; 
//         pipelineLayoutInfo.pSetLayouts = pipeline.descriptorLayouts; 
//         pipelineLayoutInfo.pushConstantRangeCount = (u32)pushConstants.size(); 
//         pipelineLayoutInfo.pPushConstantRanges = pushConstants.data(); 

//         result = vkCreatePipelineLayout(device.handle, &pipelineLayoutInfo, nullptr, &pipeline.layout);
//         ASSERT(result == VK_SUCCESS);

//         VkGraphicsPipelineCreateInfo pipelineInfo{};
//         pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
//         pipelineInfo.stageCount = (u32)shaderStages.size();
//         pipelineInfo.pStages = shaderStages.data();
//         pipelineInfo.pVertexInputState = &vertexInputInfo;
//         pipelineInfo.pInputAssemblyState = &inputAssembly;
//         pipelineInfo.pViewportState = &viewportState;
//         pipelineInfo.pRasterizationState = &rasterizer;
//         pipelineInfo.pMultisampleState = &multisampling;
//         pipelineInfo.pDepthStencilState = &depthStencil; // TODO(james): make this depend on the render target setup of the material
//         pipelineInfo.pColorBlendState = &colorBlending;
//         pipelineInfo.pDynamicState = nullptr; // Optional
//         pipelineInfo.layout = pipeline.layout;
//         pipelineInfo.renderPass = pipelineRenderPass;
//         pipelineInfo.subpass = 0;
//         // TODO(james): Figure out how to use the base pipeline definitions to make this simpler/easier
//         pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
//         pipelineInfo.basePipelineIndex = -1;

//         result = vkCreateGraphicsPipelines(device.handle, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline.pipeline);
//         ASSERT(result == VK_SUCCESS);
//     }

//     vgEndDataTransfer(device.transferBuffer);
// }

// internal void
// vgPerformResourceOperation(vg_device& device, RenderResourceOpType opType, render_manifest* manifest)
// {
//     switch(opType)
//     {
//         case RenderResourceOpType::Create:
//             vgCreateManifestResources(device, manifest);
//             break;

//         InvalidDefaultCase;
//     }
// }

// internal void
// VulkanGraphicsBeginFrame(vg_backend* vb, render_commands* cmds)
// {
//     vg_device& device = vb->device;

//     device.pPrevFrame = device.pCurFrame;
//     device.pCurFrame = &device.frames[device.currentFrameIndex];
// }

// internal
// void vgTranslateRenderCommands(vg_device& device, render_commands* commands)
// {
//     VkCommandBufferBeginInfo beginInfo = vkInit_command_buffer_begin_info(0);

//     VkCommandBuffer commandBuffer = device.pCurFrame->commandBuffer;
//     vkBeginCommandBuffer(commandBuffer, &beginInfo);
    
//     VkClearValue clearValues[2] = {};
//     clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
//     clearValues[1].depthStencil = {1.0f, 0};

//     VkFramebuffer currentScreenFramebuffer = device.paFramebuffers[device.curSwapChainIndex];

//     SceneBufferObject sbo {};
//     // TODO(james): Actually implement a dynamic viewport in the render pipelines
//     sbo.scene.window.x = commands->viewportPosition.X;
//     sbo.scene.window.y = commands->viewportPosition.Y;
//     sbo.scene.window.width = commands->viewportPosition.Width;
//     sbo.scene.window.height = commands->viewportPosition.Height;
//     sbo.frame.time = commands->time;
//     sbo.frame.timeDelta = commands->timeDelta;
//     sbo.camera.pos = commands->cameraPos;
//     sbo.camera.view = commands->cameraView;
//     sbo.camera.proj = commands->cameraProj;
//     sbo.camera.viewProj = commands->cameraProj * commands->cameraView;

//     vgTransferSceneBufferObject(device, sbo);

//     VkMemoryBarrier barrier = {};
//     barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
//     barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
//     barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

//     vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);

//     // TODO(james): MAJOR PERFORMANCE BOTTLENECK!!! Need to sort objects into render pass bins first -OR- add a command to bind the material prior to drawing!

//     VkRenderPassBeginInfo renderPassInfo = vkInit_renderpass_begin_info(
//         device.screenRenderPass, device.extent, currentScreenFramebuffer
//     );
//     renderPassInfo.clearValueCount = ARRAY_COUNT(clearValues);
//     renderPassInfo.pClearValues = clearValues;
//     vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

//     //
//     // Iterate through the render commands
//     //
//     vg_render_pipeline* activePipeline = nullptr;

//     render_cmd_header* header = (render_cmd_header*)commands->pushBufferBase;

//     while((u8*)header < commands->pushBufferDataAt && header->type != RenderCommandType::Done)
//     {
//         switch(header->type)
//         {
//             // case RenderCommandType::UpdateViewProjection:
//             // {
//             //     render_cmd_update_viewprojection* cmd = (render_cmd_update_viewprojection*)header;

//             //     FrameObject frameObject;
//             //     frameObject.viewProj = cmd->projection * cmd->view;

//             //     void* data;
//             //     vkMapMemory(device.handle, device.pCurFrame->frame_buffer.memory, 0, sizeof(frameObject), 0, &data);
//             //         Copy(sizeof(frameObject), &frameObject, data);
//             //     vkUnmapMemory(device.handle, device.pCurFrame->frame_buffer.memory);
//             // } break;
//             case RenderCommandType::UsePipeline:
//             {
//                 render_cmd_use_pipeline* cmd = (render_cmd_use_pipeline*)header;

//                 activePipeline = vgGetPoolPipelineFromId(device.resource_pool, cmd->pipeline_id);
//                 // TODO(james): Support different render passes from a material
//                 ASSERT(activePipeline->renderPass == VK_NULL_HANDLE);
//                 // VkRenderPass renderPass = device.screenRenderPass;
//                 // VkFramebuffer framebuffer = currentScreenFramebuffer;

//                 // if(material.renderPass != VK_NULL_HANDLE)
//                 // {
//                 //     renderPass = material.renderPass;
//                 //     framebuffer = material.framebuffer;
//                 // }
                
//                 vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, activePipeline->pipeline);

//                 std::vector<VkDescriptorSet> descriptors(activePipeline->descriptorSetLayoutCount, VK_NULL_HANDLE);
//                 for(u32 i = 0; i < activePipeline->descriptorSetLayoutCount; ++i)
//                 {
//                     vgAllocateDescriptor(device.pCurFrame->dynamicDescriptorAllocator, activePipeline->descriptorLayouts[i], &descriptors[i]);
//                 }

//                 u32 numBindings = 0;
//                 VkWriteDescriptorSet writes[20];    // NOTE(james): surely we won't have more than 20...

//                 // auto bind any named descriptors that we recognize
//                 for(u32 shaderIndex = 0; shaderIndex < activePipeline->shaderCount; ++shaderIndex)
//                 {
//                     vg_shader* shader = activePipeline->shaderRefs[shaderIndex];
//                     for(auto& set : shader->set_layouts)
//                     {
//                         VkDescriptorSet& descriptor = descriptors[set.setNumber];
//                         for(auto& namedBinding : set.mapSpecialBindings)
//                         {
//                             u32 binding = set.bindings[namedBinding.second].binding;

//                             switch(namedBinding.first)
//                             {
//                                 // case SpecialDescriptorBinding::Camera:
//                                 //     {
//                                 //         VkDescriptorBufferInfo bufferInfo;
//                                 //         bufferInfo.buffer = device.pCurFrame->scene_buffer.handle;
//                                 //         bufferInfo.offset = OffsetOf(SceneBufferObject, camera);
//                                 //         bufferInfo.range = sizeof(CameraData);
//                                 //         writes[numBindings++] = vkInit_write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, descriptor, &bufferInfo, binding);
//                                 //     }
//                                 //     break;
//                                 case SpecialDescriptorBinding::Scene:
//                                     {
//                                         VkDescriptorBufferInfo bufferInfo;
//                                         bufferInfo.buffer = device.pCurFrame->scene_buffer.handle;
//                                         bufferInfo.offset = 0;
//                                         bufferInfo.range = sizeof(SceneBufferObject);
//                                         writes[numBindings++] = vkInit_write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, descriptor, &bufferInfo, binding);
//                                     }
//                                     break;
//                                 // case SpecialDescriptorBinding::Frame:
//                                 //     {
//                                 //         VkDescriptorBufferInfo bufferInfo;
//                                 //         bufferInfo.buffer = device.pCurFrame->scene_buffer.handle;
//                                 //         bufferInfo.offset = OffsetOf(SceneBufferObject, frame);
//                                 //         bufferInfo.range = sizeof(FrameData);
//                                 //         writes[numBindings++] = vkInit_write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, descriptor, &bufferInfo, binding);
//                                 //     }
//                                 //     break;
//                                 case SpecialDescriptorBinding::Light:
//                                     {
//                                         VkDescriptorBufferInfo bufferInfo;
//                                         bufferInfo.buffer = device.pCurFrame->lighting_buffer.handle;
//                                         bufferInfo.offset = 0;
//                                         bufferInfo.range = sizeof(LightData);
//                                         writes[numBindings++] = vkInit_write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, descriptor, &bufferInfo, binding);
//                                     }
//                                     break;
//                                 case SpecialDescriptorBinding::Material:
//                                     {
//                                         VkDescriptorBufferInfo bufferInfo;
//                                         bufferInfo.buffer = device.resource_pool.material_buffer.handle;
//                                         bufferInfo.offset = 0;//sizeof(MaterialData) * cmd->material_id;    // TODO(james): translate material_id into an array index of the pool
//                                         bufferInfo.range = sizeof(render_material);
//                                         writes[numBindings++] = vkInit_write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, descriptor, &bufferInfo, binding);
//                                     }
//                                     break;
//                                 case SpecialDescriptorBinding::Instance:
//                                     {
//                                         VkDescriptorBufferInfo bufferInfo;
//                                         bufferInfo.buffer = device.pCurFrame->instance_buffer.handle;
//                                         bufferInfo.offset = 0;
//                                         bufferInfo.range = sizeof(InstanceData);
//                                         writes[numBindings++] = vkInit_write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, descriptor, &bufferInfo, binding);
//                                     }
//                                     break;
//                             }                            
//                         }
//                     }
//                 }

//                 vkUpdateDescriptorSets(device.handle, numBindings, writes, 0, nullptr);
//                 vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, activePipeline->layout, 0, (u32)descriptors.size(), descriptors.data(), 0, nullptr);

//             } break;
//             case RenderCommandType::UpdateLight:
//             {
//                 render_cmd_update_light* cmd = (render_cmd_update_light*)header;

//                 LightData light{};
//                 light.pos = cmd->position;
//                 light.ambient = cmd->ambient;
//                 light.diffuse = cmd->diffuse;
//                 light.specular = cmd->specular;

//                 vgTransferLightBufferObject(device, light);
//                 // VkMemoryBarrier membarrier = {};
//                 // membarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
//                 // membarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
//                 // membarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
//                 // vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, 0, 1, &membarrier, 0, nullptr, 0, nullptr);
//             } break;
//             case RenderCommandType::DrawObject:
//             {
//                 render_cmd_draw_object* cmd = (render_cmd_draw_object*)header;
//                 ASSERT(activePipeline);

//                 InstanceData instanceObject;
//                 instanceObject.mvp = cmd->mvp;
//                 instanceObject.world = cmd->world;
//                 instanceObject.worldNormal = cmd->worldNormal;
//                 instanceObject.materialIndex = (u32)cmd->material_id;

//                 // TODO(james): bulk copy all the objects at once prior to any rendering
//                 vgTransferInstanceBufferObject(device, instanceObject);
                
//                 // vkCmdUpdateBuffer(commandBuffer, device.pCurFrame->instance_buffer.handle, 0, sizeof(instanceObject), &instanceObject);
//                 // Copy(sizeof(InstanceObject), &instanceObject, device.pCurFrame->instance_buffer.mapped);
//                                 #if 0
//                 vg_render_pipeline& pipe = *vgGetPoolPipelineFromId(device.resource_pool, cmd->material_id);
                
//                 if(cmd->materialBindingCount)
//                 {
//                     u32 numBindings = 0;
//                     VkWriteDescriptorSet writes[20];    // NOTE(james): surely we won't have more than 20...
//                     for(u32 i = 0; i < cmd->materialBindingCount; ++i)
//                     {
//                         const render_material_binding& binding = cmd->materialBindings[i];
//                         ASSERT(binding.layoutIndex < (u32)descriptors.size());

//                         switch(binding.type)
//                         {
//                             case RenderMaterialBindingType::Buffer:
//                                 {
//                                     vg_buffer& buffer = *vgGetPoolBufferFromId(device.resource_pool, binding.buffer_id);
//                                     VkDescriptorBufferInfo bufferInfo{};
//                                     bufferInfo.buffer = buffer.handle;
//                                     bufferInfo.offset = (VkDeviceSize)binding.buffer_offset;
//                                     bufferInfo.range = (VkDeviceSize)binding.buffer_range;
//                                     writes[numBindings++] = vkInit_write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, descriptors[binding.layoutIndex], &bufferInfo, binding.bindingIndex);
//                                 }
//                                 break;
//                             case RenderMaterialBindingType::Image:
//                                 {
//                                     vg_image& image = *vgGetPoolImageFromId(device.resource_pool, binding.image_id);
//                                     ASSERT(binding.image_sampler_index < pipe.samplerCount);
//                                     VkSampler sampler = pipe.samplers[binding.image_sampler_index];

//                                     VkDescriptorImageInfo imageInfo{};
//                                     imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
//                                     imageInfo.imageView = image.view;
//                                     imageInfo.sampler = sampler;
//                                     writes[numBindings++] = vkInit_write_descriptor_image(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, descriptors[binding.layoutIndex], &imageInfo, binding.bindingIndex);
//                                 }
//                                 break;
                            
//                             InvalidDefaultCase;
//                         }
//                     }

                    
//                 }
//                 #endif
                
//                 vg_buffer& vertexBuffer = *vgGetPoolBufferFromId(device.resource_pool, cmd->vertexBuffer);
//                 vg_buffer& indexBuffer = *vgGetPoolBufferFromId(device.resource_pool, cmd->indexBuffer);

//                 VkDeviceSize offsets[] = {0};
//                 vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer.handle, offsets);
//                 vkCmdBindIndexBuffer(commandBuffer, indexBuffer.handle, 0, VK_INDEX_TYPE_UINT32);

//                 vkCmdPushConstants(commandBuffer, activePipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(instanceObject.mvp), &instanceObject.mvp);
//                 vkCmdDrawIndexed(commandBuffer, (u32)cmd->indexCount, 1, 0, 0, 0);                
//             } break;
//             default:
//                 // command is not supported
//                 break;
//         }

//         // move to the next command
//         header = (render_cmd_header*)OffsetPtr(header, header->size);
//     }

//     vkCmdEndRenderPass(commandBuffer);
//     vkEndCommandBuffer(commandBuffer);    
// }

// internal
// void VulkanGraphicsEndFrame(vg_backend* vb, render_commands* commands)
// {
//     vg_device& device = vb->device;

//     VkResult result = vkWaitForFences(device.handle, 1, &device.pCurFrame->renderFence, VK_TRUE, UINT64_MAX);
//     result = vkAcquireNextImageKHR(device.handle, device.swapChain, UINT64_MAX, device.pCurFrame->presentSemaphore, VK_NULL_HANDLE, &device.curSwapChainIndex);

//     if(result == VK_ERROR_OUT_OF_DATE_KHR) 
//     {
//         ASSERT(false);
//         // Win32RecreateSwapChain(graphics);
//     }
//     else if(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
//     {
//         ASSERT(false);
//     }

//     vkResetCommandPool(device.handle, device.pCurFrame->commandPool, 0);
//     vgResetDescriptorPools(device.pCurFrame->dynamicDescriptorAllocator);
 
//     // wait fence here...
//     u32 imageIndex = device.curSwapChainIndex;
    
//     VkSemaphore waitSemaphores[] = { device.pCurFrame->presentSemaphore };
//     VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
//     VkSemaphore signalSemaphores[] = { device.pCurFrame->renderSemaphore };

//     vgTranslateRenderCommands(device, commands);
//     //vgTempBuildRenderCommands(device, device.curSwapChainIndex);

//     VkSubmitInfo submitInfo{};
//     submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
//     submitInfo.waitSemaphoreCount = 1;
//     submitInfo.pWaitSemaphores = waitSemaphores;
//     submitInfo.pWaitDstStageMask = waitStages;
//     submitInfo.commandBufferCount = 1;
//     submitInfo.pCommandBuffers = &device.pCurFrame->commandBuffer;
//     submitInfo.signalSemaphoreCount = 1;
//     submitInfo.pSignalSemaphores = signalSemaphores;

//     vkResetFences(device.handle, 1, &device.pCurFrame->renderFence);
//     result = vkQueueSubmit(device.q_graphics.handle, 1, &submitInfo, device.pCurFrame->renderFence);
//     if(DIDFAIL(result))
//     {
//         LOG_ERROR("Vulkan Submit Error: %X", result);
//         ASSERT(false);
//     }

//     VkPresentInfoKHR presentInfo{};
//     presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
//     presentInfo.waitSemaphoreCount = 1;
//     presentInfo.pWaitSemaphores = signalSemaphores;
//     presentInfo.swapchainCount = 1;
//     presentInfo.pSwapchains = &device.swapChain;
//     presentInfo.pImageIndices = &device.curSwapChainIndex;
//     presentInfo.pResults = nullptr;

//     result = vkQueuePresentKHR(device.q_present.handle, &presentInfo);

//     if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
//     {
//         //Win32RecreateSwapChain(graphics);
//         ASSERT(false);
//     }
//     else if(DIDFAIL(result))
//     {
//         LOG_ERROR("Vulkan Present Error: %X", result);
//         ASSERT(false);
//     }

//     device.currentFrameIndex = (device.currentFrameIndex + 1) % FRAME_OVERLAP;
// }

internal vg_resourceheap* 
vgAllocateResourceHeap()
{
    vg_resourceheap* pHeap = BootstrapPushStructMember(vg_resourceheap, arena);

    // TODO(james): Tune these to the game
    pHeap->buffers = hashtable_create(pHeap->arena, vg_buffer*, 1024);
    pHeap->textures = hashtable_create(pHeap->arena, vg_image*, 1024);
    pHeap->samplers = hashtable_create(pHeap->arena, vg_sampler*, 128);
    pHeap->rtvs = hashtable_create(pHeap->arena, vg_rendertargetview*, 32);
    pHeap->programs = hashtable_create(pHeap->arena, vg_program*, 128);
    pHeap->kernels = hashtable_create(pHeap->arena, vg_kernel*, 128);

    return pHeap;
}

template<typename O, typename H>
struct ObjectHandleConverter
{
    static inline O& From(H h) { ASSERT(h.id != GFX_INVALID_HANDLE); return *((O*)h.id); }
    static inline O& From(u64 id) { ASSERT(id != GFX_INVALID_HANDLE); return *((O*)id); }
    static inline H To(O& object) { return H{(u64)&object}; }
};

typedef ObjectHandleConverter<vg_device, GfxDevice> DeviceObject;


inline GfxResult
ToGfxResult(VkResult result)
{
    switch(result)
    {
        case VK_SUCCESS: return GfxResult::Ok;

        case VK_ERROR_NOT_PERMITTED_EXT:
        case VK_INCOMPLETE: return GfxResult::InvalidOperation;

        case VK_ERROR_INVALID_SHADER_NV:
        case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:
        case VK_ERROR_INVALID_EXTERNAL_HANDLE:    
        case VK_ERROR_FORMAT_NOT_SUPPORTED:
        case VK_ERROR_LAYER_NOT_PRESENT: 
        case VK_ERROR_EXTENSION_NOT_PRESENT: 
        case VK_ERROR_FEATURE_NOT_PRESENT: return GfxResult::InvalidParameter;
        
        case VK_ERROR_FRAGMENTATION:
        case VK_ERROR_OUT_OF_POOL_MEMORY:
        case VK_ERROR_OUT_OF_HOST_MEMORY: 
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: 
        case VK_ERROR_TOO_MANY_OBJECTS: return GfxResult::OutOfMemory;
    }

    return GfxResult::InternalError;
}


inline VkSampleCountFlagBits
ConvertSampleCount(GfxSampleCount count)
{
    switch(count)
    {
        case GfxSampleCount::Undefined:
        case GfxSampleCount::MSAA_1: return VK_SAMPLE_COUNT_1_BIT;
        case GfxSampleCount::MSAA_2: return VK_SAMPLE_COUNT_2_BIT;
        case GfxSampleCount::MSAA_4: return VK_SAMPLE_COUNT_4_BIT;
        case GfxSampleCount::MSAA_8: return VK_SAMPLE_COUNT_8_BIT;
        case GfxSampleCount::MSAA_16: return VK_SAMPLE_COUNT_16_BIT;
        InvalidDefaultCase;
    }

    return VK_SAMPLE_COUNT_1_BIT;
}

inline VkAttachmentLoadOp
ConvertLoadOp(GfxLoadAction action)
{
    switch(action)
    {
        case GfxLoadAction::DontCare: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        case GfxLoadAction::Load: return VK_ATTACHMENT_LOAD_OP_LOAD;
        case GfxLoadAction::Clear: return VK_ATTACHMENT_LOAD_OP_CLEAR;
        InvalidDefaultCase;
    }

    return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
}

inline VkPolygonMode
ConvertFillMode(GfxFillMode fillMode)
{
    switch(fillMode)
    {
        case GfxFillMode::Wireframe: return VK_POLYGON_MODE_LINE;
        case GfxFillMode::Solid: return VK_POLYGON_MODE_FILL;
        InvalidDefaultCase;
    }

    return VK_POLYGON_MODE_FILL;
}

inline VkCullModeFlags
ConvertCullMode(GfxCullMode cullMode)
{
    switch(cullMode)
    {
        case GfxCullMode::None: return VK_CULL_MODE_NONE;
        case GfxCullMode::Front: return VK_CULL_MODE_FRONT_BIT;
        case GfxCullMode::Back: return VK_CULL_MODE_BACK_BIT;
        InvalidDefaultCase;
    }
    return VK_CULL_MODE_NONE;
}

inline VkCompareOp
ConvertComparisonFunc(GfxComparisonFunc compare)
{
    switch(compare)
    {
        case GfxComparisonFunc::Never: return VK_COMPARE_OP_NEVER;
        case GfxComparisonFunc::Less: return VK_COMPARE_OP_LESS;
        case GfxComparisonFunc::Equal: return VK_COMPARE_OP_EQUAL;
        case GfxComparisonFunc::LessEqual: return VK_COMPARE_OP_LESS_OR_EQUAL;
        case GfxComparisonFunc::Greater: return VK_COMPARE_OP_GREATER;
        case GfxComparisonFunc::NotEqual: return VK_COMPARE_OP_NOT_EQUAL;
        case GfxComparisonFunc::GreaterEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case GfxComparisonFunc::Always: return VK_COMPARE_OP_ALWAYS;
        InvalidDefaultCase;
    }
    return VK_COMPARE_OP_NEVER;
}

inline VkStencilOp
ConvertStencilOp(GfxStencilOp op)
{
    switch(op)
    {
        case GfxStencilOp::Keep: return VK_STENCIL_OP_KEEP;
        case GfxStencilOp::Zero: return VK_STENCIL_OP_ZERO;
        case GfxStencilOp::Replace: return VK_STENCIL_OP_REPLACE;
        case GfxStencilOp::Inc: return VK_STENCIL_OP_INCREMENT_AND_WRAP;
        case GfxStencilOp::Dec: return VK_STENCIL_OP_DECREMENT_AND_WRAP;
        case GfxStencilOp::IncSat: return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
        case GfxStencilOp::DecSat: return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
        case GfxStencilOp::Invert: return VK_STENCIL_OP_INVERT;
        InvalidDefaultCase;
    }

    return VK_STENCIL_OP_KEEP;
}

inline VkBlendFactor
ConvertBlendMode(GfxBlendMode blend)
{
    switch(blend)
    {
        case GfxBlendMode::Zero: return VK_BLEND_FACTOR_ZERO;
        case GfxBlendMode::One: return VK_BLEND_FACTOR_ONE;
        case GfxBlendMode::SrcColor: return VK_BLEND_FACTOR_SRC_COLOR;
        case GfxBlendMode::InvSrcColor: return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case GfxBlendMode::SrcAlpha: return VK_BLEND_FACTOR_SRC_ALPHA;
        case GfxBlendMode::InvSrcAlpha: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case GfxBlendMode::DestAlpha: return VK_BLEND_FACTOR_DST_ALPHA;
        case GfxBlendMode::InvDestAlpha: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        case GfxBlendMode::DestColor:  return VK_BLEND_FACTOR_DST_COLOR;
        case GfxBlendMode::InvDestColor: return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        case GfxBlendMode::SrcAlphaSat: return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
        case GfxBlendMode::BlendFactor: return VK_BLEND_FACTOR_CONSTANT_COLOR;
        case GfxBlendMode::InvBlendFactor: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
        case GfxBlendMode::Src1Color: return VK_BLEND_FACTOR_SRC1_COLOR;
        case GfxBlendMode::InvSrc1Color: return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
        case GfxBlendMode::Src1Alpha: return VK_BLEND_FACTOR_SRC1_ALPHA;
        case GfxBlendMode::InvSrc1Alpha: return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
        InvalidDefaultCase;
    }
    return VK_BLEND_FACTOR_ZERO;
}

inline VkBlendOp
ConvertBlendOp(GfxBlendOp op)
{
    switch(op)
    {
        case GfxBlendOp::Add: return VK_BLEND_OP_ADD;
        case GfxBlendOp::Subtract: return VK_BLEND_OP_SUBTRACT;
        case GfxBlendOp::RevSubtract: return VK_BLEND_OP_REVERSE_SUBTRACT;
        case GfxBlendOp::Min: return VK_BLEND_OP_MIN;
        case GfxBlendOp::Max: return VK_BLEND_OP_MAX;
    }
    return VK_BLEND_OP_ADD;
}

inline VkPrimitiveTopology
ConvertPrimitiveTopology(GfxPrimitiveTopology topology)
{
    switch(topology)
    {
        case GfxPrimitiveTopology::TriangleList: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case GfxPrimitiveTopology::TriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        case GfxPrimitiveTopology::PointList: return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        case GfxPrimitiveTopology::LineList: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case GfxPrimitiveTopology::LineStrip: return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        InvalidDefaultCase;
    }

    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
}

internal
VkImageAspectFlags DetermineAspectMaskFromFormat(VkFormat format, bool includeStencilBit)
{
	VkImageAspectFlags result = 0;
	switch (format)
	{
			// Depth
		case VK_FORMAT_D16_UNORM:
		case VK_FORMAT_X8_D24_UNORM_PACK32:
		case VK_FORMAT_D32_SFLOAT:
			result = VK_IMAGE_ASPECT_DEPTH_BIT;
			break;
			// Stencil
		case VK_FORMAT_S8_UINT:
			result = VK_IMAGE_ASPECT_STENCIL_BIT;
			break;
			// Depth/stencil
		case VK_FORMAT_D16_UNORM_S8_UINT:
		case VK_FORMAT_D24_UNORM_S8_UINT:
		case VK_FORMAT_D32_SFLOAT_S8_UINT:
			result = VK_IMAGE_ASPECT_DEPTH_BIT;
			if (includeStencilBit)
				result |= VK_IMAGE_ASPECT_STENCIL_BIT;
			break;
			// Assume everything else is Color
		default: result = VK_IMAGE_ASPECT_COLOR_BIT; break;
	}
	return result;
}
VkAccessFlags ConvertResourceStateToAccessFlags(GfxResourceState state)
{
	VkAccessFlags ret = 0;
	if (IS_ANY_FLAG_SET(state, GfxResourceState::CopySrc))
	{
		ret |= VK_ACCESS_TRANSFER_READ_BIT;
	}
	if (IS_ANY_FLAG_SET(state, GfxResourceState::CopyDst))
	{
		ret |= VK_ACCESS_TRANSFER_WRITE_BIT;
	}
	if (IS_ANY_FLAG_SET(state, GfxResourceState::VertexAndConstantBuffer))
	{
		ret |= VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
	}
	if (IS_ANY_FLAG_SET(state, GfxResourceState::IndexBuffer))
	{
		ret |= VK_ACCESS_INDEX_READ_BIT;
	}
	if (IS_ANY_FLAG_SET(state, GfxResourceState::UnorderedAccess))
	{
		ret |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	}
	if (IS_ANY_FLAG_SET(state, GfxResourceState::IndirectArgument))
	{
		ret |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
	}
	if (IS_ANY_FLAG_SET(state, GfxResourceState::RenderTarget))
	{
		ret |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	}
	if (IS_ANY_FLAG_SET(state, GfxResourceState::DepthWrite))
	{
		ret |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	}
	if (IS_ANY_FLAG_SET(state, GfxResourceState::ShaderResource))
	{
		ret |= VK_ACCESS_SHADER_READ_BIT;
	}
	if (IS_ANY_FLAG_SET(state, GfxResourceState::Present))
	{
		ret |= VK_ACCESS_MEMORY_READ_BIT;
	}

    // TODO(james): This will be needed if/when ray tracing is added
	// if (state & GfxResourceState::RayTracingAccel)
	// {
	// 	ret |= VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV;
	// }

	return ret;
}

VkImageLayout ConvertResourceStateToImageLayout(GfxResourceState usage)
{
	if (IS_ANY_FLAG_SET(usage, GfxResourceState::CopySrc))
		return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

	if (IS_ANY_FLAG_SET(usage, GfxResourceState::CopyDst))
		return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

	if (IS_ANY_FLAG_SET(usage, GfxResourceState::RenderTarget))
		return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	if (IS_ANY_FLAG_SET(usage, GfxResourceState::DepthWrite))
		return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	if (IS_ANY_FLAG_SET(usage, GfxResourceState::UnorderedAccess))
		return VK_IMAGE_LAYOUT_GENERAL;

	if (IS_ANY_FLAG_SET(usage, GfxResourceState::ShaderResource))
		return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	if (IS_ANY_FLAG_SET(usage, GfxResourceState::Present))
		return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	if (usage == GfxResourceState::Common)
		return VK_IMAGE_LAYOUT_GENERAL;

	return VK_IMAGE_LAYOUT_UNDEFINED;
}


internal void
vgInitialImageStateTransition(vg_device& device, vg_image* image, GfxResourceState resourceState)
{
    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(device.internal_cmd_buffer, &beginInfo);

    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;

    barrier.srcAccessMask = 0;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if(resourceState == GfxResourceState::UnorderedAccess)
    {
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    }
    else
    {
        barrier.dstAccessMask = ConvertResourceStateToAccessFlags(resourceState);
        barrier.newLayout = ConvertResourceStateToImageLayout(resourceState);
    }

    barrier.image = image->handle;
    barrier.subresourceRange.aspectMask = DetermineAspectMaskFromFormat(image->format, true);
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    vkCmdPipelineBarrier(device.internal_cmd_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, DeterminePipelineStageFlags(device, barrier.dstAccessMask, GfxQueueType::Graphics), 0, 0, nullptr, 0, nullptr, 1, &barrier);
    vkEndCommandBuffer(device.internal_cmd_buffer);
    // submit and wait for the device to transition the images
    VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &device.internal_cmd_buffer;
	submitInfo.signalSemaphoreCount = 0;
	submitInfo.pSignalSemaphores = nullptr; 

    vkQueueSubmit(device.q_graphics.handle, 1, &submitInfo, device.internal_cmd_fence);
    vkWaitForFences(device.handle, 1, &device.internal_cmd_fence, VK_TRUE, U64MAX);
    vkResetFences(device.handle, 1, &device.internal_cmd_fence);
    vkResetCommandPool(device.handle, device.internal_cmd_pool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
}

internal vg_queue*
GetQueueFromType(vg_device& device, GfxQueueType type)
{
    switch(type)
    {
        case GfxQueueType::Graphics: return &device.q_graphics;
        case GfxQueueType::Compute: return &device.q_compute;
        case GfxQueueType::Transfer: return &device.q_transfer;            
        InvalidDefaultCase;
    }

    return 0;
}

internal VkResult
vgCreateCommandPool(vg_device& device, GfxQueueType queueType, vg_command_encoder_pool* pool)
{
    vg_queue* queue = GetQueueFromType(device, queueType);
    VkCommandPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    poolInfo.queueFamilyIndex = queue->queue_family_index;

    for(u32 i = 0; i < FRAME_OVERLAP; ++i)
    {
        VkResult result = vkCreateCommandPool(device.handle, &poolInfo, nullptr, &pool->cmdPool[i]);
        if(result != VK_SUCCESS) return result;
    }
    pool->cmdcontexts = hashtable_create(device.arena, vg_cmd_context*, 32);
    pool->queueType = queueType;
    pool->queue = queue;

    return VK_SUCCESS;
}

inline vg_buffer*
FromGfxBuffer(vg_device& device, GfxBuffer resource)
{
    vg_resourceheap* pHeap = device.resourceHeaps->get(resource.heap);
    return pHeap->buffers->get(resource.id);
}

inline vg_image*
FromGfxTexture(vg_device& device, GfxTexture resource)
{
    vg_resourceheap* pHeap = device.resourceHeaps->get(resource.heap);
    return pHeap->textures->get(resource.id);
}

inline vg_sampler*
FromGfxSampler(vg_device& device, GfxSampler resource)
{
    vg_resourceheap* pHeap = device.resourceHeaps->get(resource.heap);
    return pHeap->samplers->get(resource.id);
}

inline vg_rendertargetview* 
FromGfxRenderTarget(vg_device& device, GfxRenderTarget resource)
{
    vg_resourceheap* pHeap = device.resourceHeaps->get(resource.heap);
    return pHeap->rtvs->get(resource.id);
}

inline vg_program*
FromGfxProgram(vg_device& device, GfxProgram resource)
{
    vg_resourceheap* pHeap = device.resourceHeaps->get(resource.heap);
    return pHeap->programs->get(resource.id);
}

inline vg_kernel*
FromGfxKernel(vg_device& device, GfxKernel resource)
{
    vg_resourceheap* pHeap = device.resourceHeaps->get(resource.heap);
    return pHeap->kernels->get(resource.id);
}

inline vg_cmd_context* 
FromGfxCmdContext(vg_device& device, GfxCmdContext cmds)
{
    vg_command_encoder_pool* pool = device.encoderPools->get(cmds.poolId);
    return pool->cmdcontexts->get(cmds.id);
}

inline VkCommandPool
CurrentFrameCmdPool(vg_device& device, vg_command_encoder_pool* pool)
{
    return pool->cmdPool[device.currentFrameIndex];
}

inline VkCommandBuffer
CurrentFrameCmdBuffer(vg_device& device, vg_cmd_context* context)
{
    return context->buffer[device.currentFrameIndex];
}

internal void
EndContextRenderPass(vg_device& device, vg_cmd_context* context)
{
    if(context->activeRenderpass)
    {
        VkCommandBuffer buffer = CurrentFrameCmdBuffer(device, context);
        vkCmdEndRenderPass(buffer);

        context->activeRenderpass = 0;
        context->activeFramebuffer = 0;
        context->activeKernel = 0;
        context->activeIB = 0;
        context->activeVB = 0;
        context->activeDescriptorSets = 0;
        context->shouldBindDescriptors = false;
    }
}

internal
VkResult CreateRenderPassForPipeline(VkDevice device, const GfxPipelineDesc& pipelineDesc, VkRenderPass* pRenderPass)
{
    VkAttachmentDescription attachments[GFX_MAX_RENDERTARGETS + 1] = {}; // +1 in case depth/stencil is attached
    VkAttachmentReference colorRefs[GFX_MAX_RENDERTARGETS] = {};
    VkAttachmentReference depthStencilRef = {};

    u32 numColorAttachments = pipelineDesc.numColorTargets;
    b32 hasDepthStencilAttachment = pipelineDesc.depthStencilTarget != TinyImageFormat_UNDEFINED ? true : false;

    for(u32 i = 0; i < numColorAttachments; ++i)
    {
        attachments[i].flags = 0;
        attachments[i].format = (VkFormat)TinyImageFormat_ToVkFormat(pipelineDesc.colorTargets[i]);
        attachments[i].samples = ConvertSampleCount(pipelineDesc.sampleCount);
        attachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[i].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachments[i].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        colorRefs[i].attachment = i;
        colorRefs[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    if(hasDepthStencilAttachment)
    {
        uint32_t idx = numColorAttachments;
		attachments[idx].flags = 0;
		attachments[idx].format = (VkFormat)TinyImageFormat_ToVkFormat(pipelineDesc.depthStencilTarget);
		attachments[idx].samples = ConvertSampleCount(pipelineDesc.sampleCount);
		attachments[idx].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[idx].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[idx].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[idx].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[idx].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		attachments[idx].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		depthStencilRef.attachment = idx;    
		depthStencilRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    // ugh.. silly subpass needs to redefine all the same crap
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = numColorAttachments;
    subpass.pColorAttachments = colorRefs;
    subpass.pDepthStencilAttachment = hasDepthStencilAttachment ? &depthStencilRef : nullptr;

    VkRenderPassCreateInfo renderPassInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    renderPassInfo.attachmentCount = numColorAttachments + (hasDepthStencilAttachment ? 1 : 0);
    renderPassInfo.pAttachments = attachments;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 0;
    renderPassInfo.pDependencies = nullptr;

    return vkCreateRenderPass(device, &renderPassInfo, nullptr, pRenderPass);
}

internal
GfxRenderTarget AcquireNextSwapChainTarget(GfxDevice deviceHandle)
{
    vg_device& device = DeviceObject::From(deviceHandle);

    VkResult result = vkWaitForFences(device.handle, 1, &device.pCurFrame->renderFence, VK_TRUE, UINT64_MAX);
    result = vkAcquireNextImageKHR(device.handle, device.swapChain, UINT64_MAX, device.pCurFrame->presentSemaphore, VK_NULL_HANDLE, &device.curSwapChainIndex);

    if(result == VK_ERROR_OUT_OF_DATE_KHR) 
    {
        ASSERT(false);
        // Win32RecreateSwapChain(graphics);
    }
    else if(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        ASSERT(false);
    }

    vgResetDescriptorPools(device);

    return GfxRenderTarget{0, device.curSwapChainIndex+1};
}

internal
GfxResourceHeap CreateResourceHeap( GfxDevice deviceHandle )
{
    vg_device& device = DeviceObject::From(deviceHandle);

    if(device.resourceHeaps->size() == device.resourceHeaps->capacity())
    {
        ASSERT(false);  // out of handles
        return GfxResourceHeap{GFX_INVALID_HANDLE};
    }

    vg_resourceheap* pHeap = vgAllocateResourceHeap();
    u64 key = { HASH(device.resourceHeaps->size()+1) };

    device.resourceHeaps->set(key, pHeap);

    return GfxResourceHeap{deviceHandle.id, key};
}

internal
GfxResult DestroyResourceHeap( GfxDevice deviceHandle, GfxResourceHeap resource )
{
    ASSERT(resource.id != GFX_INVALID_HANDLE);

    // TODO(james): may want to push this onto a free list for the active frame object or something to ensure the GPU isn't using it..

    vg_device& device = DeviceObject::From(deviceHandle);
    vg_resourceheap* pHeap = device.resourceHeaps->get(resource.id);

    VkResult result = vgDestroyResourceHeap(device, pHeap);
    
    device.resourceHeaps->erase(resource.id);

    return ToGfxResult(result);
}

internal
GfxBuffer CreateBuffer( GfxDevice deviceHandle, const GfxBufferDesc& bufferDesc, void const* data)
{
    vg_device& device = DeviceObject::From(deviceHandle);
    vg_resourceheap& heap = *device.resourceHeaps->get(bufferDesc.heap.id);

    VkBufferUsageFlags usage = 0;
    VmaMemoryUsage memUsage = VMA_MEMORY_USAGE_UNKNOWN;

    if(IS_FLAG_BIT_SET(bufferDesc.usageFlags, GfxBufferUsageFlags::Uniform)) usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if(IS_FLAG_BIT_SET(bufferDesc.usageFlags, GfxBufferUsageFlags::Vertex)) usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if(IS_FLAG_BIT_SET(bufferDesc.usageFlags, GfxBufferUsageFlags::Index)) usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if(IS_FLAG_BIT_SET(bufferDesc.usageFlags, GfxBufferUsageFlags::Storage)) usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    switch(bufferDesc.access)
    {
        case GfxMemoryAccess::GpuOnly:
            usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            memUsage = VMA_MEMORY_USAGE_GPU_ONLY;
            break;
        case GfxMemoryAccess::CpuOnly:
            usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            memUsage = VMA_MEMORY_USAGE_CPU_ONLY;
            break;
        case GfxMemoryAccess::CpuToGpu:
            memUsage = VMA_MEMORY_USAGE_CPU_TO_GPU;
            break;
        case GfxMemoryAccess::GpuToCpu:
            memUsage = VMA_MEMORY_USAGE_GPU_TO_CPU;
            break;
        InvalidDefaultCase;
    }

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = (VkDeviceSize)bufferDesc.size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vg_buffer* buffer = PushStruct(heap.arena, vg_buffer);

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = memUsage;
    VkResult result = vmaCreateBuffer(device.allocator, &bufferInfo, &allocInfo, &buffer->handle, &buffer->allocation, nullptr);
    if(result != VK_SUCCESS) return GfxBuffer{};

    if(data)
    {
        vmaMapMemory(device.allocator, buffer->allocation, &buffer->mapped);
        Copy(bufferDesc.size, data, buffer->mapped);
    }

    u64 key = HASH(heap.buffers->size()+1);
    heap.buffers->set(key, buffer);

    return GfxBuffer{ bufferDesc.heap.id, key };
}

internal
void* GetBufferData( GfxDevice deviceHandle, GfxBuffer resource)
{
    vg_device& device = DeviceObject::From(deviceHandle);
    vg_resourceheap* pHeap = device.resourceHeaps->get(resource.heap);
    vg_buffer* buffer = pHeap->buffers->get(resource.id);

    if(!buffer->mapped)
    {
        vmaMapMemory(device.allocator, buffer->allocation, &buffer->mapped);
    }

    return buffer->mapped;
}

internal
GfxResult DestroyBuffer( GfxDevice deviceHandle, GfxBuffer resource)
{
    vg_device& device = DeviceObject::From(deviceHandle);
    vg_resourceheap* pHeap = device.resourceHeaps->get(resource.heap);
    vg_buffer* buffer = pHeap->buffers->get(resource.id);

    if(buffer->mapped)
    {
        vmaUnmapMemory(device.allocator, buffer->allocation);
        buffer->mapped = 0;
    }

    vmaDestroyBuffer(device.allocator, buffer->handle, buffer->allocation);
    pHeap->buffers->erase(resource.id);

    return GfxResult::Ok;
}

internal
GfxTexture CreateTexture( GfxDevice deviceHandle, const GfxTextureDesc& textureDesc)
{
    vg_device& device = DeviceObject::From(deviceHandle);
    vg_resourceheap* pHeap = device.resourceHeaps->get(textureDesc.heap.id);

    VkImageType imageType = VK_IMAGE_TYPE_2D;
    VkImageViewType imageViewType = VK_IMAGE_VIEW_TYPE_2D;
    VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
    VmaMemoryUsage memUsage = VMA_MEMORY_USAGE_UNKNOWN;
    
    switch(textureDesc.access)
    {
        case GfxMemoryAccess::GpuOnly:
            memUsage = VMA_MEMORY_USAGE_GPU_ONLY;
            break;
        case GfxMemoryAccess::CpuOnly:
            tiling = VK_IMAGE_TILING_LINEAR;
            memUsage = VMA_MEMORY_USAGE_CPU_ONLY;
            break;
        case GfxMemoryAccess::CpuToGpu:
            tiling = VK_IMAGE_TILING_LINEAR;
            memUsage = VMA_MEMORY_USAGE_CPU_TO_GPU;
            break;
        case GfxMemoryAccess::GpuToCpu:
            tiling = VK_IMAGE_TILING_LINEAR;
            memUsage = VMA_MEMORY_USAGE_GPU_TO_CPU;
            break;
        InvalidDefaultCase;
    }

    switch(textureDesc.type)
    {
        case GfxTextureType::Tex2D: imageType = VK_IMAGE_TYPE_2D; imageViewType = VK_IMAGE_VIEW_TYPE_2D; break;
        case GfxTextureType::Tex2DArray: imageType = VK_IMAGE_TYPE_2D; imageViewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY; break;
        case GfxTextureType::Tex3D: imageType = VK_IMAGE_TYPE_3D; imageViewType = VK_IMAGE_VIEW_TYPE_3D; break;
        case GfxTextureType::Cube: imageType = VK_IMAGE_TYPE_2D; imageViewType = VK_IMAGE_VIEW_TYPE_CUBE; break;
        InvalidDefaultCase;
    }

    vg_image* image = PushStruct(pHeap->arena, vg_image);

    VkImageCreateInfo imageInfo = { };
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.pNext = nullptr;
	imageInfo.imageType = imageType;
	imageInfo.format = (VkFormat)TinyImageFormat_ToVkFormat(textureDesc.format);
	imageInfo.extent = { textureDesc.width, textureDesc.height, Maximum(textureDesc.depth, 1) };
	imageInfo.mipLevels = Maximum(textureDesc.mipLevels, 1);
	imageInfo.arrayLayers = Maximum(textureDesc.slice_count, 1);
	imageInfo.samples = ConvertSampleCount(textureDesc.sampleCount);
	imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if(textureDesc.mipLevels == 0)
    {
        // not specifying any mip levels likely indicates that we'll need to generate them on the fly
        // so let's specify the transfer src usage bit just in case...
        imageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = memUsage;
    VkResult result = vmaCreateImage(device.allocator, &imageInfo, &allocInfo, &image->handle, &image->allocation, nullptr);
    if(result != VK_SUCCESS) return GfxTexture{GFX_INVALID_HANDLE};

    // TODO(james): Look into making this a first class type in the graphics interface since it defines to bind to the shader
    VkImageViewCreateInfo viewInfo = {};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.viewType = imageViewType;
	viewInfo.image = image->handle;
	viewInfo.format = imageInfo.format;
    viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
	viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
	viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
	viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = imageInfo.mipLevels;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = imageInfo.arrayLayers;
	viewInfo.subresourceRange.aspectMask = DetermineAspectMaskFromFormat(imageInfo.format, true);

    result = vkCreateImageView(device.handle, &viewInfo, nullptr, &image->view);
    if(result != VK_SUCCESS)
    {
        vmaDestroyImage(device.allocator, image->handle, image->allocation);
        return GfxTexture{};
    }

    image->format = viewInfo.format;
    image->width = textureDesc.width;
    image->height = textureDesc.height;
    image->layers = Maximum(textureDesc.slice_count, 1);
    image->numMipLevels = imageInfo.mipLevels;

    u64 key = HASH(pHeap->textures->size()+1);
    pHeap->textures->set(key, image);

    return GfxTexture{textureDesc.heap.id, key};
}

internal
GfxResult DestroyTexture( GfxDevice deviceHandle, GfxTexture resource)
{
    vg_device& device = DeviceObject::From(deviceHandle);
    vg_resourceheap* pHeap = device.resourceHeaps->get(resource.heap);
    vg_image* image = pHeap->textures->get(resource.id);

    vkDestroyImageView(device.handle, image->view, nullptr);
    vmaDestroyImage(device.allocator, image->handle, image->allocation);
    pHeap->textures->erase(resource.id);

    return GfxResult::Ok;
}

internal
GfxSampler CreateSampler( GfxDevice deviceHandle, const GfxSamplerDesc& samplerDesc)
{
    vg_device& device = DeviceObject::From(deviceHandle);
    vg_resourceheap* pHeap = device.resourceHeaps->get(samplerDesc.heap.id);
    vg_sampler* sampler = PushStruct(pHeap->arena, vg_sampler);

    VkSamplerCreateInfo samplerInfo = vkInit_sampler_create_info(
        VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT
    );
    samplerInfo.anisotropyEnable = samplerDesc.enableAnisotropy ? VK_TRUE : VK_FALSE;
    samplerInfo.maxAnisotropy = device.device_properties.limits.maxSamplerAnisotropy;
    
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = samplerDesc.coordinatesNotNormalized ? VK_TRUE : VK_FALSE;     // False = [0..1,0..1], [True = 0..Width, 0..Height]
    
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

    samplerInfo.mipmapMode = (VkSamplerMipmapMode) samplerDesc.mipmapMode;
    samplerInfo.mipLodBias = samplerDesc.mipLodBias;
    samplerInfo.minLod = samplerDesc.minLod;
    samplerInfo.maxLod = samplerDesc.maxLod;

    samplerInfo.minFilter = (VkFilter) samplerDesc.minFilter;
    samplerInfo.magFilter = (VkFilter) samplerDesc.magFilter;

    samplerInfo.addressModeU = (VkSamplerAddressMode) samplerDesc.addressMode_U;
    samplerInfo.addressModeV = (VkSamplerAddressMode) samplerDesc.addressMode_V;
    samplerInfo.addressModeW = (VkSamplerAddressMode) samplerDesc.addressMode_W;
    
    VkResult result = vkCreateSampler(device.handle, &samplerInfo, nullptr, &sampler->handle);
    if(result != VK_SUCCESS) return GfxSampler{};

    u64 key = HASH(pHeap->samplers->size()+1);
    pHeap->samplers->set(key, sampler);

    return GfxSampler{samplerDesc.heap.id, key};
}

internal
GfxResult DestroySampler( GfxDevice deviceHandle, GfxSampler resource)
{
    vg_device& device = DeviceObject::From(deviceHandle);
    vg_resourceheap* pHeap = device.resourceHeaps->get(resource.heap);
    vg_sampler* sampler = pHeap->samplers->get(resource.id);

    vkDestroySampler(device.handle, sampler->handle, nullptr);
    pHeap->samplers->erase(resource.id);

    return GfxResult::Ok;
}

internal VkResult 
CreateProgramShader(vg_resourceheap* pHeap, VkDevice device, GfxShaderDesc* shaderDesc, vg_program* program)
{
    VkShaderModuleCreateInfo createInfo = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    createInfo.codeSize = shaderDesc->size;
    createInfo.pCode = (u32*)shaderDesc->data;

    const u32 idx = program->numShaders++;

    VkShaderModule shaderModule;
    VkResult result = vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);

    SpvReflectShaderModule* reflectModule = PushStruct(pHeap->arena, SpvReflectShaderModule);
    SpvReflectResult reflectResult = spvReflectCreateShaderModule(shaderDesc->size, shaderDesc->data, reflectModule);
    // pull the entry point from the reflection module if one isn't specified
    u32 entrypoint_index = 0;
    SpvReflectEntryPoint* pEntryPoint = reflectModule->entry_points;
    if(shaderDesc->szEntryPoint)
    {
        SpvReflectEntryPoint* pIter = pEntryPoint;
        while(pIter && !CompareStrings(shaderDesc->szEntryPoint, pIter->name))
        {
            entrypoint_index++;
            if(entrypoint_index >= reflectModule->entry_point_count)
            {
                ASSERT(false);
                pIter = 0;
                break;
            }

            if(pIter)
            {
                pEntryPoint = pIter;
            }
        }
    }
    program->entrypoints[idx] = pEntryPoint;
    program->shaderReflections[idx] = reflectModule;
    program->shaders[idx] = shaderModule;
    return result;
}

internal
GfxProgram CreateProgram( GfxDevice deviceHandle, const GfxProgramDesc& programDesc)
{
    VkResult result = VK_SUCCESS;
    vg_device& device = DeviceObject::From(deviceHandle);
    vg_resourceheap* pHeap = device.resourceHeaps->get(programDesc.heap.id);
    vg_program* program = PushStruct(pHeap->arena, vg_program);

    if(programDesc.compute)
    {
        result = CreateProgramShader(pHeap, device.handle, programDesc.compute, program);
    }
    
    if(programDesc.vertex && result == VK_SUCCESS)
    {
        result = CreateProgramShader(pHeap, device.handle, programDesc.vertex, program);
    }
    
    if(programDesc.hull && result == VK_SUCCESS)
    {
        result = CreateProgramShader(pHeap, device.handle, programDesc.hull, program);
    }
    
    if(programDesc.domain && result == VK_SUCCESS)
    {
        result = CreateProgramShader(pHeap, device.handle, programDesc.domain, program);
    }
    
    if(programDesc.geometry && result == VK_SUCCESS)
    {
        result = CreateProgramShader(pHeap, device.handle, programDesc.geometry, program);
    }
    
    if(programDesc.fragment && result == VK_SUCCESS)
    {
        result = CreateProgramShader(pHeap, device.handle, programDesc.fragment, program);
    }

    if(result == VK_SUCCESS)
    {
        u32 totalPushConstants = 0;
        u32 descriptorSetLayoutCount = 0;
        for(u32 shaderIdx = 0; shaderIdx < program->numShaders; ++shaderIdx)
        {
            totalPushConstants += program->entrypoints[shaderIdx]->used_push_constant_count;
            descriptorSetLayoutCount += program->entrypoints[shaderIdx]->descriptor_set_count;
        }
        
        u32 pushConstantCount = 0;
        VkPushConstantRange* pushConstants = nullptr;
        if(totalPushConstants > 0)
        {
            pushConstants = PushArray(*device.frameArena, totalPushConstants, VkPushConstantRange);
        }

        program->descriptorSetLayouts = array_create(pHeap->arena, VkDescriptorSetLayout, descriptorSetLayoutCount);
        program->mapBindingDesc = hashtable_create(pHeap->arena, vg_program_binding_desc, 1024); // NOTE(james): 1024 bindings is waaay overkill, but it's just a pointer...
        program->mapPushConstantDesc = hashtable_create(pHeap->arena, vg_program_pushconstant_desc, 32);    // NOTE(james): 32 push constants should be enough. Only have 128 bytes

        temporary_memory temp = BeginTemporaryMemory(pHeap->arena);

        for(u32 shaderIdx = 0; shaderIdx < program->numShaders; ++shaderIdx)
        {
            SpvReflectShaderModule& module = *program->shaderReflections[shaderIdx];
            SpvReflectEntryPoint& entrypoint = *program->entrypoints[shaderIdx];

            for(u32 setIdx = 0; setIdx < entrypoint.descriptor_set_count; ++setIdx)
            {
                SpvReflectDescriptorSet& set = entrypoint.descriptor_sets[setIdx];
                VkDescriptorSetLayoutBinding* descriptorBindings = PushArray(pHeap->arena, set.binding_count, VkDescriptorSetLayoutBinding);

                for(u32 bindingIdx = 0; bindingIdx < set.binding_count; ++bindingIdx)
                {
                    const SpvReflectDescriptorBinding& spvBinding = *set.bindings[bindingIdx];
                    VkDescriptorSetLayoutBinding& binding = descriptorBindings[bindingIdx];
                    binding.binding = spvBinding.binding;
                    binding.descriptorType = (VkDescriptorType)spvBinding.descriptor_type;
                    binding.descriptorCount = spvBinding.count;
                    binding.stageFlags = (VkShaderStageFlags)entrypoint.shader_stage;

                    vg_program_binding_desc binding_desc = {};
#if PROJECTSUPER_INTERNAL
                    CopyString(spvBinding.name, binding_desc.name, GFX_MAX_SHADER_IDENTIFIER_NAME_LENGTH);
#endif
                    binding_desc.set = set.set;
                    binding_desc.binding = spvBinding.binding;
                    program->mapBindingDesc->set(strhash64(spvBinding.name, StringLength(spvBinding.name)-1), binding_desc);
                }

                VkDescriptorSetLayoutCreateInfo descLayoutInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
                descLayoutInfo.bindingCount = set.binding_count;
                descLayoutInfo.pBindings = descriptorBindings;

                VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
                result = vkCreateDescriptorSetLayout(device.handle, &descLayoutInfo, nullptr, &setLayout);
                if(result == VK_SUCCESS)
                {
                    program->descriptorSetLayouts->push_back(setLayout);
                }
            }

            for(u32 i = 0; i < entrypoint.used_push_constant_count; ++i)
            {
                SpvReflectBlockVariable* block = nullptr;
                for(u32 pci = 0; !block && pci < module.push_constant_block_count; ++pci)
                {
                    if(module.push_constant_blocks[pci].spirv_id == entrypoint.used_push_constants[i])
                        block = &module.push_constant_blocks[pci];
                }
                VkPushConstantRange& pushConstant = pushConstants[pushConstantCount++];
                pushConstant.offset = (VkDeviceSize)block->offset;
                pushConstant.size = (VkDeviceSize)block->size;
                pushConstant.stageFlags = entrypoint.shader_stage;

                vg_program_pushconstant_desc pc_desc = {};
#if PROJECTSUPER_INTERNAL
                CopyString(block->name, pc_desc.name, GFX_MAX_SHADER_IDENTIFIER_NAME_LENGTH);
#endif
                pc_desc.offset = block->offset;
                pc_desc.size = block->size;
                pc_desc.shaderStage = entrypoint.shader_stage;

                program->mapPushConstantDesc->set(strhash64(block->name, StringLength(block->name)-1), pc_desc);
            }
        }

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pipelineLayoutInfo.flags = 0;
        pipelineLayoutInfo.setLayoutCount = program->descriptorSetLayouts->size();
        pipelineLayoutInfo.pSetLayouts = program->descriptorSetLayouts->data();
        pipelineLayoutInfo.pushConstantRangeCount = pushConstantCount;
        pipelineLayoutInfo.pPushConstantRanges = pushConstants;

        VkPipelineLayout pipelineLayout = nullptr;    
        result = vkCreatePipelineLayout(device.handle, &pipelineLayoutInfo, nullptr, &pipelineLayout);

        EndTemporaryMemory(temp);
        if(result == VK_SUCCESS)
        {
            program->pipelineLayout = pipelineLayout;
        }
    }

    if(result != VK_SUCCESS)
    {
        for(u32 i = 0; i < program->numShaders; ++i)
        {
            spvReflectDestroyShaderModule(program->shaderReflections[i]);
            vkDestroyShaderModule(device.handle, program->shaders[i], nullptr);
        }

        if(program->pipelineLayout != nullptr)
        {
            vkDestroyPipelineLayout(device.handle, program->pipelineLayout, nullptr);
        }

        for(auto setLayout: *(program->descriptorSetLayouts))
        {
            vkDestroyDescriptorSetLayout(device.handle, setLayout, nullptr);
        }

        return GfxProgram{};
    }

    u64 key = HASH(pHeap->programs->size()+1);
    pHeap->programs->set(key, program);

    return GfxProgram{programDesc.heap.id, key};
}

internal
GfxResult DestroyProgram( GfxDevice deviceHandle, GfxProgram resource)
{
    vg_device& device = DeviceObject::From(deviceHandle);
    vg_resourceheap* pHeap = device.resourceHeaps->get(resource.heap);
    vg_program* program = pHeap->programs->get(resource.id);

    for(u32 i = 0; i < program->numShaders; ++i)
    {
        spvReflectDestroyShaderModule(program->shaderReflections[i]);
        vkDestroyShaderModule(device.handle, program->shaders[i], nullptr);
    }
    if(program->pipelineLayout != nullptr)
    {
        vkDestroyPipelineLayout(device.handle, program->pipelineLayout, nullptr);
    }
    for(auto setLayout: *(program->descriptorSetLayouts))
    {
        vkDestroyDescriptorSetLayout(device.handle, setLayout, nullptr);
    }
    pHeap->programs->erase(resource.id);

    return GfxResult::Ok;
}

internal
GfxKernel CreateComputeKernel( GfxDevice deviceHandle, GfxProgram program)
{
    NotImplemented;
    return GfxKernel{};
}

internal
GfxRenderTarget CreateRenderTarget( GfxDevice deviceHandle, const GfxRenderTargetDesc& rtvDesc)
{
    vg_device& device = DeviceObject::From(deviceHandle);
    vg_resourceheap* pHeap = device.resourceHeaps->get(rtvDesc.heap.id);

    temporary_memory scoped = BeginTemporaryMemory(pHeap->arena);

    vg_image* image = PushStruct(pHeap->arena, vg_image);

    b32 isDepthStencil = TinyImageFormat_IsDepthOnly(rtvDesc.format) || TinyImageFormat_IsDepthAndStencil(rtvDesc.format);

    VkImageType imageType = VK_IMAGE_TYPE_2D;
    VkImageViewType imageViewType = VK_IMAGE_VIEW_TYPE_2D;
    VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
    VmaMemoryUsage memUsage = VMA_MEMORY_USAGE_UNKNOWN;
    
    switch(rtvDesc.access)
    {
        case GfxMemoryAccess::GpuOnly:
            memUsage = VMA_MEMORY_USAGE_GPU_ONLY;
            break;
        case GfxMemoryAccess::CpuOnly:
            tiling = VK_IMAGE_TILING_LINEAR;
            memUsage = VMA_MEMORY_USAGE_CPU_ONLY;
            break;
        case GfxMemoryAccess::CpuToGpu:
            tiling = VK_IMAGE_TILING_LINEAR;
            memUsage = VMA_MEMORY_USAGE_CPU_TO_GPU;
            break;
        case GfxMemoryAccess::GpuToCpu:
            tiling = VK_IMAGE_TILING_LINEAR;
            memUsage = VMA_MEMORY_USAGE_GPU_TO_CPU;
            break;
        InvalidDefaultCase;
    }

    switch(rtvDesc.type)
    {
        case GfxTextureType::Tex2D: imageType = VK_IMAGE_TYPE_2D; imageViewType = VK_IMAGE_VIEW_TYPE_2D; break;
        case GfxTextureType::Tex2DArray: imageType = VK_IMAGE_TYPE_2D; imageViewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY; break;
        case GfxTextureType::Tex3D: imageType = VK_IMAGE_TYPE_3D; imageViewType = VK_IMAGE_VIEW_TYPE_3D; break;
        case GfxTextureType::Cube: imageType = VK_IMAGE_TYPE_2D; imageViewType = VK_IMAGE_VIEW_TYPE_CUBE; break;
        InvalidDefaultCase;
    }

    VkImageCreateInfo imageInfo = { };
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.pNext = nullptr;
	imageInfo.imageType = imageType;
	imageInfo.format = (VkFormat)TinyImageFormat_ToVkFormat(rtvDesc.format);
	imageInfo.extent = { rtvDesc.width, rtvDesc.height, Maximum(rtvDesc.depth, 1) };
	imageInfo.mipLevels = Maximum(rtvDesc.mipLevels, 1);
	imageInfo.arrayLayers = Maximum(rtvDesc.slice_count, 1);
	imageInfo.samples = ConvertSampleCount(rtvDesc.sampleCount);
	imageInfo.usage = isDepthStencil ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    image->width = rtvDesc.width;
    image->height = rtvDesc.height;
    image->layers = Maximum(rtvDesc.depth, 1);
    image->format = imageInfo.format;
    image->numMipLevels = imageInfo.mipLevels;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = memUsage;
    VkResult result = vmaCreateImage(device.allocator, &imageInfo, &allocInfo, &image->handle, &image->allocation, nullptr);
    if(result != VK_SUCCESS) 
    {
        EndTemporaryMemory(scoped);
        return GfxRenderTarget{};
    }

    VkImageViewCreateInfo viewInfo = {};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.viewType = imageViewType;  
	viewInfo.image = image->handle;
	viewInfo.format = image->format;
    viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
	viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
	viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
	viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = image->layers;   
	viewInfo.subresourceRange.aspectMask = DetermineAspectMaskFromFormat(image->format, true);  

    result = vkCreateImageView(device.handle, &viewInfo, nullptr, &image->view);
    if(result != VK_SUCCESS) 
    {
        vkDestroyImage(device.handle, image->handle, nullptr);
        EndTemporaryMemory(scoped);
        return GfxRenderTarget{};
    }

    KeepTemporaryMemory(scoped);

    vgInitialImageStateTransition(device, image, rtvDesc.initialState);

    u64 imageKey = HASH(pHeap->textures->size() + 1);
    pHeap->textures->set(imageKey, image);

    // This is just an image view with a little extra information on the device side
    vg_rendertargetview* rtv = PushStruct(pHeap->arena, vg_rendertargetview);
    rtv->image = image;
    rtv->textureKey = imageKey;
    rtv->loadOp = ConvertLoadOp(rtvDesc.loadOp);
    rtv->clearValue = VkClearValue{rtvDesc.clearValue[0],rtvDesc.clearValue[1],rtvDesc.clearValue[2],rtvDesc.clearValue[3]};
    rtv->sampleCount = ConvertSampleCount(rtvDesc.sampleCount);

    u64 key = HASH(pHeap->rtvs->size() + 1);
    pHeap->rtvs->set(key, rtv);

    return GfxRenderTarget{rtvDesc.heap.id, key};
}

internal
GfxResult DestroyRenderTarget( GfxDevice deviceHandle, GfxRenderTarget resource )
{
    vg_device& device = DeviceObject::From(deviceHandle);
    vg_resourceheap* pHeap = device.resourceHeaps->get(resource.heap);
    vg_rendertargetview& rtv = *pHeap->rtvs->get(resource.id);
    vg_image* image = pHeap->textures->get(rtv.textureKey);

    vkDestroyImageView(device.handle, image->view, nullptr);
    vmaDestroyImage(device.allocator, image->handle, image->allocation);
    pHeap->textures->erase(rtv.textureKey);
    pHeap->rtvs->erase(resource.id);

    return GfxResult::Ok;
}

internal
TinyImageFormat GetDeviceBackBufferFormat( GfxDevice deviceHandle )
{
    vg_device& device = DeviceObject::From(deviceHandle);

    return TinyImageFormat_FromVkFormat((TinyImageFormat_VkFormat)device.swapChainFormat);
}

internal
GfxKernel CreateGraphicsKernel( GfxDevice deviceHandle, GfxProgram resource, const GfxPipelineDesc& pipelineDesc)
{
    vg_device& device = DeviceObject::From(deviceHandle);
    vg_resourceheap* pHeap = device.resourceHeaps->get(resource.heap);
    vg_program* program = pHeap->programs->get(resource.id);
    pHeap = device.resourceHeaps->get(pipelineDesc.heap.id);

    // just create a temporary render pass for pipeline creation
    VkRenderPass renderpass = VK_NULL_HANDLE;
    VkResult result = CreateRenderPassForPipeline(device.handle, pipelineDesc, &renderpass);
    if(result != VK_SUCCESS) return GfxKernel{};     

    temporary_memory temp = BeginTemporaryMemory(pHeap->arena);

    VkVertexInputBindingDescription vertexBindingDesc = {};
    vertexBindingDesc.binding = 0;
    vertexBindingDesc.stride = 0;   // full stride will get computed from vertex shader reflection
    vertexBindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; // OR VK_VERTEX_INPUT_RATE_INSTANCE??

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;

    VkPipelineShaderStageCreateInfo shaderStages[VG_MAX_PROGRAM_SHADER_COUNT];

    for(u32 i = 0; i < program->numShaders; ++i)
    {
        const SpvReflectEntryPoint& entrypoint = *program->entrypoints[i];
        shaderStages[i] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        shaderStages[i].stage = (VkShaderStageFlagBits)entrypoint.shader_stage;
        shaderStages[i].module = program->shaders[i];
        shaderStages[i].pName = entrypoint.name;

        if(entrypoint.shader_stage & SPV_REFLECT_SHADER_STAGE_VERTEX_BIT)
        {
            // Simplifying assumptions:
            // - All vertex input attributes are sourced from a single vertex buffer,
            //   bound to VB slot 0.
            // - Each vertex's attribute are laid out in ascending order by location.
            // - The format of each attribute matches its usage in the shader;
            //   float4 -> VK_FORMAT_R32G32B32A32_FLOAT, etc. No attribute compression is applied.
            // - All attributes are provided per-vertex, not per-instance.
            vertexInputInfo.vertexBindingDescriptionCount = 1;
            vertexInputInfo.pVertexBindingDescriptions = &vertexBindingDesc;
            vertexInputInfo.vertexAttributeDescriptionCount = entrypoint.input_variable_count;

            VkVertexInputAttributeDescription* pAttributeDescriptions = PushArray(pHeap->arena, entrypoint.input_variable_count, VkVertexInputAttributeDescription);
            vertexInputInfo.pVertexAttributeDescriptions = pAttributeDescriptions;
            slice<VkVertexInputAttributeDescription> attrSlice = make_slice(pAttributeDescriptions, entrypoint.input_variable_count);

            for(u32 attr_idx = 0; attr_idx < attrSlice.size(); ++attr_idx)
            {
                const SpvReflectInterfaceVariable& input = *(entrypoint.input_variables[attr_idx]);
                VkVertexInputAttributeDescription& attr = attrSlice[attr_idx];
                attr.location = input.location;
                attr.binding = vertexBindingDesc.binding;
                attr.format = (VkFormat)input.format;
                attr.offset = 0;    // final value computed below
            }

            sort::quickSort(attrSlice, [](const VkVertexInputAttributeDescription& a, const VkVertexInputAttributeDescription& b){return a.location < b.location;});

            for(auto& attr: attrSlice)
            {
                u32 size = vkInit_GetFormatSize(attr.format);
                attr.offset = vertexBindingDesc.stride;
                vertexBindingDesc.stride += size;
            }
        }
    }

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssembly.primitiveRestartEnable = VK_FALSE;
    inputAssembly.topology = ConvertPrimitiveTopology(pipelineDesc.primitiveTopology);

    VkPipelineViewportStateCreateInfo viewportState = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.flags = 0;
    viewportState.viewportCount = 1;    // using dynamic viewport state, but still have to set to 1
    viewportState.pViewports = nullptr;
    viewportState.scissorCount = 1;
    viewportState.pScissors = nullptr;

    VkPipelineMultisampleStateCreateInfo multisampleState = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampleState.flags = 0;
    multisampleState.rasterizationSamples = ConvertSampleCount(pipelineDesc.sampleCount);
    multisampleState.sampleShadingEnable = VK_FALSE;
    multisampleState.minSampleShading = 0.0f;
    multisampleState.pSampleMask = 0;
    multisampleState.alphaToCoverageEnable = VK_FALSE;
    multisampleState.alphaToOneEnable = VK_FALSE;

    VkDynamicState dyn_states[] =
    {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        // VK_DYNAMIC_STATE_BLEND_CONSTANTS,
        // VK_DYNAMIC_STATE_DEPTH_BOUNDS,
        // VK_DYNAMIC_STATE_STENCIL_REFERENCE,
    };
    VkPipelineDynamicStateCreateInfo dynamicInfo = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamicInfo.flags = 0;
    dynamicInfo.dynamicStateCount = ARRAY_COUNT(dyn_states);
    dynamicInfo.pDynamicStates = dyn_states;

    VkPipelineRasterizationStateCreateInfo rs = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.flags = 0;
    rs.depthClampEnable = pipelineDesc.rasterizerState.depthClampEnable ? VK_TRUE : VK_FALSE;
    rs.rasterizerDiscardEnable = VK_FALSE;
    rs.polygonMode = ConvertFillMode(pipelineDesc.rasterizerState.fillMode);
    rs.cullMode = ConvertCullMode(pipelineDesc.rasterizerState.cullMode);
    rs.frontFace = pipelineDesc.rasterizerState.frontCCW ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
    rs.depthBiasEnable = pipelineDesc.rasterizerState.depthBias != 0 ? VK_TRUE : VK_FALSE;
    rs.depthBiasConstantFactor = (f32)pipelineDesc.rasterizerState.depthBias;
    rs.depthBiasClamp = 0.0f;
    rs.depthBiasSlopeFactor = pipelineDesc.rasterizerState.slopeScaledDepthBias;
    rs.lineWidth = 1;

    VkPipelineDepthStencilStateCreateInfo ds = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    ds.flags = 0;
    ds.depthTestEnable = pipelineDesc.depthStencilState.depthEnable ? VK_TRUE : VK_FALSE;
    ds.depthWriteEnable = pipelineDesc.depthStencilState.depthWriteMask == GfxDepthWriteMask::All ? VK_TRUE : VK_FALSE;
    ds.depthCompareOp = ConvertComparisonFunc(pipelineDesc.depthStencilState.depthFunc);
    ds.depthBoundsTestEnable = pipelineDesc.depthStencilState.depthBoundsTestEnable ? VK_TRUE : VK_FALSE;
    ds.stencilTestEnable = pipelineDesc.depthStencilState.stencilEnable ? VK_TRUE : VK_FALSE;
    ds.front.failOp = ConvertStencilOp(pipelineDesc.depthStencilState.frontFace.stencilFail);
    ds.front.passOp = ConvertStencilOp(pipelineDesc.depthStencilState.frontFace.stencilPass);
    ds.front.depthFailOp = ConvertStencilOp(pipelineDesc.depthStencilState.frontFace.depthFail);
    ds.front.compareOp = ConvertComparisonFunc(pipelineDesc.depthStencilState.frontFace.stencilFunc);
    ds.front.compareMask = pipelineDesc.depthStencilState.stencilReadMask;
    ds.front.writeMask = pipelineDesc.depthStencilState.stencilWriteMask;
    ds.front.reference = 0;
    ds.back.failOp = ConvertStencilOp(pipelineDesc.depthStencilState.backFace.stencilFail);
    ds.back.passOp = ConvertStencilOp(pipelineDesc.depthStencilState.backFace.stencilPass);
    ds.back.depthFailOp = ConvertStencilOp(pipelineDesc.depthStencilState.backFace.depthFail);
    ds.back.compareOp = ConvertComparisonFunc(pipelineDesc.depthStencilState.backFace.stencilFunc);
    ds.back.compareMask = pipelineDesc.depthStencilState.stencilReadMask;
    ds.back.writeMask = pipelineDesc.depthStencilState.stencilWriteMask;
    ds.back.reference = 0;
    ds.minDepthBounds = 0;
    ds.maxDepthBounds = 1;

    VkPipelineColorBlendAttachmentState blendAttachments[GFX_MAX_RENDERTARGETS];
    for(u32 i = 0; i < pipelineDesc.numColorTargets; ++i)
    {
        const GfxRenderTargetBlendState& rtblend = pipelineDesc.blendState.renderTargets[i];

        blendAttachments[i].blendEnable = rtblend.blendEnable ? VK_TRUE : VK_FALSE;
        blendAttachments[i].colorWriteMask = (VkColorComponentFlags)rtblend.colorWriteMask;
        blendAttachments[i].srcColorBlendFactor = ConvertBlendMode(rtblend.srcBlend);
        blendAttachments[i].dstColorBlendFactor = ConvertBlendMode(rtblend.destBlend);
        blendAttachments[i].colorBlendOp = ConvertBlendOp(rtblend.blendOp);
        blendAttachments[i].srcAlphaBlendFactor = ConvertBlendMode(rtblend.srcAlphaBlend);
        blendAttachments[i].dstAlphaBlendFactor = ConvertBlendMode(rtblend.destAlphaBlend);
        blendAttachments[i].alphaBlendOp = ConvertBlendOp(rtblend.blendOpAlpha);
    }

    VkPipelineColorBlendStateCreateInfo cb = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cb.flags = 0;
	cb.logicOpEnable = VK_FALSE;
	cb.logicOp = VK_LOGIC_OP_COPY;
    cb.attachmentCount = pipelineDesc.numColorTargets;
	cb.pAttachments = blendAttachments;
	cb.blendConstants[0] = 0.0f;
	cb.blendConstants[1] = 0.0f;
	cb.blendConstants[2] = 0.0f;
	cb.blendConstants[3] = 0.0f;

    VkGraphicsPipelineCreateInfo pipelineInfo = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipelineInfo.stageCount = program->numShaders;  
    pipelineInfo.pStages = shaderStages; 
    pipelineInfo.pVertexInputState = &vertexInputInfo; 
    pipelineInfo.pInputAssemblyState = &inputAssembly; 
    pipelineInfo.pViewportState = &viewportState; 
    pipelineInfo.pRasterizationState = &rs; 
    pipelineInfo.pMultisampleState = &multisampleState;  
    pipelineInfo.pDepthStencilState = &ds; 
    pipelineInfo.pColorBlendState = &cb; 
    pipelineInfo.pDynamicState = &dynamicInfo;  
    pipelineInfo.layout = program->pipelineLayout;    
    pipelineInfo.renderPass = renderpass;
    // TODO(james): use base pipeline for templating...

    VkPipeline pipeline;
    // TODO(james): Use pipeline cache to speed up initialization
    result = vkCreateGraphicsPipelines(device.handle, nullptr, 1, &pipelineInfo, nullptr, &pipeline);

    EndTemporaryMemory(temp);

    // NOTE(james): No matter what, clean up the temporary render pass...
    vkDestroyRenderPass(device.handle, renderpass, nullptr);

    if(result != VK_SUCCESS)
    {
        return GfxKernel{};
    }

    // track the kernel resource...
    vg_kernel* kernel = PushStruct(pHeap->arena, vg_kernel);

    kernel->pipeline = pipeline;
    kernel->program = program;

    u64 key = HASH(pHeap->kernels->size()+1);
    pHeap->kernels->set(key, kernel);

    return GfxKernel{pipelineDesc.heap.id, key};
}

internal
GfxResult DestroyKernel( GfxDevice deviceHandle, GfxKernel resource)
{
    vg_device& device = DeviceObject::From(deviceHandle);
    vg_resourceheap* pHeap = device.resourceHeaps->get(resource.heap);
    vg_kernel* kernel = pHeap->kernels->get(resource.id);    

    vkDestroyPipeline(device.handle, kernel->pipeline, nullptr);
    pHeap->kernels->erase(resource.id);

    return GfxResult::Ok;
}

internal
GfxCmdEncoderPool CreateEncoderPool( GfxDevice deviceHandle, const GfxCmdEncoderPoolDesc& poolDesc)
{
    vg_device& device = DeviceObject::From(deviceHandle);

    if(device.encoderPools->full()) return GfxCmdEncoderPool{};  // out of handles
    
    temporary_memory temp = BeginTemporaryMemory(device.arena);
    vg_command_encoder_pool* pool = PushStruct(device.arena, vg_command_encoder_pool);

    VkResult result = vgCreateCommandPool(device, poolDesc.queueType, pool);
    if(result != VK_SUCCESS)
    {
        EndTemporaryMemory(temp);
        return GfxCmdEncoderPool{};
    }
    KeepTemporaryMemory(temp);

    u64 key = HASH(device.encoderPools->size()+1);
    device.encoderPools->set(key, pool);
    return GfxCmdEncoderPool{deviceHandle.id, key};
}

internal
GfxResult DestroyCmdEncoderPool( GfxDevice deviceHandle, GfxCmdEncoderPool resource)
{
    vg_device& device = DeviceObject::From(deviceHandle);
    vg_command_encoder_pool* pool = device.encoderPools->get(resource.id);

    vgDestroyCmdEncoderPool(device, pool);

    device.encoderPools->erase(resource.id);

    return GfxResult::Ok;
}

internal
GfxCmdContext CreateEncoderContext( GfxCmdEncoderPool resource)
{
    vg_device& device = DeviceObject::From(resource.deviceId);
    vg_command_encoder_pool* pool = device.encoderPools->get(resource.id);

    if(pool->cmdcontexts->full()) return GfxCmdContext{};

    temporary_memory scoped = BeginTemporaryMemory(device.arena);
    vg_cmd_context* context = PushStruct(device.arena, vg_cmd_context);

    for(u32 i = 0; i < FRAME_OVERLAP; ++i)
    {
        VkCommandBufferAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocInfo.commandBufferCount = 1;
        allocInfo.commandPool = pool->cmdPool[i];
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

        VkResult result = vkAllocateCommandBuffers(device.handle, &allocInfo, &context->buffer[i]);
        if(result != VK_SUCCESS)
        {
            EndTemporaryMemory(scoped);
            return GfxCmdContext{};
        }
    }

    KeepTemporaryMemory(scoped);

    u64 key = HASH(pool->cmdcontexts->size()+1);
    pool->cmdcontexts->set(key, context);

    return GfxCmdContext{resource.deviceId, resource.id, key};
}

internal
GfxResult CreateEncoderContexts(GfxCmdEncoderPool resource, u32 numContexts, GfxCmdContext* pContexts)
{
    vg_device& device = DeviceObject::From(resource.deviceId);
    vg_command_encoder_pool* pool = device.encoderPools->get(resource.id);

    temporary_memory scoped = BeginTemporaryMemory(device.arena);

    for(u32 frameIdx = 0; frameIdx < FRAME_OVERLAP; ++frameIdx)
    {
        VkCommandBuffer* buffers = PushArray(*device.frameArena, numContexts, VkCommandBuffer);
        VkCommandBufferAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocInfo.commandBufferCount = numContexts;
        allocInfo.commandPool = pool->cmdPool[frameIdx];
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

        VkResult result = vkAllocateCommandBuffers(device.handle, &allocInfo, buffers);
        if(result != VK_SUCCESS) 
        {
            EndTemporaryMemory(scoped);
            return ToGfxResult(result);
        }

        for(u32 i = 0; i < numContexts; ++i)
        {
            if(pool->cmdcontexts->full()) 
            {
                EndTemporaryMemory(scoped);
                return GfxResult::OutOfHandles;
            }

            vg_cmd_context* context = PushStruct(device.arena, vg_cmd_context);
            context->buffer[frameIdx] = buffers[i];

            u64 key = HASH(pool->cmdcontexts->size()+1);
            pool->cmdcontexts->set(key, context);
            pContexts[i].deviceId = resource.deviceId;
            pContexts[i].poolId = resource.id;
            pContexts[i].id = key;
        }
    }

    KeepTemporaryMemory(scoped);

    return GfxResult::Ok;
}

internal
GfxResult ResetCmdEncoderPool( GfxCmdEncoderPool resource)
{
    vg_device& device = DeviceObject::From(resource.deviceId);
    vg_command_encoder_pool* pool = device.encoderPools->get(resource.id);
    
    vkResetCommandPool(device.handle, CurrentFrameCmdPool(device, pool), VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);

    return GfxResult::Ok;
}

internal
GfxResult BeginEncodingCmds(GfxCmdContext cmds)
{
    vg_device& device = DeviceObject::From(cmds.deviceId);
    vg_cmd_context* context = FromGfxCmdContext(device, cmds);

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VkResult result = vkBeginCommandBuffer(CurrentFrameCmdBuffer(device, context), &beginInfo);

    return ToGfxResult(result);
}

internal
GfxResult EndEncodingCmds(GfxCmdContext cmds)
{
    vg_device& device = DeviceObject::From(cmds.deviceId);
    vg_cmd_context* context = FromGfxCmdContext(device, cmds);
    VkCommandBuffer buffer = CurrentFrameCmdBuffer(device, context);

    EndContextRenderPass(device, context);

    VkResult result = vkEndCommandBuffer(buffer);

    return ToGfxResult(result);
}

internal GfxResult
CmdResourceBarrier(GfxCmdContext cmds, 
    u32 numBufferBarriers, GfxBufferBarrier* pBufferBarriers,
    u32 numTextureBarriers, GfxTextureBarrier* pTextureBarriers,
    u32 numRenderTargetBarriers, GfxRenderTargetBarrier* pRenderTargetBarriers)
{
    vg_device& device = DeviceObject::From(cmds.deviceId);
    vg_command_encoder_pool* pool = device.encoderPools->get(cmds.poolId);
    vg_cmd_context* context = pool->cmdcontexts->get(cmds.id);;
    VkCommandBuffer cmdBuffer = CurrentFrameCmdBuffer(device, context);

    u32 numImgBarriers = numTextureBarriers + numRenderTargetBarriers;

    VkBufferMemoryBarrier* bufBarriers = numBufferBarriers ? PushArray(*device.frameArena, numBufferBarriers, VkBufferMemoryBarrier) : nullptr;
    VkImageMemoryBarrier*  imgBarriers = numImgBarriers ? PushArray(*device.frameArena, numImgBarriers, VkImageMemoryBarrier) : nullptr;

    VkAccessFlags srcAccessFlags = 0;
	VkAccessFlags dstAccessFlags = 0;

    for(u32 i = 0; i < numBufferBarriers; ++i)
    {
        const GfxBufferBarrier& gfxBarrier = pBufferBarriers[i];
        VkBufferMemoryBarrier& barrier = bufBarriers[i];
        vg_buffer* buffer = FromGfxBuffer(device, gfxBarrier.buffer);

        barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;

        if(gfxBarrier.currentState == GfxResourceState::UnorderedAccess && gfxBarrier.newState == GfxResourceState::UnorderedAccess)
        {
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
        }
        else
        {
            barrier.srcAccessMask = ConvertResourceStateToAccessFlags(gfxBarrier.currentState);
            barrier.dstAccessMask = ConvertResourceStateToAccessFlags(gfxBarrier.newState);
        }

        barrier.buffer = buffer->handle;
        barrier.size = VK_WHOLE_SIZE;
        barrier.offset = 0;

        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        if(gfxBarrier.currentState != GfxResourceState::Undefined)
        {
            switch(gfxBarrier.resourceOp)
            {
                case GfxQueueResourceOp::None:
                    break;
                case GfxQueueResourceOp::Acquire:
                    barrier.srcQueueFamilyIndex = GetQueueFromType(device, gfxBarrier.resourceQueue)->queue_family_index;
                    barrier.dstQueueFamilyIndex = pool->queue->queue_family_index;
                    break;
                case GfxQueueResourceOp::Release:
                    barrier.srcQueueFamilyIndex = pool->queue->queue_family_index;
                    barrier.dstQueueFamilyIndex = GetQueueFromType(device, gfxBarrier.resourceQueue)->queue_family_index;
                    break;
                InvalidDefaultCase;
            }
        }

        srcAccessFlags |= barrier.srcAccessMask;
        dstAccessFlags |= barrier.dstAccessMask;
    }
    
    for(u32 i = 0; i < numTextureBarriers; ++i)
    {
        const GfxTextureBarrier& gfxBarrier = pTextureBarriers[i];
        VkImageMemoryBarrier& barrier = imgBarriers[i];
        vg_image* image = FromGfxTexture(device, gfxBarrier.texture);

        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;

        if(gfxBarrier.currentState == GfxResourceState::UnorderedAccess && gfxBarrier.newState == GfxResourceState::UnorderedAccess)
        {
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        }
        else
        {
            barrier.srcAccessMask = ConvertResourceStateToAccessFlags(gfxBarrier.currentState);
            barrier.dstAccessMask = ConvertResourceStateToAccessFlags(gfxBarrier.newState);
            barrier.oldLayout = ConvertResourceStateToImageLayout(gfxBarrier.currentState);
            barrier.newLayout = ConvertResourceStateToImageLayout(gfxBarrier.newState);
        }

        barrier.image = image->handle;
        barrier.subresourceRange.aspectMask = DetermineAspectMaskFromFormat(image->format, true);
        barrier.subresourceRange.baseMipLevel = gfxBarrier.subresourceBarrier ? gfxBarrier.mipLevel : 0;
        barrier.subresourceRange.levelCount = gfxBarrier.subresourceBarrier ? 1 : VK_REMAINING_MIP_LEVELS;
        barrier.subresourceRange.baseArrayLayer = gfxBarrier.subresourceBarrier ? gfxBarrier.layer : 0;
        barrier.subresourceRange.layerCount = gfxBarrier.subresourceBarrier ? 1 : VK_REMAINING_ARRAY_LAYERS;

        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        if(gfxBarrier.currentState != GfxResourceState::Undefined)
        {
            switch(gfxBarrier.resourceOp)
            {
                case GfxQueueResourceOp::None:
                    break;
                case GfxQueueResourceOp::Acquire:
                    barrier.srcQueueFamilyIndex = GetQueueFromType(device, gfxBarrier.resourceQueue)->queue_family_index;
                    barrier.dstQueueFamilyIndex = pool->queue->queue_family_index;
                    break;
                case GfxQueueResourceOp::Release:
                    barrier.srcQueueFamilyIndex = pool->queue->queue_family_index;
                    barrier.dstQueueFamilyIndex = GetQueueFromType(device, gfxBarrier.resourceQueue)->queue_family_index;
                    break;
                InvalidDefaultCase;
            }
        }

        srcAccessFlags |= barrier.srcAccessMask;
        dstAccessFlags |= barrier.dstAccessMask;
    }
    
    for(u32 i = 0; i < numRenderTargetBarriers; ++i)
    {
        const GfxRenderTargetBarrier& gfxBarrier = pRenderTargetBarriers[i];
        VkImageMemoryBarrier& barrier = imgBarriers[numTextureBarriers + i];
        vg_rendertargetview* rtv = FromGfxRenderTarget(device, gfxBarrier.rtv);

        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;

        if(gfxBarrier.currentState == GfxResourceState::UnorderedAccess && gfxBarrier.newState == GfxResourceState::UnorderedAccess)
        {
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        }
        else
        {
            barrier.srcAccessMask = ConvertResourceStateToAccessFlags(gfxBarrier.currentState);
            barrier.dstAccessMask = ConvertResourceStateToAccessFlags(gfxBarrier.newState);
            barrier.oldLayout = ConvertResourceStateToImageLayout(gfxBarrier.currentState);
            barrier.newLayout = ConvertResourceStateToImageLayout(gfxBarrier.newState);
        }

        barrier.image = rtv->image->handle;
        barrier.subresourceRange.aspectMask = DetermineAspectMaskFromFormat(rtv->image->format, true);
        barrier.subresourceRange.baseMipLevel = gfxBarrier.subresourceBarrier ? gfxBarrier.mipLevel : 0;
        barrier.subresourceRange.levelCount = gfxBarrier.subresourceBarrier ? 1 : VK_REMAINING_MIP_LEVELS;
        barrier.subresourceRange.baseArrayLayer = gfxBarrier.subresourceBarrier ? gfxBarrier.layer : 0;
        barrier.subresourceRange.layerCount = gfxBarrier.subresourceBarrier ? 1 : VK_REMAINING_ARRAY_LAYERS;

        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        if(gfxBarrier.currentState != GfxResourceState::Undefined)
        {
            switch(gfxBarrier.resourceOp)
            {
                case GfxQueueResourceOp::None:
                    break;
                case GfxQueueResourceOp::Acquire:
                    barrier.srcQueueFamilyIndex = GetQueueFromType(device, gfxBarrier.resourceQueue)->queue_family_index;
                    barrier.dstQueueFamilyIndex = pool->queue->queue_family_index;
                    break;
                case GfxQueueResourceOp::Release:
                    barrier.srcQueueFamilyIndex = pool->queue->queue_family_index;
                    barrier.dstQueueFamilyIndex = GetQueueFromType(device, gfxBarrier.resourceQueue)->queue_family_index;
                    break;
                InvalidDefaultCase;
            }
        }

        srcAccessFlags |= barrier.srcAccessMask;
        dstAccessFlags |= barrier.dstAccessMask;
    }

    VkPipelineStageFlags srcStageMask = DeterminePipelineStageFlags(device, srcAccessFlags, pool->queueType);
    VkPipelineStageFlags dstStageMask = DeterminePipelineStageFlags(device, dstAccessFlags, pool->queueType);

    if(numBufferBarriers || numImgBarriers)
    {
        vkCmdPipelineBarrier(cmdBuffer, srcStageMask, dstStageMask, 0, 0, nullptr, numBufferBarriers, bufBarriers, numImgBarriers, imgBarriers);
    }

    return GfxResult::Ok;
}

internal
GfxResult CmdCopyBuffer( GfxCmdContext cmds, GfxBuffer src, GfxBuffer dest)
{
    vg_device& device = DeviceObject::From(cmds.deviceId);
    vg_cmd_context* context = FromGfxCmdContext(device, cmds);
    VkCommandBuffer cmdBuffer = CurrentFrameCmdBuffer(device, context);

    vg_buffer* srcBuffer = FromGfxBuffer(device, src);
    vg_buffer* dstBuffer = FromGfxBuffer(device, dest);

    VkBufferCopy region = {};
    region.srcOffset = 0;
    region.dstOffset = 0;
    region.size = VK_WHOLE_SIZE;

    vkCmdCopyBuffer(cmdBuffer, srcBuffer->handle, dstBuffer->handle, 1, &region);
    
    return GfxResult::Ok;
}

internal
GfxResult CmdCopyBufferRange( GfxCmdContext cmds, GfxBuffer src, u64 srcOffset, GfxBuffer dest, u64 destOffset, u64 size)
{
    vg_device& device = DeviceObject::From(cmds.deviceId);
    vg_cmd_context* context = FromGfxCmdContext(device, cmds);
    VkCommandBuffer cmdBuffer = CurrentFrameCmdBuffer(device, context);

    vg_buffer* srcBuffer = FromGfxBuffer(device, src);
    vg_buffer* dstBuffer = FromGfxBuffer(device, dest);

    VkBufferCopy region = {};
    region.srcOffset = (VkDeviceSize)srcOffset;
    region.dstOffset = (VkDeviceSize)destOffset;
    region.size = (VkDeviceSize)size;

    vkCmdCopyBuffer(cmdBuffer, srcBuffer->handle, dstBuffer->handle, 1, &region);
    return GfxResult::Ok;
}

internal
GfxResult CmdClearBuffer( GfxCmdContext cmds, GfxBuffer buffer, u32 clearValue)
{
    vg_device& device = DeviceObject::From(cmds.deviceId);
    vg_cmd_context* context = FromGfxCmdContext(device, cmds);
    VkCommandBuffer cmdBuffer = CurrentFrameCmdBuffer(device, context);

    vg_buffer* dstBuffer = FromGfxBuffer(device, buffer);
    vkCmdFillBuffer(cmdBuffer, dstBuffer->handle, 0, VK_WHOLE_SIZE, clearValue);
    
    return GfxResult::Ok;
}

internal
GfxResult CmdClearTexture( GfxCmdContext cmds, GfxTexture texture, GfxColor color)
{
    vg_device& device = DeviceObject::From(cmds.deviceId);
    vg_cmd_context* context = FromGfxCmdContext(device, cmds);
    VkCommandBuffer cmdBuffer = CurrentFrameCmdBuffer(device, context);

    // Cannot perform a clear during a render pass!
    ASSERT(!context->activeKernel);
    vg_image* image = FromGfxTexture(device, texture);

    VkClearColorValue clear = { color.r, color.g, color.b, color.a };
    VkImageSubresourceRange range{};
    range.aspectMask = DetermineAspectMaskFromFormat(image->format, false);
    range.baseMipLevel = 0;
    range.levelCount = image->numMipLevels;
    range.baseArrayLayer = 0;
    range.layerCount = image->layers;

    // NOTE(james): layout must be in VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL or VK_IMAGE_LAYOUT_GENERAL for this to work
    vkCmdClearColorImage(cmdBuffer, image->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1, &range);

    return GfxResult::Ok;
}

internal
GfxResult CmdCopyTexture( GfxCmdContext cmds, GfxTexture src, GfxTexture dest)
{
    vg_device& device = DeviceObject::From(cmds.deviceId);
    vg_cmd_context* context = FromGfxCmdContext(device, cmds);
    VkCommandBuffer cmdBuffer = CurrentFrameCmdBuffer(device, context);

    // Cannot perform a copy during a render pass!
    ASSERT(!context->activeKernel);

    vg_image* srcImage = FromGfxTexture(device, src);
    vg_image* dstImage = FromGfxTexture(device, dest);

    VkImageCopy region{};
    region.srcSubresource.aspectMask = DetermineAspectMaskFromFormat(srcImage->format, false);
    region.srcSubresource.mipLevel = 0;
    region.srcSubresource.baseArrayLayer = 0;
    region.srcSubresource.layerCount = srcImage->layers;
    region.srcOffset = {0,0,0};
    region.dstSubresource.aspectMask = DetermineAspectMaskFromFormat(dstImage->format, false);
    region.dstSubresource.mipLevel = 0;
    region.dstSubresource.baseArrayLayer = 0;
    region.dstSubresource.layerCount = dstImage->layers;
    region.dstOffset = {0,0,0};
    region.extent = {srcImage->width,srcImage->height,srcImage->layers};

    // NOTE(james): source layout must be in VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL and dest layout must be in VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL or VK_IMAGE_LAYOUT_GENERAL for this to work
    vkCmdCopyImage(cmdBuffer, srcImage->handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    return GfxResult::Ok;
}

internal
GfxResult CmdClearImage( GfxCmdContext cmds, GfxTexture texture, u32 mipLevel, u32 slice, GfxColor color)
{
    vg_device& device = DeviceObject::From(cmds.deviceId);
    vg_cmd_context* context = FromGfxCmdContext(device, cmds);
    VkCommandBuffer cmdBuffer = CurrentFrameCmdBuffer(device, context);

    // Cannot perform a clear during a render pass!
    ASSERT(!context->activeKernel);

    vg_image* image = FromGfxTexture(device, texture);

    VkClearColorValue clear = { color.r, color.g, color.b, color.a };
    VkImageSubresourceRange range{};
    range.aspectMask = DetermineAspectMaskFromFormat(image->format, false);
    range.baseMipLevel = mipLevel;
    range.levelCount = 1;
    range.baseArrayLayer = slice;
    range.layerCount = 1;

    // NOTE(james): layout must be in VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL or VK_IMAGE_LAYOUT_GENERAL for this to work
    vkCmdClearColorImage(cmdBuffer, image->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1, &range);

    return GfxResult::Ok;
}

internal
GfxResult CmdClearRenderTarget( GfxCmdContext cmds, GfxRenderTarget renderTarget, GfxColor color, f32 depth, u8 stencil )
{
    vg_device& device = DeviceObject::From(cmds.deviceId);
    vg_cmd_context* context = FromGfxCmdContext(device, cmds);
    VkCommandBuffer cmdBuffer = CurrentFrameCmdBuffer(device, context);

    // Cannot perform a clear during a render pass!
    ASSERT(!context->activeKernel);

    vg_rendertargetview* rtv = FromGfxRenderTarget(device, renderTarget);
    vg_image* image = rtv->image;

    VkClearColorValue clear = { color.r, color.g, color.b, color.a };
    VkImageSubresourceRange range{};
    range.aspectMask = DetermineAspectMaskFromFormat(image->format, true);
    range.baseMipLevel = 0;
    range.levelCount = 1;
    range.baseArrayLayer = 0;
    range.layerCount = 1;

    if(range.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT)
    {
        // NOTE(james): layout must be in VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL or VK_IMAGE_LAYOUT_GENERAL for this to work
        vkCmdClearColorImage(cmdBuffer, image->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1, &range);
    }
    else
    {
        // NOTE(james): layout must be in VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL or VK_IMAGE_LAYOUT_GENERAL for this to work
        VkClearDepthStencilValue depthClear = {depth, stencil};
        vkCmdClearDepthStencilImage(cmdBuffer, image->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &depthClear, 1, &range);
    }

    return GfxResult::Ok;
}

internal
GfxResult CmdCopyBufferToTexture( GfxCmdContext cmds, GfxBuffer src, u64 srcOffset, GfxTexture dest)
{
    vg_device& device = DeviceObject::From(cmds.deviceId);
    vg_cmd_context* context = FromGfxCmdContext(device, cmds);
    VkCommandBuffer cmdBuffer = CurrentFrameCmdBuffer(device, context);

    vg_buffer* buffer = FromGfxBuffer(device, src);
    vg_image* image = FromGfxTexture(device, dest);

    VkBufferImageCopy region{};
    region.bufferOffset = (VkDeviceSize)srcOffset;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    // TODO(james): Copy more than just a single layer...
    region.imageSubresource.aspectMask = DetermineAspectMaskFromFormat(image->format, true);
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = {0,0,0};
    region.imageExtent = { image->width, image->height, image->layers };

    vkCmdCopyBufferToImage(cmdBuffer, buffer->handle, image->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    return GfxResult::Ok;
}

internal
GfxResult CmdGenerateMips( GfxCmdContext cmds, GfxTexture texture)
{
    NotImplemented;
    // image barrier -> all mip levels to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL

    // for each mip level starting at 1
    //   image barrier VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL -> VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
    //   vkCmdBlitImage for each mip level with VK_FILTER_LINEAR
    return GfxResult::Ok; 
}

internal GfxResult
CmdBindRenderTargets(GfxCmdContext cmds, u32 numRenderTargets, GfxRenderTarget* pColorRTVs, GfxRenderTarget* pDepthStencilRTV)
{
    // Need to lookup or create a valid renderpass / framebuffer combo OR use the fancy vk_KHR_dynamic_rendering extension
    // vkCmdBeginRenderPass / vkCmdEndRenderPass if numRenderTargets is 0
    vg_device& device = DeviceObject::From(cmds.deviceId);
    vg_cmd_context* context = FromGfxCmdContext(device, cmds);
    VkCommandBuffer cmdBuffer = CurrentFrameCmdBuffer(device, context);
    
    // NOTE(james): First we have to end a renderpass if one has been started
    EndContextRenderPass(device, context);    

    // only start a new render pass if render targets have been specified
    if(numRenderTargets || pDepthStencilRTV)
    {
        vg_rendertargetview* rtvList[GFX_MAX_RENDERTARGETS];
        u32 numClearValues = 0;
        VkClearValue clearValues[GFX_MAX_RENDERTARGETS+1];
        
        for(u32 rtIdx = 0; rtIdx < numRenderTargets; ++rtIdx)
        {
            rtvList[rtIdx] = FromGfxRenderTarget(device, pColorRTVs[rtIdx]);

            if(rtvList[rtIdx]->loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
            {
                clearValues[numClearValues++] = rtvList[rtIdx]->clearValue;
            }
        }
        u64 renderpassKey = MurmurHash64(rtvList, numRenderTargets * sizeof(vg_rendertargetview), numRenderTargets);
        
        vg_rendertargetview* dsRTV = nullptr;  
        if(pDepthStencilRTV)
        {
            dsRTV = FromGfxRenderTarget(device, *pDepthStencilRTV);
            renderpassKey = MurmurHash64(dsRTV, sizeof(vg_rendertargetview), renderpassKey);

            if(dsRTV->loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
            {
                clearValues[numClearValues++] = dsRTV->clearValue;
            }
        }

        vg_renderpass* renderpass = nullptr;
        if(!device.mapRenderpasses->try_get(renderpassKey, &renderpass))
        {
            // Create the render pass here
            VkAttachmentDescription attachments[GFX_MAX_RENDERTARGETS + 1] = {}; // +1 in case depth/stencil is attached
            VkAttachmentReference colorRefs[GFX_MAX_RENDERTARGETS] = {};
            VkAttachmentReference depthStencilRef = {};

            b32 hasDepthStencilAttachment = dsRTV ? true : false;

            for(u32 i = 0; i < numRenderTargets; ++i)
            {
                attachments[i].flags = 0;
                attachments[i].format = rtvList[i]->image->format;
                attachments[i].samples = rtvList[i]->sampleCount;
                attachments[i].loadOp = rtvList[i]->loadOp;
                attachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                attachments[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                attachments[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
                attachments[i].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                attachments[i].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;   

                colorRefs[i].attachment = i;
                colorRefs[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            }

            if(hasDepthStencilAttachment)
            {
                uint32_t idx = numRenderTargets;
                attachments[idx].flags = 0;
                attachments[idx].format = dsRTV->image->format;
                attachments[idx].samples = dsRTV->sampleCount;
                attachments[idx].loadOp = dsRTV->loadOp;
                attachments[idx].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                attachments[idx].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                attachments[idx].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
                attachments[idx].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                attachments[idx].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

                depthStencilRef.attachment = idx;    
                depthStencilRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            }

            // ugh.. silly subpass needs to redefine all the same crap
            VkSubpassDescription subpass = {};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount = numRenderTargets;
            subpass.pColorAttachments = colorRefs;
            subpass.pDepthStencilAttachment = hasDepthStencilAttachment ? &depthStencilRef : nullptr;

            VkRenderPassCreateInfo renderPassInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
            renderPassInfo.attachmentCount = numRenderTargets + (hasDepthStencilAttachment ? 1 : 0);
            renderPassInfo.pAttachments = attachments;
            renderPassInfo.subpassCount = 1;
            renderPassInfo.pSubpasses = &subpass;
            renderPassInfo.dependencyCount = 0;
            renderPassInfo.pDependencies = nullptr;

            // NOTE(james): First check the freelist for any free objects
            //  Use one if there is one, otherwise need to allocate one
            if(device.freelist_renderpass)
            {
                renderpass = device.freelist_renderpass;
                device.freelist_renderpass = renderpass->next;
            }
            else
            {
                renderpass = PushStruct(device.arena, vg_renderpass);
            }
            
            VkResult result = vkCreateRenderPass(device.handle, &renderPassInfo, nullptr, &renderpass->handle);
            ASSERT(result == VK_SUCCESS);
            
            // cache the renderpass since it will likely be used again on the next frame
            device.mapRenderpasses->set(renderpassKey, renderpass);
        }

        vg_framebuffer* framebuffer = nullptr;
        if(!device.mapFramebuffers->try_get(renderpassKey, &framebuffer))
        {
            // Create the framebuffer here
            VkImageView attachments[GFX_MAX_RENDERTARGETS + 1];

            for(u32 i = 0; i < numRenderTargets; ++i)
            {
                attachments[i] = rtvList[i]->image->view;
            }
            if(dsRTV)
            {
                attachments[numRenderTargets] = dsRTV->image->view;
            }
            
            VkFramebufferCreateInfo info = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
            info.renderPass = renderpass->handle;
            info.attachmentCount = numRenderTargets + (dsRTV ? 1 : 0);
            info.pAttachments = attachments;
            info.width = device.extent.width;
            info.height = device.extent.height;
            info.layers = 1;

            
            // NOTE(james): First check the freelist for any free objects
            //  Use one if there is one, otherwise need to allocate one
            if(device.freelist_framebuffer)
            {
                framebuffer = device.freelist_framebuffer;
                device.freelist_framebuffer = framebuffer->next;
            }
            else
            {
                framebuffer = PushStruct(device.arena, vg_framebuffer);
            }

            VkResult result = vkCreateFramebuffer(device.handle, &info, nullptr, &framebuffer->handle);

            device.mapFramebuffers->set(renderpassKey, framebuffer);
        }

        renderpass->lastUsedInFrameIndex = device.curSwapChainIndex;
        framebuffer->lastUsedInFrameIndex = device.curSwapChainIndex;

        context->activeRenderpass = renderpass;
        context->activeFramebuffer = framebuffer;

        VkRenderPassBeginInfo renderPassBegin = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        renderPassBegin.renderPass = renderpass->handle;
        renderPassBegin.renderArea.offset.x = 0;
        renderPassBegin.renderArea.offset.y = 0;
        renderPassBegin.renderArea.extent = device.extent;
        renderPassBegin.framebuffer = framebuffer->handle;
        renderPassBegin.clearValueCount = numClearValues;
        renderPassBegin.pClearValues = clearValues;
        vkCmdBeginRenderPass(cmdBuffer, &renderPassBegin, VK_SUBPASS_CONTENTS_INLINE);
    }

    return GfxResult::Ok;
}

internal
GfxResult CmdBindKernel( GfxCmdContext cmds, GfxKernel resource)
{
    vg_device& device = DeviceObject::From(cmds.deviceId);
    vg_cmd_context* context = FromGfxCmdContext(device, cmds);
    VkCommandBuffer cmdBuffer = CurrentFrameCmdBuffer(device, context);

    vg_kernel* kernel = FromGfxKernel(device, resource);

    // setup for the binding of descriptor sets
    context->activeDescriptorSets = PushArray(*device.frameArena, kernel->program->descriptorSetLayouts->size(), VkDescriptorSet);
    context->activeKernel = kernel;
    context->shouldBindDescriptors = false;
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, kernel->pipeline);
    
    return GfxResult::Ok;
}

internal
GfxResult CmdBindIndexBuffer( GfxCmdContext cmds, GfxBuffer indexBuffer)
{
    vg_device& device = DeviceObject::From(cmds.deviceId);
    vg_cmd_context* context = FromGfxCmdContext(device, cmds);
    VkCommandBuffer cmdBuffer = CurrentFrameCmdBuffer(device, context);
    vg_buffer* ib = FromGfxBuffer(device, indexBuffer);

    context->activeIB = ib;
    vkCmdBindIndexBuffer(cmdBuffer, ib->handle, 0, VK_INDEX_TYPE_UINT32);
    return GfxResult::Ok;
}

internal
GfxResult CmdBindVertexBuffer( GfxCmdContext cmds, GfxBuffer vertexBuffer)
{
    vg_device& device = DeviceObject::From(cmds.deviceId);
    vg_cmd_context* context = FromGfxCmdContext(device, cmds);
    VkCommandBuffer cmdBuffer = CurrentFrameCmdBuffer(device, context);
    vg_buffer* vb = FromGfxBuffer(device, vertexBuffer);

    context->activeVB = vb;
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &vb->handle, offsets);
    return GfxResult::Ok;
}

internal
GfxResult CmdBindDescriptorSet(GfxCmdContext cmds, const GfxDescriptorSet& descSet)
{
    vg_device& device = DeviceObject::From(cmds.deviceId);
    vg_cmd_context* context = FromGfxCmdContext(device, cmds);
    VkCommandBuffer cmdBuffer = CurrentFrameCmdBuffer(device, context);

    if(!context->activeKernel)
    {
        return GfxResult::InvalidOperation;
    }

    vg_program* program = context->activeKernel->program;

    if(descSet.setLocation >= program->descriptorSetLayouts->size())
    {
        return GfxResult::InvalidParameter;
    }

    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VkResult result = vgAllocateDescriptor(device, program->descriptorSetLayouts->at(descSet.setLocation), &descriptorSet);

    if(result != VK_SUCCESS)
    {
        return ToGfxResult(result);
    }

    u32 writeDescriptors = 0;
    VkWriteDescriptorSet* writes = PushArray(*device.frameArena, descSet.count, VkWriteDescriptorSet);

    for(u32 i = 0; i < descSet.count; ++i)
    {
        VkWriteDescriptorSet& writeData = writes[writeDescriptors++];
        const GfxDescriptor& desc = descSet.pDescriptors[i];

        writeData.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeData.dstSet = descriptorSet;
        writeData.dstBinding = desc.bindingLocation;

        if(desc.name)
        {
            // lookup using the reflection name
            vg_program_binding_desc& bindingDesc = program->mapBindingDesc->get(strhash64(desc.name, StringLength(desc.name)-1));
            ASSERT(bindingDesc.set == descSet.setLocation);
#if PROJECTSUPER_INTERNAL
            ASSERT(CompareStrings(desc.name, bindingDesc.name));
#endif
            writeData.dstBinding = bindingDesc.binding;
        }

        switch(desc.type)
        {
            case GfxDescriptorType::Buffer:
                {
                    vg_buffer* buffer = FromGfxBuffer(device, desc.buffer);
                    
                    VkDescriptorBufferInfo bufferInfo{};
                    bufferInfo.buffer = buffer->handle;
                    bufferInfo.offset = 0;
                    bufferInfo.range = VK_WHOLE_SIZE;

                    writeData.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                    writeData.descriptorCount = 1;
                    writeData.pBufferInfo = &bufferInfo;
                }
                break;
            case GfxDescriptorType::Image:
                {
                    vg_image* image = FromGfxTexture(device, desc.texture);
                    vg_sampler* sampler = FromGfxSampler(device, desc.sampler);

                    VkDescriptorImageInfo imageInfo{};
                    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    imageInfo.imageView = image->view;
                    imageInfo.sampler = sampler->handle;

                    writeData.descriptorCount = 1;
                    writeData.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    writeData.pImageInfo = &imageInfo;
                }
                break;
            InvalidDefaultCase;
        }
    }

    vkUpdateDescriptorSets(device.handle, descSet.count, writes, 0, nullptr);
    context->activeDescriptorSets[descSet.setLocation] = descriptorSet;
    context->shouldBindDescriptors = true;
        
    return GfxResult::Ok;
}

internal
GfxResult CmdBindPushConstant(GfxCmdContext cmds, const char* name, const void* data)
{
    vg_device& device = DeviceObject::From(cmds.deviceId);
    vg_cmd_context* context = FromGfxCmdContext(device, cmds);
    VkCommandBuffer cmdBuffer = CurrentFrameCmdBuffer(device, context);

    vg_program* program = context->activeKernel->program;
    vg_program_pushconstant_desc& pc = program->mapPushConstantDesc->get(strhash64(name, StringLength(name)-1));

#if PROJECTSUPER_INTERNAL
    ASSERT(CompareStrings(name, pc.name));
#endif

    vkCmdPushConstants(cmdBuffer, program->pipelineLayout, pc.shaderStage, pc.offset, pc.size, data);

    return GfxResult::Ok;
}

internal
GfxResult CmdSetViewport( GfxCmdContext cmds, f32 x, f32 y, f32 width, f32 height)
{
    vg_device& device = DeviceObject::From(cmds.deviceId);
    vg_cmd_context* context = FromGfxCmdContext(device, cmds);
    VkCommandBuffer cmdBuffer = CurrentFrameCmdBuffer(device, context);

    VkViewport vp = {};
    vp.x = x;
    vp.y = y;
    vp.width = width;
    vp.height = height;
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;   
    vkCmdSetViewport(cmdBuffer, 0, 1, &vp);
    return GfxResult::Ok;
}

internal
GfxResult CmdSetScissorRect( GfxCmdContext cmds, i32 x, i32 y, u32 width, u32 height)
{
    vg_device& device = DeviceObject::From(cmds.deviceId);
    vg_cmd_context* context = FromGfxCmdContext(device, cmds);
    VkCommandBuffer cmdBuffer = CurrentFrameCmdBuffer(device, context);
    
    VkRect2D rc = {x, y, width, height};
    vkCmdSetScissor(cmdBuffer, 0, 1, &rc);
    return GfxResult::Ok;
}

internal
GfxResult CmdDraw( GfxCmdContext cmds, u32 vertexCount, u32 instanceCount, u32 baseVertex, u32 baseInstance)
{
    vg_device& device = DeviceObject::From(cmds.deviceId);
    vg_cmd_context* context = FromGfxCmdContext(device, cmds);
    VkCommandBuffer cmdBuffer = CurrentFrameCmdBuffer(device, context);

    if(context->shouldBindDescriptors)
    {
        vg_program* program = context->activeKernel->program;
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, program->pipelineLayout, 0, program->descriptorSetLayouts->size(), context->activeDescriptorSets, 0, nullptr);
        context->shouldBindDescriptors = false;
    }
    
    vkCmdDraw(cmdBuffer, vertexCount, instanceCount, 0, 0);
    return GfxResult::Ok;
}

internal
GfxResult CmdDrawIndexed( GfxCmdContext cmds, u32 indexCount, u32 instanceCount, u32 firstIndex, u32 baseVertex, u32 baseInstance)
{
    vg_device& device = DeviceObject::From(cmds.deviceId);
    vg_cmd_context* context = FromGfxCmdContext(device, cmds);
    VkCommandBuffer cmdBuffer = CurrentFrameCmdBuffer(device, context);

    if(context->shouldBindDescriptors)
    {
        vg_program* program = context->activeKernel->program;
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, program->pipelineLayout, 0, program->descriptorSetLayouts->size(), context->activeDescriptorSets, 0, nullptr);
        context->shouldBindDescriptors = false;
    }
    
    vkCmdDrawIndexed(cmdBuffer, indexCount, instanceCount, firstIndex, baseVertex, baseInstance);
    return GfxResult::Ok;
}

internal
GfxResult CmdDrawIndirect( GfxCmdContext cmds, GfxBuffer argsBuffer, u32 argsCount)
{
    NotImplemented;
    // vkCmdDrawIndirect
    return GfxResult::Ok;
}

internal
GfxResult CmdDrawIndexedIndirect( GfxCmdContext cmds, GfxBuffer argsBuffer, u32 argsCount)
{
    NotImplemented;
    // vkCmdDrawIndexedIndirect
    return GfxResult::Ok;
}

internal
GfxResult CmdDispatch( GfxCmdContext cmds, u32 numGroupsX, u32 numGroupsY, u32 numGroupsZ)
{
    vg_device& device = DeviceObject::From(cmds.deviceId);
    vg_cmd_context* context = FromGfxCmdContext(device, cmds);
    VkCommandBuffer cmdBuffer = CurrentFrameCmdBuffer(device, context);

    vkCmdDispatch(cmdBuffer, numGroupsX, numGroupsY, numGroupsZ);
    return GfxResult::Ok;
}

internal
GfxResult CmdDispatchIndirect( GfxCmdContext cmds, GfxBuffer argsBuffer)
{
    vg_device& device = DeviceObject::From(cmds.deviceId);
    vg_cmd_context* context = FromGfxCmdContext(device, cmds);
    VkCommandBuffer cmdBuffer = CurrentFrameCmdBuffer(device, context);

    vg_buffer* buffer = FromGfxBuffer(device, argsBuffer);
    vkCmdDispatchIndirect(cmdBuffer, buffer->handle, 0);

    return GfxResult::Ok;
}

internal
GfxResult SubmitCommands( GfxDevice deviceHandle, u32 count, GfxCmdContext* pContexts)
{
    vg_device& device = DeviceObject::From(deviceHandle);
    
    VkCommandBuffer* pCmdBuffers = PushArray(*device.frameArena, count, VkCommandBuffer);
    for(u32 i = 0; i < count; ++i)
    {
        vg_cmd_context* context = FromGfxCmdContext(device, pContexts[i]);
        pCmdBuffers[i] = CurrentFrameCmdBuffer(device, context);
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 0;
    submitInfo.pWaitSemaphores = nullptr;
    submitInfo.pWaitDstStageMask = nullptr;
    submitInfo.commandBufferCount = count;
    submitInfo.pCommandBuffers = pCmdBuffers;
    submitInfo.signalSemaphoreCount = 0;
    submitInfo.pSignalSemaphores = nullptr;

    VkResult result = vkQueueSubmit(device.q_graphics.handle, 1, &submitInfo, VK_NULL_HANDLE);
    if(DIDFAIL(result))
    {
        LOG_ERROR("Vulkan Submit Error: %X", result);
        ASSERT(false);
    }

    return ToGfxResult(result);
}

internal
GfxResult Frame( GfxDevice deviceHandle, u32 contextCount, GfxCmdContext* pContexts)
{
    vg_device& device = DeviceObject::From(deviceHandle);
    
    u32 imageIndex = device.curSwapChainIndex;
    VkSemaphore waitSemaphores[] = { device.pCurFrame->presentSemaphore };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSemaphore signalSemaphores[] = { device.pCurFrame->renderSemaphore };

    VkCommandBuffer* pCmdBuffers = PushArray(*device.frameArena, contextCount, VkCommandBuffer);
    for(u32 i = 0; i < contextCount; ++i)
    {
        vg_cmd_context* context = FromGfxCmdContext(device, pContexts[i]);
        pCmdBuffers[i] = CurrentFrameCmdBuffer(device, context);
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = contextCount;
    submitInfo.pCommandBuffers = pCmdBuffers;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    vkResetFences(device.handle, 1, &device.pCurFrame->renderFence);
    VkResult result = vkQueueSubmit(device.q_graphics.handle, 1, &submitInfo, device.pCurFrame->renderFence);
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
    presentInfo.pImageIndices = &device.curSwapChainIndex;
    presentInfo.pResults = nullptr;

    result = vkQueuePresentKHR(device.q_present.handle, &presentInfo);

    if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        //Win32RecreateSwapChain(graphics);
        ASSERT(false);
    }
    else if(DIDFAIL(result))
    {
        LOG_ERROR("Vulkan Present Error: %X", result);
        ASSERT(false);
    }

    // Now move to the next frame...

    device.currentFrameIndex = (device.currentFrameIndex + 1) % FRAME_OVERLAP;
    device.pPrevFrame = device.pCurFrame;
    device.pCurFrame = &device.frames[device.currentFrameIndex];

    EndTemporaryMemory(device.frameTemp);
    device.frameTemp = BeginTemporaryMemory(*device.frameArena);

    return ToGfxResult(result);
}

internal
GfxResult Finish(GfxDevice deviceHandle)
{
    vg_device& device = DeviceObject::From(deviceHandle);
    VkResult result = vkDeviceWaitIdle(device.handle);
    return ToGfxResult(result);
}

internal
GfxResult CleanupUnusedRenderingResources(GfxDevice deviceHandle)
{
    vg_device& device = DeviceObject::From(deviceHandle);
    // NOTE(james): to do this safely, must wait until the resources aren't being used
    VkResult result = vkDeviceWaitIdle(device.handle);

    if(result == VK_SUCCESS)
    {
        // Store objects in the free list after releasing vulkan resources
        for(auto entry = device.mapRenderpasses->begin(); entry != device.mapRenderpasses->end();)
        {
            vg_renderpass* renderpass = **entry;
            if(renderpass->lastUsedInFrameIndex != device.curSwapChainIndex)
            {
                vkDestroyRenderPass(device.handle, renderpass->handle, nullptr);
                renderpass->lastUsedInFrameIndex = U32MAX;
                renderpass->next = device.freelist_renderpass;
                device.freelist_renderpass = renderpass;
                entry = device.mapRenderpasses->erase(entry);
            }
            else
            {
                entry++;
            }
        }

        for(auto entry = device.mapFramebuffers->begin(); entry != device.mapFramebuffers->end();)
        {
            vg_framebuffer* framebuffer = **entry;
            if(framebuffer->lastUsedInFrameIndex != device.curSwapChainIndex)
            {
                vkDestroyFramebuffer(device.handle, framebuffer->handle, nullptr);
                framebuffer->lastUsedInFrameIndex = U32MAX;
                framebuffer->next = device.freelist_framebuffer;
                device.freelist_framebuffer = framebuffer;
                entry = device.mapFramebuffers->erase(entry);
            }
            else
            {
                entry++;
            }
        }
    }

    return ToGfxResult(result);
}

internal
GfxTimestampQuery CreateTimestampQuery( GfxDevice deviceHandle)
{
    NotImplemented;
    return GfxTimestampQuery{};
}

internal
GfxResult DestroyTimestampQuery( GfxDevice deviceHandle, GfxTimestampQuery timestampQuery)
{
    NotImplemented;
    return GfxResult::Ok;
}

internal
f32 GetTimestampQueryDuration( GfxDevice deviceHandle, GfxTimestampQuery timestampQuery)
{
    NotImplemented;
    return 0.0f;
}

internal
GfxResult BeginTimestampQuery( GfxCmdContext cmds, GfxTimestampQuery query)
{
    NotImplemented;
    return GfxResult::Ok;
}

internal
GfxResult EndTimestampQuery( GfxCmdContext cmds, GfxTimestampQuery query)
{
    NotImplemented;
    return GfxResult::Ok;
}

internal
GfxResult BeginEvent( GfxCmdContext cmds, const char* name)
{
    NotImplemented;
    return GfxResult::Ok;
}

internal
GfxResult BeginColorEvent( GfxCmdContext cmds, const char* name)
{
    NotImplemented;
    return GfxResult::Ok;
}

internal
GfxResult EndEvent( GfxCmdContext cmds)
{
    NotImplemented;
    return GfxResult::Ok;
}
