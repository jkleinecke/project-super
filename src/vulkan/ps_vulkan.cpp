
#include <vulkan/vulkan.h>

#include "ps_vulkan.h"
#include "ps_vulkan_extensions.h"

#include <vector>
#include <array>

#if defined(PROJECTSUPER_INTERNAL)
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
struct UniformBufferObject
{
    alignas(16) m4 model;
    alignas(16) m4 view;
    alignas(16) m4 proj;
};

struct ps_vertex
{
    v3 pos;
    v3 color;
    v2 texCoord;
};

global const ps_vertex g_Vertices[] = {
    {{-0.5f, -0.5f,  0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
    {{ 0.5f, -0.5f,  0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
    {{ 0.5f,  0.5f,  0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
    {{-0.5f,  0.5f,  0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},

    {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
    {{ 0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
    {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
    {{-0.5f,  0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},

};

global const u16 g_Indices[] {
    0, 1, 2, 2, 3, 0,
    4, 5, 6, 6, 7, 4
};

internal VkVertexInputBindingDescription vbGetVertexBindingDescription()
{
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(ps_vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDescription;
}

internal std::array<VkVertexInputAttributeDescription, 3> vbGetVertexAttributeDescriptions()
{
    std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = OffsetOf(ps_vertex, pos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = OffsetOf(ps_vertex, color);

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[2].offset = OffsetOf(ps_vertex, texCoord);

    return attributeDescriptions;
}

internal
void vbDestroyBuffer(VkDevice device, ps_vulkan_buffer& buffer)
{
    IFF(buffer.handle, vkDestroyBuffer(device, buffer.handle, nullptr));
    IFF(buffer.memory_handle, vkFreeMemory(device, buffer.memory_handle, nullptr));
}

internal
void vbDestroyImage(VkDevice device, ps_vulkan_image& image)
{
    IFF(image.view, vkDestroyImageView(device, image.view, nullptr));
    IFF(image.handle, vkDestroyImage(device, image.handle, nullptr));
    IFF(image.memory_handle, vkFreeMemory(device, image.memory_handle, nullptr));
}

internal
VkCommandBuffer vbBeginSingleTimeCommands(ps_vulkan_backend& vb)
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = vb.command_pool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(vb.device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

internal
void vbEndSingleTimeCommands(ps_vulkan_backend& vb, VkCommandBuffer commandBuffer)
{
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    // TODO(james): give it a fence so we can tell when it is done?
    vkQueueSubmit(vb.q_graphics.handle, 1, &submitInfo, VK_NULL_HANDLE);
    // Have to call QueueWaitIdle unless we're going to go the event route
    vkQueueWaitIdle(vb.q_graphics.handle);

    vkFreeCommandBuffers(vb.device, vb.command_pool, 1, &commandBuffer);
}

internal
VkFormat vbFindSupportedFormat(ps_vulkan_backend& vb, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
{
    for(VkFormat format : candidates)
    {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(vb.physicalDevice, format, &props);

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
VkFormat vbFindDepthFormat(ps_vulkan_backend& vb)
{
    return vbFindSupportedFormat(
        vb,
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

internal 
bool vbHasStencilComponent(VkFormat format)
{
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

internal VKAPI_ATTR VkBool32 VKAPI_CALL vbDebugCallback(
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
        LOG_WARN("Validation Layer: %s", pCallbackData->pMessage);
    }
    else
    {
        LOG_INFO("Validation Layer: %s", pCallbackData->pMessage);
    }

    return VK_FALSE;
}


internal
bool vbCheckValidationLayerSupport() {
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
void vbPopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = vbDebugCallback;
    createInfo.pUserData = nullptr; // Optional
}

internal
void vbGetAvailableExtensions(VkPhysicalDevice device, std::vector<VkExtensionProperties>& extensions)
{
    // first get the number of extensions
    u32 extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    // now get the full list
    extensions.assign(extensionCount, VkExtensionProperties{});
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data());
}

internal
void vbLogAvailableExtensions(VkPhysicalDevice device)
{
// NOTE(james): This is what is available on my 3090 
// Available Vulkan Extensions:
//     VK_KHR_device_group_creation
//     VK_KHR_display
//     VK_KHR_external_fence_capabilities
//     VK_KHR_external_memory_capabilities
//     VK_KHR_external_semaphore_capabilities
//     VK_KHR_get_display_properties2
//     VK_KHR_get_physical_device_properties2
//     VK_KHR_get_surface_capabilities2
//     VK_KHR_surface
//     VK_KHR_surface_protected_capabilities
//     VK_KHR_win32_surface
//     VK_EXT_debug_report
//     VK_EXT_debug_utils
//     VK_EXT_swapchain_colorspace
//     VK_NV_external_memory_capabilities

    // now get the full list
    std::vector<VkExtensionProperties> extensions;
    vbGetAvailableExtensions(device, extensions);

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
VkResult vbInitialize(ps_vulkan_backend& vb, const std::vector<const char*>* platformExtensions)
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

            vbPopulateDebugMessengerCreateInfo(debugCreateInfo);
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

        //--vbLogAvailableExtensions(nullptr);

        // Assign the values to the instance
        vb.debugMessenger = debugMessenger;
        vb.instance = instance;
    }

#undef VERIFY_SUCCESS

    return VK_SUCCESS;
}

internal bool
vbFindPresentQueueFamily(VkPhysicalDevice device, VkSurfaceKHR platformSurface, u32* familyIndex)
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
vbFindQueueFamily(VkPhysicalDevice device, VkFlags withFlags, u32* familyIndex)
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
vbIsDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR platformSurface, const std::vector<const char*>& requiredExtensions)
{
    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

    if(!supportedFeatures.samplerAnisotropy)
    {
        // missing sampler anisotropy?? how old is this gpu?
        return false;   
    }

    u32 graphicsQueueIndex = 0;
    if(!vbFindQueueFamily(device, VK_QUEUE_GRAPHICS_BIT, &graphicsQueueIndex))
    {
        // no graphics queues... which is a minimum requirement
        return false;   // not suitable
    }

    if(!vbFindPresentQueueFamily(device, platformSurface, nullptr))
    {
        // since a platform surface was defined, there has to be at least one present queue family
        return false;
    }

    std::vector<VkExtensionProperties> extensions;
    vbGetAvailableExtensions(device, extensions);

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
vbGetPhysicalDeviceSuitability(VkPhysicalDevice device, VkSurfaceKHR platformSurface)
{
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);

    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
    
    int nScore = 0;     // not suitable

    // TODO(james): Look at memory capabilities
    
    // Look at Queue Family capabilities
    u32 graphicsQueueIndex = 0;
    if(!vbFindQueueFamily(device, VK_QUEUE_GRAPHICS_BIT, &graphicsQueueIndex))
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
    else if(!vbFindPresentQueueFamily(device, platformSurface, nullptr))
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
i32 vbFindMemoryType(ps_vulkan_backend& vb, u32 typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties& memProperties = vb.device_memory_properties;

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
VkResult vbCreateBuffer(ps_vulkan_backend& vb, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, ps_vulkan_buffer* pBuffer)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult result = vkCreateBuffer(vb.device, &bufferInfo, nullptr, &pBuffer->handle);
    if(DIDFAIL(result))
    {
        ASSERT(false);
        return result;
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(vb.device, pBuffer->handle, &memRequirements);

    i32 memoryIndex = vbFindMemoryType(vb, memRequirements.memoryTypeBits, properties);
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

    result = vkAllocateMemory(vb.device, &allocInfo, nullptr, &pBuffer->memory_handle);
    if(DIDFAIL(result))
    {
        ASSERT(false);
        return result;
    }

    vkBindBufferMemory(vb.device, pBuffer->handle, pBuffer->memory_handle, 0);

    return VK_SUCCESS;
}

internal
void vbCopyBuffer(ps_vulkan_backend& vb, VkBuffer src, VkBuffer dest, VkDeviceSize size)
{
    VkCommandBuffer commandBuffer = vbBeginSingleTimeCommands(vb);

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, src, dest, 1, &copyRegion);

    vbEndSingleTimeCommands(vb, commandBuffer);
}

internal
VkResult vbCreateImage(ps_vulkan_backend& vb, u32 width, u32 height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, ps_vulkan_image* pImage)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult result = vkCreateImage(vb.device, &imageInfo, nullptr, &pImage->handle);
    ASSERT(DIDSUCCEED(result));

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(vb.device, pImage->handle, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = vbFindMemoryType(vb, memRequirements.memoryTypeBits, properties);
    ASSERT(allocInfo.memoryTypeIndex > 0);

    vkAllocateMemory(vb.device, &allocInfo, nullptr, &pImage->memory_handle);

    vkBindImageMemory(vb.device, pImage->handle, pImage->memory_handle, 0);

    return VK_SUCCESS;
}

internal
void vbTransitionImageLayout(ps_vulkan_backend& vb, ps_vulkan_image& image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
{
    VkCommandBuffer commandBuffer = vbBeginSingleTimeCommands(vb);

    VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_NONE_KHR;
    VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_NONE_KHR;

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image.handle;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    // TODO(james): This feels dirty, think about a simpler more robust solution here
    if(newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

        if(vbHasStencilComponent(format)) 
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

    vbEndSingleTimeCommands(vb, commandBuffer);
}

VkResult vbCreateImageView(ps_vulkan_backend& vb, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, VkImageView* pView)
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    // TODO(james): add swizzling support

    VkResult result = vkCreateImageView(vb.device, &viewInfo, nullptr, pView);
    ASSERT(DIDSUCCEED(result));

    return result;
}

internal
void vbCopyBufferToImage(ps_vulkan_backend& vb, ps_vulkan_buffer& buffer, ps_vulkan_image& image, u32 width, u32 height)
{
    VkCommandBuffer commandBuffer = vbBeginSingleTimeCommands(vb);

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

    vbEndSingleTimeCommands(vb, commandBuffer);
}

internal VkResult
vbCreateDevice(ps_vulkan_backend& vb, const std::vector<const char*>* platformDeviceExtensions)
{
    VkResult result = VK_ERROR_UNKNOWN;

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
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

        u32 deviceCount = 0;
        vkEnumeratePhysicalDevices(vb.instance, &deviceCount, nullptr);

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(vb.instance, &deviceCount, devices.data());

        u32 physicalDeviceSuitability = 0;
        for (const auto& device : devices)
        {
            if(vbIsDeviceSuitable(device, vb.swap_chain.platform_surface, deviceExtensions))
            {
                vbLogAvailableExtensions(device);
                // select the device with the highest suitability score
                u32 suitability = vbGetPhysicalDeviceSuitability(device, vb.swap_chain.platform_surface);

                if(suitability >= physicalDeviceSuitability)
                {
                    physicalDevice = device;
                    physicalDeviceSuitability = suitability;
                }
            }
        }

        if(physicalDevice == VK_NULL_HANDLE)
        {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        vb.physicalDevice = physicalDevice;

        vkGetPhysicalDeviceProperties(vb.physicalDevice, &vb.device_properties);
        vkGetPhysicalDeviceMemoryProperties(vb.physicalDevice, &vb.device_memory_properties);
    }

    // Create the Logical Device
    {
        VkDevice device = VK_NULL_HANDLE;

        u32 graphicsQueueIndex = 0;
        u32 presentQueueIndex = 0;
        vbFindQueueFamily(vb.physicalDevice, VK_QUEUE_GRAPHICS_BIT, &graphicsQueueIndex);

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
        vkGetPhysicalDeviceSurfaceSupportKHR(vb.physicalDevice, graphicsQueueIndex, vb.swap_chain.platform_surface, &presentSupport);

        if(presentSupport)
        {
            presentQueueIndex = graphicsQueueIndex;
        }
        else
        {
            // Needs a separate queue for presenting the platform surface
            vbFindPresentQueueFamily(vb.physicalDevice, vb.swap_chain.platform_surface, &presentQueueIndex);

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

        result = vkCreateDevice(vb.physicalDevice, &createInfo, nullptr, &device);
        VERIFY_SUCCESS(result);

        vb.device = device;

        vb.q_graphics.queue_family_index = graphicsQueueIndex;
        vb.q_present.queue_family_index = presentQueueIndex;

        vkGetDeviceQueue(device, graphicsQueueIndex, 0, &vb.q_graphics.handle);
        vkGetDeviceQueue(device, presentQueueIndex, 0, &vb.q_present.handle);
    }

#undef VERIFY_SUCCESS

    return VK_SUCCESS;
}


internal
VkResult vbCreateDescriptorPool(ps_vulkan_backend& vb)
{
    VkDescriptorPoolSize poolSizes[2] = {};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = (u32)vb.swap_chain.images.size();
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = (u32)vb.swap_chain.images.size();

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = ARRAY_COUNT(poolSizes);
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = (u32)vb.swap_chain.images.size();
    poolInfo.flags = 0; // optional

    VkResult result = vkCreateDescriptorPool(vb.device, &poolInfo, nullptr, &vb.descriptor_pool);
    ASSERT(DIDSUCCEED(result));

    return result;
}

internal
VkResult vbCreateDescriptorSets(ps_vulkan_backend& vb)
{
    ASSERT(vb.descriptorSetLayout);
    std::vector<VkDescriptorSetLayout> layouts(vb.swap_chain.images.size(), vb.descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = vb.descriptor_pool;
    allocInfo.descriptorSetCount = (u32)vb.swap_chain.images.size();
    allocInfo.pSetLayouts = layouts.data();

    vb.descriptor_sets.resize(layouts.size());

    VkResult result = vkAllocateDescriptorSets(vb.device, &allocInfo, vb.descriptor_sets.data());
    ASSERT(DIDSUCCEED(result));

    for(size_t i = 0; i < layouts.size(); ++i)
    {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = vb.uniform_buffers[i].handle;
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        VkDescriptorImageInfo  imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = vb.texture.view;
        imageInfo.sampler = vb.sampler;

        VkWriteDescriptorSet writes[2] = {};
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = vb.descriptor_sets[i];
        writes[0].dstBinding = 0;
        writes[0].dstArrayElement = 0;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[0].descriptorCount = 1;
        writes[0].pBufferInfo = &bufferInfo;
        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = vb.descriptor_sets[i];
        writes[1].dstBinding = 1;
        writes[1].dstArrayElement = 0;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[1].descriptorCount = 1;
        writes[1].pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(vb.device, ARRAY_COUNT(writes), writes, 0, nullptr);
    }

    return VK_SUCCESS;
}

internal
VkResult vbCreateSwapChain(ps_vulkan_backend& vb, const VkSurfaceCapabilitiesKHR& surfaceCaps, const VkSurfaceFormatKHR& surfaceFormat, VkPresentModeKHR presentMode, VkExtent2D extent)
{
#define VERIFY_SUCCESS(result) if(DIDFAIL(result)) { LOG_ERROR("Vulkan Error: %X", (result)); ASSERT(false); return result; }

    u32 imageCount = surfaceCaps.minImageCount + 1;

    if(surfaceCaps.maxImageCount > 0 && imageCount > surfaceCaps.maxImageCount)
    {
        imageCount = surfaceCaps.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = vb.swap_chain.platform_surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    
    if(vb.q_graphics.handle == vb.q_present.handle)
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }
    else
    {
        u32 families[2] = { vb.q_graphics.queue_family_index, vb.q_present.queue_family_index };
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

    VkResult result = vkCreateSwapchainKHR(vb.device, &createInfo, nullptr, &swapChain);
    VERIFY_SUCCESS(result);

    vb.swap_chain.handle = swapChain;
    vb.swap_chain.format = surfaceFormat.format;
    vb.swap_chain.extent = extent;
    
    vkGetSwapchainImagesKHR(vb.device, swapChain, &imageCount, nullptr);
    std::vector<VkImage> images(imageCount);
    vkGetSwapchainImagesKHR(vb.device, swapChain, &imageCount, images.data());

    vb.swap_chain.images.resize(imageCount);
    for(u32 index = 0; index < imageCount; ++index)
    {
        vb.swap_chain.images[index].handle = images[index];

        // now setup the image view for use in the swap chain
        result = vbCreateImageView(vb, vb.swap_chain.images[index].handle, surfaceFormat.format, VK_IMAGE_ASPECT_COLOR_BIT, &vb.swap_chain.images[index].view);
        VERIFY_SUCCESS(result);
    }

    result = vbCreateDescriptorPool(vb);
    VERIFY_SUCCESS(result);

#undef VERIFY_SUCCESS

    return VK_SUCCESS;
}

// TODO(james): remove this once the asset system is in place
#include <stdio.h>
internal
buffer DEBUG_readFile(const char* szFilepath)
{
    buffer buf{};
    FILE* file = 0 ;
    fopen_s(&file, szFilepath, "rb");
    if(file)
    {
        fseek(file, 0, SEEK_END);
        u32 size = ftell(file);
        rewind(file);

        buf.size = size;

        buf.data = new u8[size];
        buf.size = fread(buf.data, sizeof(u8), size, file);

        fclose(file);
    }

    return buf;
}

internal
VkResult vbCreateShaderModule(VkDevice device, buffer& bytes, VkShaderModule* shader)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = bytes.size;
    createInfo.pCode = (u32*)bytes.data; 

    VkResult result = vkCreateShaderModule(device, &createInfo, nullptr, shader);

    return result;
}

internal
VkResult vbCreateRenderPass(ps_vulkan_backend& vb)
{
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = vbFindDepthFormat(vb);
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
    colorAttachment.format = vb.swap_chain.format;
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

    VkResult result = vkCreateRenderPass(vb.device, &renderPassInfo, nullptr, &vb.renderPass);

    return result;
}

internal
VkResult vbCreateDescriptorSetLayout(ps_vulkan_backend& vb)
{
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    uboLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 1;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = nullptr;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding layoutBindings[] = {uboLayoutBinding, samplerLayoutBinding};
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = ARRAY_COUNT(layoutBindings);
    layoutInfo.pBindings = layoutBindings;

    VkResult result = vkCreateDescriptorSetLayout(vb.device, &layoutInfo, nullptr, &vb.descriptorSetLayout);
    ASSERT(DIDSUCCEED(result));

    return result;
}

internal
VkResult vbCreateGraphicsPipeline(ps_vulkan_backend& vb)
{
#define VERIFY_SUCCESS(result) if(DIDFAIL(result)) { LOG_ERROR("Vulkan Error: %X", (result)); ASSERT(false); return result; }
    VkResult result = VK_SUCCESS;   
    result = vbCreateDescriptorSetLayout(vb);

    buffer vertShaderBuffer = DEBUG_readFile("../data/vert.spv");
    buffer fragShaderBuffer = DEBUG_readFile("../data/frag.spv");

    result = vbCreateShaderModule(vb.device, vertShaderBuffer, &vb.vertShader);
    VERIFY_SUCCESS(result);
    result = vbCreateShaderModule(vb.device, fragShaderBuffer, &vb.fragShader);
    VERIFY_SUCCESS(result);
    
    delete[] vertShaderBuffer.data;
    delete[] fragShaderBuffer.data;

    VkPipelineShaderStageCreateInfo shaderStages[2] = {};
    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vb.vertShader;
    shaderStages[0].pName = "main";
    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = vb.fragShader;
    shaderStages[1].pName = "main";

    auto bindingDescription = vbGetVertexBindingDescription();
    auto attributeDescriptions = vbGetVertexAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.vertexAttributeDescriptionCount = (u32)attributeDescriptions.size();
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (f32)vb.swap_chain.extent.width;
    viewport.height = (f32)vb.swap_chain.extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0,0};
    scissor.extent = vb.swap_chain.extent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f;
    rasterizer.depthBiasClamp = 0.0f;
    rasterizer.depthBiasSlopeFactor = 0.0f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f; // Optional
    multisampling.pSampleMask = nullptr; // Optional
    multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
    multisampling.alphaToOneEnable = VK_FALSE; // Optional

    // using nullptr for VkPipelineDepthStencilStateCreateInfo

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
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
    colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f; // Optional
    colorBlending.blendConstants[1] = 0.0f; // Optional
    colorBlending.blendConstants[2] = 0.0f; // Optional
    colorBlending.blendConstants[3] = 0.0f; // Optional

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.minDepthBounds = 0.0f;
    depthStencil.maxDepthBounds = 1.0f;
    depthStencil.stencilTestEnable = VK_FALSE;
    depthStencil.front = {};
    depthStencil.back = {};

    // Possible to change some values at draw time, here's an example of how to set that up
    // VkDynamicState dynamicStates[] = {
    //     VK_DYNAMIC_STATE_VIEWPORT,
    //     VK_DYNAMIC_STATE_LINE_WIDTH
    // };

    // VkPipelineDynamicStateCreateInfo dynamicState{};
    // dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    // dynamicState.dynamicStateCount = 2;
    // dynamicState.pDynamicStates = dynamicStates;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1; // Optional
    pipelineLayoutInfo.pSetLayouts = &vb.descriptorSetLayout; // Optional
    pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
    pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

    result = vkCreatePipelineLayout(vb.device, &pipelineLayoutInfo, nullptr, &vb.pipelineLayout);
    VERIFY_SUCCESS(result);

    result = vbCreateRenderPass(vb);
    VERIFY_SUCCESS(result);

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil; // Optional
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = nullptr; // Optional
    pipelineInfo.layout = vb.pipelineLayout;
    pipelineInfo.renderPass = vb.renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    result = vkCreateGraphicsPipelines(vb.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &vb.graphicsPipeline);
    VERIFY_SUCCESS(result);


#undef VERIFY_SUCCESS
    return VK_SUCCESS;
}

internal
VkResult vbCreateFramebuffers(ps_vulkan_backend& vb)
{
    size_t numImages = vb.swap_chain.images.size();
    vb.swap_chain.frame_buffers.resize(numImages);

    for(size_t i = 0; i < numImages; i++)
    {
        VkImageView attachments[] = {
            vb.swap_chain.images[i].view,
            vb.depth_image.view
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = vb.renderPass;
        framebufferInfo.attachmentCount = ARRAY_COUNT(attachments);
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = vb.swap_chain.extent.width;
        framebufferInfo.height = vb.swap_chain.extent.height;
        framebufferInfo.layers = 1;

        VkResult result = vkCreateFramebuffer(vb.device, &framebufferInfo, nullptr, &vb.swap_chain.frame_buffers[i]);
        if(DIDFAIL(result)) { 
            LOG_ERROR("Vulkan Error: %X", (result));
            ASSERT(false);
            return result; 
        }
    }

    return VK_SUCCESS;
}

internal
VkResult vbTempCreateVertexBuffers(ps_vulkan_backend& vb)
{
    VkDeviceSize bufferSize = sizeof(g_Vertices[0]) * ARRAY_COUNT(g_Vertices);

    // Create a staging buffer for upload to the GPU
    ps_vulkan_buffer stagingBuffer{};
    VkResult result = vbCreateBuffer(vb, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer);
    if(DIDFAIL(result))
    {
        LOG_ERROR("Vulkan Error: %X", (result));
        ASSERT(false);
        return result;
    }

    // now we can map the vertex buffer memory to host memory, copy it, and then unmap it for copying to VRAM
    void* data;
    vkMapMemory(vb.device, stagingBuffer.memory_handle, 0, bufferSize, 0, &data);
        CopyArray(ARRAY_COUNT(g_Vertices), g_Vertices, data);
    vkUnmapMemory(vb.device, stagingBuffer.memory_handle);

    // Now create the actual device buffer in *FAST* GPU only memory
    result = vbCreateBuffer(vb, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &vb.vertex_buffer);
    if(DIDFAIL(result))
    {
        LOG_ERROR("Vulkan Error: %X", (result));
        ASSERT(false);
        return result;
    }

    // Now we can copy the uploaded staging buffer data over to the faster buffer location
    vbCopyBuffer(vb, stagingBuffer.handle, vb.vertex_buffer.handle, bufferSize);

    vbDestroyBuffer(vb.device, stagingBuffer); // clean up the staging buffer

    return VK_SUCCESS;
}

internal
VkResult vbTempCreateIndexBuffers(ps_vulkan_backend& vb)
{
    VkDeviceSize bufferSize = sizeof(g_Indices[0]) * ARRAY_COUNT(g_Indices);

    ps_vulkan_buffer stagingBuffer{};
    VkResult result = vbCreateBuffer(vb, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer);
    if(DIDFAIL(result))
    {
        LOG_ERROR("Vulkan Error: %X", (result));
        ASSERT(false);
        return result;
    }

    // now we can map the vertex buffer memory to host memory, copy it, and then unmap it for copying to VRAM
    void* data;
    vkMapMemory(vb.device, stagingBuffer.memory_handle, 0, bufferSize, 0, &data);
        CopyArray(ARRAY_COUNT(g_Indices), g_Indices, data);
    vkUnmapMemory(vb.device, stagingBuffer.memory_handle);

    // Now create the actual device buffer in *FAST* GPU only memory
    result = vbCreateBuffer(vb, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &vb.index_buffer);
    if(DIDFAIL(result))
    {
        LOG_ERROR("Vulkan Error: %X", (result));
        ASSERT(false);
        return result;
    }

    // Now we can copy the uploaded staging buffer data over to the faster buffer location
    vbCopyBuffer(vb, stagingBuffer.handle, vb.index_buffer.handle, bufferSize);

    vbDestroyBuffer(vb.device, stagingBuffer); // clean up the staging buffer

    return VK_SUCCESS;
}

internal
VkResult vbTempCreateUniformBuffers(ps_vulkan_backend& vb)
{
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    vb.uniform_buffers.resize(vb.swap_chain.images.size());

    for (size_t i = 0; i < vb.swap_chain.images.size(); ++i)
    {
        VkResult result = vbCreateBuffer(vb, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &vb.uniform_buffers[i]);
        if (DIDFAIL(result))
        {
            LOG_ERROR("Vulkan Error: %X", (result));
            ASSERT(false);
            return result;
        }
    }

    return VK_SUCCESS;
}

internal
VkResult vbTempCreateTextureImages(ps_vulkan_backend& vb)
{
    int texWidth, texHeight, texChannels;
    stbi_uc *pixels = stbi_load("../data/texture.jpg", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    VkDeviceSize imageSize = texWidth * texHeight * 4; // use texChannels?

    ASSERT(pixels);

    ps_vulkan_buffer stagingBuffer{};
    VkResult result = vbCreateBuffer(vb, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer);
    if (DIDFAIL(result))
    {
        LOG_ERROR("Vulkan Error: %X", (result));
        ASSERT(false);
        return result;
    }

    // now we can map the vertex buffer memory to host memory, copy it, and then unmap it for copying to VRAM
    void *data;
    vkMapMemory(vb.device, stagingBuffer.memory_handle, 0, imageSize, 0, &data);
        Copy(imageSize, pixels, data);
    vkUnmapMemory(vb.device, stagingBuffer.memory_handle);

    stbi_image_free(pixels);

    vbCreateImage(vb, texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &vb.texture);

    vbTransitionImageLayout(vb, vb.texture, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    vbCopyBufferToImage(vb, stagingBuffer, vb.texture, (u32)texWidth, (u32)texHeight);
    vbTransitionImageLayout(vb, vb.texture, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vbDestroyBuffer(vb.device, stagingBuffer);

    return VK_SUCCESS;
}

internal
VkResult vbTempCreateTextureImageViews(ps_vulkan_backend& vb)
{
    VkResult result = vbCreateImageView(vb, vb.texture.handle, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, &vb.texture.view);

    return result;
}

internal
VkResult vbTempCreateTextureSamplers(ps_vulkan_backend& vb)
{
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = vb.device_properties.limits.maxSamplerAnisotropy;
    
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;     // False = [0..1,0..1], [True = 0..Width, 0..Height]
    
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    
    VkResult result = vkCreateSampler(vb.device, &samplerInfo, nullptr, &vb.sampler);
    ASSERT(DIDSUCCEED(result));

    return result;
}

internal
VkResult vbCreateDepthResources(ps_vulkan_backend& vb)
{
    VkFormat depthFormat = vbFindDepthFormat(vb);

    VkResult result = vbCreateImage(vb, vb.swap_chain.extent.width, vb.swap_chain.extent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &vb.depth_image);
    if(DIDFAIL(result))
    {
        ASSERT(false);
        return result;
    }

    result = vbCreateImageView(vb, vb.depth_image.handle, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, &vb.depth_image.view);

    return result;
}


internal
VkResult vbCreateCommandPool(ps_vulkan_backend& vb)
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = vb.q_graphics.queue_family_index;
    poolInfo.flags = 0;

    VkResult result = vkCreateCommandPool(vb.device, &poolInfo, nullptr, &vb.command_pool);

    if(DIDFAIL(result)) { 
        LOG_ERROR("Vulkan Error: %X", (result));
        ASSERT(false);
        return result; 
    }

    u32 numBuffers = (u32)vb.swap_chain.images.size();
    vb.command_buffers.resize(numBuffers);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = vb.command_pool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = numBuffers;

    result = vkAllocateCommandBuffers(vb.device, &allocInfo, vb.command_buffers.data());

    if(DIDFAIL(result)) { 
        LOG_ERROR("Vulkan Error: %X", (result));
        ASSERT(false);
        return result; 
    }

    ///////////////////////////////////
    // Temporary Vertex Buffer Creation
    result = vbTempCreateVertexBuffers(vb);
    ///////////////////////////////////

    ///////////////////////////////////
    // Temporarty Index Buffer Creation
    result = vbTempCreateIndexBuffers(vb);
    ///////////////////////////////////

    ///////////////////////////////////
    // Temporarty Uniform Buffer Creation
    result = vbTempCreateUniformBuffers(vb);
    ///////////////////////////////////

    ///////////////////////////////////
    // Temporary Image Creation
    result = vbTempCreateTextureImages(vb);
    ///////////////////////////////////

    ///////////////////////////////////
    // Temporary Image View Creation
    result = vbTempCreateTextureImageViews(vb);
    ///////////////////////////////////

    ///////////////////////////////////
    // Temporary Sampler Creation
    result = vbTempCreateTextureSamplers(vb);
    ///////////////////////////////////


    result = vbCreateDescriptorSets(vb);
    if(DIDFAIL(result))
    {
        LOG_ERROR("Vulkan Error: %X", (result));
        ASSERT(false);
    }

    // temporary triangle draw that must be done on all the command buffers
    for(u32 i = 0; i < numBuffers; ++i)
    {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;
        beginInfo.pInheritanceInfo = nullptr;

        result = vkBeginCommandBuffer(vb.command_buffers[i], &beginInfo);
        if(DIDFAIL(result)) { 
            LOG_ERROR("Vulkan Error: %X", (result));
            ASSERT(false);
            return result; 
        }

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = vb.renderPass;
        renderPassInfo.framebuffer = vb.swap_chain.frame_buffers[i];
        renderPassInfo.renderArea.offset = {0,0};
        renderPassInfo.renderArea.extent = vb.swap_chain.extent;

        VkClearValue clearValues[2] = {};
        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};
        renderPassInfo.clearValueCount = ARRAY_COUNT(clearValues);
        renderPassInfo.pClearValues = clearValues;
        vkCmdBeginRenderPass(vb.command_buffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(vb.command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, vb.graphicsPipeline);

        VkBuffer vertexBuffers[] = {vb.vertex_buffer.handle};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(vb.command_buffers[i], 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(vb.command_buffers[i], vb.index_buffer.handle, 0, VK_INDEX_TYPE_UINT16);

        vkCmdBindDescriptorSets(vb.command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, vb.pipelineLayout, 0, 1, &vb.descriptor_sets[i], 0, nullptr);
        // and wait for it....
        u32 count = ARRAY_COUNT(g_Indices);
        vkCmdDrawIndexed(vb.command_buffers[i], ARRAY_COUNT(g_Indices), 1, 0, 0, 0); // ta-da!!! we're finally drawing.. only 1000 lines of setup code required

        vkCmdEndRenderPass(vb.command_buffers[i]);

        result = vkEndCommandBuffer(vb.command_buffers[i]);
        if(DIDFAIL(result)) { 
            LOG_ERROR("Vulkan Error: %X", (result));
            ASSERT(false);
            return result; 
        }
    }

    return VK_SUCCESS;
}

internal
void vbDestroySwapChain(ps_vulkan_backend& vb)
{
    VkDevice device = vb.device;
    ps_vulkan_swapchain& swapChain = vb.swap_chain;

    for(auto& ubo : vb.uniform_buffers)
    {
        vbDestroyBuffer(vb.device, ubo);
    }

    IFF(vb.descriptor_pool, vkDestroyDescriptorPool(vb.device, vb.descriptor_pool, nullptr));

    for(const auto& fb : swapChain.frame_buffers)
    {
        vkDestroyFramebuffer(device, fb, nullptr);
    }

    vkFreeCommandBuffers(device, vb.command_pool, (u32)vb.command_buffers.size(), vb.command_buffers.data());

    IFF(vb.graphicsPipeline, vkDestroyPipeline(vb.device, vb.graphicsPipeline, nullptr));
    IFF(vb.pipelineLayout, vkDestroyPipelineLayout(vb.device, vb.pipelineLayout, nullptr));
    IFF(vb.renderPass, vkDestroyRenderPass(vb.device, vb.renderPass, nullptr));
    
    for(const auto& image : swapChain.images)
    {
        vkDestroyImageView(device, image.view, nullptr);
    }

    vkDestroySwapchainKHR(device, swapChain.handle, nullptr);
}

internal
void vbDestroy(ps_vulkan_backend& vb)
{
    // NOTE(james): Clean up the rest of the backend members prior to destroying the instance
    if(vb.device)
    {
        vbDestroySwapChain(vb);

        IFF(vb.sampler, vkDestroySampler(vb.device, vb.sampler, nullptr));
        vbDestroyImage(vb.device, vb.texture);
        vbDestroyBuffer(vb.device, vb.index_buffer);
        vbDestroyBuffer(vb.device, vb.vertex_buffer);

        vbDestroyImage(vb.device, vb.depth_image);

        IFF(vb.command_pool, vkDestroyCommandPool(vb.device, vb.command_pool, nullptr));
        IFF(vb.vertShader, vkDestroyShaderModule(vb.device, vb.vertShader, nullptr));
        IFF(vb.fragShader, vkDestroyShaderModule(vb.device, vb.fragShader, nullptr));

        IFF(vb.descriptorSetLayout, vkDestroyDescriptorSetLayout(vb.device, vb.descriptorSetLayout, nullptr));

        vkDestroyDevice(vb.device, nullptr);
    }

    if(vb.swap_chain.platform_surface)
    {
        vkDestroySurfaceKHR(vb.instance, vb.swap_chain.platform_surface, nullptr);
    }

    if(vb.debugMessenger)
    {
        vbDestroyDebugUtilsMessengerEXT(vb.instance, vb.debugMessenger, nullptr);
    }

    vkDestroyInstance(vb.instance, nullptr);
}