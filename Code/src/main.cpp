#include <vulkan/vulkan_core.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

class HelloTriangleApplication {
public:
    // Controls the lifetime of the application
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    GLFWwindow* window;

    /* Vulkan instance represents the connection between
       the application and the Vulkan driver/runtime */
    VkInstance instance;

    void initWindow() {
        glfwInit();

        /* Prevent GLFW from creating an OpenGL context,
           we use Vulkan for rendering instead */
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window = glfwCreateWindow(WIDTH, HEIGHT, "TheSpookiestAmdusWindow", nullptr, nullptr);
    }

    void initVulkan() {
        // Creating an instance of Vulkan
        createInstance();
    }

    void mainLoop() {
        // Polls OS window/input events until the user closes the window
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
        }
    }

    void cleanup() {
        // Destroys the Vulkan instance and releases driver resources
        vkDestroyInstance(instance, nullptr);

        glfwDestroyWindow(window);

        glfwTerminate();
    }

    void createInstance() {

        /* Provides metadata about the application to the Vulkan driver,
           mostly used by debugging tools and validation layers */
        VkApplicationInfo appInfo{};

        /* Vulkan requires every struct to identify its type
           so the driver knows how to interpret the memory */
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Chronica Regna Fractorum";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        // Structure describing how the Vulkan instance should be created
        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

        // Refrence to the application metadata defined above
        createInfo.pApplicationInfo = &appInfo;

        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;

        /* GLFW returns the list of Vulkan extensions required
           to interface Vulkan with the window system (surface creation) */
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        // Tell Vulkan which extensions must be enabled
        createInfo.enabledExtensionCount = glfwExtensionCount;
        createInfo.ppEnabledExtensionNames = glfwExtensions;

        createInfo.enabledLayerCount = 0;

        // Creates the Vulkan instance using the parameters above
        VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to create instance!");
        }
    }
};

int main() {
    HelloTriangleApplication app;

    try {
        app.run();
    } catch (const std::exception& e) {
        // Print any error thrown during initialization
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
