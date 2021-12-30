
#include <vector>

constexpr unsigned int FRAME_OVERLAP = 2;

struct vg_render_commands
{
    VkCommandBuffer buffer;
};

struct vg_buffer
{
    render_buffer_id id;
    
    VkBuffer handle;
    VkDeviceMemory memory;
};

struct vg_image
{
    render_image_id id;

    VkImage handle;
    VkImageView view;
    VkDeviceMemory memory;
};

struct vg_sampler
{
    VkSampler handle;
};

struct vg_shader_descriptorset_layoutdata
{
    u32 setNumber;
    std::vector<VkDescriptorSetLayoutBinding> bindings;
};

struct vg_shader
{
    render_shader_id id;

    VkShaderModule shaderModule;
    VkShaderStageFlags shaderStageMask;

    VkVertexInputBindingDescription vertexBindingDesc;
    std::vector<VkVertexInputAttributeDescription> vertexAttributes;
    std::vector<vg_shader_descriptorset_layoutdata> set_layouts;
    std::vector<VkPushConstantRange> pushConstants;
};

struct vg_shaderset
{
    vg_shader vertex;
    vg_shader frag;
};

struct vg_pipeline
{
    VkPipeline handle;
    VkPipelineLayout layout;
    vg_shaderset shaders;
    VkDescriptorSetLayout descriptorLayout;
};

struct vg_renderpass
{
    VkRenderPass handle;
    // TODO(james): add framebuffers
};

struct vg_descriptorlayout_cache_info
{
    u64 key;
    VkDescriptorSetLayout value;
};

struct vg_descriptorlayout_cache
{
    vg_descriptorlayout_cache_info* hm_layouts;
    VkDevice device;
};

struct vg_descriptor_allocator
{
    VkDevice device;
    VkDescriptorPool current;
    VkDescriptorPool* a_free_pools;
    VkDescriptorPool* a_used_pools;
};

struct vg_queue
{
    u32 queue_family_index;
    VkQueue handle;
};

struct vg_framedata
{
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
    VkSemaphore renderSemaphore;
    VkSemaphore presentSemaphore;
    VkFence renderFence;

    vg_descriptor_allocator dynamicDescriptorAllocator;

    // TODO(james): make some room for buffers that are updated each frame
    vg_buffer frame_buffer;
};

#define RESOURCE_FACTOR 1024
#define RESOURCE_MULTIPLIER(scalar) (umm)((scalar) * RESOURCE_FACTOR)

struct vg_material_resource
{
    render_material_id id;

    VkPipeline pipeline;
    VkPipelineLayout layout;
    u32 descriptorSetLayoutCount;
    VkDescriptorSetLayout descriptorLayouts[20];

    VkRenderPass renderPass;
    VkFramebuffer framebuffer; 

    u32 shaderCount;
    vg_shader* shaderRefs[10];

    u32 samplerCount;
    VkSampler samplers[16];    // TODO(james): pool these instead of allocate per material
};

struct vg_device_resource_pool
{
    u32 materialCount;
    vg_material_resource materials[RESOURCE_MULTIPLIER(1)];

    u32 shaderCount;
    vg_shader shaders[RESOURCE_MULTIPLIER(0.5)];

    u32 imageCount;
    vg_image images[RESOURCE_MULTIPLIER(1)];

    u32 bufferCount;
    vg_buffer buffers[RESOURCE_MULTIPLIER(1.5)];
};

struct vg_device
{
    VkDevice handle;

    VkPhysicalDevice physicalDevice;
    VkPhysicalDeviceProperties device_properties;
    VkPhysicalDeviceMemoryProperties device_memory_properties;

    vg_queue q_graphics;
    vg_queue q_present;

    u32 currentFrameIndex;
    vg_framedata frames[FRAME_OVERLAP];
    vg_framedata* pPrevFrame;
    vg_framedata* pCurFrame;

    u32 curSwapChainIndex;
    u32 numSwapChainImages;
    VkSurfaceKHR platform_surface;
    VkSwapchainKHR swapChain;
    VkFormat swapChainFormat;
    vg_image* paSwapChainImages;
    VkFramebuffer* paFramebuffers;

    vg_descriptor_allocator descriptorAllocator;
    vg_descriptorlayout_cache descriptorLayoutCache;

    // TODO(james): these are temp until I figure out
    //    how to deal with them.

    vg_device_resource_pool resource_pool;
    
    VkExtent2D extent;
    VkRenderPass screenRenderPass;
    vg_image depth_image;
};

struct vg_backend
{
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;

    // TODO(james): Is this enough room for rendering commands?
    u8 pushBuffer[U16MAX];

    vg_device device;
};