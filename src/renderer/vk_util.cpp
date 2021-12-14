#include "vk_types.h"

// TODO: Just so vscode does not complain about memcpy
#include <string.h>

uint32_t vk_get_memory_type_index(
    VkPhysicalDevice gpu,
    VkMemoryRequirements memRequirements,
    VkMemoryPropertyFlags memProps)
{
    uint32_t typeIdx = INVALID_IDX;

    VkPhysicalDeviceMemoryProperties gpuMemProps;
    vkGetPhysicalDeviceMemoryProperties(gpu, &gpuMemProps);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    for (uint32_t i = 0; i < gpuMemProps.memoryTypeCount; i++)
    {
        if (memRequirements.memoryTypeBits & (1 << i) &&
            (gpuMemProps.memoryTypes[i].propertyFlags & memProps) == memProps)
        {
            typeIdx = i;
            break;
        }
    }

    CAKEZ_ASSERT(typeIdx != INVALID_IDX, "Failed to fine proper type Index for Memory Properties: %d", memProps);

    return typeIdx;
}

Image vk_allocate_image(
    VkDevice device,
    VkPhysicalDevice gpu,
    uint32_t width,
    uint32_t height,
    VkFormat format)
{
    Image image = {};

    VkImageCreateInfo imgInfo = {};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = format;
    imgInfo.extent = {width, height, 1};
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    // imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VK_CHECK(vkCreateImage(device, &imgInfo, 0, &image.image));

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image.image, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = vk_get_memory_type_index(gpu, memRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK(vkAllocateMemory(device, &allocInfo, 0, &image.memory));
    VK_CHECK(vkBindImageMemory(device, image.image, image.memory, 0));

    return image;
}

Buffer vk_allocate_buffer(
    VkDevice device,
    VkPhysicalDevice gpu,
    uint32_t size,
    VkBufferUsageFlags bufferUsage,
    VkMemoryPropertyFlags memProps)
{
    Buffer buffer = {};
    buffer.size = size;

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.usage = bufferUsage;
    bufferInfo.size = size;
    VK_CHECK(vkCreateBuffer(device, &bufferInfo, 0, &buffer.buffer));

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer.buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = buffer.size;
    allocInfo.memoryTypeIndex = vk_get_memory_type_index(gpu, memRequirements, memProps);

    VK_CHECK(vkAllocateMemory(device, &allocInfo, 0, &buffer.memory));

    // Only map memory we can actually write to from the CPU
    if (memProps & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    {
        VK_CHECK(vkMapMemory(device, buffer.memory, 0, MB(1), 0, &buffer.data));
    }

    VK_CHECK(vkBindBufferMemory(device, buffer.buffer, buffer.memory, 0));

    return buffer;
}

//TODO: Implement
void vk_copy_to_buffer(Buffer *buffer, void *data, uint32_t size)
{
    CAKEZ_ASSERT(buffer->size >= size, "Buffer too small: %d for data: %d", buffer->size, size);

    // If we have mapped data
    if (buffer->data)
    {
        memcpy(buffer->data, data, size);
    }
    else
    {
        // TODO: Implement, copy data using command buffer
    }
}