
#if 0
VkCommandBuffer vgGetCmdBuffer(vg_device* device)
{
    return device->pCurFrame->commandBuffer;
}

VkFramebuffer vgGetFramebuffer(vg_device* device)
{
    return device->paFramebuffers[device->curSwapChainIndex];
}

VkDescriptorSet vgCreateDescriptor(vg_device* device, VkDescriptorSetLayout layout)
{
    VkDescriptorSet ret = VK_NULL_HANDLE;
    bool success = vgAllocateDescriptor(device->pCurFrame->dynamicDescriptorAllocator, layout, &ret);
    ASSERT(success);
    return ret;
}

void vgUpdateBufferData(vg_device* device, vg_buffer& buffer, mem_size size, const void* data)
{
    void* gpuMemory;
    vkMapMemory(device->handle, buffer.memory, 0, size, 0, &gpuMemory);
        Copy(size, data, gpuMemory);
    vkUnmapMemory(device->handle, buffer.memory);
}

void vgUpdateDescriptorSets(vg_device* device, u32 count, VkWriteDescriptorSet* pWrites)
{
    vkUpdateDescriptorSets(device->handle, count, pWrites, 0, nullptr);
}

void vgBeginRecordingCmds(VkCommandBuffer cmds)
{
    VkCommandBufferBeginInfo beginInfo = vkInit_command_buffer_begin_info(0);
    VkResult result = vkBeginCommandBuffer(cmds, &beginInfo);
    ASSERT(result == VK_SUCCESS);
}

void vgEndRecordingCmds(VkCommandBuffer cmds)
{
    VkResult result = vkEndCommandBuffer(cmds);
    ASSERT(result == VK_SUCCESS);
}

void vgBeginRenderPass(VkCommandBuffer cmds, VkRenderPass renderPass, u32 numClearValues, VkClearValue* pClearValues, VkExtent2D extent, VkFramebuffer framebuffer)
{
    VkRenderPassBeginInfo renderPassInfo = vkInit_renderpass_begin_info(
        renderPass, extent, framebuffer
    );

    renderPassInfo.clearValueCount = numClearValues;
    renderPassInfo.pClearValues = pClearValues;
    vkCmdBeginRenderPass(cmds, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void vgEndRenderPass(VkCommandBuffer cmds)
{
    vkCmdEndRenderPass(cmds);
}

void vgBindPipeline(VkCommandBuffer cmds, VkPipeline pipeline)
{
    vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
}

void vgBindDescriptorSets(VkCommandBuffer cmds, VkPipelineLayout layout, u32 count, VkDescriptorSet* pDescriptorSets)
{
    vkCmdBindDescriptorSets(cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, count, pDescriptorSets, 0, nullptr);
}

void vgBindVertexBuffers(VkCommandBuffer cmds, u32 count, VkBuffer* pBuffers, VkDeviceSize* pOffsets)
{
    vkCmdBindVertexBuffers(cmds, 0, count, pBuffers, pOffsets);
}

void vgBindIndexBuffer(VkCommandBuffer cmds, VkBuffer buffer)
{
    vkCmdBindIndexBuffer(cmds, buffer, 0, VK_INDEX_TYPE_UINT32);
}

void vgDrawIndexed(VkCommandBuffer cmds, u32 indexCount, u32 instanceCount)
{
    vkCmdDrawIndexed(cmds, indexCount, instanceCount, 0, 0, 0);
}

internal
void vgLoadApi(vg_backend& vb, ps_graphics_api& api)
{
    api.GetCmdBuffer = &vgGetCmdBuffer;
    api.GetFramebuffer = &vgGetFramebuffer;
    api.CreateDescriptor = &vgCreateDescriptor;
    api.UpdateBufferData = &vgUpdateBufferData;
    api.UpdateDescriptorSets = &vgUpdateDescriptorSets;
    api.BeginRecordingCmds = &vgBeginRecordingCmds;
    api.EndRecordingCmds = &vgEndRecordingCmds;
    api.BeginRenderPass = &vgBeginRenderPass;
    api.EndRenderPass = &vgEndRenderPass;
    api.BindPipeline = &vgBindPipeline;
    api.BindDescriptorSets = &vgBindDescriptorSets;
    api.BindVertexBuffers = &vgBindVertexBuffers;
    api.BindIndexBuffer = &vgBindIndexBuffer;
    api.DrawIndexed = &vgDrawIndexed;
}
#endif