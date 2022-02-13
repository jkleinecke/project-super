
#include <string>
#include <unordered_map>
#include <vector>

constexpr unsigned int FRAME_OVERLAP = 2;
constexpr unsigned int MAX_DESCRIPTOR_SETS = 4;

// struct vg_render_commands
// {
//     VkCommandBuffer buffer;
// };

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

    VkFormat format;
    u32 width;
    u32 height;
    u32 layers;
};

struct vg_sampler
{
    VkSampler handle;
};

// enum class SpecialDescriptorBinding
// {
//     Undefined,
//     Camera,
//     Scene,
//     Frame,
//     Light,
//     Material,
//     Instance
// };

// struct vg_shader_descriptorset_layoutdata
// {
//     u32 setNumber;
//     std::unordered_map<SpecialDescriptorBinding, u32> mapSpecialBindings;
//     std::vector<VkDescriptorSetLayoutBinding> bindings;
// };

// struct vg_shader
// {
//     render_shader_id id;

//     VkShaderModule shaderModule;
//     VkShaderStageFlags shaderStageMask;

//     VkVertexInputBindingDescription vertexBindingDesc;
//     std::vector<VkVertexInputAttributeDescription> vertexAttributes;
//     std::vector<VkPushConstantRange> pushConstants;
//     std::vector<vg_shader_descriptorset_layoutdata> set_layouts;
// };


// struct vg_shaderset
// {
//     vg_shader vertex;
//     vg_shader frag;
// };

// struct vg_pipeline
// {
//     VkPipeline handle;
//     VkPipelineLayout layout;
//     vg_shaderset shaders;
//     VkDescriptorSetLayout descriptorLayout;
// };

// struct vg_descriptorlayout_cache_info
// {
//     u64 key;
//     VkDescriptorSetLayout value;
// };

// struct vg_descriptorlayout_cache
// {
//     vg_descriptorlayout_cache_info* hm_layouts;
//     VkDevice device;
// };

struct vg_descriptor_pool
{
    VkDescriptorPool handle;
    vg_descriptor_pool* next;
};

// struct vg_descriptor_allocator
// {
//     VkDevice device;
//     VkDescriptorPool current;
//     VkDescriptorPool* a_free_pools;
//     VkDescriptorPool* a_used_pools;
// };

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

    // vg_buffer scene_buffer;
    // vg_buffer lighting_buffer;
    // vg_buffer instance_buffer;
};

// #define RESOURCE_FACTOR 1024
// #define RESOURCE_MULTIPLIER(scalar) (umm)((scalar) * RESOURCE_FACTOR)

// struct vg_render_pipeline
// {
//     render_pipeline_id id;

//     VkPipeline pipeline;
//     VkPipelineLayout layout;
//     u32 descriptorSetLayoutCount;
//     VkDescriptorSetLayout descriptorLayouts[20];
//     std::vector<vg_shader_descriptorset_layoutdata> set_layouts;
    

//     VkRenderPass renderPass;
//     VkFramebuffer framebuffer; 

//     u32 shaderCount;
//     vg_shader* shaderRefs[10];

//     u32 samplerCount;
//     VkSampler samplers[16];    // TODO(james): pool these instead of allocate per pipeline
// };

// struct vg_device_resource_pool
// {
//     u32 pipelineCount;
//     vg_render_pipeline pipelines[RESOURCE_MULTIPLIER(1)];

//     u32 shaderCount;
//     vg_shader shaders[RESOURCE_MULTIPLIER(0.5)];

//     u32 imageCount;
//     vg_image images[RESOURCE_MULTIPLIER(1)];

//     u32 bufferCount;
//     vg_buffer buffers[RESOURCE_MULTIPLIER(1.5)];

//     u32 materialsCount;
//     vg_buffer material_buffer;
// };

// struct vg_transfer_buffer
// {
//     VkDevice        device;
//     VkQueue         queue;
//     vg_buffer       staging_buffer;

//     VkCommandPool   cmdPool;
//     VkCommandBuffer cmds;
//     VkFence         fence;

//     umm             lastWritePosition;
//     VkDeviceSize    stagingBufferSize;
// };




struct vg_program_binding_desc
{
    u16 set;
    u16 binding;

#if PROJECTSUPER_INTERNAL
    char name[GFX_MAX_SHADER_IDENTIFIER_NAME_LENGTH];
#endif
};

#define VG_MAX_PROGRAM_SHADER_COUNT 6
struct vg_program
{
    u32 numShaders;
    VkShaderModule  shaders[VG_MAX_PROGRAM_SHADER_COUNT];
    SpvReflectShaderModule* shaderReflections[VG_MAX_PROGRAM_SHADER_COUNT];
    SpvReflectEntryPoint* entrypoints[VG_MAX_PROGRAM_SHADER_COUNT];

    VkPipelineLayout pipelineLayout;
    array<VkDescriptorSetLayout>* descriptorSetLayouts;
    hashtable<vg_program_binding_desc>* mapBindingDesc;
};

struct vg_kernel
{
    VkPipeline pipeline;
    vg_program* program;
    VkSampleCountFlagBits sampleCount;
};

struct vg_rendertargetview
{
    vg_image* image;
    VkImageView view;
    VkSampleCountFlagBits sampleCount;
    VkAttachmentLoadOp loadOp;
    VkClearValue clearValue;
};

struct vg_resourceheap
{
    memory_arena arena;

    hashtable<vg_buffer*>* buffers;
    hashtable<vg_image*>* textures;
    hashtable<vg_sampler*>* samplers;
    hashtable<vg_rendertargetview*>* rtvs;
    hashtable<vg_program*>* programs;
    hashtable<vg_kernel*>* kernels;
};

struct vg_renderpass
{
    u32 lastUsedInFrameIndex;
    VkRenderPass handle;

    vg_renderpass* next;    // used when maintaining a freelist
};

struct vg_framebuffer
{
    u32 lastUsedInFrameIndex;
    VkFramebuffer handle;

    vg_framebuffer* next;    // used when maintaining a freelist
};

struct vg_cmd_context
{
    VkCommandBuffer buffer[FRAME_OVERLAP];

    vg_renderpass* activeRenderpass;
    vg_framebuffer* activeFramebuffer;
    vg_kernel* activeKernel;
    vg_buffer* activeIB;
    vg_buffer* activeVB;
    VkDescriptorSet* activeDescriptorSets;
    b32 needsDescriptorSetsBound;
};

struct vg_command_encoder_pool
{
    VkCommandPool   cmdPool[FRAME_OVERLAP];
    GfxQueueType    queueType;
    vg_queue*       queue;

    hashtable<vg_cmd_context*>* cmdcontexts;
};

struct vg_device
{
    VkDevice handle;

    VkPhysicalDevice physicalDevice;
    VkPhysicalDeviceProperties device_properties;
    VkPhysicalDeviceMemoryProperties device_memory_properties;

    // u32 minUniformBufferOffsetAlignment;

    vg_queue q_graphics;
    vg_queue q_compute;
    vg_queue q_transfer;
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
    array<vg_image*>* swapChainImages;  // NOTE(james): these are just easy references, they are owned by the default resource heap
    // VkFramebuffer* paFramebuffers;

    vg_descriptor_pool* descriptorPools[FRAME_OVERLAP];
    vg_descriptor_pool* freelist_descriptorPool;
    // vg_descriptor_allocator descriptorAllocator;
    // vg_descriptorlayout_cache descriptorLayoutCache;

    // vg_transfer_buffer transferBuffer;

    // vg_device_resource_pool resource_pool;
    
    VkExtent2D extent;
    // VkRenderPass screenRenderPass;
    // vg_image depth_image;

    // Managed Resources
    VmaAllocator allocator;
    memory_arena arena;
    memory_arena* frameArena;    // use for transient memory only valid for the current frame
    temporary_memory frameTemp;

    hashtable<vg_resourceheap*>* resourceHeaps;
    hashtable<vg_command_encoder_pool*>* encoderPools;

    // used by internal backend to initial transition images, etc..
    VkCommandPool internal_cmd_pool; 
    VkCommandBuffer internal_cmd_buffer;
    VkSemaphore internal_wait_semaphore;
    VkSemaphore internal_signal_semaphore;
    VkFence internal_cmd_fence;

    // NOTE(james): Render passes and framebuffer objects are created as required.  Can be
    //   garbage collected later and put into the free list for re-use later
    hashtable<vg_renderpass*>* mapRenderpasses;
    vg_renderpass* freelist_renderpass;

    hashtable<vg_framebuffer*>* mapFramebuffers;
    vg_framebuffer* freelist_framebuffer;
};

struct vg_backend
{
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;

    vg_device device;
};