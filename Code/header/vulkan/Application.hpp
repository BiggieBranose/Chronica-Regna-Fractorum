#pragma once

#include <GLFW/glfw3.h>

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
    };
}
