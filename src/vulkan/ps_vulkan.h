


struct ps_vulkan_image
{
    VkImage handle;
    VkImageView view;
};

struct ps_vulkan_swapchain
{
    VkSurfaceKHR platform_surface;
    VkSwapchainKHR handle;
    VkFormat format;
    VkExtent2D extent;
    std::vector<ps_vulkan_image> images;
    std::vector<VkFramebuffer> frame_buffers;
};

struct ps_vulkan_queue
{
    u32 queue_family_index;
    VkQueue handle;
};

struct ps_vulkan_buffer
{
    VkBuffer handle;
    // TODO(james): Should really be an allocator reference
    VkDeviceMemory memory_handle;
};

struct ps_vulkan_backend
{
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;

    ps_vulkan_swapchain swap_chain;
    
    VkPhysicalDevice physicalDevice;
    VkPhysicalDeviceMemoryProperties device_memory_properties;
    VkDevice device;

    ps_vulkan_queue q_graphics;
    ps_vulkan_queue q_present;

    u32 currentFrameIndex;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    std::vector<VkFence> imagesInFlight;

    // TODO(james): these are temp until I figure out
    //    how to deal with them.
    VkPipeline graphicsPipeline;
    VkRenderPass renderPass;
    VkDescriptorSetLayout descriptorSetLayout;
    VkPipelineLayout pipelineLayout;
    VkShaderModule vertShader;
    VkShaderModule fragShader;

    ps_vulkan_buffer index_buffer;
    ps_vulkan_buffer vertex_buffer;
    std::vector<ps_vulkan_buffer> uniform_buffers;

    VkDescriptorPool descriptor_pool;
    std::vector<VkDescriptorSet> descriptor_sets;

    // finally the drawing commands
    VkCommandPool command_pool;
    std::vector<VkCommandBuffer> command_buffers;
};
