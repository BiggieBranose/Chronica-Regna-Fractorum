#pragma once

#include "../../header/vulkan/TextureMapping.hpp"
#include <GLFW/glfw3.h>

constexpr uint32_t WIDTH  = 800;
constexpr uint32_t HEIGHT = 600;
constexpr int MAX_FRAMES_IN_FLIGHT = 2;

namespace vkapp
{
    class VulkanInstance;
    class VulkanDevice;
    class SwapchainPipeline;
    class Buffers;
    class Commands;

    class Application
    {
    public:
        Application();
        ~Application();

        void run();

    private:
        void initWindow();
        void initVulkan();
        void mainLoop();
        void cleanup();

    private:
        GLFWwindow* m_window = nullptr;
        bool m_framebufferResized = false;
        static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

        VulkanInstance*     m_instance  = nullptr;
        VulkanDevice*       m_device    = nullptr;
        SwapchainPipeline*  m_pipeline  = nullptr;
        Buffers*            m_buffers   = nullptr;
        Commands*           m_commands  = nullptr;
        TexMap*             m_texmap    = nullptr;
    };
}
