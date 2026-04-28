#include "../../header/vulkan/Instance.hpp"

#include <iostream>
#include <cstring>
#include <stdexcept>

namespace vkapp
{
    VulkanInstance::VulkanInstance()
        : m_context()
    {
    }

    void VulkanInstance::initialize(GLFWwindow* window)
    {
        createInstance();
        setupDebugMessenger();
        createSurface(window);
    }

    void VulkanInstance::cleanup()
    {
        // Surface must be destroyed before instance
        m_surface = nullptr;

        if (m_enableValidationLayers)
        {
            m_debugMessenger = nullptr;
        }

        m_instance = nullptr;
        // m_context is RAII and will clean itself up
    }

    std::vector<char const*> VulkanInstance::getRequiredInstanceExtensions()
    {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<char const*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
        if (m_enableValidationLayers)
        {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        return extensions;
    }

    void VulkanInstance::createInstance()
    {
        vk::ApplicationInfo appInfo{};
        appInfo.pApplicationName   = "CRF";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName        = "BranoseEngine";
        appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion         = vk::ApiVersion14;

        std::vector<char const*> requiredLayers;
        if (m_enableValidationLayers)
        {
            requiredLayers.assign(m_validationLayers.begin(), m_validationLayers.end());
        }

        auto layerProperties = m_context.enumerateInstanceLayerProperties();
        for (auto const* requiredLayer : requiredLayers)
        {
            bool found = false;
            for (auto const& layerProperty : layerProperties)
            {
                if (std::strcmp(layerProperty.layerName, requiredLayer) == 0)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                throw std::runtime_error("Required layer not supported: " + std::string(requiredLayer));
            }
        }

        auto requiredExtensions  = getRequiredInstanceExtensions();
        auto extensionProperties = m_context.enumerateInstanceExtensionProperties();
        for (auto const* requiredExtension : requiredExtensions)
        {
            bool found = false;
            for (auto const& extensionProperty : extensionProperties)
            {
                if (std::strcmp(extensionProperty.extensionName, requiredExtension) == 0)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                throw std::runtime_error("Required extension not supported: " + std::string(requiredExtension));
            }
        }

        vk::InstanceCreateInfo createInfo{};
        createInfo.pApplicationInfo        = &appInfo;
        createInfo.enabledLayerCount       = static_cast<uint32_t>(requiredLayers.size());
        createInfo.ppEnabledLayerNames     = requiredLayers.data();
        createInfo.enabledExtensionCount   = static_cast<uint32_t>(requiredExtensions.size());
        createInfo.ppEnabledExtensionNames = requiredExtensions.data();

        m_instance = vk::raii::Instance(m_context, createInfo);
    }

    VKAPI_ATTR vk::Bool32 VKAPI_CALL VulkanInstance::debugCallback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
        vk::DebugUtilsMessageTypeFlagsEXT,
        const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void*)
    {
        if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError ||
            severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
        {
            std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
        }
        return vk::False;
    }

    void VulkanInstance::setupDebugMessenger()
    {
        if (!m_enableValidationLayers)
            return;

        vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);

        vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
            vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
            vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);

        vk::DebugUtilsMessengerCreateInfoEXT createInfo{};
        createInfo.messageSeverity = severityFlags;
        createInfo.messageType     = messageTypeFlags;
        createInfo.pfnUserCallback = &VulkanInstance::debugCallback;

        m_debugMessenger = m_instance.createDebugUtilsMessengerEXT(createInfo);
    }

    void VulkanInstance::createSurface(GLFWwindow* window)
    {
        VkSurfaceKHR rawSurface;
        if (glfwCreateWindowSurface(*m_instance, window, nullptr, &rawSurface) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create window surface!");
        }
        m_surface = vk::raii::SurfaceKHR(m_instance, rawSurface);
    }

} // namespace vkapp
