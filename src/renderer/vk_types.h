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
    void* data;
};