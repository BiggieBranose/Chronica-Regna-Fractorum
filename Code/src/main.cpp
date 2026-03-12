#include <vulkan/vulkan.h>
#include <iostream>

int main() {
    VkInstance instance;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        std::cout << "Failed to create instance\n";
        return -1;
    }

    std::cout << "Vulkan initialized!\n";

    vkDestroyInstance(instance, nullptr);
}