#include "../../header/vulkan/Application.hpp"

#include "../../header/vulkan/Instance.hpp"
#include "../../header/vulkan/Device.hpp"
#include "../../header/vulkan/SwapchainPipeline.hpp"
#include "../../header/vulkan/Buffers.hpp"
#include "../../header/vulkan/Commands.hpp"

#include <stdexcept>

namespace vkapp
{
    void Application::framebufferResizeCallback(GLFWwindow* window, int, int)
    {
        auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
        app->m_framebufferResized = true;
    }


    Application::Application()
    {
        initWindow();

        m_instance  = new VulkanInstance();
        m_device    = new VulkanDevice();
        m_pipeline  = new SwapchainPipeline();
        m_buffers   = new Buffers();
        m_commands  = new Commands();
        m_texmap    = new TexMap();
    }

    Application::~Application()
    {
        cleanup();
    }

    void Application::run()
    {
        initVulkan();
        mainLoop();
    }

    // ----------------- WINDOW -----------------

    void Application::initWindow()
    {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        m_window = glfwCreateWindow(800, 600, "Vulkan", nullptr, nullptr);
        glfwSetWindowUserPointer(m_window, this);
        glfwSetFramebufferSizeCallback(m_window, Application::framebufferResizeCallback);
    }

    // ----------------- VULKAN -----------------

    void Application::initVulkan()
    {
        m_instance->initialize(m_window);
        m_device->initialize(*m_instance);
        m_pipeline->initialize(m_window, *m_instance, *m_device);
        m_buffers->initialize(*m_device, *m_pipeline);
        m_commands->initialize(*m_instance, *m_device, *m_pipeline, *m_buffers);
        m_texmap->initialize(*m_device, m_commands->getCommandPool());
        
    }

    // ----------------- MAIN LOOP -----------------

    void Application::mainLoop()
    {
        while (!glfwWindowShouldClose(m_window))
        {
            glfwPollEvents();

            m_commands->drawFrame(*m_instance, *m_device, *m_pipeline, *m_buffers, m_framebufferResized);

            if (m_framebufferResized)
            {
                m_framebufferResized = false;
                recreateSwapChain();
            }
        }

        m_device->getDevice().waitIdle();
    }

    // ----------------- RECREATE SWAP CHAIN -----------------

    void Application::recreateSwapChain()
    {
        int width = 0, height = 0;
        glfwGetFramebufferSize(m_window, &width, &height);
        while (width == 0 || height == 0)
        {
            glfwGetFramebufferSize(m_window, &width, &height);
            glfwWaitEvents();
        }

        m_device->getDevice().waitIdle();

        m_commands->cleanup(*m_device);
        m_buffers->cleanup(*m_device);
        m_pipeline->recreateSwapchain(*m_instance, *m_device);
        m_buffers->initialize(*m_device, *m_pipeline);
        m_commands->initialize(*m_instance, *m_device, *m_pipeline, *m_buffers);
    }

    // ----------------- CLEANUP -----------------

    void Application::cleanup()
    {
        if (m_device)
            m_device->getDevice().waitIdle();

        if (m_commands)
        {
            m_commands->cleanup(*m_device);
            delete m_commands;
        }

        if (m_buffers)
        {
            m_buffers->cleanup(*m_device);
            delete m_buffers;
        }

        if (m_pipeline)
        {
            m_pipeline->cleanup();
            delete m_pipeline;
        }

        if (m_device)
        {
            m_device->cleanup();
            delete m_device;
        }

        if (m_instance)
        {
            m_instance->cleanup();
            delete m_instance;
        }

        glfwDestroyWindow(m_window);
        glfwTerminate();
    }

}
