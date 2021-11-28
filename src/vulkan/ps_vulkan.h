
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

struct ps_vulkan_backend
{
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;

    ps_vulkan_swapchain swap_chain;
    
    VkPhysicalDevice physicalDevice;
    VkDevice device;

    ps_vulkan_queue q_graphics;
    ps_vulkan_queue q_present;

    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;

    // TODO(james): these are temp until I figure out
    //    how to deal with them.
    VkPipeline graphicsPipeline;
    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;
    VkShaderModule vertShader;
    VkShaderModule fragShader;

    // finally the drawing commands
    VkCommandPool command_pool;
    std::vector<VkCommandBuffer> command_buffers;
};