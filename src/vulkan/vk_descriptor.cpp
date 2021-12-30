
/*******************************************************************************

    Descriptor Layout Cache

********************************************************************************/

internal
void vgInitDescriptorLayoutCache(vg_descriptorlayout_cache& cache, VkDevice device)
{
    cache.device = device;
    // just set the default to VK_NULL_HANDLE
    hmdefault(cache.hm_layouts, VK_NULL_HANDLE);
}

internal
void vgCleanupDescriptorLayoutCache(vg_descriptorlayout_cache& cache)
{
    for(int i = 0; i < hmlen(cache.hm_layouts); ++i)
    {
        vkDestroyDescriptorSetLayout(cache.device, cache.hm_layouts[i].value, nullptr);
    }

    hmfree(cache.hm_layouts);
    ZeroStruct(cache);
}

internal
VkDescriptorSetLayout vgGetDescriptorLayoutFromCache(vg_descriptorlayout_cache& cache, u32 numBindings, VkDescriptorSetLayoutBinding* pBindings)
{
    // compute the hash key
    u64 hash = 0;
    for(u32 i = 0; i < numBindings; ++i)
    {
        // roughly pack the binding into a single value
        // NOTE(james): if this isn't sufficient, just call the hash function for each specific value that
        // needs better collision handling
        u64 bindingHash = pBindings[i].binding | pBindings[i].descriptorType << 8 | pBindings[i].descriptorCount << 16 | pBindings[i].stageFlags;
        hash = MurmurHash64(&bindingHash, sizeof(u64), hash);
    }

    // now pull the layout from the cache
    VkDescriptorSetLayout layout = hmget(cache.hm_layouts, hash);

    if(layout != VK_NULL_HANDLE) // did we already create a layout for this binding set?
    {   
        return layout;
    }

    // if we got here, we need to build a new layout since it isn't already in the cache

    VkDescriptorSetLayoutCreateInfo layoutInfo = vkInit_descriptorset_layout_create_info(
        numBindings, pBindings
    );

    VkResult result = vkCreateDescriptorSetLayout(cache.device, &layoutInfo, nullptr, &layout);
    ASSERT(result == VK_SUCCESS);

    hmput(cache.hm_layouts, hash, layout);

    return layout;
}

/*******************************************************************************

    Descriptor Allocator Functions

********************************************************************************/

internal void
vgInitDescriptorAllocator(vg_descriptor_allocator& alloc, VkDevice device)
{
    alloc.device = device;
    alloc.current = VK_NULL_HANDLE;
    arrsetcap(alloc.a_free_pools, 10);
    arrsetcap(alloc.a_used_pools, 10);
}

internal void
vgCleanupDescriptorAllocator(vg_descriptor_allocator& alloc)
{
    for(int i = 0; i < arrlen(alloc.a_free_pools); ++i)
    {
        vkDestroyDescriptorPool(alloc.device, alloc.a_free_pools[i], nullptr);
    }

    for(int i = 0; i < arrlen(alloc.a_used_pools); ++i)
    {
        vkDestroyDescriptorPool(alloc.device, alloc.a_used_pools[i], nullptr);
    }

    arrfree(alloc.a_free_pools);
    arrfree(alloc.a_used_pools);

    ZeroStruct(alloc);
}

#define VG_POOLSIZER(x, size) (u32)((x) * (size))
internal VkDescriptorPool
vgCreateDescriptorPool(vg_descriptor_allocator& alloc, u32 poolSize, VkDescriptorPoolCreateFlags createFlags)
{

    // NOTE(james): Tune these numbers for better VRAM efficiency, multiples of the entire descriptor pool count
    VkDescriptorPoolSize poolSizes[] = {
				{ VK_DESCRIPTOR_TYPE_SAMPLER,                  VG_POOLSIZER( 0.5, poolSize ) },
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,   VG_POOLSIZER( 1.0, poolSize ) },
				{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,            VG_POOLSIZER( 4.0, poolSize ) },
				{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,            VG_POOLSIZER( 1.0, poolSize ) },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,     VG_POOLSIZER( 1.0, poolSize ) },
				{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,     VG_POOLSIZER( 1.0, poolSize ) },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,           VG_POOLSIZER( 2.0, poolSize ) },
				{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,           VG_POOLSIZER( 2.0, poolSize ) },
                { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,   VG_POOLSIZER( 1.0, poolSize ) },
				{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,   VG_POOLSIZER( 1.0, poolSize ) },
				{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,         VG_POOLSIZER( 0.5, poolSize ) }
			};

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = ARRAY_COUNT(poolSizes);
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = poolSize;
    poolInfo.flags = createFlags; 

    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkResult result = vkCreateDescriptorPool(alloc.device, &poolInfo, nullptr, &pool);
    ASSERT(result == VK_SUCCESS);

    return pool;
}

internal VkDescriptorPool
vgGrabFreeDescriptorPool(vg_descriptor_allocator& alloc)
{
    VkDescriptorPool pool = VK_NULL_HANDLE;

    if(arrlen(alloc.a_free_pools) > 0)
    {
        pool = arrpop(alloc.a_free_pools);
    }
    else
    {
        pool = vgCreateDescriptorPool(alloc, 1000, 0);
    }

    return pool;
}

internal void
vgResetDescriptorPools(vg_descriptor_allocator& alloc)
{
    while(arrlen(alloc.a_used_pools))
    {
        VkDescriptorPool pool = arrpop(alloc.a_used_pools);
        vkResetDescriptorPool(alloc.device, pool, 0);
        arrpush(alloc.a_free_pools, pool);
    }

    alloc.current = VK_NULL_HANDLE;
}

internal bool
vgAllocateDescriptor(vg_descriptor_allocator& alloc, VkDescriptorSetLayout layout, VkDescriptorSet* pDescriptor)
{
    if(!alloc.current)
    {
        alloc.current = vgGrabFreeDescriptorPool(alloc);
        arrpush(alloc.a_used_pools, alloc.current);
    }

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.pNext = nullptr;
    allocInfo.descriptorPool = alloc.current;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkResult result = vkAllocateDescriptorSets(alloc.device, &allocInfo, pDescriptor);
    bool needsRealloc = false;
    
    switch(result)
    {
        case VK_SUCCESS:
            return true;
        
        case VK_ERROR_FRAGMENTED_POOL:
        case VK_ERROR_OUT_OF_POOL_MEMORY:
            needsRealloc = true;
            break;
        
    }

    if(needsRealloc)
    {
        alloc.current = vgGrabFreeDescriptorPool(alloc);
        arrpush(alloc.a_used_pools, alloc.current);
        
        allocInfo.descriptorPool = alloc.current;
        result = vkAllocateDescriptorSets(alloc.device, &allocInfo, pDescriptor);

        // if this fails we have bigger issues than a missing descriptor set
        if(result == VK_SUCCESS)
        {
            return true;
        }
    }

    // uh-oh this is some bad-juju
    ASSERT(false);
    *pDescriptor = VK_NULL_HANDLE;
    return false;
}
