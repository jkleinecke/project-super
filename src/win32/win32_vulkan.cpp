
#include <vulkan/vulkan.h>
#include "win32_vulkan.h"

#include <vector>

#if defined(PROJECTSUPER_INTERNAL)
#define GRAPHICS_DEBUG
#endif

// #define VERIFY_SUCCESS(result) if(DIDFAIL(result)) { LOG_ERROR("Vulkan Error: %X", (result)); ASSERT(false); return nullptr; }
#define VERIFY_SUCCESS(result) ASSERT(DIDSUCCEED(result))
#define DIDFAIL(result) ((result) != VK_SUCCESS)
#define DIDSUCCEED(result) ((result) == VK_SUCCESS)

global win32_vulkan_backend g_Vulkan;

global const std::vector<const char*> g_validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};
#if defined(GRAPHICS_DEBUG)
    global const bool g_enableValidationLayers = true;
#else
    global const bool g_enableValidationLayers = false;
#endif

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

internal VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {


    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) 
    {
        LOG_ERROR("Validation Layer: %s", pCallbackData->pMessage);
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
bool VulkanCheckValidationLayerSupport() {
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

extern "C"
void* Win32LoadGraphicsBackend(Win32Window& window)
{
    win32_vulkan_backend& graphics = g_Vulkan;

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

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = (u32)extensions.size();
    createInfo.ppEnabledExtensionNames = extensions.data();

    if(g_enableValidationLayers)
    {
        createInfo.enabledLayerCount = (u32)g_validationLayers.size();
        createInfo.ppEnabledLayerNames = g_validationLayers.data();
    }
    else
    {
        createInfo.enabledLayerCount = 0;
    }

    // TODO(james): use the custom allocators???
    VkInstance instance = 0;
    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
    VERIFY_SUCCESS(result);

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
    {
        // first get the number of extensions
        u32 extensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
        // now get the full list
        std::vector<VkExtensionProperties> extensionProperties(extensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensionProperties.data());

        LOG_INFO("Available Vulkan Extensions:");
        for(u32 index = 0; index < extensionCount; ++index)
        {
            LOG_INFO("\t%s", extensionProperties[index].extensionName);
        }
        LOG_INFO("");
    }

    VkDebugUtilsMessengerEXT debugMessenger = 0;

    if(g_enableValidationLayers)
    {
        // TODO(james): Enable passing this to the create/destroy instance calls, ie createInfo.pNext
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugCreateInfo.pfnUserCallback = VulkanDebugCallback;
        debugCreateInfo.pUserData = nullptr; // Optional

        result = CreateDebugUtilsMessengerEXT(instance, &debugCreateInfo, nullptr, &debugMessenger);
        VERIFY_SUCCESS(result);
    }

    g_Vulkan.debugMessenger = debugMessenger;
    g_Vulkan.instance = instance;

    return &g_Vulkan;
}

extern "C"
void Win32UnloadGraphicsBackend(void* backend_data)
{
    win32_vulkan_backend& graphics = (win32_vulkan_backend&)backend_data;

    if(graphics.debugMessenger)
    {
        DestroyDebugUtilsMessengerEXT(graphics.instance, graphics.debugMessenger, nullptr);
    }

    // NOTE(james): Clean up the rest of the backend members prior to destroying the instance

    vkDestroyInstance(graphics.instance, nullptr);

    // Now release all the temporary memory
    VirtualFree(backend_data, 0, MEM_RELEASE);
}

extern "C"
void Win32GraphicsBeginFrame(void* backend_data)
{
    win32_vulkan_backend& graphics = (win32_vulkan_backend&)backend_data;
}

extern "C"
void Win32GraphicsEndFrame(void* backend_data)
{
    win32_vulkan_backend& graphics = (win32_vulkan_backend&)backend_data;
}