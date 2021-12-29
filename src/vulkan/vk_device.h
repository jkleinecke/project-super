

constexpr unsigned int FRAME_OVERLAP = 2;

struct vg_render_commands
{
    VkCommandBuffer buffer;
};

struct vg_buffer
{
    VkBuffer handle;
    VkDeviceMemory memory;
};

struct vg_image
{
    VkImage handle;
    VkImageView view;
    VkDeviceMemory memory;
};

struct vg_sampler
{
    VkSampler handle;
};

struct vg_shaderset
{
    VkShaderModule vertex;
    VkShaderModule frag;
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
    vg_buffer instance_buffer;
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

    
    VkExtent2D extent;
    vg_renderpass renderPass;
    vg_pipeline pipeline;

    vg_image depth_image;
    vg_image texture;
    vg_sampler sampler;

    vg_buffer index_buffer;
    vg_buffer vertex_buffer;
};

struct vg_backend
{
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;

    // TODO(james): Is this enough room for rendering commands?
    u8 pushBuffer[U16MAX];

    vg_device device;
};