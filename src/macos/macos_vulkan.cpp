
#define VK_USE_PLATFORM_METAL_EXT
#include <vulkan/vulkan.h>

#include <vector>
#include <cstdint>
#include <algorithm>

internal
inline void fopen_s(FILE** ppFile, const char* szPath, const char* mode)
{
    *ppFile = fopen(szPath, mode);
}

#include "../ps_image.h"
#include "../vulkan/ps_vulkan.cpp"


struct macos_vulkan_backend
{
    ps_vulkan_backend vulkan;
};
global macos_vulkan_backend g_MacosVulkanBackend = {};

extern "C" 
void *MacosLoadGraphicsBackend()
{
    std::vector<const char*> platformExtensions = {};

    ps_vulkan_backend& vb = g_MacosVulkanBackend.vulkan;
    VkResult result = vbInitialize(vb, &platformExtensions);

    return &g_MacosVulkanBackend;
}