#pragma once

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>
#include <vector>

namespace vkapp
{
    class VulkanInstance;
    class VulkanDevice;

    class SwapchainPipeline
    {
    public:
        SwapchainPipeline() = default;
        ~SwapchainPipeline() = default;

        SwapchainPipeline(const SwapchainPipeline&) = delete;
        SwapchainPipeline& operator=(const SwapchainPipeline&) = delete;

        // Now explicitly takes the window
        void initialize(GLFWwindow* window, VulkanInstance& instance, VulkanDevice& device);
        void cleanup();
        void recreateSwapchain(VulkanInstance& instance, VulkanDevice& device);

        // Accessors
        vk::raii::SwapchainKHR&       getSwapchain()       { return m_swapchain; }
        const vk::raii::SwapchainKHR& getSwapchain() const { return m_swapchain; }

        const std::vector<vk::Image>&           getSwapchainImages() const { return m_swapchainImages; }
        const std::vector<vk::raii::ImageView>& getImageViews()      const { return m_imageViews; }

        vk::Extent2D getExtent() const { return m_extent; }
        vk::Format   getFormat() const { return m_surfaceFormat.format; }

        vk::raii::PipelineLayout&      getPipelineLayout()      { return m_pipelineLayout; }
        vk::raii::Pipeline&            getPipeline()            { return m_graphicsPipeline; }
        vk::raii::DescriptorSetLayout& getDescriptorSetLayout() { return m_descriptorSetLayout; }

    private:
        // internal helpers
        void createSwapchain(VulkanInstance& instance, VulkanDevice& device);
        void createImageViews(VulkanDevice& device);
        void createDescriptorSetLayout(VulkanDevice& device);
        void createGraphicsPipeline(VulkanDevice& device);

        static uint32_t          chooseMinImageCount(const vk::SurfaceCapabilitiesKHR& caps);
        static vk::SurfaceFormatKHR chooseSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& formats);
        static vk::PresentModeKHR   choosePresentMode(const std::vector<vk::PresentModeKHR>& modes);
        vk::Extent2D                chooseExtent(const vk::SurfaceCapabilitiesKHR& caps, GLFWwindow* window);

    private:
        GLFWwindow*                 m_window = nullptr;

        vk::raii::SwapchainKHR      m_swapchain = nullptr;
        std::vector<vk::Image>      m_swapchainImages;
        std::vector<vk::raii::ImageView> m_imageViews;

        vk::SurfaceFormatKHR        m_surfaceFormat{};
        vk::Extent2D                m_extent{};

        vk::raii::DescriptorSetLayout m_descriptorSetLayout = nullptr;
        vk::raii::PipelineLayout      m_pipelineLayout      = nullptr;
        vk::raii::Pipeline            m_graphicsPipeline    = nullptr;
    };
}
