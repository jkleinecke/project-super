

struct win32_vulkan_backend
{
    MemoryArena memory;

    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;

};