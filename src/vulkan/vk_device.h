
#include <string>
#include <unordered_map>
#include <vector>

constexpr unsigned int FRAME_OVERLAP = 2;

struct vg_render_commands
{
    VkCommandBuffer buffer;
};

struct vg_buffer
{
    VkBuffer handle;
    VmaAllocation allocation;
    void* mapped;
};

struct vg_image
{
    VkImage handle;
    VkImageView view;
    VmaAllocation allocation;
};

struct vg_sampler
{
    VkSampler handle;
};

enum class SpecialDescriptorBinding
{
    Undefined,
    Camera,
    Scene,
    Frame,
    Light,
    Material,
    Instance
};

struct vg_shader_descriptorset_layoutdata
{
    u32 setNumber;
    std::unordered_map<SpecialDescriptorBinding, u32> mapSpecialBindings;
    std::vector<VkDescriptorSetLayoutBinding> bindings;
};

struct vg_shader
{
    render_shader_id id;

    VkShaderModule shaderModule;
    VkShaderStageFlags shaderStageMask;

    VkVertexInputBindingDescription vertexBindingDesc;
    std::vector<VkVertexInputAttributeDescription> vertexAttributes;
    std::vector<VkPushConstantRange> pushConstants;
    std::vector<vg_shader_descriptorset_layoutdata> set_layouts;
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

    vg_buffer scene_buffer;
    vg_buffer lighting_buffer;
    vg_buffer instance_buffer;
};

#define RESOURCE_FACTOR 1024
#define RESOURCE_MULTIPLIER(scalar) (umm)((scalar) * RESOURCE_FACTOR)

struct vg_render_pipeline
{
    render_pipeline_id id;

    VkPipeline pipeline;
    VkPipelineLayout layout;
    u32 descriptorSetLayoutCount;
    VkDescriptorSetLayout descriptorLayouts[20];
    std::vector<vg_shader_descriptorset_layoutdata> set_layouts;
    

    VkRenderPass renderPass;
    VkFramebuffer framebuffer; 

    u32 shaderCount;
    vg_shader* shaderRefs[10];

    u32 samplerCount;
    VkSampler samplers[16];    // TODO(james): pool these instead of allocate per pipeline
};

struct vg_device_resource_pool
{
    u32 pipelineCount;
    vg_render_pipeline pipelines[RESOURCE_MULTIPLIER(1)];

    u32 shaderCount;
    vg_shader shaders[RESOURCE_MULTIPLIER(0.5)];

    u32 imageCount;
    vg_image images[RESOURCE_MULTIPLIER(1)];

    u32 bufferCount;
    vg_buffer buffers[RESOURCE_MULTIPLIER(1.5)];

    u32 materialsCount;
    vg_buffer material_buffer;
};

struct vg_transfer_buffer
{
    VkDevice        device;
    VkQueue         queue;
    vg_buffer       staging_buffer;

    VkCommandPool   cmdPool;
    VkCommandBuffer cmds;
    VkFence         fence;

    umm             lastWritePosition;
    VkDeviceSize    stagingBufferSize;
};

struct vg_command_encoder_pool
{
    VkCommandPool   cmdPool;
};

struct vg_cmd_context
{
    VkCommandBuffer buffer;

};

struct vg_program
{
    u32 numShaders;
    VkShaderModule shaders[6];
    
    // TODO(james): store shader reflection data here..
};

struct vg_kernel
{
    VkRenderPass renderpass;

    VkPipeline pipeline;
    VkPipelineLayout layout;

    // TODO(james): store shader descriptor map here
};

struct vg_device
{
    VkDevice handle;

    VkPhysicalDevice physicalDevice;
    VkPhysicalDeviceProperties device_properties;
    VkPhysicalDeviceMemoryProperties device_memory_properties;

    u32 minUniformBufferOffsetAlignment;

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

    vg_transfer_buffer transferBuffer;

    vg_device_resource_pool resource_pool;
    
    VkExtent2D extent;
    VkRenderPass screenRenderPass;
    vg_image depth_image;

    // Managed Resources
    VmaAllocator allocator;

    memory_arena resourceArena;
    array<vg_command_encoder_pool>& encoderPools;
    array<vg_buffer>& buffers;
    array<vg_image>& textures;
    array<vg_sampler>& samplers;
    array<vg_program>& programs;
    array<vg_kernel>& kernels;

};

struct vg_backend
{
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;

    // TODO(james): Is this enough room for rendering commands?
    u8 pushBuffer[U16MAX];

    vg_device device;
};