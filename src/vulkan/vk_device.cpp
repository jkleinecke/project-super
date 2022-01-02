
#include <vulkan/vulkan.h>

#include <SPIRV-Reflect/spirv_reflect.h>
#include "vk_device.h"

#include "vk_extensions.h"
#include "vk_initializers.cpp"
#include "vk_descriptor.cpp"

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
struct FrameObject
{
    alignas(16) m4 viewProj;
};

struct InstanceObject
{
    alignas(16) m4 mvp;
};

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

internal
void vgDestroyBuffer(VkDevice device, vg_buffer& buffer)
{
    IFF(buffer.handle, vkDestroyBuffer(device, buffer.handle, nullptr));
    IFF(buffer.memory, vkFreeMemory(device, buffer.memory, nullptr));
}

internal
void vgDestroyImage(VkDevice device, vg_image& image)
{
    IFF(image.view, vkDestroyImageView(device, image.view, nullptr));
    IFF(image.handle, vkDestroyImage(device, image.handle, nullptr));
    IFF(image.memory, vkFreeMemory(device, image.memory, nullptr));
}

internal
VkCommandBuffer vgBeginSingleTimeCommands(vg_device& device)
{
    VkCommandBufferAllocateInfo allocInfo = vkInit_command_buffer_allocate_info(
        device.pCurFrame->commandPool, 1, VK_COMMAND_BUFFER_LEVEL_PRIMARY
    );

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device.handle, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo = vkInit_command_buffer_begin_info(
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    );
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

internal
void vgEndSingleTimeCommands(vg_device& device, VkCommandBuffer commandBuffer)
{
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo = vkInit_submit_info(&commandBuffer);

    // TODO(james): give it a fence so we can tell when it is done?
    vkQueueSubmit(device.q_graphics.handle, 1, &submitInfo, VK_NULL_HANDLE);
    // Have to call QueueWaitIdle unless we're going to go the event route
    vkQueueWaitIdle(device.q_graphics.handle);

    vkFreeCommandBuffers(device.handle, device.pCurFrame->commandPool, 1, &commandBuffer);
}

internal
VkFormat vgFindSupportedFormat(vg_device& device, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
{
    for(VkFormat format : candidates)
    {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(device.physicalDevice, format, &props);

        if(tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
        {
            return format;
        }
        else if(tiling == VK_IMAGE_TILING_OPTIMAL  && (props.optimalTilingFeatures & features) == features)
        {
            return format;
        }
    }

    return VK_FORMAT_UNDEFINED;
}

internal
VkFormat vgFindDepthFormat(vg_device& device)
{
    return vgFindSupportedFormat(
        device,
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

internal 
bool vgHasStencilComponent(VkFormat format)
{
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

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
vgFindQueueFamily(VkPhysicalDevice device, VkFlags withFlags, u32* familyIndex)
{
    bool bFound = false;
    u32 count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, queueFamilies.data());

    for(u32 index = 0; index < count; ++index)
    {
        if(queueFamilies[index].queueFlags & withFlags)
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


internal
i32 vgFindMemoryType(vg_device& device, u32 typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties& memProperties = device.device_memory_properties;

    for(u32 i = 0; i < memProperties.memoryTypeCount; ++i)
    {
        b32x bValidMemoryType = typeFilter & (1 << i);
        bValidMemoryType = bValidMemoryType && (memProperties.memoryTypes[i].propertyFlags & properties) == properties;
        if(bValidMemoryType)
        {
            return i;
        }
    }

    return -1;
}

internal
VkResult vgCreateBuffer(vg_device& device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, vg_buffer* pBuffer)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult result = vkCreateBuffer(device.handle, &bufferInfo, nullptr, &pBuffer->handle);
    if(DIDFAIL(result))
    {
        ASSERT(false);
        return result;
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device.handle, pBuffer->handle, &memRequirements);

    i32 memoryIndex = vgFindMemoryType(device, memRequirements.memoryTypeBits, properties);
    if(memoryIndex < 0)
    {
        // failed
        ASSERT(false);
        return VK_ERROR_MEMORY_MAP_FAILED;  // seems like a decent fit for what happened
    }
    
    // TODO(james): We should really allocate several large chunks and use a custom allocator for this
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = memoryIndex;

    result = vkAllocateMemory(device.handle, &allocInfo, nullptr, &pBuffer->memory);
    if(DIDFAIL(result))
    {
        ASSERT(false);
        return result;
    }

    vkBindBufferMemory(device.handle, pBuffer->handle, pBuffer->memory, 0);

    return VK_SUCCESS;
}

internal
void vgCopyBuffer(vg_device& device, VkBuffer src, VkBuffer dest, VkDeviceSize size)
{
    VkCommandBuffer commandBuffer = vgBeginSingleTimeCommands(device);

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, src, dest, 1, &copyRegion);

    vgEndSingleTimeCommands(device, commandBuffer);
}

internal
VkResult vgCreateImage(vg_device& device, u32 width, u32 height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, vg_image* pImage)
{
    VkImageCreateInfo imageInfo = vkInit_image_create_info(
        format, usage, { width, height, 1 }
    );
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult result = vkCreateImage(device.handle, &imageInfo, nullptr, &pImage->handle);
    ASSERT(DIDSUCCEED(result));

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device.handle, pImage->handle, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = vgFindMemoryType(device, memRequirements.memoryTypeBits, properties);
    ASSERT(allocInfo.memoryTypeIndex > 0);

    vkAllocateMemory(device.handle, &allocInfo, nullptr, &pImage->memory);

    vkBindImageMemory(device.handle, pImage->handle, pImage->memory, 0);

    return VK_SUCCESS;
}

internal
void vgTransitionImageLayout(vg_device& device, vg_image& image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
{
    VkCommandBuffer commandBuffer = vgBeginSingleTimeCommands(device);

    VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_NONE_KHR;
    VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_NONE_KHR;

    VkImageMemoryBarrier barrier = vkInit_image_barrier(
        image.handle, 0, 0, oldLayout, newLayout, 0
    );
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    // TODO(james): This feels dirty, think about a simpler more robust solution here
    if(newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

        if(vgHasStencilComponent(format)) 
        {
            barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    }
    else
    {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    if(oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) 
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if(oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if(oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    }
    else
    {
        // invalid layout transition
        ASSERT(false);
    }

    vkCmdPipelineBarrier(commandBuffer, 
            sourceStage, destinationStage,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );

    vgEndSingleTimeCommands(device, commandBuffer);
}

VkResult vgCreateImageView(vg_device& device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, VkImageView* pView)
{
    VkImageViewCreateInfo viewInfo = vkInit_imageview_create_info(
        format, image, aspectFlags
    );

    VkResult result = vkCreateImageView(device.handle, &viewInfo, nullptr, pView);
    ASSERT(DIDSUCCEED(result));

    return result;
}

internal
void vgCopyBufferToImage(vg_device& device, vg_buffer& buffer, vg_image& image, u32 width, u32 height)
{
    VkCommandBuffer commandBuffer = vgBeginSingleTimeCommands(device);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = {0,0,0};
    region.imageExtent = { width, height, 1 };

    vkCmdCopyBufferToImage(commandBuffer, buffer.handle, image.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    vgEndSingleTimeCommands(device, commandBuffer);
}

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

        vkGetPhysicalDeviceProperties(device.physicalDevice, &device.device_properties);
        vkGetPhysicalDeviceMemoryProperties(device.physicalDevice, &device.device_memory_properties);
    }

    // TODO(james): This seems over complicated, I bet there's room to simplify, specifically the queue family stuff
    // Create the Logical Device
    {
        device.handle = VK_NULL_HANDLE;

        u32 graphicsQueueIndex = 0;
        u32 presentQueueIndex = 0;
        vgFindQueueFamily(device.physicalDevice, VK_QUEUE_GRAPHICS_BIT, &graphicsQueueIndex);

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        // setup graphics queue

        f32 queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = graphicsQueueIndex;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);

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
            presentQueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            presentQueueInfo.queueFamilyIndex = presentQueueIndex;
            presentQueueInfo.queueCount = 1;
            presentQueueInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(presentQueueInfo);
        }
        
        VkPhysicalDeviceFeatures deviceFeatures{};
        deviceFeatures.samplerAnisotropy = VK_TRUE;

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = (u32)queueCreateInfos.size();
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
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

        vkGetDeviceQueue(device.handle, graphicsQueueIndex, 0, &device.q_graphics.handle);
        vkGetDeviceQueue(device.handle, presentQueueIndex, 0, &device.q_present.handle);
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
    device.extent = extent; // is this needed?
    
    vkGetSwapchainImagesKHR(device.handle, swapChain, &imageCount, nullptr);
    std::vector<VkImage> images(imageCount);
    vkGetSwapchainImagesKHR(device.handle, swapChain, &imageCount, images.data());

    arrsetlen(device.paSwapChainImages, imageCount);
    //device.swapChainImages.resize(imageCount);
    for(u32 index = 0; index < imageCount; ++index)
    {
        device.paSwapChainImages[index].handle = images[index];

        // now setup the image view for use in the swap chain
        result = vgCreateImageView(device, device.paSwapChainImages[index].handle, surfaceFormat.format, VK_IMAGE_ASPECT_COLOR_BIT, &device.paSwapChainImages[index].view);
        VERIFY_SUCCESS(result);
    }
    
    VERIFY_SUCCESS(result);

#undef VERIFY_SUCCESS

    return VK_SUCCESS;
}

internal
VkResult vgInitializeMemory(vg_device& device)
{
    vgInitDescriptorAllocator(device.descriptorAllocator, device.handle);
    vgInitDescriptorLayoutCache(device.descriptorLayoutCache, device.handle);

    for(int i = 0; i < FRAME_OVERLAP; ++i)
    {
        vgInitDescriptorAllocator(device.frames[i].dynamicDescriptorAllocator, device.handle);
    }

    return VK_SUCCESS;
}

internal
VkResult vgCreateShader(VkDevice device, buffer& bytes, vg_shader* shader)
{
    {
        // TODO(james): Only load reflection at asset compile time, build out structures we need at that point
        SpvReflectShaderModule module = {};
        SpvReflectResult result = spvReflectCreateShaderModule(bytes.size, bytes.data, &module);

        shader->shaderStageMask = (VkShaderStageFlagBits)module.shader_stage;
        uint32_t count = 0;

        // NOTE(james): Straight from SPIRV-Reflect examples...
        // Loads the shader's vertex buffer description attributes
        {
            result = spvReflectEnumerateInputVariables(&module, &count, NULL);
            ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);

            std::vector<SpvReflectInterfaceVariable*> input_vars(count);
            result = spvReflectEnumerateInputVariables(&module, &count, input_vars.data());
            ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);

            count = 0;
            result = spvReflectEnumerateOutputVariables(&module, &count, NULL);
            ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);

            std::vector<SpvReflectInterfaceVariable*> output_vars(count);
            result = spvReflectEnumerateOutputVariables(&module, &count, output_vars.data());
            ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);

            if(module.shader_stage & SPV_REFLECT_SHADER_STAGE_VERTEX_BIT)
            {
                // Demonstrates how to generate all necessary data structures to populate
                // a VkPipelineVertexInputStateCreateInfo structure, given the module's
                // expected input variables.
                //
                // Simplifying assumptions:
                // - All vertex input attributes are sourced from a single vertex buffer,
                //   bound to VB slot 0.
                // - Each vertex's attribute are laid out in ascending order by location.
                // - The format of each attribute matches its usage in the shader;
                //   float4 -> VK_FORMAT_R32G32B32A32_FLOAT, etc. No attribute compression is applied.
                // - All attributes are provided per-vertex, not per-instance.
                shader->vertexBindingDesc.binding = 0;
                shader->vertexBindingDesc.stride = 0;  // computed below
                shader->vertexBindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

                VkPipelineVertexInputStateCreateInfo vertexInfo = vkInit_vertex_input_state_create_info();
                shader->vertexAttributes.assign(input_vars.size(), VkVertexInputAttributeDescription{});
                for(size_t index = 0; index < input_vars.size(); ++index)
                {
                    const SpvReflectInterfaceVariable& input = *(input_vars[index]);
                    VkVertexInputAttributeDescription& attr = shader->vertexAttributes[index];
                    attr.location = input.location;
                    attr.binding = shader->vertexBindingDesc.binding;
                    attr.format = (VkFormat)input.format;
                    attr.offset = 0;    // final value computed below
                }

                // sort by location
                std::sort(shader->vertexAttributes.begin(), shader->vertexAttributes.end(),
                [](const VkVertexInputAttributeDescription& a, const VkVertexInputAttributeDescription& b)
                {
                    return a.location < b.location;
                });

                for (auto& attribute : shader->vertexAttributes) {
                    uint32_t format_size = vkInit_GetFormatSize(attribute.format);
                    attribute.offset = shader->vertexBindingDesc.stride;
                    shader->vertexBindingDesc.stride += format_size;
                }
            }
        }

        // Loads the shader's descriptor sets
        {
            result = spvReflectEnumerateDescriptorSets(&module, &count, NULL);
            ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);

            std::vector<SpvReflectDescriptorSet*> sets(count);
            result = spvReflectEnumerateDescriptorSets(&module, &count, sets.data());
            ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);
            
            shader->set_layouts.assign(sets.size(), vg_shader_descriptorset_layoutdata{});
            for(size_t index = 0; index < sets.size(); ++index)
            {
                const SpvReflectDescriptorSet& reflSet = *sets[index];
                vg_shader_descriptorset_layoutdata& layoutdata = shader->set_layouts[index];

                layoutdata.bindings.resize(reflSet.binding_count);
                for(u32 bindingIndex = 0; bindingIndex < reflSet.binding_count; ++bindingIndex)
                {
                    const SpvReflectDescriptorBinding& refl_binding = *(reflSet.bindings[bindingIndex]);
                    VkDescriptorSetLayoutBinding& layout_binding = layoutdata.bindings[bindingIndex];

                    layout_binding.binding = refl_binding.binding;
                    layout_binding.descriptorType = (VkDescriptorType)refl_binding.descriptor_type;
                    layout_binding.descriptorCount = 1;
                    for (u32 i_dim = 0; i_dim < refl_binding.array.dims_count; ++i_dim)
                    {
                        layout_binding.descriptorCount *= refl_binding.array.dims[i_dim];
                    }
                    layout_binding.stageFlags = (VkShaderStageFlagBits)module.shader_stage;
                }
                layoutdata.setNumber = reflSet.set;
            }
        }

        // loads the push constant blocks
        {
            result = spvReflectEnumeratePushConstantBlocks(&module, &count, NULL);
            ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);

            std::vector<SpvReflectBlockVariable*> blocks(count);
            result = spvReflectEnumeratePushConstantBlocks(&module, &count, blocks.data());
            ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);

            shader->pushConstants.assign(count, VkPushConstantRange{});
            for(size_t index = 0; index < blocks.size(); ++index)
            {
                const SpvReflectBlockVariable& block = *blocks[index];
                VkPushConstantRange& pushConstant = shader->pushConstants[index];

                pushConstant.offset = block.offset;
                pushConstant.size = block.size;
                pushConstant.stageFlags = shader->shaderStageMask;
            }
        }

        spvReflectDestroyShaderModule(&module);
    }

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = bytes.size;
    createInfo.pCode = (u32*)bytes.data; 

    VkResult vulkanResult = vkCreateShaderModule(device, &createInfo, nullptr, &shader->shaderModule);

    return vulkanResult;
}

internal void
vgDestroyShader(VkDevice device, vg_shader& shader)
{
    vkDestroyShaderModule(device, shader.shaderModule, nullptr);
}

internal
VkResult vgCreateScreenRenderPass(vg_device& device)
{
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = vgFindDepthFormat(device);
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = device.swapChainFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkAttachmentDescription attachments[] = {colorAttachment, depthAttachment};
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = ARRAY_COUNT(attachments);
    renderPassInfo.pAttachments = attachments;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VkResult result = vkCreateRenderPass(device.handle, &renderPassInfo, nullptr, &device.screenRenderPass);

    return result;
}

internal
VkResult vgCreateFramebuffers(vg_device& device)
{
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
    }

    device.pCurFrame = &device.frames[0];
    device.pPrevFrame = &device.frames[FRAME_OVERLAP - 1];

    arrsetlen(device.paFramebuffers, device.numSwapChainImages);
    for(u32 i = 0; i < device.numSwapChainImages; ++i)
    {
        VkImageView attachments[] = {
            device.paSwapChainImages[i].view,
            device.depth_image.view
        };

        VkFramebufferCreateInfo framebufferInfo = vkInit_framebuffer_create_info(
            device.screenRenderPass, device.extent
        );
        framebufferInfo.attachmentCount = ARRAY_COUNT(attachments);
        framebufferInfo.pAttachments = attachments;

        VkResult result = vkCreateFramebuffer(device.handle, &framebufferInfo, nullptr, &device.paFramebuffers[i]);
        ASSERT(DIDSUCCEED(result));
    }

    return VK_SUCCESS;
}


internal
VkResult vgCreateDepthResources(vg_device& device)
{
    VkFormat depthFormat = vgFindDepthFormat(device);

    VkResult result = vgCreateImage(device, device.extent.width, device.extent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &device.depth_image);
    if(DIDFAIL(result))
    {
        ASSERT(false);
        return result;
    }

    result = vgCreateImageView(device, device.depth_image.handle, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, &device.depth_image.view);

    return result;
}

internal
VkResult vgCreateCommandPool(vg_device& device)
{
    VkResult result = VK_SUCCESS;

    for(u32 i = 0; i < FRAME_OVERLAP; ++i)
    {
        VkCommandPoolCreateInfo poolInfo = vkInit_command_pool_create_info
            (device.q_graphics.queue_family_index, 0);
        result = vkCreateCommandPool(device.handle, &poolInfo, nullptr, &device.frames[i].commandPool);

        if(DIDFAIL(result)) { 
            LOG_ERROR("Vulkan Error: %X", (result));
            ASSERT(false);
            return result; 
        }

        VkCommandBufferAllocateInfo allocInfo = vkInit_command_buffer_allocate_info(
            device.frames[i].commandPool, 1, VK_COMMAND_BUFFER_LEVEL_PRIMARY
        );

        result = vkAllocateCommandBuffers(device.handle, &allocInfo, &device.frames[i].commandBuffer);

        if(DIDFAIL(result)) { 
            LOG_ERROR("Vulkan Error: %X", (result));
            ASSERT(false);
            return result; 
        }
    }


    // temporary triangle draw that must be done on all the command buffers
 

    return VK_SUCCESS;
}

internal
void vgDestroySwapChain(vg_device& device)
{
    for(u32 i = 0; i < FRAME_OVERLAP; ++i)
    {
        vg_framedata& frame = device.frames[i];
        
        vgCleanupDescriptorAllocator(frame.dynamicDescriptorAllocator);
        vkFreeCommandBuffers(device.handle, frame.commandPool, 1, &frame.commandBuffer);
        vkDestroyCommandPool(device.handle, frame.commandPool, nullptr);
        vkDestroySemaphore(device.handle, frame.renderSemaphore, nullptr);
        vkDestroySemaphore(device.handle, frame.presentSemaphore, nullptr);
        vkDestroyFence(device.handle, frame.renderFence, nullptr);
        vgDestroyBuffer(device.handle, frame.frame_buffer);
    }

    IFF(device.screenRenderPass, vkDestroyRenderPass(device.handle, device.screenRenderPass, nullptr));
    
    for(size_t i = 0; i < arrlenu(device.paSwapChainImages); ++i)
    {
        vkDestroyImageView(device.handle, device.paSwapChainImages[i].view, nullptr);
    }
    arrfree(device.paSwapChainImages);

    for(size_t i = 0; i < arrlenu(device.paFramebuffers); ++i)
    {
        vkDestroyFramebuffer(device.handle, device.paFramebuffers[i], nullptr);
    }
    arrfree(device.paFramebuffers);

    vkDestroySwapchainKHR(device.handle, device.swapChain, nullptr);
}


internal void
vgCleanupResourcePool(vg_device& device)
{
    vg_device_resource_pool& pool = device.resource_pool;

    for(u32 i = 0; i < pool.materialCount; ++i)
    {
        vkDestroyPipeline(device.handle, pool.materials[i].pipeline, nullptr);
        vkDestroyPipelineLayout(device.handle, pool.materials[i].layout, nullptr);

        if(pool.materials[i].renderPass != VK_NULL_HANDLE)
        {
            vkDestroyRenderPass(device.handle, pool.materials[i].renderPass, nullptr);
            vkDestroyFramebuffer(device.handle, pool.materials[i].framebuffer, nullptr);
        }

        for(u32 k = 0; k < pool.materials[i].samplerCount; ++k)
        {
            vkDestroySampler(device.handle, pool.materials[i].samplers[k], nullptr);
        }
        pool.materials[i].samplerCount = 0;
    }

    for(u32 i = 0; i < pool.imageCount; ++i)
    {
        vgDestroyImage(device.handle, pool.images[i]);
    }

    for(u32 i = 0; i < pool.shaderCount; ++i)
    {
        vgDestroyShader(device.handle, pool.shaders[i]);
    }

    for(u32 i = 0; i < pool.bufferCount; ++i)
    {
        vgDestroyBuffer(device.handle, pool.buffers[i]);
    }

    pool.materialCount = 0;
    pool.imageCount = 0;
    pool.shaderCount = 0;
    pool.bufferCount = 0;
}

internal
void vgDestroy(vg_backend& vb)
{
    // NOTE(james): Clean up the rest of the backend members prior to destroying the instance
    if(vb.device.handle)
    {
        vg_device& device = vb.device;
        vgCleanupResourcePool(device);

        vgDestroySwapChain(device);

        // TODO(james): Account for this in the window resize
        vgDestroyImage(device.handle, device.depth_image);
        
        vgCleanupDescriptorAllocator(device.descriptorAllocator);
        vgCleanupDescriptorLayoutCache(device.descriptorLayoutCache);

        vkDestroyDevice(device.handle, nullptr);
    }

    vkDestroySurfaceKHR(vb.instance, vb.device.platform_surface, nullptr);

    if(vb.debugMessenger)
    {
        vbDestroyDebugUtilsMessengerEXT(vb.instance, vb.debugMessenger, nullptr);
    }

    vkDestroyInstance(vb.instance, nullptr);
}

inline vg_shader* 
vgGetPoolShaderFromId(vg_device_resource_pool& pool, render_shader_id id)
{
    vg_shader* shader = 0;

    for(u32 i = 0; i < pool.shaderCount && !shader; ++i)
    {
        if(pool.shaders[i].id == id)
        {
            shader = &pool.shaders[i];
        }
    }

    return shader;
}

inline vg_image* 
vgGetPoolImageFromId(vg_device_resource_pool& pool, render_image_id id)
{
    vg_image* image = 0;

    for(u32 i = 0; i < pool.imageCount && !image; ++i)
    {
        if(pool.images[i].id == id)
        {
            image = &pool.images[i];
        }
    }

    return image;
}

inline vg_buffer* 
vgGetPoolBufferFromId(vg_device_resource_pool& pool, render_buffer_id id)
{
    vg_buffer* buffer = 0;

    for(u32 i = 0; i < pool.bufferCount && !buffer; ++i)
    {
        if(pool.buffers[i].id == id)
        {
            buffer = &pool.buffers[i];
        }
    }

    return buffer;
}

inline vg_material_resource* 
vgGetPoolMaterialFromId(vg_device_resource_pool& pool, render_material_id id)
{
    vg_material_resource* material = 0;

    for(u32 i = 0; i < pool.materialCount && !material; ++i)
    {
        if(pool.materials[i].id == id)
        {
            material = &pool.materials[i];
        }
    }

    return material;
}

internal void
vgCreateManifestResources(vg_device& device, render_manifest* manifest)
{
    VkResult result = VK_SUCCESS;

    // TODO(james): Just create a single persistant staging buffer per device
    VkDeviceSize stagingBufferSize = Megabytes(256);
    vg_buffer stagingBuffer{};
    result = vgCreateBuffer(device, stagingBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer);
    ASSERT(result == VK_SUCCESS);
    void* mappedStagingData;    // NOTE(james): using persistant mapped staging memory here
    umm lastStagingWriteOffset = 0;
    vkMapMemory(device.handle, stagingBuffer.memory, 0, stagingBufferSize, 0, &mappedStagingData);

    // TODO(james): allocate the command buffer from a thread specific command pool for background creation/transfer of resources
    VkCommandBuffer cmds = vgBeginSingleTimeCommands(device);

    vg_device_resource_pool& pool = device.resource_pool;

    for(u32 i = 0; i < manifest->shaderCount; ++i)
    {
        const render_shader_desc& shaderDesc = manifest->shaders[i];
        vg_shader& shader = pool.shaders[pool.shaderCount++];

        buffer shaderBuffer;
        shaderBuffer.size = shaderDesc.sizeInBytes;
        shaderBuffer.data = (u8*)shaderDesc.bytes;

        result = vgCreateShader(device.handle, shaderBuffer, &shader);
        ASSERT(result == VK_SUCCESS);

        shader.id = shaderDesc.id;
    }

    for(u32 i = 0; i < manifest->bufferCount; ++i)
    {
        const render_buffer_desc& bufferDesc = manifest->buffers[i];
        vg_buffer& buffer = pool.buffers[pool.bufferCount++];

        VkBufferUsageFlags usageFlags = 0;
        VkMemoryPropertyFlags memProperties = 0;
        VkDeviceSize bufferSize = bufferDesc.sizeInBytes;

        switch(bufferDesc.type)
        {
            case RenderBufferType::Vertex:
                usageFlags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
                break;
            case RenderBufferType::Index:
                usageFlags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
                break;
            case RenderBufferType::Uniform:
                usageFlags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
                break;

            InvalidDefaultCase;
        }

        b32 useStagingBuffer = false;

        switch(bufferDesc.usage)
        {
            case RenderUsage::Static:
                useStagingBuffer = true;
                usageFlags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
                memProperties |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
                break;
            case RenderUsage::Dynamic:
                usageFlags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
                memProperties |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;    // TODO(james): confirm these properties are the most efficient for a dynamically changing buffer
                break;

            InvalidDefaultCase;
        }

        result = vgCreateBuffer(device, bufferSize, usageFlags, memProperties, &buffer);
        ASSERT(result == VK_SUCCESS);

        buffer.id = bufferDesc.id;

        // now upload the buffer data
        if(useStagingBuffer)
        {
            // NOTE(james): If this asserts, we'll need to go ahead and flush the staging buffer before proceeding further...
            ASSERT(lastStagingWriteOffset + bufferDesc.sizeInBytes <= Megabytes(256));
            void* stagingOffsetPtr = OffsetPtr(mappedStagingData, lastStagingWriteOffset);
            Copy(bufferDesc.sizeInBytes, bufferDesc.bytes, stagingOffsetPtr);

            VkBufferCopy copyRegion{};
            copyRegion.srcOffset = lastStagingWriteOffset;
            copyRegion.size = bufferDesc.sizeInBytes;
            copyRegion.dstOffset = 0;
            vkCmdCopyBuffer(cmds, stagingBuffer.handle, buffer.handle, 1, &copyRegion);

            lastStagingWriteOffset += bufferDesc.sizeInBytes;
        }
        else
        {
            // NOTE(james): dynamic buffers need to have a copy for each in-flight frame so that they can be updated
            // TODO(james): actually support this dynamic update per frame...
            NotImplemented;
            void* data;
            vkMapMemory(device.handle, buffer.memory, 0, bufferDesc.sizeInBytes, 0, &data);
                Copy(bufferDesc.sizeInBytes, bufferDesc.bytes, data);
            vkUnmapMemory(device.handle, buffer.memory);
        }
    }

    for(u32 i = 0; i < manifest->imageCount; ++i)
    {
        const render_image_desc& imageDesc = manifest->images[i];
        vg_image& image = pool.images[pool.imageCount++];

        VkBufferUsageFlags usageFlags = VK_IMAGE_USAGE_SAMPLED_BIT;
        VkMemoryPropertyFlags memProperties = 0;
        VkFormat format = GetVkFormatFromRenderFormat(imageDesc.format);
        VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;

        b32 useStagingBuffer = false;

        switch(imageDesc.usage)
        {
            case RenderUsage::Static:
                useStagingBuffer = true;
                usageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT ;
                memProperties |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
                break;
            case RenderUsage::Dynamic:
                tiling = VK_IMAGE_TILING_LINEAR;
                usageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
                memProperties |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;    // TODO(james): confirm these properties are the most efficient for a dynamically changing buffer
                break;

            InvalidDefaultCase;
        }

        result = vgCreateImage(device, (u32)imageDesc.dimensions.Width, (u32)imageDesc.dimensions.Height, format, tiling, usageFlags, memProperties, &image);
        ASSERT(result == VK_SUCCESS);

        image.id = imageDesc.id;

        // now setup the image "view"
        
        VkImageViewCreateInfo viewInfo = vkInit_imageview_create_info(
            format, image.handle, VK_IMAGE_ASPECT_COLOR_BIT
        );

        result = vkCreateImageView(device.handle, &viewInfo, nullptr, &image.view);
        ASSERT(result == VK_SUCCESS);

        // and finally copy the image pixels over to VRAM

        if(useStagingBuffer)
        {
            umm pixelSizeInBytes = (umm)(imageDesc.dimensions.Width * imageDesc.dimensions.Height * vkInit_GetFormatSize(format));
            ASSERT(lastStagingWriteOffset + pixelSizeInBytes <= Megabytes(256));
            void* stagingOffsetPtr = OffsetPtr(mappedStagingData, lastStagingWriteOffset);
            Copy(pixelSizeInBytes, imageDesc.pixels, stagingOffsetPtr);

            // NOTE(james): Images are weird in that you have to transfer them to the proper format for each stage you use them
            VkImageMemoryBarrier copyBarrier = vkInit_image_barrier(
                image.handle, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT
            );

            vkCmdPipelineBarrier(cmds, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,  VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &copyBarrier);

            VkBufferImageCopy region{};
            region.bufferOffset = lastStagingWriteOffset;
            region.bufferRowLength = 0;
            region.bufferImageHeight = 0;

            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel = 0;
            region.imageSubresource.baseArrayLayer = 0;
            region.imageSubresource.layerCount = 1;

            region.imageOffset = {0,0,0};
            region.imageExtent = { (u32)imageDesc.dimensions.Width, (u32)imageDesc.dimensions.Height, 1 };

            vkCmdCopyBufferToImage(cmds, stagingBuffer.handle, image.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

            lastStagingWriteOffset += pixelSizeInBytes;

            VkImageMemoryBarrier useBarrier = vkInit_image_barrier(
                image.handle, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT
            );

            vkCmdPipelineBarrier(cmds, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &useBarrier);
        }
        else
        {
            NotImplemented;
        }
    }
    
    for(u32 i = 0; i < manifest->materialCount; ++i)
    {
        const render_material_desc& materialDesc = manifest->materials[i];
        vg_material_resource& material = pool.materials[pool.materialCount++];
        material.id = materialDesc.id;

        // first gather the shader references
        for(u32 k = 0; k < materialDesc.shaderCount; ++k)
        {
            material.shaderRefs[material.shaderCount++] = vgGetPoolShaderFromId(pool, materialDesc.shaders[k]);
        }

        // now create the samplers
        for(u32 k = 0; k < materialDesc.samplerCount; ++k)
        {
            const render_sampler_desc& samplerDesc = materialDesc.samplers[k];
            VkSampler& sampler = material.samplers[material.samplerCount++];

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
            
            result = vkCreateSampler(device.handle, &samplerInfo, nullptr, &sampler);
            ASSERT(result == VK_SUCCESS);
        }

        // and finally it is time to slog through the render pass and pipeline creation, ugh...

        // NOTE(james): A render pass of VK_NULL_HANDLE will indicate that the material will use the default screen render pass
        // TODO(james): implement render target creation logic
        material.renderPass = VK_NULL_HANDLE;
        material.framebuffer = VK_NULL_HANDLE;

        VkRenderPass pipelineRenderPass = device.screenRenderPass;

        VkVertexInputBindingDescription* vertexBindingDescription = 0;
        std::vector<VkVertexInputAttributeDescription>* vertexAttributeDescriptions = 0;

        std::vector<VkPipelineShaderStageCreateInfo> shaderStages(material.shaderCount);
        for(u32 k = 0; k < material.shaderCount; ++k)
        {
            shaderStages[k] = vkInit_pipeline_shader_stage_create_info((VkShaderStageFlagBits)material.shaderRefs[k]->shaderStageMask, material.shaderRefs[k]->shaderModule);

            if(material.shaderRefs[k]->shaderStageMask == VK_SHADER_STAGE_VERTEX_BIT)
            {
                vertexBindingDescription = &material.shaderRefs[k]->vertexBindingDesc;
                vertexAttributeDescriptions = &material.shaderRefs[k]->vertexAttributes;
            }
        }
        // NOTE(james): I'm assuming these have to be valid for now
        ASSERT(vertexBindingDescription);
        ASSERT(vertexAttributeDescriptions);

        VkPipelineVertexInputStateCreateInfo vertexInputInfo = vkInit_vertex_input_state_create_info();
        vertexInputInfo.vertexBindingDescriptionCount = vertexBindingDescription ? 1 : 0;
        vertexInputInfo.pVertexBindingDescriptions = vertexBindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = vertexAttributeDescriptions ? (u32)vertexAttributeDescriptions->size() : 0;
        vertexInputInfo.pVertexAttributeDescriptions = vertexAttributeDescriptions ? vertexAttributeDescriptions->data() : 0;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly = vkInit_input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (f32)device.extent.width;
        viewport.height = (f32)device.extent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = {0,0};
        scissor.extent = device.extent;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterizer = vkInit_rasterization_state_create_info(
            VK_POLYGON_MODE_FILL
        );
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

        VkPipelineMultisampleStateCreateInfo multisampling = vkInit_multisampling_state_create_info();

        VkPipelineColorBlendAttachmentState colorBlendAttachment = vkInit_color_blend_attachment_state();
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY; 
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f; 
        colorBlending.blendConstants[1] = 0.0f; 
        colorBlending.blendConstants[2] = 0.0f; 
        colorBlending.blendConstants[3] = 0.0f; 

        VkPipelineDepthStencilStateCreateInfo depthStencil = vkInit_depth_stencil_create_info(
            true, true, VK_COMPARE_OP_LESS
        );

        std::vector<VkPushConstantRange> pushConstants;
        for(u32 k = 0; k < material.shaderCount; ++k)
        {
            pushConstants.insert(pushConstants.end(), material.shaderRefs[k]->pushConstants.begin(), material.shaderRefs[k]->pushConstants.end());

            for(auto& layout : material.shaderRefs[k]->set_layouts)
            {
                ASSERT(material.descriptorSetLayoutCount < 20);
                material.descriptorLayouts[material.descriptorSetLayoutCount++] = vgGetDescriptorLayoutFromCache( device.descriptorLayoutCache, (u32)layout.bindings.size(), layout.bindings.data());
            }
        }

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = vkInit_pipeline_layout_create_info();
        pipelineLayoutInfo.setLayoutCount = material.descriptorSetLayoutCount; 
        pipelineLayoutInfo.pSetLayouts = material.descriptorLayouts; 
        pipelineLayoutInfo.pushConstantRangeCount = (u32)pushConstants.size(); 
        pipelineLayoutInfo.pPushConstantRanges = pushConstants.data(); 

        result = vkCreatePipelineLayout(device.handle, &pipelineLayoutInfo, nullptr, &material.layout);
        ASSERT(result == VK_SUCCESS);

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = (u32)shaderStages.size();
        pipelineInfo.pStages = shaderStages.data();
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil; // TODO(james): make this depend on the render target setup of the material
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = nullptr; // Optional
        pipelineInfo.layout = material.layout;
        pipelineInfo.renderPass = pipelineRenderPass;
        pipelineInfo.subpass = 0;
        // TODO(james): Figure out how to use the base pipeline definitions to make this simpler/easier
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex = -1;

        result = vkCreateGraphicsPipelines(device.handle, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &material.pipeline);
        ASSERT(result == VK_SUCCESS);
        
    }

    vkUnmapMemory(device.handle, stagingBuffer.memory);

    vgEndSingleTimeCommands(device, cmds);
    vgDestroyBuffer(device.handle, stagingBuffer);
}

internal void
vgPerformResourceOperation(vg_device& device, RenderResourceOpType opType, render_manifest* manifest)
{
    switch(opType)
    {
        case RenderResourceOpType::Create:
            vgCreateManifestResources(device, manifest);
            break;

        InvalidDefaultCase;
    }
}

internal void
VulkanGraphicsBeginFrame(vg_backend* vb, render_commands* cmds)
{
    vg_device& device = vb->device;

    device.pPrevFrame = device.pCurFrame;
    device.pCurFrame = &device.frames[device.currentFrameIndex];
    
    cmds->pushBufferBase = vb->pushBuffer;
    cmds->pushBufferDataAt = vb->pushBuffer;
    cmds->maxPushBufferSize = U16MAX;
}

internal
void vgTranslateRenderCommands(vg_device& device, render_commands* commands)
{
    VkCommandBufferBeginInfo beginInfo = vkInit_command_buffer_begin_info(0);

    VkCommandBuffer commandBuffer = device.pCurFrame->commandBuffer;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    
    VkClearValue clearValues[2] = {};
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    VkFramebuffer currentScreenFramebuffer = device.paFramebuffers[device.curSwapChainIndex];

    // TODO(james): MAJOR PERFORMANCE BOTTLENECK!!! Need to sort objects into render pass bins first -OR- add a command to bind the material prior to drawing!

    VkRenderPassBeginInfo renderPassInfo = vkInit_renderpass_begin_info(
        device.screenRenderPass, device.extent, currentScreenFramebuffer
    );
    renderPassInfo.clearValueCount = ARRAY_COUNT(clearValues);
    renderPassInfo.pClearValues = clearValues;
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    //
    // Iterate through the render commands
    //

    render_cmd_header* header = (render_cmd_header*)commands->pushBufferBase;

    while(header->type != RenderCommandType::Done)
    {
        switch(header->type)
        {
            case RenderCommandType::UpdateViewProjection:
            {
                render_cmd_update_viewprojection* cmd = (render_cmd_update_viewprojection*)header;

                FrameObject frameObject;
                frameObject.viewProj = cmd->projection * cmd->view;

                void* data;
                vkMapMemory(device.handle, device.pCurFrame->frame_buffer.memory, 0, sizeof(frameObject), 0, &data);
                    Copy(sizeof(frameObject), &frameObject, data);
                vkUnmapMemory(device.handle, device.pCurFrame->frame_buffer.memory);
            } break;
            case RenderCommandType::DrawObject:
            {
                render_cmd_draw_object* cmd = (render_cmd_draw_object*)header;

                InstanceObject instanceObject;
                instanceObject.mvp = cmd->mvp;

                vg_material_resource& material = *vgGetPoolMaterialFromId(device.resource_pool, cmd->material_id);
                
                // TODO(james): Support different render passes from a material
                ASSERT(material.renderPass == VK_NULL_HANDLE);
                // VkRenderPass renderPass = device.screenRenderPass;
                // VkFramebuffer framebuffer = currentScreenFramebuffer;

                // if(material.renderPass != VK_NULL_HANDLE)
                // {
                //     renderPass = material.renderPass;
                //     framebuffer = material.framebuffer;
                // }
                
                vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, material.pipeline);

                vg_buffer& vertexBuffer = *vgGetPoolBufferFromId(device.resource_pool, cmd->vertexBuffer);
                vg_buffer& indexBuffer = *vgGetPoolBufferFromId(device.resource_pool, cmd->indexBuffer);

                std::vector<VkDescriptorSet> descriptors(material.descriptorSetLayoutCount, VK_NULL_HANDLE);
                for(u32 i = 0; i < material.descriptorSetLayoutCount; ++i)
                {
                    vgAllocateDescriptor(device.pCurFrame->dynamicDescriptorAllocator, material.descriptorLayouts[i], &descriptors[i]);
                }

                if(cmd->materialBindingCount)
                {
                    std::vector<VkWriteDescriptorSet> writes(cmd->materialBindingCount);
                    for(u32 i = 0; i < cmd->materialBindingCount; ++i)
                    {
                        const render_material_binding& binding = cmd->materialBindings[i];
                        ASSERT(binding.layoutIndex < (u32)descriptors.size());

                        switch(binding.type)
                        {
                            case RenderMaterialBindingType::Buffer:
                                {
                                    vg_buffer& buffer = *vgGetPoolBufferFromId(device.resource_pool, binding.buffer_id);
                                    VkDescriptorBufferInfo bufferInfo{};
                                    bufferInfo.buffer = buffer.handle;
                                    bufferInfo.offset = (VkDeviceSize)binding.buffer_offset;
                                    bufferInfo.range = (VkDeviceSize)binding.buffer_range;
                                    writes[i] = vkInit_write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, descriptors[binding.layoutIndex], &bufferInfo, binding.bindingIndex);
                                }
                                break;
                            case RenderMaterialBindingType::Image:
                                {
                                    vg_image& image = *vgGetPoolImageFromId(device.resource_pool, binding.image_id);
                                    ASSERT(binding.image_sampler_index < material.samplerCount);
                                    VkSampler sampler = material.samplers[binding.image_sampler_index];

                                    VkDescriptorImageInfo imageInfo{};
                                    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                                    imageInfo.imageView = image.view;
                                    imageInfo.sampler = sampler;
                                    writes[i] = vkInit_write_descriptor_image(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, descriptors[binding.layoutIndex], &imageInfo, binding.bindingIndex);
                                }
                                break;
                            
                            InvalidDefaultCase;
                        }
                    }

                    vkUpdateDescriptorSets(device.handle, (u32)writes.size(), writes.data(), 0, nullptr);
                    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, material.layout, 0, (u32)descriptors.size(), descriptors.data(), 0, nullptr);
                }

                VkDeviceSize offsets[] = {0};
                vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer.handle, offsets);
                vkCmdBindIndexBuffer(commandBuffer, indexBuffer.handle, 0, VK_INDEX_TYPE_UINT32);

                vkCmdPushConstants(commandBuffer, material.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(instanceObject.mvp), &instanceObject.mvp);
                vkCmdDrawIndexed(commandBuffer, (u32)cmd->indexCount, 1, 0, 0, 0);                
            } break;
            default:
                // command is not supported
                break;
        }

        // move to the next command
        header = (render_cmd_header*)OffsetPtr(header, header->size);
    }

    vkCmdEndRenderPass(commandBuffer);
    vkEndCommandBuffer(commandBuffer);    
}

internal
void VulkanGraphicsEndFrame(vg_backend* vb, render_commands* commands)
{
    vg_device& device = vb->device;

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

    vkResetCommandPool(device.handle, device.pCurFrame->commandPool, 0);
    vgResetDescriptorPools(device.pCurFrame->dynamicDescriptorAllocator);
 
    // wait fence here...
    u32 imageIndex = device.curSwapChainIndex;
    
    VkSemaphore waitSemaphores[] = { device.pCurFrame->presentSemaphore };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSemaphore signalSemaphores[] = { device.pCurFrame->renderSemaphore };

    vgTranslateRenderCommands(device, commands);
    //vgTempBuildRenderCommands(device, device.curSwapChainIndex);

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

    device.currentFrameIndex = (device.currentFrameIndex + 1) % FRAME_OVERLAP;
}