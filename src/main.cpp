#include <vulkan/vulkan.h>
#include <iostream>

int main()
{
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Pong";
    appInfo.pEngineName = "Ponggine";

    VkInstanceCreateInfo instanceInfo = {};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;

    VkInstance instance;

    VkResult result = vkCreateInstance(&instanceInfo, 0, &instance);
    if(result == VK_SUCCESS)
    {
        std::cout << "Successfully created vulkan instance" << std::endl;
    }


    return 0;
}