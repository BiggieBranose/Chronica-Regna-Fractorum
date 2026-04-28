#pragma once

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>
#include <string>

namespace vkapp
{
    class VulkanInstance
    {
    public:
        VulkanInstance();
        ~VulkanInstance() = default;

        // Non-copyable, movable if you want later
        VulkanInstance(const VulkanInstance&) = delete;
        VulkanInstance& operator=(const VulkanInstance&) = delete;

        void initialize(GLFWwindow* window);
        void cleanup(); // explicit so Application can control order

        // Accessors
        vk::raii::Instance&       getInstance()       { return m_instance; }
        const vk::raii::Instance& getInstance() const { return m_instance; }

        vk::raii::SurfaceKHR&       getSurface()       { return m_surface; }
        const vk::raii::SurfaceKHR& getSurface() const { return m_surface; }

        vk::raii::Context&       getContext()       { return m_context; }
        const vk::raii::Context& getContext() const { return m_context; }

        bool validationEnabled() const { return m_enableValidationLayers; }

    private:
        // --- internal helpers ---
        std::vector<char const*> getRequiredInstanceExtensions();
        void createInstance();
        void setupDebugMessenger();
        void createSurface(GLFWwindow* window);

        static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
            vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
            vk::DebugUtilsMessageTypeFlagsEXT        type,
            const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
            void* pUserData);

    private:
        // mirrors your original globals, but now encapsulated
        const std::vector<char const*> m_validationLayers = {
            "VK_LAYER_KHRONOS_validation"
        };

#ifdef NDEBUG
        static constexpr bool m_enableValidationLayers = false;
#else
        static constexpr bool m_enableValidationLayers = true;
#endif

        vk::raii::Context                m_context;
        vk::raii::Instance               m_instance       = nullptr;
        vk::raii::DebugUtilsMessengerEXT m_debugMessenger = nullptr;
        vk::raii::SurfaceKHR             m_surface        = nullptr;
    };
} // namespace vkapp
