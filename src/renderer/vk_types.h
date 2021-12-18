#pragma once

#include "logger.h"

#include <vulkan/vulkan.h>

#define VK_CHECK(result)                         \
    if (result != VK_SUCCESS)                    \
    {                                            \
        CAKEZ_ERROR("Vulkan Error: %d", result); \
        __debugbreak();                          \
    }

struct Image
{
    VkImage image;
    VkImageView view;
    VkDeviceMemory memory;
};

struct Buffer
{
    VkBuffer buffer;
    VkDeviceMemory memory;
    uint32_t size;
    void *data;
};

struct DescriptorInfo
{
    union
    {
        VkDescriptorBufferInfo bufferInfo;
        VkDescriptorImageInfo imageInfo;
    };

    DescriptorInfo(VkSampler sampler, VkImageView imageView)
    {
        imageInfo.sampler = sampler;
        imageInfo.imageView = imageView;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    DescriptorInfo(VkBuffer buffer)
    {
        bufferInfo.buffer = buffer;
        bufferInfo.offset = 0;
        bufferInfo.range = VK_WHOLE_SIZE;
    }

    DescriptorInfo(VkBuffer buffer, uint32_t offset, uint32_t range)
    {
        bufferInfo.buffer = buffer;
        bufferInfo.offset = offset;
        bufferInfo.range = range;
    }
};