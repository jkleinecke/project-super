

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
};

struct vg_renderpass
{
    VkRenderPass handle;
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
    //VkFramebuffer frameBuffer;
    VkSemaphore renderSemaphore;
    VkSemaphore presentSemaphore;
    VkFence renderFence;

    // TODO(james): make some room for buffers that are updated each frame
    VkDescriptorSet globalDescriptor;

    vg_buffer camera_buffer;
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

    u32 numSwapChainImages;
    VkSurfaceKHR platform_surface;
    VkSwapchainKHR swapChain;
    VkFormat swapChainFormat;
    std::vector<vg_image> swapChainImages;
    std::vector<VkFramebuffer> framebuffers;

    // TODO(james): these are temp until I figure out
    //    how to deal with them.
    VkDescriptorSetLayout descriptorLayout;
    VkDescriptorPool descriptorPool;
    
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

    vg_device device;
};