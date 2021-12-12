
#include <vulkan/vulkan.h>

#include "ps_vulkan.h"
#include "ps_vulkan_extensions.h"
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
    VkCommandBufferAllocateInfo allocInfo
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = device.pCurFrame->commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device.handle, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

internal
void vgEndSingleTimeCommands(vg_device& device, VkCommandBuffer commandBuffer)
{
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

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
        LOG_WARN("Validation Layer: %s", pCallbackData->pMessage);
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
VkResult vgCreateDescriptorPool(vg_device& device)
{
    VkDescriptorPoolSize poolSizes[2] = {};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 10;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 10;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = ARRAY_COUNT(poolSizes);
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = 10;
    poolInfo.flags = 0; // optional

    VkResult result = vkCreateDescriptorPool(device.handle, &poolInfo, nullptr, &device.descriptorPool);
    ASSERT(DIDSUCCEED(result));

    return result;
}

internal
VkResult vgCreateDescriptorSets(vg_device& device)
{

    for(size_t i = 0; i < FRAME_OVERLAP; ++i)
    {
        // Allocate 1 descriptor per frame
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = device.descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &device.descriptorLayout;

        VkResult result = vkAllocateDescriptorSets(device.handle, &allocInfo, &device.frames[i].globalDescriptor);
        

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = device.frames[i].camera_buffer.handle;
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        VkDescriptorImageInfo  imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = device.texture.view;
        imageInfo.sampler = device.sampler.handle;

        VkWriteDescriptorSet writes[2] = {};
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = device.frames[i].globalDescriptor;
        writes[0].dstBinding = 0;
        writes[0].dstArrayElement = 0;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[0].descriptorCount = 1;
        writes[0].pBufferInfo = &bufferInfo;
        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = device.frames[i].globalDescriptor;
        writes[1].dstBinding = 1;
        writes[1].dstArrayElement = 0;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[1].descriptorCount = 1;
        writes[1].pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(device.handle, ARRAY_COUNT(writes), writes, 0, nullptr);
    }

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

    device.swapChainImages.resize(imageCount);
    for(u32 index = 0; index < imageCount; ++index)
    {
        device.swapChainImages[index].handle = images[index];

        // now setup the image view for use in the swap chain
        result = vgCreateImageView(device, device.swapChainImages[index].handle, surfaceFormat.format, VK_IMAGE_ASPECT_COLOR_BIT, &device.swapChainImages[index].view);
        VERIFY_SUCCESS(result);
    }

    result = vgCreateDescriptorPool(device);
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

internal
VkResult vgCreateDescriptorSetLayout(vg_device& device)
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

    VkResult result = vkCreateDescriptorSetLayout(device.handle, &layoutInfo, nullptr, &device.descriptorLayout);
    ASSERT(DIDSUCCEED(result));

    return result;
}

internal
VkResult vgCreateGraphicsPipeline(vg_device& device)
{
#define VERIFY_SUCCESS(result) if(DIDFAIL(result)) { LOG_ERROR("Vulkan Error: %X", (result)); ASSERT(false); return result; }
    VkResult result = VK_SUCCESS;   
    result = vgCreateDescriptorSetLayout(device);

    buffer vertShaderBuffer = DEBUG_readFile("../data/vert.spv");
    buffer fragShaderBuffer = DEBUG_readFile("../data/frag.spv");

    result = vgCreateShaderModule(device.handle, vertShaderBuffer, &device.pipeline.shaders.vertex);
    VERIFY_SUCCESS(result);
    result = vgCreateShaderModule(device.handle, fragShaderBuffer, &device.pipeline.shaders.frag);
    VERIFY_SUCCESS(result);
    
    delete[] vertShaderBuffer.data;
    delete[] fragShaderBuffer.data;

    VkPipelineShaderStageCreateInfo shaderStages[2] = {};
    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = device.pipeline.shaders.vertex;
    shaderStages[0].pName = "main";
    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = device.pipeline.shaders.frag;
    shaderStages[1].pName = "main";

    auto bindingDescription = vgGetVertexBindingDescription();
    auto attributeDescriptions = vgGetVertexAttributeDescriptions();

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
    pipelineLayoutInfo.pSetLayouts = &device.descriptorLayout; // Optional
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

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

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

    device.framebuffers.resize(device.numSwapChainImages);
    for(u32 i = 0; i < device.numSwapChainImages; ++i)
    {
        VkImageView attachments[] = {
            device.swapChainImages[i].view,
            device.depth_image.view
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = device.renderPass.handle;
        framebufferInfo.attachmentCount = ARRAY_COUNT(attachments);
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = device.extent.width;
        framebufferInfo.height = device.extent.height;
        framebufferInfo.layers = 1;

        VkResult result = vkCreateFramebuffer(device.handle, &framebufferInfo, nullptr, &device.framebuffers[i]);
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
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    for (size_t i = 0; i < FRAME_OVERLAP; ++i)
    {
        VkResult result = vgCreateBuffer(device, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &device.frames[i].camera_buffer);
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
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
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
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = device.q_graphics.queue_family_index;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        result = vkCreateCommandPool(device.handle, &poolInfo, nullptr, &device.frames[i].commandPool);

        if(DIDFAIL(result)) { 
            LOG_ERROR("Vulkan Error: %X", (result));
            ASSERT(false);
            return result; 
        }

        VkCommandBufferAllocateInfo allocInfo
        {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = device.frames[i].commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };
        result = vkAllocateCommandBuffers(device.handle, &allocInfo, &device.frames[i].commandBuffer);

        if(DIDFAIL(result)) { 
            LOG_ERROR("Vulkan Error: %X", (result));
            ASSERT(false);
            return result; 
        }
    }

    ///////////////////////////////////
    // Temporary Model Loading
    vgTempLoadModel(device);
    ///////////////////////////////////


    ///////////////////////////////////
    // Temporary Vertex Buffer Creation
    result = vgTempCreateVertexBuffers(device);
    ///////////////////////////////////

    ///////////////////////////////////
    // Temporarty Index Buffer Creation
    result = vgTempCreateIndexBuffers(device);
    ///////////////////////////////////

    ///////////////////////////////////
    // Temporarty Uniform Buffer Creation
    result = vgTempCreateUniformBuffers(device);
    ///////////////////////////////////

    ///////////////////////////////////
    // Temporary Image Creation
    result = vgTempCreateTextureImages(device);
    ///////////////////////////////////

    ///////////////////////////////////
    // Temporary Image View Creation
    result = vgTempCreateTextureImageViews(device);
    ///////////////////////////////////

    ///////////////////////////////////
    // Temporary Sampler Creation
    result = vgTempCreateTextureSamplers(device);
    ///////////////////////////////////


    result = vgCreateDescriptorSets(device);
    if(DIDFAIL(result))
    {
        LOG_ERROR("Vulkan Error: %X", (result));
        ASSERT(false);
    }

    // temporary triangle draw that must be done on all the command buffers
 

    return VK_SUCCESS;
}

internal
void vgTempBuildRenderCommands(vg_device& device, u32 swapChainImageIndex)
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;
    beginInfo.pInheritanceInfo = nullptr;

    VkCommandBuffer commandBuffer = device.pCurFrame->commandBuffer;
    VkResult result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    if(DIDFAIL(result)) { 
        LOG_ERROR("Vulkan Error: %X", (result));
        ASSERT(false);
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = device.renderPass.handle;
    renderPassInfo.framebuffer = device.framebuffers[swapChainImageIndex];
    renderPassInfo.renderArea.offset = {0,0};
    renderPassInfo.renderArea.extent = device.extent;

    VkClearValue clearValues[2] = {};
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};
    renderPassInfo.clearValueCount = ARRAY_COUNT(clearValues);
    renderPassInfo.pClearValues = clearValues;
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, device.pipeline.handle);

    VkBuffer vertexBuffers[] = {device.vertex_buffer.handle};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, device.index_buffer.handle, 0, VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, device.pipeline.layout, 0, 1, &device.pCurFrame->globalDescriptor, 0, nullptr);
    // and wait for it....
    u32 count = (u32)g_ModelIndices.size();
    vkCmdDrawIndexed(commandBuffer, count, 1, 0, 0, 0); // ta-da!!! we're finally drawing.. only 1000 lines of setup code required

    vkCmdEndRenderPass(commandBuffer);

    result = vkEndCommandBuffer(commandBuffer);
    if(DIDFAIL(result)) { 
        LOG_ERROR("Vulkan Error: %X", (result));
        ASSERT(false);
    }
}

internal
void vgDestroySwapChain(vg_device& device)
{
    IFF(device.descriptorPool, vkDestroyDescriptorPool(device.handle, device.descriptorPool, nullptr));

    for(u32 i = 0; i < FRAME_OVERLAP; ++i)
    {
        vg_framedata& frame = device.frames[i];
        
        vkFreeCommandBuffers(device.handle, frame.commandPool, 1, &frame.commandBuffer);
        vkDestroyCommandPool(device.handle, frame.commandPool, nullptr);
        vkDestroySemaphore(device.handle, frame.renderSemaphore, nullptr);
        vkDestroySemaphore(device.handle, frame.presentSemaphore, nullptr);
        vkDestroyFence(device.handle, frame.renderFence, nullptr);
        vgDestroyBuffer(device.handle, frame.camera_buffer);
    }

    IFF(device.pipeline.handle, vkDestroyPipeline(device.handle, device.pipeline.handle, nullptr));
    IFF(device.pipeline.layout, vkDestroyPipelineLayout(device.handle, device.pipeline.layout, nullptr));
    IFF(device.renderPass.handle, vkDestroyRenderPass(device.handle, device.renderPass.handle, nullptr));
    
    for(const auto& image : device.swapChainImages)
    {
        vkDestroyImageView(device.handle, image.view, nullptr);
    }

    for(const auto& fb : device.framebuffers)
    {
        vkDestroyFramebuffer(device.handle, fb, nullptr);
    }

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

        vkDestroyDescriptorSetLayout(device.handle, device.descriptorLayout, nullptr);

        vkDestroyDevice(device.handle, nullptr);
    }

    vkDestroySurfaceKHR(vb.instance, vb.device.platform_surface, nullptr);

    if(vb.debugMessenger)
    {
        vbDestroyDebugUtilsMessengerEXT(vb.instance, vb.debugMessenger, nullptr);
    }

    vkDestroyInstance(vb.instance, nullptr);
}