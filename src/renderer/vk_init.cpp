#include <vulkan/vulkan.h>

VkCommandBufferBeginInfo cmd_begin_info()
{
    VkCommandBufferBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    return info;
}

VkCommandBufferAllocateInfo cmd_alloc_info(VkCommandPool pool)
{
    VkCommandBufferAllocateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.commandBufferCount = 1;
    info.commandPool = pool;
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    return info;
}

VkFenceCreateInfo fence_info(VkFenceCreateFlags flags = 0)
{
    VkFenceCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    info.flags = flags;
    return info;
}

VkSubmitInfo submit_info(VkCommandBuffer *cmd, uint32_t cmdCount = 1)
{
    VkSubmitInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.commandBufferCount = cmdCount;
    info.pCommandBuffers = cmd;

    return info;
}

VkDescriptorSetLayoutBinding layout_binding(
    VkDescriptorType type,
    VkShaderStageFlags shaderStages,
    uint32_t count,
    uint32_t bindingNumber)
{
    VkDescriptorSetLayoutBinding binding = {};
    binding.binding = bindingNumber;
    binding.descriptorCount = count;
    binding.descriptorType = type;
    binding.stageFlags = shaderStages;

    return binding;
}

VkWriteDescriptorSet write_set(
    VkDescriptorSet set,
    VkDescriptorType type,
    DescriptorInfo *descInfo,
    uint32_t bindingNumber,
    uint32_t count)
{
    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = set;
    write.pImageInfo = &descInfo->imageInfo;
    write.pBufferInfo = &descInfo->bufferInfo;
    write.dstBinding = bindingNumber;
    write.descriptorCount = count;
    write.descriptorType = type;

    return write;
}