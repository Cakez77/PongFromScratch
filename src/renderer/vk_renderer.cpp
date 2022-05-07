#include <vulkan/vulkan.h>
#ifdef WINDOWS_BUILD
#include <vulkan/vulkan_win32.h>
#elif LINUX_BUILD
#endif

#include "dds_strcuts.h"
#include "logger.h"
#include "platform.h"

#include "vk_types.h"
#include "vk_init.cpp"
#include "vk_util.cpp"
#include "vk_shader_util.cpp"

uint32_t constexpr MAX_IMAGES = 100;
uint32_t constexpr MAX_DESCRIPTORS = 100;
uint32_t constexpr MAX_RENDER_COMMANDS = 100;
uint32_t constexpr MAX_TRANSFORMS = MAX_ENTITIES;

static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT msgSeverity,
    VkDebugUtilsMessageTypeFlagsEXT msgFlags,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *pUserData)
{
    // CAKEZ_ERROR(pCallbackData->pMessage);
    CAKEZ_ASSERT(0, pCallbackData->pMessage);
    return false;
}

struct VkContext
{
    VkExtent2D screenSize;

    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkSurfaceKHR surface;
    VkSurfaceFormatKHR surfaceFormat;
    VkPhysicalDevice gpu;
    VkDevice device;
    VkQueue graphicsQueue;
    VkSwapchainKHR swapchain;
    VkRenderPass renderPass;
    VkCommandPool commandPool;
    VkCommandBuffer cmd;

    uint32_t imageCount;
    Image images[MAX_IMAGES];

    uint32_t descCount;
    Descriptor descriptors[MAX_DESCRIPTORS];

    uint32_t renderCommandCount;
    RenderCommand renderCommands[MAX_RENDER_COMMANDS];

    uint32_t transformCount;
    Transform transforms[MAX_TRANSFORMS];

    Buffer stagingBuffer;
    Buffer transformStorageBuffer;
    Buffer materialStorageBuffer;
    Buffer globalUBO;
    Buffer indexBuffer;

    VkDescriptorPool descPool;

    // TODO: We will abstract this later
    VkSampler sampler;
    VkDescriptorSetLayout setLayout;
    VkPipelineLayout pipeLayout;
    VkPipeline pipeline;

    VkSemaphore aquireSemaphore;
    VkSemaphore submitSemaphore;
    VkFence imgAvailableFence;

    uint32_t scImgCount;
    // TODO: Suballocation from Main Memory
    VkImage scImages[5];
    VkImageView scImgViews[5];
    VkFramebuffer framebuffers[5];

    int graphicsIdx;
};

Image *vk_create_image(VkContext *vkcontext, AssetTypeID assetTypeID)
{
    Image *image = 0;
    if (vkcontext->imageCount < MAX_IMAGES)
    {
        const char *data = get_asset(assetTypeID);
        uint32_t width = 1;
        uint32_t height = 1;
        const char *dataBegin = data;

        if (assetTypeID != ASSET_SPRITE_WHITE)
        {
            const DDSFile *file = (const DDSFile *)data;
            width = file->header.Width;
            height = file->header.Height;
            dataBegin = &file->dataBegin;
        }

        uint32_t textureSize = width * height * 4;

        vk_copy_to_buffer(&vkcontext->stagingBuffer, dataBegin, textureSize);

        //TODO: Assertions
        image = &vkcontext->images[vkcontext->imageCount];
        *image = vk_allocate_image(vkcontext->device,
                                   vkcontext->gpu,
                                   width,
                                   height,
                                   VK_FORMAT_R8G8B8A8_UNORM);

        if (image->image != VK_NULL_HANDLE && image->memory != VK_NULL_HANDLE)
        {
            VkCommandBuffer cmd;
            VkCommandBufferAllocateInfo cmdAlloc = cmd_alloc_info(vkcontext->commandPool);
            VK_CHECK(vkAllocateCommandBuffers(vkcontext->device, &cmdAlloc, &cmd));

            VkCommandBufferBeginInfo beginInfo = cmd_begin_info();
            VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

            VkImageSubresourceRange range = {};
            range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            range.layerCount = 1;
            range.levelCount = 1;

            // Transition Layout to Transfer optimal
            VkImageMemoryBarrier imgMemBarrier = {};
            imgMemBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imgMemBarrier.image = image->image;
            imgMemBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imgMemBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imgMemBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            imgMemBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imgMemBarrier.subresourceRange = range;

            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, 0, 0, 0,
                                 1, &imgMemBarrier);

            VkBufferImageCopy copyRegion = {};
            copyRegion.imageExtent = {width, height, 1};
            copyRegion.imageSubresource.layerCount = 1;
            copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            vkCmdCopyBufferToImage(cmd, vkcontext->stagingBuffer.buffer, image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

            imgMemBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imgMemBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imgMemBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imgMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, 0, 0, 0,
                                 1, &imgMemBarrier);

            VK_CHECK(vkEndCommandBuffer(cmd));

            VkFence uploadFence;
            VkFenceCreateInfo fenceInfo = fence_info();
            VK_CHECK(vkCreateFence(vkcontext->device, &fenceInfo, 0, &uploadFence));

            VkSubmitInfo submitInfo = submit_info(&cmd);
            VK_CHECK(vkQueueSubmit(vkcontext->graphicsQueue, 1, &submitInfo, uploadFence));

            // Create Image View while waiting on the GPU
            {
                VkImageViewCreateInfo viewInfo = {};
                viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                viewInfo.image = image->image;
                viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
                viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                viewInfo.subresourceRange.layerCount = 1;
                viewInfo.subresourceRange.levelCount = 1;
                viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

                VK_CHECK(vkCreateImageView(vkcontext->device, &viewInfo, 0, &image->view));
            }

            VK_CHECK(vkWaitForFences(vkcontext->device, 1, &uploadFence, true, UINT64_MAX));
        }
        else
        {
            CAKEZ_ASSERT(image->image != VK_NULL_HANDLE,
                         "Failed to allocate Memory for Image: %d", assetTypeID);
            CAKEZ_ASSERT(image->memory != VK_NULL_HANDLE,
                         "Failed to allocate Memory for Image: %d", assetTypeID);
            image = 0;
        }

        // Memory is freed
        // TODO: Allocater
        delete data;
    }
    else
    {
        CAKEZ_ASSERT(0, "Reached Maximum amount of Images");
    }

    return image;
}

Image *vk_get_image(VkContext *vkcontext, AssetTypeID assetTypeID)
{
    Image *image = 0;
    for (uint32_t i = 0; i < vkcontext->imageCount; i++)
    {
        Image *img = &vkcontext->images[i];

        if (img->assetTypeID == assetTypeID)
        {
            image = img;
            break;
        }
    }

    if (!image)
    {
        image = vk_create_image(vkcontext, assetTypeID);
    }

    return image;
}

internal Descriptor *vk_create_descriptor(VkContext *vkcontext, AssetTypeID assetTypeID)
{
    Descriptor *desc = 0;

    if (vkcontext->descCount < MAX_DESCRIPTORS)
    {
        desc = &vkcontext->descriptors[vkcontext->descCount];
        *desc = {};
        desc->assetTypeID = assetTypeID;

        // Allocation
        {
            VkDescriptorSetAllocateInfo allocInfo = {};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.pSetLayouts = &vkcontext->setLayout;
            allocInfo.descriptorSetCount = 1;
            allocInfo.descriptorPool = vkcontext->descPool;
            VK_CHECK(vkAllocateDescriptorSets(vkcontext->device, &allocInfo, &desc->set));
        }

        if (desc->set != VK_NULL_HANDLE)
        {
            // Update Descriptor Set
            {
                Image *image = vk_get_image(vkcontext, assetTypeID);

                DescriptorInfo descInfos[] = {
                    DescriptorInfo(vkcontext->globalUBO.buffer),
                    DescriptorInfo(vkcontext->transformStorageBuffer.buffer),
                    DescriptorInfo(vkcontext->sampler, image->view),
                    DescriptorInfo(vkcontext->materialStorageBuffer.buffer)};

                VkWriteDescriptorSet writes[] = {
                    write_set(desc->set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                              &descInfos[0], 0, 1),
                    write_set(desc->set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                              &descInfos[1], 1, 1),
                    write_set(desc->set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                              &descInfos[2], 2, 1),
                    write_set(desc->set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                              &descInfos[3], 3, 1)};

                vkUpdateDescriptorSets(vkcontext->device, ArraySize(writes), writes, 0, 0);

                // Actually add the descriptor;
                vkcontext->descCount++;
            }
        }
        else
        {
            desc = 0;
            CAKEZ_ASSERT(0, "Failed to Allocate Descriptor Set for AssetTypeID: %d", assetTypeID);
            CAKEZ_ERROR("Failed to Allocate Descriptor Set for AssetTypeID: %d", assetTypeID);
        }
    }
    else
    {
        CAKEZ_ASSERT(0, "Reached Maximum amount of Descriptors");
    }
    return desc;
}

internal Descriptor *vk_get_descriptor(VkContext *vkcontext, AssetTypeID assetTypeID)
{
    Descriptor *desc = 0;

    for (uint32_t i = 0; i < vkcontext->descCount; i++)
    {
        Descriptor *d = &vkcontext->descriptors[i];

        if (d->assetTypeID == assetTypeID)
        {
            desc = d;
            break;
        }
    }

    if (!desc)
    {
        desc = vk_create_descriptor(vkcontext, assetTypeID);
    }

    return desc;
}

internal RenderCommand *vk_add_render_command(VkContext *vkcontext, Descriptor *desc)
{
    CAKEZ_ASSERT(desc, "No Descriptor supplied!");

    RenderCommand *rc = 0;

    if (vkcontext->renderCommandCount < MAX_RENDER_COMMANDS)
    {
        rc = &vkcontext->renderCommands[vkcontext->renderCommandCount++];
        *rc = {};
        rc->desc = desc;
        rc->pushData.transformIdx = vkcontext->transformCount;
    }
    else
    {
        CAKEZ_ERROR(0, "Reached Maximum amount of Render Commands");
        CAKEZ_ASSERT(0, "Reached Maximum amount of Render Commands");
    }

    return rc;
}

internal void vk_add_transform(
    VkContext *vkcontext,
    uint32_t materialIdx,
    AssetTypeID assetTypeID,
    Vec2 pos,
    uint32_t animationIdx = 0)
{
    Texture texture = get_texture(assetTypeID);

    uint32_t cols = texture.size.x / texture.subSize.x;
    uint32_t rows = texture.size.y / texture.subSize.y;

    float uvWidth = 1.0f / (float)cols;
    float uvHeight = 1.0f / (float)rows;

    Transform t = {};
    t.materialIdx = materialIdx;
    t.xPos = pos.x;
    t.yPos = pos.y;
    t.sizeX = texture.subSize.x;
    t.sizeY = texture.subSize.y;
    t.topV = float(animationIdx / cols) * uvHeight;
    t.bottomV = t.topV + uvHeight;
    t.leftU = float(animationIdx % cols) * uvWidth;
    t.rightU = t.leftU + uvWidth;

    vkcontext->transforms[vkcontext->transformCount++] = t;
}

internal void vk_render_text(VkContext *vkcontext, RenderCommand *rc,
                             uint32_t materialIdx, char *text, Vec2 origin)
{
    float originX = origin.x;

    // This assumes that l.text is NULL terminated!
    while (char c = *(text++))
    {
        if (c < 0)
        {
            CAKEZ_ASSERT(0, "Wrong Char!");
            continue;
        }

        if (c == ' ')
        {
            origin.x += 15.0f;
            continue;
        }

        if (c == '\n')
        {
            origin.y += 15.0f;
            origin.x = originX;
            continue;
        }

        vk_add_transform(vkcontext, materialIdx, ASSET_SPRITE_FONT_ATLAS, origin, c);
        rc->instanceCount++;
        origin.x += 15.0f;
    }
}

bool vk_init(VkContext *vkcontext, void *window)
{
    vk_compile_shader("assets/shaders/shader.vert", "assets/shaders/compiled/shader.vert.spv");
    vk_compile_shader("assets/shaders/shader.frag", "assets/shaders/compiled/shader.frag.spv");

    platform_get_window_size(&vkcontext->screenSize.width, &vkcontext->screenSize.height);

    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Pong";
    appInfo.pEngineName = "Ponggine";

    char *extensions[] = {
#ifdef WINDOWS_BUILD
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif LINUX_BUILD
#endif
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        VK_KHR_SURFACE_EXTENSION_NAME};

    char *layers[]{
        "VK_LAYER_KHRONOS_validation"};

    VkInstanceCreateInfo instanceInfo = {};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;
    instanceInfo.ppEnabledExtensionNames = extensions;
    instanceInfo.enabledExtensionCount = ArraySize(extensions);
    instanceInfo.ppEnabledLayerNames = layers;
    instanceInfo.enabledLayerCount = ArraySize(layers);
    VK_CHECK(vkCreateInstance(&instanceInfo, 0, &vkcontext->instance));

    auto vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vkcontext->instance, "vkCreateDebugUtilsMessengerEXT");

    if (vkCreateDebugUtilsMessengerEXT)
    {
        VkDebugUtilsMessengerCreateInfoEXT debugInfo = {};
        debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
        debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        debugInfo.pfnUserCallback = vk_debug_callback;

        vkCreateDebugUtilsMessengerEXT(vkcontext->instance, &debugInfo, 0, &vkcontext->debugMessenger);
    }
    else
    {
        return false;
    }

    // Create Surface
    {
#ifdef WINDOWS_BUILD
        VkWin32SurfaceCreateInfoKHR surfaceInfo = {};
        surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        surfaceInfo.hwnd = (HWND)window;
        surfaceInfo.hinstance = GetModuleHandleA(0);
        VK_CHECK(vkCreateWin32SurfaceKHR(vkcontext->instance, &surfaceInfo, 0, &vkcontext->surface));
#elif LINUX_BUILD
#endif
    }

    // Choose GPU
    {
        vkcontext->graphicsIdx = -1;

        uint32_t gpuCount = 0;
        //TODO: Suballocation from Main Allocation
        VkPhysicalDevice gpus[10];
        VK_CHECK(vkEnumeratePhysicalDevices(vkcontext->instance, &gpuCount, 0));
        VK_CHECK(vkEnumeratePhysicalDevices(vkcontext->instance, &gpuCount, gpus));

        for (uint32_t i = 0; i < gpuCount; i++)
        {
            VkPhysicalDevice gpu = gpus[i];

            uint32_t queueFamilyCount = 0;
            //TODO: Suballocation from Main Allocation
            VkQueueFamilyProperties queueProps[10];
            vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, 0);
            vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, queueProps);

            for (uint32_t j = 0; j < queueFamilyCount; j++)
            {
                if (queueProps[j].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                {
                    VkBool32 surfaceSupport = VK_FALSE;
                    VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(gpu, j, vkcontext->surface, &surfaceSupport));

                    if (surfaceSupport)
                    {
                        vkcontext->graphicsIdx = j;
                        vkcontext->gpu = gpu;
                        break;
                    }
                }
            }
        }

        if (vkcontext->graphicsIdx < 0)
        {
            return false;
        }
    }

    // Logical Device
    {
        float queuePriority = 1.0f;

        VkDeviceQueueCreateInfo queueInfo = {};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = vkcontext->graphicsIdx;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &queuePriority;

        char *extensions[] = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME};

        VkDeviceCreateInfo deviceInfo = {};
        deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceInfo.pQueueCreateInfos = &queueInfo;
        deviceInfo.queueCreateInfoCount = 1;
        deviceInfo.ppEnabledExtensionNames = extensions;
        deviceInfo.enabledExtensionCount = ArraySize(extensions);

        VK_CHECK(vkCreateDevice(vkcontext->gpu, &deviceInfo, 0, &vkcontext->device));

        // Get Graphics Queue
        vkGetDeviceQueue(vkcontext->device, vkcontext->graphicsIdx, 0, &vkcontext->graphicsQueue);
    }

    // Swapchain
    {
        uint32_t formatCount = 0;
        //TODO: Suballocation from Main Memory
        VkSurfaceFormatKHR surfaceFormats[10];
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(vkcontext->gpu, vkcontext->surface, &formatCount, 0));
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(vkcontext->gpu, vkcontext->surface, &formatCount, surfaceFormats));

        for (uint32_t i = 0; i < formatCount; i++)
        {
            VkSurfaceFormatKHR format = surfaceFormats[i];

            if (format.format == VK_FORMAT_B8G8R8A8_SRGB)
            {
                vkcontext->surfaceFormat = format;
                break;
            }
        }

        VkSurfaceCapabilitiesKHR surfaceCaps = {};
        VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkcontext->gpu, vkcontext->surface, &surfaceCaps));
        uint32_t imgCount = surfaceCaps.minImageCount + 1;
        imgCount = imgCount > surfaceCaps.maxImageCount ? imgCount - 1 : imgCount;

        VkSwapchainCreateInfoKHR scInfo = {};
        scInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        scInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        scInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        scInfo.surface = vkcontext->surface;
        scInfo.imageFormat = vkcontext->surfaceFormat.format;
        scInfo.preTransform = surfaceCaps.currentTransform;
        scInfo.imageExtent = surfaceCaps.currentExtent;
        scInfo.minImageCount = imgCount;
        scInfo.imageArrayLayers = 1;

        VK_CHECK(vkCreateSwapchainKHR(vkcontext->device, &scInfo, 0, &vkcontext->swapchain));

        VK_CHECK(vkGetSwapchainImagesKHR(vkcontext->device, vkcontext->swapchain, &vkcontext->scImgCount, 0));
        VK_CHECK(vkGetSwapchainImagesKHR(vkcontext->device, vkcontext->swapchain, &vkcontext->scImgCount, vkcontext->scImages));

        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.format = vkcontext->surfaceFormat.format;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        for (uint32_t i = 0; i < vkcontext->scImgCount; i++)
        {
            viewInfo.image = vkcontext->scImages[i];
            VK_CHECK(vkCreateImageView(vkcontext->device, &viewInfo, 0, &vkcontext->scImgViews[i]));
        }
    }

    // Render Pass
    {
        VkAttachmentDescription colorAttachment = {};
        colorAttachment.format = vkcontext->surfaceFormat.format;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentDescription attachments[] = {
            colorAttachment};

        VkAttachmentReference colorAttachmentRef = {};
        colorAttachmentRef.attachment = 0; // This is an index into the attachments array
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpassDesc = {};
        subpassDesc.colorAttachmentCount = 1;
        subpassDesc.pColorAttachments = &colorAttachmentRef;

        VkRenderPassCreateInfo rpInfo = {};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpInfo.pAttachments = attachments;
        rpInfo.attachmentCount = ArraySize(attachments);
        rpInfo.subpassCount = 1;
        rpInfo.pSubpasses = &subpassDesc;

        VK_CHECK(vkCreateRenderPass(vkcontext->device, &rpInfo, 0, &vkcontext->renderPass));
    }

    // Frame Buffers
    {
        VkFramebufferCreateInfo fbInfo = {};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = vkcontext->renderPass;
        fbInfo.width = vkcontext->screenSize.width;
        fbInfo.height = vkcontext->screenSize.height;
        fbInfo.layers = 1;
        fbInfo.attachmentCount = 1;

        for (uint32_t i = 0; i < vkcontext->scImgCount; i++)
        {
            fbInfo.pAttachments = &vkcontext->scImgViews[i];
            VK_CHECK(vkCreateFramebuffer(vkcontext->device, &fbInfo, 0, &vkcontext->framebuffers[i]));
        }
    }

    // Command Pool
    {
        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = vkcontext->graphicsIdx;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        VK_CHECK(vkCreateCommandPool(vkcontext->device, &poolInfo, 0, &vkcontext->commandPool));
    }

    // Command Buffer
    {
        VkCommandBufferAllocateInfo allocInfo = cmd_alloc_info(vkcontext->commandPool);
        VK_CHECK(vkAllocateCommandBuffers(vkcontext->device, &allocInfo, &vkcontext->cmd));
    }

    // Sync Objects
    {
        VkSemaphoreCreateInfo semaInfo = {};
        semaInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VK_CHECK(vkCreateSemaphore(vkcontext->device, &semaInfo, 0, &vkcontext->aquireSemaphore));
        VK_CHECK(vkCreateSemaphore(vkcontext->device, &semaInfo, 0, &vkcontext->submitSemaphore));

        VkFenceCreateInfo fenceInfo = fence_info(VK_FENCE_CREATE_SIGNALED_BIT);
        VK_CHECK(vkCreateFence(vkcontext->device, &fenceInfo, 0, &vkcontext->imgAvailableFence));
    }

    // Create Descriptor Set Layouts
    {
        VkDescriptorSetLayoutBinding layoutBindings[] = {
            layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1, 0),
            layout_binding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1, 1),
            layout_binding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1, 2),
            layout_binding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 1, 3)};

        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = ArraySize(layoutBindings);
        layoutInfo.pBindings = layoutBindings;

        VK_CHECK(vkCreateDescriptorSetLayout(vkcontext->device, &layoutInfo, 0, &vkcontext->setLayout));
    }

    // Create Pipeline Layout
    {
        VkPushConstantRange pushConstant = {};
        pushConstant.size = sizeof(PushData);
        pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkPipelineLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &vkcontext->setLayout;
        layoutInfo.pPushConstantRanges = &pushConstant;
        layoutInfo.pushConstantRangeCount = 1;
        VK_CHECK(vkCreatePipelineLayout(vkcontext->device, &layoutInfo, 0, &vkcontext->pipeLayout));
    }

    // Create a Pipeline
    {

        VkPipelineVertexInputStateCreateInfo vertexInputState = {};
        vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlendState = {};
        colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlendState.pAttachments = &colorBlendAttachment;
        colorBlendState.attachmentCount = 1;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkViewport viewport = {};
        viewport.maxDepth = 1.0;

        VkRect2D scissor = {};

        VkPipelineViewportStateCreateInfo viewportState = {};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.pViewports = &viewport;
        viewportState.viewportCount = 1;
        viewportState.pScissors = &scissor;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizationState = {};
        rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizationState.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisampleState = {};
        multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkShaderModule vertexShader, fragmentShader;

        // Vertex Shader
        {
            uint32_t lengthInBytes;
            uint32_t *vertexCode = (uint32_t *)platform_read_file("assets/shaders/compiled/shader.vert.spv", &lengthInBytes);

            VkShaderModuleCreateInfo shaderInfo = {};
            shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            shaderInfo.pCode = vertexCode;
            shaderInfo.codeSize = lengthInBytes;
            VK_CHECK(vkCreateShaderModule(vkcontext->device, &shaderInfo, 0, &vertexShader));

            delete vertexCode;
        }

        // Fragment Shader
        {
            uint32_t lengthInBytes;
            uint32_t *fragmentCode = (uint32_t *)platform_read_file("assets/shaders/compiled/shader.frag.spv", &lengthInBytes);

            VkShaderModuleCreateInfo shaderInfo = {};
            shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            shaderInfo.pCode = fragmentCode;
            shaderInfo.codeSize = lengthInBytes;
            VK_CHECK(vkCreateShaderModule(vkcontext->device, &shaderInfo, 0, &fragmentShader));

            delete fragmentCode;
        }

        VkPipelineShaderStageCreateInfo vertStage = {};
        vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertStage.pName = "main";
        vertStage.module = vertexShader;

        VkPipelineShaderStageCreateInfo fragStage = {};
        fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragStage.pName = "main";
        fragStage.module = fragmentShader;

        VkPipelineShaderStageCreateInfo shaderStages[2]{
            vertStage,
            fragStage};

        VkDynamicState dynamicStates[]{
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR};

        VkPipelineDynamicStateCreateInfo dynamicState = {};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.pDynamicStates = dynamicStates;
        dynamicState.dynamicStateCount = ArraySize(dynamicStates);

        VkGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.renderPass = vkcontext->renderPass;
        pipelineInfo.pVertexInputState = &vertexInputState;
        pipelineInfo.pColorBlendState = &colorBlendState;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizationState;
        pipelineInfo.pMultisampleState = &multisampleState;
        pipelineInfo.stageCount = ArraySize(shaderStages);
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = vkcontext->pipeLayout;
        pipelineInfo.pStages = shaderStages;

        VK_CHECK(vkCreateGraphicsPipelines(vkcontext->device, 0, 1, &pipelineInfo, 0, &vkcontext->pipeline));

        vkDestroyShaderModule(vkcontext->device, vertexShader, 0);
        vkDestroyShaderModule(vkcontext->device, fragmentShader, 0);
    }

    // Staging Buffer
    {
        vkcontext->stagingBuffer = vk_allocate_buffer(vkcontext->device, vkcontext->gpu,
                                                      MB(1), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    }

    // Create Image
    {
        Image *image = vk_create_image(vkcontext, ASSET_SPRITE_WHITE);

        if (!image->memory || !image->image || !image->view)
        {
            CAKEZ_ASSERT(0, "Failed to allocate White Texure");
            CAKEZ_FATAL("Failed to allocate White Texure");
            return false;
        }
    }

    // Create Sampler
    {
        VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.magFilter = VK_FILTER_NEAREST;

        VK_CHECK(vkCreateSampler(vkcontext->device, &samplerInfo, 0, &vkcontext->sampler));
    }

    // Create Transform Storage Buffer
    {
        vkcontext->transformStorageBuffer = vk_allocate_buffer(
            vkcontext->device,
            vkcontext->gpu,
            sizeof(Transform) * MAX_TRANSFORMS,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    }

    // Create Material Storage Buffer
    {
        vkcontext->materialStorageBuffer = vk_allocate_buffer(
            vkcontext->device,
            vkcontext->gpu,
            sizeof(MaterialData) * MAX_MATERIALS,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    }

    // Create Global Uniform Buffer Object
    {
        vkcontext->globalUBO = vk_allocate_buffer(
            vkcontext->device,
            vkcontext->gpu,
            sizeof(GlobalData),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

        GlobalData globalData = {
            (int)vkcontext->screenSize.width,
            (int)vkcontext->screenSize.height};

        vk_copy_to_buffer(&vkcontext->globalUBO, &globalData, sizeof(globalData));
    }

    // Create Index Buffer
    {
        vkcontext->indexBuffer = vk_allocate_buffer(
            vkcontext->device,
            vkcontext->gpu,
            sizeof(uint32_t) * 6,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        // Copy Indices to the buffer
        {
            uint32_t indices[] = {0, 1, 2, 2, 3, 0};
            vk_copy_to_buffer(&vkcontext->indexBuffer, &indices, sizeof(uint32_t) * 6);
        }
    }

    // Create Descriptor Pool
    {
        VkDescriptorPoolSize poolSizes[] = {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}};

        VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.maxSets = MAX_DESCRIPTORS;
        poolInfo.poolSizeCount = ArraySize(poolSizes);
        poolInfo.pPoolSizes = poolSizes;
        VK_CHECK(vkCreateDescriptorPool(vkcontext->device, &poolInfo, 0, &vkcontext->descPool));
    }

    // Create Descriptor Set

    return true;
}

bool vk_render(VkContext *vkcontext, GameState *gameState, UIState *ui)
{
    uint32_t imgIdx;

    // We wait on the GPU to be done with the work
    VK_CHECK(vkWaitForFences(vkcontext->device, 1, &vkcontext->imgAvailableFence, VK_TRUE, UINT64_MAX));
    VK_CHECK(vkResetFences(vkcontext->device, 1, &vkcontext->imgAvailableFence));

    // Entity Rendering
    {
        for (uint32_t i = 0; i < gameState->entityCount; i++)
        {
            Entity *e = &gameState->entities[i];

            Material *m = get_material(gameState, e->materialIdx);
            Descriptor *desc = vk_get_descriptor(vkcontext, m->assetTypeID);
            if (desc)
            {
                RenderCommand *rc = vk_add_render_command(vkcontext, desc);
                if (rc)
                {
                    rc->instanceCount = 1;
                }
            }

            vk_add_transform(vkcontext, e->materialIdx, m->assetTypeID, e->origin + e->spriteOffset);
        }
    }

    // UI Rendering
    {
        if (ui->labelCount)
        {
            Descriptor *desc = vk_get_descriptor(vkcontext, ASSET_SPRITE_FONT_ATLAS);
            if (desc)
            {
                RenderCommand *rc = vk_add_render_command(vkcontext, desc);

                if (rc)
                {
                    for (uint32_t labelIdx = 0; labelIdx < ui->labelCount; labelIdx++)
                    {
                        Label l = ui->labels[labelIdx];
                        uint32_t materialIdx = get_material(gameState, ASSET_SPRITE_FONT_ATLAS);

                        vk_render_text(vkcontext, rc, materialIdx, l.text, l.pos);
                    }
                }
            }
        }
    }

    // Copy Data to buffers
    {
        vk_copy_to_buffer(&vkcontext->transformStorageBuffer, &vkcontext->transforms, sizeof(Transform) * vkcontext->transformCount);
        vkcontext->transformCount = 0;

        MaterialData materialData[MAX_MATERIALS];
        for (uint32_t i = 0; i < gameState->materialCount; i++)
        {
            materialData[i] = gameState->materials[i].materialData;
        }

        vk_copy_to_buffer(&vkcontext->materialStorageBuffer, materialData, sizeof(MaterialData) * gameState->materialCount);
    }

    // This waits on the timeout until the image is ready, if timeout reached -> VK_TIMEOUT
    VK_CHECK(vkAcquireNextImageKHR(vkcontext->device, vkcontext->swapchain, UINT64_MAX, vkcontext->aquireSemaphore, 0, &imgIdx));

    VkCommandBuffer cmd = vkcontext->cmd;
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo = cmd_begin_info();
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    // Clear Color to Yellow
    VkClearValue clearValue = {};
    clearValue.color = {0, 0, 0, 1};

    VkRenderPassBeginInfo rpBeginInfo = {};
    rpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBeginInfo.renderArea.extent = vkcontext->screenSize;
    rpBeginInfo.clearValueCount = 1;
    rpBeginInfo.pClearValues = &clearValue;
    rpBeginInfo.renderPass = vkcontext->renderPass;
    rpBeginInfo.framebuffer = vkcontext->framebuffers[imgIdx];
    vkCmdBeginRenderPass(cmd, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport = {};
    viewport.maxDepth = 1.0f;
    viewport.width = vkcontext->screenSize.width;
    viewport.height = vkcontext->screenSize.height;

    VkRect2D scissor = {};
    scissor.extent = vkcontext->screenSize;

    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindIndexBuffer(cmd, vkcontext->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vkcontext->pipeline);

    // Render Loop
    {
        for (uint32_t i = 0; i < vkcontext->renderCommandCount; i++)
        {
            RenderCommand *rc = &vkcontext->renderCommands[i];

            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vkcontext->pipeLayout,
                                    0, 1, &rc->desc->set, 0, 0);

            vkCmdPushConstants(cmd, vkcontext->pipeLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushData), &rc->pushData);

            vkCmdDrawIndexed(cmd, 6, rc->instanceCount, 0, 0, 0);
        }

        // Reset the Render Commands for next Frame
        vkcontext->renderCommandCount = 0;
    }

    vkCmdEndRenderPass(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    // This call will signal the Fence when the GPU Work is done
    VkSubmitInfo submitInfo = submit_info(&cmd);
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.pSignalSemaphores = &vkcontext->submitSemaphore;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &vkcontext->aquireSemaphore;
    submitInfo.waitSemaphoreCount = 1;
    VK_CHECK(vkQueueSubmit(vkcontext->graphicsQueue, 1, &submitInfo, vkcontext->imgAvailableFence));

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pSwapchains = &vkcontext->swapchain;
    presentInfo.swapchainCount = 1;
    presentInfo.pImageIndices = &imgIdx;
    presentInfo.pWaitSemaphores = &vkcontext->submitSemaphore;
    presentInfo.waitSemaphoreCount = 1;
    vkQueuePresentKHR(vkcontext->graphicsQueue, &presentInfo);

    return true;
}