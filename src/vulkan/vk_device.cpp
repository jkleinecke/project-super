
#include <vulkan/vulkan.h>

#include "vk_device.h"
#include "../ps_graphics.h"

#include "vk_extensions.h"
#include "vk_initializers.cpp"
#include "vk_descriptor.cpp"
#include "ps_vulkan_graphics_api.cpp"

#include <vector>
#include <array>
#include <unordered_map>

// TODO(james): Remove this from the main graphics driver... just for testing
//#define TINYOBJLOADER_IMPLEMENTATION
#include "libs/tinyobjloader/tiny_obj_loader.h"

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
struct FrameObject
{
    alignas(16) m4 viewProj;
};

struct InstanceObject
{
    alignas(16) m4 model;
};

struct ps_vertex
{
    v3 pos;
    v3 color;
    v2 texCoord;
    
    bool operator==(const ps_vertex& other) const {
        return pos == other.pos && color == other.color && texCoord == other.texCoord;
    }
};

internal
inline void ps_hash_combine(size_t &seed, size_t hash)
{
    hash +=  0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= hash;
}

internal
inline size_t ps_hash_v2(v2 const& v)
{
    size_t seed = 0;
    std::hash<f32> hasher;
    ps_hash_combine(seed, hasher(v.X));
    ps_hash_combine(seed, hasher(v.Y));
    return seed;
}

internal
inline size_t ps_hash_v3(v3 const& v)
{
    size_t seed = 0;
    std::hash<f32> hasher;
    ps_hash_combine(seed, hasher(v.X));
    ps_hash_combine(seed, hasher(v.Y));
    ps_hash_combine(seed, hasher(v.Z));
    return seed;
}

namespace std {
    template<> struct hash<ps_vertex> {
        size_t operator()(ps_vertex const& vertex) const {
            // pos ^ color ^ texCoord
            return (ps_hash_v3(vertex.pos) ^ (ps_hash_v3(vertex.color) << 1) >> 1) ^ (ps_hash_v2(vertex.texCoord) << 1);
        }
    };
}

// Uncomment to compute a proper index buffer and reduce the number of vertices
#define COMPUTE_MODEL_VERTEX_REUSE

global const char* MODEL_PATH = "../data/viking_room.obj";
global const char* TEXTURE_PATH = "../data/viking_room.png"; 

global std::vector<ps_vertex> g_ModelVertices;
global std::vector<u32> g_ModelIndices;

// global const ps_vertex g_Vertices[] = {
//     {{-0.5f, -0.5f,  0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
//     {{ 0.5f, -0.5f,  0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
//     {{ 0.5f,  0.5f,  0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
//     {{-0.5f,  0.5f,  0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},

//     {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
//     {{ 0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
//     {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
//     {{-0.5f,  0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
// };

// global const u16 g_Indices[] {
//     0, 1, 2, 2, 3, 0,
//     4, 5, 6, 6, 7, 4
// };

internal VkVertexInputBindingDescription vgGetVertexBindingDescription()
{
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(ps_vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDescription;
}

internal std::array<VkVertexInputAttributeDescription, 3> vgGetVertexAttributeDescriptions()
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

        //--vbLogAvailableExtensions(nullptr);

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
VkResult vgCreateShaderModule(VkDevice device, buffer& bytes, VkShaderModule* shader)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = bytes.size;
    createInfo.pCode = (u32*)bytes.data; 

    VkResult result = vkCreateShaderModule(device, &createInfo, nullptr, shader);

    return result;
}

internal
VkResult vgCreateRenderPass(vg_device& device)
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

    VkResult result = vkCreateRenderPass(device.handle, &renderPassInfo, nullptr, &device.renderPass.handle);

    return result;
}

// internal
// VkResult vgCreateDescriptorSetLayout(vg_device& device)
// {
//     VkDescriptorSetLayoutBinding layoutBindings[] = {
//         // ubo
//         vkInit_descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0), 
//         // sampler
//         vkInit_descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1)
//     };

//     VkDescriptorSetLayoutCreateInfo layoutInfo = vkInit_descritorset_layout_create_info(
//         ARRAY_COUNT(layoutBindings), layoutBindings
//     );

//     VkResult result = vkCreateDescriptorSetLayout(device.handle, &layoutInfo, nullptr, &device.pipeline.descriptorLayout);
//     ASSERT(DIDSUCCEED(result));

//     return result;
// }

internal
VkResult vgCreateGraphicsPipeline(vg_device& device)
{
#define VERIFY_SUCCESS(result) if(DIDFAIL(result)) { LOG_ERROR("Vulkan Error: %X", (result)); ASSERT(false); return result; }
    VkResult result = VK_SUCCESS;   

    buffer vertShaderBuffer = DEBUG_readFile("../data/vert.spv");
    buffer fragShaderBuffer = DEBUG_readFile("../data/frag.spv");

    result = vgCreateShaderModule(device.handle, vertShaderBuffer, &device.pipeline.shaders.vertex);
    VERIFY_SUCCESS(result);
    result = vgCreateShaderModule(device.handle, fragShaderBuffer, &device.pipeline.shaders.frag);
    VERIFY_SUCCESS(result);
    
    delete[] vertShaderBuffer.data;
    delete[] fragShaderBuffer.data;

    VkPipelineShaderStageCreateInfo shaderStages[2] = {
        vkInit_pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, device.pipeline.shaders.vertex),
        vkInit_pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, device.pipeline.shaders.frag)
    };

    auto bindingDescription = vgGetVertexBindingDescription();
    auto attributeDescriptions = vgGetVertexAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = vkInit_vertex_input_state_create_info();
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.vertexAttributeDescriptionCount = (u32)attributeDescriptions.size();
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

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
    colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f; // Optional
    colorBlending.blendConstants[1] = 0.0f; // Optional
    colorBlending.blendConstants[2] = 0.0f; // Optional
    colorBlending.blendConstants[3] = 0.0f; // Optional

    VkPipelineDepthStencilStateCreateInfo depthStencil = vkInit_depth_stencil_create_info(
        true, true, VK_COMPARE_OP_LESS
    );

    // Possible to change some values at draw time, here's an example of how to set that up
    // VkDynamicState dynamicStates[] = {
    //     VK_DYNAMIC_STATE_VIEWPORT,
    //     VK_DYNAMIC_STATE_LINE_WIDTH
    // };

    // VkPipelineDynamicStateCreateInfo dynamicState{};
    // dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    // dynamicState.dynamicStateCount = 2;
    // dynamicState.pDynamicStates = dynamicStates;

    // TODO(james): use reflection on the shaderset to discover these
    VkDescriptorSetLayoutBinding layoutBindings[] = {
        // camera
        vkInit_descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
        // instance 
        vkInit_descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1),
        // sampler
        vkInit_descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2)
    };

    VkDescriptorSetLayout layout = vgGetDescriptorLayoutFromCache( device.descriptorLayoutCache, ARRAY_COUNT(layoutBindings), layoutBindings );
    device.pipeline.descriptorLayout = layout;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = vkInit_pipeline_layout_create_info();
    pipelineLayoutInfo.setLayoutCount = 1; // Optional
    pipelineLayoutInfo.pSetLayouts = &layout; // Optional
    pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
    pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

    result = vkCreatePipelineLayout(device.handle, &pipelineLayoutInfo, nullptr, &device.pipeline.layout);
    VERIFY_SUCCESS(result);

    result = vgCreateRenderPass(device);
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
    pipelineInfo.layout = device.pipeline.layout;
    pipelineInfo.renderPass = device.renderPass.handle;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    result = vkCreateGraphicsPipelines(device.handle, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &device.pipeline.handle);
    VERIFY_SUCCESS(result);

#undef VERIFY_SUCCESS
    return VK_SUCCESS;
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
            device.renderPass.handle, device.extent
        );
        framebufferInfo.attachmentCount = ARRAY_COUNT(attachments);
        framebufferInfo.pAttachments = attachments;

        VkResult result = vkCreateFramebuffer(device.handle, &framebufferInfo, nullptr, &device.paFramebuffers[i]);
        ASSERT(DIDSUCCEED(result));
    }

    return VK_SUCCESS;
}

internal
void vgTempLoadModel(vg_device& device)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    bool bResult = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, MODEL_PATH);
    ASSERT(bResult);

    std::unordered_map<ps_vertex, u32> uniqueVertices{};

    for(const auto& shape : shapes)
    {
        for(const auto& index : shape.mesh.indices)
        {
            ps_vertex vertex{};
            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            vertex.texCoord = {
                attrib.texcoords[2 * index.texcoord_index + 0],
                1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
            };

            vertex.color = {1.0f, 1.0f, 1.0f};

#ifdef COMPUTE_MODEL_VERTEX_REUSE
            if(uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = (u32)g_ModelVertices.size();
                g_ModelVertices.push_back(vertex);
            }

            g_ModelIndices.push_back(uniqueVertices[vertex]);    // TODO(james): fix this awfulness
#else
            g_ModelVertices.push_back(vertex);
            g_ModelIndices.push_back((u32)g_ModelIndices.size());
#endif
        }
    }
}

internal
VkResult vgTempCreateVertexBuffers(vg_device& device)
{
    //VkDeviceSize bufferSize = sizeof(g_Vertices[0]) * ARRAY_COUNT(g_Vertices);
    VkDeviceSize bufferSize = sizeof(g_ModelVertices[0]) * g_ModelVertices.size();

    // Create a staging buffer for upload to the GPU
    vg_buffer stagingBuffer{};
    VkResult result = vgCreateBuffer(device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer);
    if(DIDFAIL(result))
    {
        LOG_ERROR("Vulkan Error: %X", (result));
        ASSERT(false);
        return result;
    }

    // now we can map the vertex buffer memory to host memory, copy it, and then unmap it for copying to VRAM
    void* data;
    vkMapMemory(device.handle, stagingBuffer.memory, 0, bufferSize, 0, &data);
        CopyArray(g_ModelVertices.size(), g_ModelVertices.data(), data);
    vkUnmapMemory(device.handle, stagingBuffer.memory);

    // Now create the actual device buffer in *FAST* GPU only memory
    result = vgCreateBuffer(device, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &device.vertex_buffer);
    if(DIDFAIL(result))
    {
        LOG_ERROR("Vulkan Error: %X", (result));
        ASSERT(false);
        return result;
    }

    // Now we can copy the uploaded staging buffer data over to the faster buffer location
    vgCopyBuffer(device, stagingBuffer.handle, device.vertex_buffer.handle, bufferSize);

    vgDestroyBuffer(device.handle, stagingBuffer); // clean up the staging buffer

    return VK_SUCCESS;
}

internal
VkResult vgTempCreateIndexBuffers(vg_device& device)
{
    //VkDeviceSize bufferSize = sizeof(g_Indices[0]) * ARRAY_COUNT(g_Indices);
    VkDeviceSize bufferSize = sizeof(g_ModelIndices[0]) * g_ModelIndices.size();

    vg_buffer stagingBuffer{};
    VkResult result = vgCreateBuffer(device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer);
    if(DIDFAIL(result))
    {
        LOG_ERROR("Vulkan Error: %X", (result));
        ASSERT(false);
        return result;
    }

    // now we can map the vertex buffer memory to host memory, copy it, and then unmap it for copying to VRAM
    void* data;
    vkMapMemory(device.handle, stagingBuffer.memory, 0, bufferSize, 0, &data);
        CopyArray(g_ModelIndices.size(), g_ModelIndices.data(), data);
    vkUnmapMemory(device.handle, stagingBuffer.memory);

    // Now create the actual device buffer in *FAST* GPU only memory
    result = vgCreateBuffer(device, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &device.index_buffer);
    if(DIDFAIL(result))
    {
        LOG_ERROR("Vulkan Error: %X", (result));
        ASSERT(false);
        return result;
    }

    // Now we can copy the uploaded staging buffer data over to the faster buffer location
    vgCopyBuffer(device, stagingBuffer.handle, device.index_buffer.handle, bufferSize);

    vgDestroyBuffer(device.handle, stagingBuffer); // clean up the staging buffer

    return VK_SUCCESS;
}

internal
VkResult vgTempCreateUniformBuffers(vg_device& device)
{
    for (size_t i = 0; i < FRAME_OVERLAP; ++i)
    {
        VkResult result = vgCreateBuffer(device, sizeof(FrameObject), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &device.frames[i].frame_buffer);
        ASSERT(result == VK_SUCCESS);
        result = vgCreateBuffer(device, sizeof(InstanceObject), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &device.frames[i].instance_buffer);
        ASSERT(result == VK_SUCCESS);
    }

    return VK_SUCCESS;
}

internal
VkResult vgTempCreateTextureImages(vg_device& device)
{
    int texWidth, texHeight, texChannels;
    stbi_uc *pixels = stbi_load(TEXTURE_PATH, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    VkDeviceSize imageSize = texWidth * texHeight * 4; // use texChannels?

    ASSERT(pixels);

    vg_buffer stagingBuffer{};
    VkResult result = vgCreateBuffer(device, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer);
    if (DIDFAIL(result))
    {
        LOG_ERROR("Vulkan Error: %X", (result));
        ASSERT(false);
        return result;
    }

    // now we can map the vertex buffer memory to host memory, copy it, and then unmap it for copying to VRAM
    void *data;
    vkMapMemory(device.handle, stagingBuffer.memory, 0, imageSize, 0, &data);
        Copy(imageSize, pixels, data);
    vkUnmapMemory(device.handle, stagingBuffer.memory);

    stbi_image_free(pixels);

    vgCreateImage(device, texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &device.texture);

    vgTransitionImageLayout(device, device.texture, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    vgCopyBufferToImage(device, stagingBuffer, device.texture, (u32)texWidth, (u32)texHeight);
    vgTransitionImageLayout(device, device.texture, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vgDestroyBuffer(device.handle, stagingBuffer);

    return VK_SUCCESS;
}

internal
VkResult vgTempCreateTextureImageViews(vg_device& device)
{
    VkResult result = vgCreateImageView(device, device.texture.handle, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, &device.texture.view);

    return result;
}

internal
VkResult vgTempCreateTextureSamplers(vg_device& device)
{
    VkSamplerCreateInfo samplerInfo = vkInit_sampler_create_info(
        VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT
    );
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = device.device_properties.limits.maxSamplerAnisotropy;
    
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;     // False = [0..1,0..1], [True = 0..Width, 0..Height]
    
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    
    VkResult result = vkCreateSampler(device.handle, &samplerInfo, nullptr, &device.sampler.handle);
    ASSERT(DIDSUCCEED(result));

    return result;
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

#if 0
internal
void vgTempBuildRenderCommands(vg_device& device, u32 swapChainImageIndex)
{
    

    VkDescriptorSet shaderDescriptor = vgCreateDescriptor(&device, device.pipeline.descriptorLayout);
    // vgAllocateDescriptor(device.pCurFrame->dynamicDescriptorAllocator, device.pipeline.descriptorLayout, &shaderDescriptor);

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = device.pCurFrame->frame_buffer.handle;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(UniformBufferObject);

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = device.texture.view;
    imageInfo.sampler = device.sampler.handle;

    VkWriteDescriptorSet writes[] = {
        vkInit_write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, shaderDescriptor, &bufferInfo, 0),
        vkInit_write_descriptor_image(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, shaderDescriptor, &imageInfo, 1)};

    vgUpdateDescriptorSets(&device, ARRAY_COUNT(writes), writes);
    // vkUpdateDescriptorSets(device.handle, ARRAY_COUNT(writes), writes, 0, nullptr);

    // VkCommandBufferBeginInfo beginInfo = vkInit_command_buffer_begin_info(0);

    VkCommandBuffer commandBuffer = device.pCurFrame->commandBuffer;
    vgBeginRecordingCmds(commandBuffer);
    // VkResult result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    // if(DIDFAIL(result)) { 
    //     LOG_ERROR("Vulkan Error: %X", (result));
    //     ASSERT(false);
    // }

    // VkRenderPassBeginInfo renderPassInfo = vkInit_renderpass_begin_info(
    //     device.renderPass.handle, device.extent, device.paFramebuffers[swapChainImageIndex]
    // );

    VkClearValue clearValues[2] = {};
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};
    vgBeginRenderPass(commandBuffer, device.renderPass.handle, 2, clearValues, device.extent, device.paFramebuffers[swapChainImageIndex]);
    // renderPassInfo.clearValueCount = ARRAY_COUNT(clearValues);
    // renderPassInfo.pClearValues = clearValues;
    // vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vgBindPipeline(commandBuffer, device.pipeline.handle);
    // vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, device.pipeline.handle);

    VkBuffer vertexBuffers[] = {device.vertex_buffer.handle};
    VkDeviceSize offsets[] = {0};
    // vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vgBindVertexBuffers(commandBuffer, 1, vertexBuffers, offsets);

    vgBindIndexBuffer(commandBuffer, device.index_buffer.handle);
    // vkCmdBindIndexBuffer(commandBuffer, device.index_buffer.handle, 0, VK_INDEX_TYPE_UINT32);

    vgBindDescriptorSets(commandBuffer, device.pipeline.layout, 1, &shaderDescriptor);
    // vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, device.pipeline.layout, 0, 1, &shaderDescriptor, 0, nullptr);
    // and wait for it....
    u32 count = (u32)g_ModelIndices.size();

    vgDrawIndexed(commandBuffer, count, 1);

    //vkCmdDrawIndexed(commandBuffer, count, 1, 0, 0, 0); // ta-da!!! we're finally drawing.. only 1000 lines of setup code required

    vgEndRenderPass(commandBuffer);
    // vkCmdEndRenderPass(commandBuffer);

    vgEndRecordingCmds(commandBuffer);

    // result = vkEndCommandBuffer(commandBuffer);
    // if(DIDFAIL(result)) { 
    //     LOG_ERROR("Vulkan Error: %X", (result));
    //     ASSERT(false);
    // }
}
#endif

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
        vgDestroyBuffer(device.handle, frame.instance_buffer);
    }

    IFF(device.pipeline.handle, vkDestroyPipeline(device.handle, device.pipeline.handle, nullptr));
    IFF(device.pipeline.layout, vkDestroyPipelineLayout(device.handle, device.pipeline.layout, nullptr));
    IFF(device.renderPass.handle, vkDestroyRenderPass(device.handle, device.renderPass.handle, nullptr));
    
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

internal
void vgDestroy(vg_backend& vb)
{
    // NOTE(james): Clean up the rest of the backend members prior to destroying the instance
    if(vb.device.handle)
    {
        vg_device& device = vb.device;
        vgDestroySwapChain(device);

        IFF(device.sampler.handle, vkDestroySampler(device.handle, device.sampler.handle, nullptr));
        vgDestroyImage(device.handle, device.texture);
        vgDestroyBuffer(device.handle, device.index_buffer);
        vgDestroyBuffer(device.handle, device.vertex_buffer);

        // TODO(james): Account for this in the window resize
        vgDestroyImage(device.handle, device.depth_image);

        vkDestroyShaderModule(device.handle, device.pipeline.shaders.vertex, nullptr);
        vkDestroyShaderModule(device.handle, device.pipeline.shaders.frag, nullptr);

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

internal
void VulkanGraphicsBeginFrame(vg_backend* vb)
{
    vg_device& device = vb->device;

    device.pPrevFrame = device.pCurFrame;
    device.pCurFrame = &device.frames[device.currentFrameIndex];
    
 

    // update the g_UBO with the latest matrices
    // {
    //     local_persist f32 accumlated_elapsedFrameTime = 0.0f;

    //     //accumlated_elapsedFrameTime += clock.elapsedFrameTime;

    //     UniformBufferObject ubo{};
    //     // Rotates 90 degrees a second
    //     ubo.model = Rotate(accumlated_elapsedFrameTime * 90.0f, Vec3(0.0f, 0.0f, 1.0f));
    //     ubo.view = LookAt(Vec3(4.0f, 4.0f, 4.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f));
    //     ubo.proj = Perspective(45.0f, (f32)device.extent.width, (f32)device.extent.height, 0.1f, 10.0f);
    //     //ubo.proj.Elements[1][1] *= -1;

    //     void* data;
    //     vkMapMemory(device.handle, device.pCurFrame->frame_buffer.memory, 0, sizeof(ubo), 0, &data);
    //         Copy(sizeof(ubo), &ubo, data);
    //     vkUnmapMemory(device.handle, device.pCurFrame->frame_buffer.memory);
    // }
}

internal
void vgTranslateRenderCommands(vg_device& device, render_commands* commands)
{
    // TODO(james): Turn this into an actual renderer, this is just to get some stuff on the screen

    // prepare for recording...
    VkDescriptorSet shaderDescriptor = VK_NULL_HANDLE;
    vgAllocateDescriptor(device.pCurFrame->dynamicDescriptorAllocator, device.pipeline.descriptorLayout, &shaderDescriptor);

    VkDescriptorBufferInfo cameraBufferInfo{};
    cameraBufferInfo.buffer = device.pCurFrame->frame_buffer.handle;
    cameraBufferInfo.offset = 0;
    cameraBufferInfo.range = sizeof(FrameObject);

    VkDescriptorBufferInfo instanceBufferInfo{};
    instanceBufferInfo.buffer = device.pCurFrame->instance_buffer.handle;
    instanceBufferInfo.offset = 0;
    instanceBufferInfo.range = sizeof(InstanceObject);

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = device.texture.view;
    imageInfo.sampler = device.sampler.handle;

    VkWriteDescriptorSet writes[] = {
        vkInit_write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, shaderDescriptor, &cameraBufferInfo, 0),
        vkInit_write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, shaderDescriptor, &instanceBufferInfo, 1),
        vkInit_write_descriptor_image(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, shaderDescriptor, &imageInfo, 2)};

    vkUpdateDescriptorSets(device.handle, ARRAY_COUNT(writes), writes, 0, nullptr);

    VkCommandBufferBeginInfo beginInfo = vkInit_command_buffer_begin_info(0);

    VkCommandBuffer commandBuffer = device.pCurFrame->commandBuffer;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    
    VkClearValue clearValues[2] = {};
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};
    
    VkRenderPassBeginInfo renderPassInfo = vkInit_renderpass_begin_info(
        device.renderPass.handle, device.extent, device.paFramebuffers[device.curSwapChainIndex]
    );
    renderPassInfo.clearValueCount = ARRAY_COUNT(clearValues);
    renderPassInfo.pClearValues = clearValues;
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, device.pipeline.handle);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, device.pipeline.layout, 0, 1, &shaderDescriptor, 0, nullptr);

    //
    // Iterate through the render commands
    //

    render_cmd_header* header = (render_cmd_header*)commands->cmd_arena.basePointer;

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
                instanceObject.model = cmd->model;

                void* data;
                vkMapMemory(device.handle, device.pCurFrame->instance_buffer.memory, 0, sizeof(InstanceObject), 0, &data);
                    Copy(sizeof(instanceObject), &instanceObject, data);
                vkUnmapMemory(device.handle, device.pCurFrame->instance_buffer.memory);

                VkDeviceSize offsets[] = {0};
                vkCmdBindVertexBuffers(commandBuffer, 0, 1, &device.vertex_buffer.handle, offsets);
                vkCmdBindIndexBuffer(commandBuffer, device.index_buffer.handle, 0, VK_INDEX_TYPE_UINT32);

                vkCmdDrawIndexed(commandBuffer, (u32)g_ModelIndices.size(), 1, 0, 0, 0);                
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