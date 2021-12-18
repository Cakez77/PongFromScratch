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

    // TODO: Will be inside an array
    Image image;

    Buffer stagingBuffer;
    Buffer transformStorageBuffer;
    Buffer globalUBO;
    Buffer indexBuffer;

    VkDescriptorPool descPool;

    // TODO: We will abstract this later
    VkSampler sampler;
    VkDescriptorSet descSet;
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
            layout_binding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1, 2)};

        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = ArraySize(layoutBindings);
        layoutInfo.pBindings = layoutBindings;

        VK_CHECK(vkCreateDescriptorSetLayout(vkcontext->device, &layoutInfo, 0, &vkcontext->setLayout));
    }

    // Create Pipeline Layout
    {
        VkPipelineLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &vkcontext->setLayout;
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
        uint32_t fileSize;
        DDSFile *file = (DDSFile *)platform_read_file("assets/textures/cakez.DDS", &fileSize);
        uint32_t textureSize = file->header.Width * file->header.Height * 4;

        vk_copy_to_buffer(&vkcontext->stagingBuffer, &file->dataBegin, textureSize);

        //TODO: Assertions
        vkcontext->image = vk_allocate_image(vkcontext->device,
                                             vkcontext->gpu,
                                             file->header.Width,
                                             file->header.Height,
                                             VK_FORMAT_R8G8B8A8_UNORM);

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
        imgMemBarrier.image = vkcontext->image.image;
        imgMemBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imgMemBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imgMemBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        imgMemBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imgMemBarrier.subresourceRange = range;

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, 0, 0, 0,
                             1, &imgMemBarrier);

        VkBufferImageCopy copyRegion = {};
        copyRegion.imageExtent = {file->header.Width, file->header.Height, 1};
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vkCmdCopyBufferToImage(cmd, vkcontext->stagingBuffer.buffer, vkcontext->image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

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

        VK_CHECK(vkWaitForFences(vkcontext->device, 1, &uploadFence, true, UINT64_MAX));
    }

    // Create Image View
    {
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = vkcontext->image.image;
        viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.layerCount = 1;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

        VK_CHECK(vkCreateImageView(vkcontext->device, &viewInfo, 0, &vkcontext->image.view));
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
            sizeof(Transform) * MAX_ENTITIES,
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
        poolInfo.maxSets = 1;
        poolInfo.poolSizeCount = ArraySize(poolSizes);
        poolInfo.pPoolSizes = poolSizes;
        VK_CHECK(vkCreateDescriptorPool(vkcontext->device, &poolInfo, 0, &vkcontext->descPool));
    }

    // Create Descriptor Set
    {
        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.pSetLayouts = &vkcontext->setLayout;
        allocInfo.descriptorSetCount = 1;
        allocInfo.descriptorPool = vkcontext->descPool;
        VK_CHECK(vkAllocateDescriptorSets(vkcontext->device, &allocInfo, &vkcontext->descSet));
    }

    // Update Descriptor Set
    {
        DescriptorInfo descInfos[] = {
            DescriptorInfo(vkcontext->globalUBO.buffer),
            DescriptorInfo(vkcontext->transformStorageBuffer.buffer),
            DescriptorInfo(vkcontext->sampler, vkcontext->image.view)};

        VkWriteDescriptorSet writes[] = {
            write_set(vkcontext->descSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                      &descInfos[0], 0, 1),
            write_set(vkcontext->descSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                      &descInfos[1], 1, 1),
            write_set(vkcontext->descSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                      &descInfos[2], 2, 1)};

        vkUpdateDescriptorSets(vkcontext->device, ArraySize(writes), writes, 0, 0);
    }

    return true;
}

bool vk_render(VkContext *vkcontext, GameState *gameState)
{
    uint32_t imgIdx;

    // We wait on the GPU to be done with the work
    VK_CHECK(vkWaitForFences(vkcontext->device, 1, &vkcontext->imgAvailableFence, VK_TRUE, UINT64_MAX));
    VK_CHECK(vkResetFences(vkcontext->device, 1, &vkcontext->imgAvailableFence));

    // Copy transforms to the buffer
    {
        vk_copy_to_buffer(&vkcontext->transformStorageBuffer, &gameState->entities, sizeof(Transform) * gameState->entityCount);
    }

    // This waits on the timeout until the image is ready, if timeout reached -> VK_TIMEOUT
    VK_CHECK(vkAcquireNextImageKHR(vkcontext->device, vkcontext->swapchain, UINT64_MAX, vkcontext->aquireSemaphore, 0, &imgIdx));

    VkCommandBuffer cmd = vkcontext->cmd;
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo = cmd_begin_info();
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    // Clear Color to Yellow
    VkClearValue clearValue = {};
    clearValue.color = {1, 1, 0, 1};

    VkRenderPassBeginInfo rpBeginInfo = {};
    rpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBeginInfo.renderArea.extent = vkcontext->screenSize;
    rpBeginInfo.clearValueCount = 1;
    rpBeginInfo.pClearValues = &clearValue;
    rpBeginInfo.renderPass = vkcontext->renderPass;
    rpBeginInfo.framebuffer = vkcontext->framebuffers[imgIdx];
    vkCmdBeginRenderPass(cmd, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Rendering Commands
    {
        VkViewport viewport = {};
        viewport.maxDepth = 1.0f;
        viewport.width = vkcontext->screenSize.width;
        viewport.height = vkcontext->screenSize.height;

        VkRect2D scissor = {};
        scissor.extent = vkcontext->screenSize;

        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vkcontext->pipeLayout,
                                0, 1, &vkcontext->descSet, 0, 0);

        vkCmdBindIndexBuffer(cmd, vkcontext->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vkcontext->pipeline);
        vkCmdDrawIndexed(cmd, 6, gameState->entityCount, 0, 0, 0);
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