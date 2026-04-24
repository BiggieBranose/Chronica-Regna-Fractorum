#include "vulkan/vulkan.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#   include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

constexpr uint32_t WIDTH  = 800;
constexpr uint32_t HEIGHT = 600;

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

const std::vector<char const*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

struct Vertex
{
    glm::vec2 pos;
    glm::vec3 color;

    static vk::VertexInputBindingDescription getBindingDescription()
    {
        vk::VertexInputBindingDescription desc{};
        desc.binding   = 0;
        desc.stride    = sizeof(Vertex);
        desc.inputRate = vk::VertexInputRate::eVertex;
        return desc;
    }

    static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions()
    {
        std::array<vk::VertexInputAttributeDescription, 2> attrs{};

        attrs[0].location = 0;
        attrs[0].binding  = 0;
        attrs[0].format   = vk::Format::eR32G32Sfloat;
        attrs[0].offset   = offsetof(Vertex, pos);

        attrs[1].location = 1;
        attrs[1].binding  = 0;
        attrs[1].format   = vk::Format::eR32G32B32Sfloat;
        attrs[1].offset   = offsetof(Vertex, color);

        return attrs;
    }
};

const std::vector<Vertex> vertices = {
    {{ 0.0f, -20.5f }, { 0.0f, 0.0f, 1.0f }},
    {{ 0.5f,  0.5f }, { 0.0f, 1.0f, 0.0f }},
    {{-0.5f,  0.5f }, { 1.0f, 0.0f, 0.0f }}
};

class HelloTriangleApplication
{
public:
    void run()
    {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    GLFWwindow*                      window = nullptr;
    bool                             framebufferResized = false;

    vk::raii::Context                context;
    vk::raii::Instance               instance       = nullptr;
    vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;
    vk::raii::SurfaceKHR             surface        = nullptr;
    vk::raii::PhysicalDevice         physicalDevice = nullptr;
    vk::raii::Device                 device         = nullptr;
    vk::raii::Queue                  queue          = nullptr;

    vk::raii::SwapchainKHR           swapChain      = nullptr;
    std::vector<vk::Image>           swapChainImages;
    vk::SurfaceFormatKHR             swapChainSurfaceFormat;
    vk::Extent2D                     swapChainExtent;
    std::vector<vk::raii::ImageView> swapChainImageViews;

    vk::raii::PipelineLayout         pipelineLayout = nullptr;
    vk::raii::Pipeline               graphicsPipeline = nullptr;

    vk::raii::CommandPool            commandPool = nullptr;
    std::vector<vk::raii::CommandBuffer> commandBuffers;

    std::vector<vk::raii::Semaphore> imageAvailableSemaphores;   // per frame
    std::vector<vk::raii::Semaphore> renderFinishedSemaphores;   // per swapchain image
    std::vector<vk::raii::Fence>     inFlightFences;             // per frame

    uint32_t                         graphicsQueueFamilyIndex = 0;
    uint32_t                         frameIndex = 0;

    vk::raii::Buffer       vertexBuffer       = nullptr;
    vk::raii::DeviceMemory vertexBufferMemory = nullptr;

    std::vector<const char*> requiredDeviceExtension = {
        vk::KHRSwapchainExtensionName
    };

    // ----------------- INIT -----------------

    void initWindow()
    {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    }

    static void framebufferResizeCallback(GLFWwindow* window, int, int)
    {
        auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
        app->framebufferResized = true;
    }

    void initVulkan()
    {
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createGraphicsPipeline();
        createCommandPool();
        createVertexBuffer();
        createCommandBuffers();
        createSyncObjects();
    }

    void mainLoop()
    {
        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();
            drawFrame();
        }

        device.waitIdle();
    }

    void cleanupSwapChain()
    {
        swapChainImageViews.clear();
        swapChain = nullptr;
    }

    void cleanup()
    {
        cleanupSwapChain();

        vertexBufferMemory = nullptr;
        vertexBuffer       = nullptr;

        commandPool = nullptr;

        device = nullptr;
        surface = nullptr;

        if (enableValidationLayers)
        {
            debugMessenger = nullptr;
        }

        instance = nullptr;

        glfwDestroyWindow(window);
        glfwTerminate();
    }

    // ----------------- INSTANCE / DEBUG -----------------

    void createInstance()
    {
        vk::ApplicationInfo appInfo{};
        appInfo.pApplicationName   = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName        = "No Engine";
        appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion         = vk::ApiVersion14;

        std::vector<char const*> requiredLayers;
        if (enableValidationLayers)
        {
            requiredLayers.assign(validationLayers.begin(), validationLayers.end());
        }

        auto layerProperties = context.enumerateInstanceLayerProperties();
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

        auto requiredExtensions = getRequiredInstanceExtensions();
        auto extensionProperties = context.enumerateInstanceExtensionProperties();
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

        instance = vk::raii::Instance(context, createInfo);
    }

    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
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

    void setupDebugMessenger()
    {
        if (!enableValidationLayers)
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
        createInfo.pfnUserCallback = &debugCallback;

        debugMessenger = instance.createDebugUtilsMessengerEXT(createInfo);
    }

    // ----------------- SURFACE / DEVICE -----------------

    void createSurface()
    {
        VkSurfaceKHR rawSurface;
        if (glfwCreateWindowSurface(*instance, window, nullptr, &rawSurface) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create window surface!");
        }
        surface = vk::raii::SurfaceKHR(instance, rawSurface);
    }

    bool isDeviceSuitable(vk::raii::PhysicalDevice const& pd)
    {
        bool supportsVulkan1_3 = pd.getProperties().apiVersion >= VK_API_VERSION_1_3;

        auto queueFamilies = pd.getQueueFamilyProperties();
        bool supportsGraphics = false;
        bool supportsPresent  = false;
        uint32_t index        = 0;

        for (auto const& qfp : queueFamilies)
        {
            if (qfp.queueFlags & vk::QueueFlagBits::eGraphics)
            {
                supportsGraphics = true;
            }
            if (pd.getSurfaceSupportKHR(index, *surface))
            {
                supportsPresent = true;
            }
            ++index;
        }

        auto availableDeviceExtensions = pd.enumerateDeviceExtensionProperties();
        bool supportsAllRequiredExtensions = true;
        for (auto const* reqExt : requiredDeviceExtension)
        {
            bool found = false;
            for (auto const& availExt : availableDeviceExtensions)
            {
                if (std::strcmp(availExt.extensionName, reqExt) == 0)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                supportsAllRequiredExtensions = false;
                break;
            }
        }

        auto features = pd.getFeatures2<
            vk::PhysicalDeviceFeatures2,
            vk::PhysicalDeviceVulkan11Features,
            vk::PhysicalDeviceVulkan13Features,
            vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();

        bool supportsRequiredFeatures =
            features.get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
            features.get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
            features.get<vk::PhysicalDeviceVulkan13Features>().synchronization2 &&
            features.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

        return supportsVulkan1_3 && supportsGraphics && supportsPresent &&
               supportsAllRequiredExtensions && supportsRequiredFeatures;
    }

    void pickPhysicalDevice()
    {
        std::vector<vk::raii::PhysicalDevice> physicalDevices = instance.enumeratePhysicalDevices();
        for (auto const& pd : physicalDevices)
        {
            if (isDeviceSuitable(pd))
            {
                physicalDevice = pd;
                return;
            }
        }
        throw std::runtime_error("failed to find a suitable GPU!");
    }

    void createLogicalDevice()
    {
        std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

        uint32_t queueIndex = ~0u;
        for (uint32_t i = 0; i < queueFamilyProperties.size(); ++i)
        {
            if ((queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics) &&
                physicalDevice.getSurfaceSupportKHR(i, *surface))
            {
                queueIndex = i;
                break;
            }
        }
        if (queueIndex == ~0u)
        {
            throw std::runtime_error("Could not find a queue for graphics and present");
        }
        graphicsQueueFamilyIndex = queueIndex;

        vk::PhysicalDeviceFeatures2 baseFeatures{};
        vk::PhysicalDeviceVulkan11Features f11{};
        vk::PhysicalDeviceVulkan13Features f13{};
        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT fExt{};

        baseFeatures.pNext = &f11;
        f11.pNext          = &f13;
        f13.pNext          = &fExt;

        f11.shaderDrawParameters  = VK_TRUE;
        f13.dynamicRendering      = VK_TRUE;
        f13.synchronization2      = VK_TRUE;
        fExt.extendedDynamicState = VK_TRUE;

        float queuePriority = 1.0f;
        vk::DeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.queueFamilyIndex = queueIndex;
        queueCreateInfo.queueCount       = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        vk::DeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.pNext                   = &baseFeatures;
        deviceCreateInfo.queueCreateInfoCount    = 1;
        deviceCreateInfo.pQueueCreateInfos       = &queueCreateInfo;
        deviceCreateInfo.enabledExtensionCount   = static_cast<uint32_t>(requiredDeviceExtension.size());
        deviceCreateInfo.ppEnabledExtensionNames = requiredDeviceExtension.data();

        device = vk::raii::Device(physicalDevice, deviceCreateInfo);
        queue  = vk::raii::Queue(device, queueIndex, 0);
    }

    // ----------------- SWAPCHAIN -----------------

    void createSwapChain()
    {
        vk::SurfaceCapabilitiesKHR surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(*surface);
        swapChainExtent = chooseSwapExtent(surfaceCapabilities);
        uint32_t minImageCount = chooseSwapMinImageCount(surfaceCapabilities);

        std::vector<vk::SurfaceFormatKHR> availableFormats = physicalDevice.getSurfaceFormatsKHR(*surface);
        swapChainSurfaceFormat = chooseSwapSurfaceFormat(availableFormats);

        std::vector<vk::PresentModeKHR> availablePresentModes = physicalDevice.getSurfacePresentModesKHR(*surface);
        vk::PresentModeKHR presentMode = chooseSwapPresentMode(availablePresentModes);

        vk::SwapchainCreateInfoKHR createInfo{};
        createInfo.surface          = *surface;
        createInfo.minImageCount    = minImageCount;
        createInfo.imageFormat      = swapChainSurfaceFormat.format;
        createInfo.imageColorSpace  = swapChainSurfaceFormat.colorSpace;
        createInfo.imageExtent      = swapChainExtent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage       = vk::ImageUsageFlagBits::eColorAttachment;
        createInfo.imageSharingMode = vk::SharingMode::eExclusive;
        createInfo.preTransform     = surfaceCapabilities.currentTransform;
        createInfo.compositeAlpha   = vk::CompositeAlphaFlagBitsKHR::eOpaque;
        createInfo.presentMode      = presentMode;
        createInfo.clipped          = VK_TRUE;

        swapChain       = vk::raii::SwapchainKHR(device, createInfo);
        swapChainImages = swapChain.getImages();
    }

    void createImageViews()
    {
        swapChainImageViews.clear();
        swapChainImageViews.reserve(swapChainImages.size());

        for (auto& image : swapChainImages)
        {
            vk::ImageViewCreateInfo viewInfo{};
            viewInfo.image    = image;
            viewInfo.viewType = vk::ImageViewType::e2D;
            viewInfo.format   = swapChainSurfaceFormat.format;
            viewInfo.subresourceRange.aspectMask     = vk::ImageAspectFlagBits::eColor;
            viewInfo.subresourceRange.baseMipLevel   = 0;
            viewInfo.subresourceRange.levelCount     = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount     = 1;

            swapChainImageViews.emplace_back(device, viewInfo);
        }
    }

    static uint32_t chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const& surfaceCapabilities)
    {
        uint32_t minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
        if ((surfaceCapabilities.maxImageCount > 0) && (surfaceCapabilities.maxImageCount < minImageCount))
        {
            minImageCount = surfaceCapabilities.maxImageCount;
        }
        return minImageCount;
    }

    static vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats)
    {
        assert(!availableFormats.empty());
        for (auto const& format : availableFormats)
        {
            if (format.format == vk::Format::eB8G8R8A8Srgb &&
                format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
            {
                return format;
            }
        }
        return availableFormats[0];
    }

    static vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes)
    {
        bool hasMailbox = false;
        bool hasFifo    = false;
        for (auto pm : availablePresentModes)
        {
            if (pm == vk::PresentModeKHR::eMailbox)
                hasMailbox = true;
            if (pm == vk::PresentModeKHR::eFifo)
                hasFifo = true;
        }
        assert(hasFifo);
        return hasMailbox ? vk::PresentModeKHR::eMailbox : vk::PresentModeKHR::eFifo;
    }

    vk::Extent2D chooseSwapExtent(vk::SurfaceCapabilitiesKHR const& capabilities)
    {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            return capabilities.currentExtent;
        }

        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);

        vk::Extent2D actualExtent{};
        actualExtent.width  = std::clamp<uint32_t>(width,  capabilities.minImageExtent.width,  capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        return actualExtent;
    }

    // ----------------- PIPELINE -----------------

    void createGraphicsPipeline()
    {
        auto shaderCode = readFile("shaders/slang.spv");
        vk::raii::ShaderModule shaderModule = createShaderModule(shaderCode);

        vk::PipelineShaderStageCreateInfo vertStage{};
        vertStage.stage  = vk::ShaderStageFlagBits::eVertex;
        vertStage.module = *shaderModule;
        vertStage.pName  = "vertMain";

        vk::PipelineShaderStageCreateInfo fragStage{};
        fragStage.stage  = vk::ShaderStageFlagBits::eFragment;
        fragStage.module = *shaderModule;
        fragStage.pName  = "fragMain";

        vk::PipelineShaderStageCreateInfo shaderStages[] = { vertStage, fragStage };

        auto bindingDescription    = Vertex::getBindingDescription();
        auto attributeDescriptions = Vertex::getAttributeDescriptions();

        vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.vertexBindingDescriptionCount   = 1;
        vertexInputInfo.pVertexBindingDescriptions      = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions    = attributeDescriptions.data();

        vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.topology               = vk::PrimitiveTopology::eTriangleList;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        vk::Viewport viewport{};
        viewport.x        = 0.0f;
        viewport.y        = 0.0f;
        viewport.width    = static_cast<float>(swapChainExtent.width);
        viewport.height   = static_cast<float>(swapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        vk::Rect2D scissor{};
        scissor.offset = vk::Offset2D{0, 0};
        scissor.extent = swapChainExtent;

        vk::PipelineViewportStateCreateInfo viewportState{};
        viewportState.viewportCount = 1;
        viewportState.pViewports    = &viewport;
        viewportState.scissorCount  = 1;
        viewportState.pScissors     = &scissor;

        vk::PipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.depthClampEnable        = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode             = vk::PolygonMode::eFill;
        rasterizer.lineWidth               = 1.0f;
        rasterizer.cullMode                = vk::CullModeFlagBits::eBack;
        rasterizer.frontFace               = vk::FrontFace::eClockwise;
        rasterizer.depthBiasEnable         = VK_FALSE;

        vk::PipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sampleShadingEnable  = VK_FALSE;
        multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

        vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask =
            vk::ColorComponentFlagBits::eR |
            vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB |
            vk::ColorComponentFlagBits::eA;
        colorBlendAttachment.blendEnable = VK_FALSE;

        vk::PipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.logicOpEnable   = VK_FALSE;
        colorBlending.logicOp         = vk::LogicOp::eCopy;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments    = &colorBlendAttachment;

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);

        vk::GraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.stageCount          = 2;
        pipelineInfo.pStages             = shaderStages;
        pipelineInfo.pVertexInputState   = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState      = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState   = &multisampling;
        pipelineInfo.pDepthStencilState  = nullptr;
        pipelineInfo.pColorBlendState    = &colorBlending;
        pipelineInfo.pDynamicState       = nullptr;
        pipelineInfo.layout              = *pipelineLayout;
        pipelineInfo.renderPass          = nullptr;
        pipelineInfo.subpass             = 0;

        vk::PipelineRenderingCreateInfo pipelineRenderingInfo{};
        pipelineRenderingInfo.colorAttachmentCount    = 1;
        pipelineRenderingInfo.pColorAttachmentFormats = &swapChainSurfaceFormat.format;

        vk::StructureChain<
            vk::GraphicsPipelineCreateInfo,
            vk::PipelineRenderingCreateInfo> chain(pipelineInfo, pipelineRenderingInfo);

        graphicsPipeline = vk::raii::Pipeline(
            device,
            nullptr,
            chain.get<vk::GraphicsPipelineCreateInfo>()
        );
    }

    vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const
    {
        vk::ShaderModuleCreateInfo createInfo{};
        createInfo.codeSize = code.size();
        createInfo.pCode    = reinterpret_cast<const uint32_t*>(code.data());
        return vk::raii::ShaderModule(device, createInfo);
    }

    // ----------------- COMMANDS / SYNC -----------------

    void createCommandPool()
    {
        vk::CommandPoolCreateInfo poolInfo{};
        poolInfo.queueFamilyIndex = graphicsQueueFamilyIndex;
        poolInfo.flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

        commandPool = vk::raii::CommandPool(device, poolInfo);
    }

    void createCommandBuffers()
    {
        commandBuffers.clear();
        commandBuffers.reserve(MAX_FRAMES_IN_FLIGHT);

        vk::CommandBufferAllocateInfo allocInfo{};
        allocInfo.commandPool        = *commandPool;
        allocInfo.level              = vk::CommandBufferLevel::ePrimary;
        allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

        auto bufs = vk::raii::CommandBuffers(device, allocInfo);
        for (auto& cb : bufs)
        {
            commandBuffers.emplace_back(std::move(cb));
        }
    }

    void createSyncObjects()
    {
        imageAvailableSemaphores.clear();
        renderFinishedSemaphores.clear();
        inFlightFences.clear();

        imageAvailableSemaphores.reserve(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.reserve(MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores.reserve(swapChainImages.size());

        vk::SemaphoreCreateInfo semaphoreInfo{};
        vk::FenceCreateInfo     fenceInfo{};
        fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            imageAvailableSemaphores.emplace_back(device, semaphoreInfo);
            inFlightFences.emplace_back(device, fenceInfo);
        }

        for (size_t i = 0; i < swapChainImages.size(); ++i)
        {
            renderFinishedSemaphores.emplace_back(device, semaphoreInfo);
        }
    }

    void transition_image_layout(
        vk::CommandBuffer cb,
        vk::Image image,
        vk::ImageLayout oldLayout,
        vk::ImageLayout newLayout,
        vk::AccessFlags2 srcAccessMask,
        vk::AccessFlags2 dstAccessMask,
        vk::PipelineStageFlags2 srcStageMask,
        vk::PipelineStageFlags2 dstStageMask)
    {
        vk::ImageMemoryBarrier2 barrier{};
        barrier.srcStageMask        = srcStageMask;
        barrier.srcAccessMask       = srcAccessMask;
        barrier.dstStageMask        = dstStageMask;
        barrier.dstAccessMask       = dstAccessMask;
        barrier.oldLayout           = oldLayout;
        barrier.newLayout           = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image               = image;
        barrier.subresourceRange.aspectMask     = vk::ImageAspectFlagBits::eColor;
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.subresourceRange.levelCount     = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = 1;

        vk::DependencyInfo depInfo{};
        depInfo.imageMemoryBarrierCount = 1;
        depInfo.pImageMemoryBarriers    = &barrier;

        cb.pipelineBarrier2(depInfo);
    }

    void recordCommandBuffer(vk::CommandBuffer cb, uint32_t imageIndex)
    {
        vk::CommandBufferBeginInfo beginInfo{};
        cb.begin(beginInfo);

        transition_image_layout(
            cb,
            swapChainImages[imageIndex],
            vk::ImageLayout::ePresentSrcKHR,
            vk::ImageLayout::eColorAttachmentOptimal,
            vk::AccessFlagBits2::eNone,
            vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::PipelineStageFlagBits2::eNone,
            vk::PipelineStageFlagBits2::eColorAttachmentOutput
        );

        vk::ClearValue clearColor = vk::ClearColorValue(0.f, 0.f, 0.f, 1.f);

        vk::RenderingAttachmentInfo attachmentInfo{};
        attachmentInfo.imageView   = *swapChainImageViews[imageIndex];
        attachmentInfo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
        attachmentInfo.loadOp      = vk::AttachmentLoadOp::eClear;
        attachmentInfo.storeOp     = vk::AttachmentStoreOp::eStore;
        attachmentInfo.clearValue  = clearColor;

        vk::RenderingInfo renderingInfo{};
        renderingInfo.renderArea.offset = vk::Offset2D{0, 0};
        renderingInfo.renderArea.extent = swapChainExtent;
        renderingInfo.layerCount        = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &attachmentInfo;

        cb.beginRendering(renderingInfo);

        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);

        vk::Viewport viewport(
            0.0f, 0.0f,
            static_cast<float>(swapChainExtent.width),
            static_cast<float>(swapChainExtent.height),
            0.0f, 1.0f
        );
        cb.setViewport(0, viewport);

        vk::Rect2D scissor({0, 0}, swapChainExtent);
        cb.setScissor(0, scissor);

        vk::DeviceSize offsets[] = { 0 };
        vk::Buffer vb = *vertexBuffer;
        cb.bindVertexBuffers(0, 1, &vb, offsets);

        cb.draw(static_cast<uint32_t>(vertices.size()), 1, 0, 0);

        cb.endRendering();

        transition_image_layout(
            cb,
            swapChainImages[imageIndex],
            vk::ImageLayout::eColorAttachmentOptimal,
            vk::ImageLayout::ePresentSrcKHR,
            vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::AccessFlagBits2::eNone,
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::PipelineStageFlagBits2::eNone
        );

        cb.end();
    }

    void drawFrame()
    {
        auto fenceResult = device.waitForFences(*inFlightFences[frameIndex], VK_TRUE, UINT64_MAX);
        if (fenceResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to wait for fence!");
        }

        auto acquire = swapChain.acquireNextImage(UINT64_MAX, *imageAvailableSemaphores[frameIndex], nullptr);
        vk::Result result    = acquire.result;
        uint32_t   imageIndex = acquire.value;

        if (result == vk::Result::eErrorOutOfDateKHR)
        {
            recreateSwapChain();
            return;
        }
        if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
        {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        device.resetFences(*inFlightFences[frameIndex]);

        commandBuffers[frameIndex].reset();
        recordCommandBuffer(*commandBuffers[frameIndex], imageIndex);

        vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;

        vk::SubmitInfo submitInfo{};
        submitInfo.waitSemaphoreCount   = 1;
        submitInfo.pWaitSemaphores      = &*imageAvailableSemaphores[frameIndex];
        submitInfo.pWaitDstStageMask    = &waitStage;

        vk::CommandBuffer rawCB = *commandBuffers[frameIndex];
        submitInfo.commandBufferCount   = 1;
        submitInfo.pCommandBuffers      = &rawCB;

        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores    = &*renderFinishedSemaphores[imageIndex];

        queue.submit(submitInfo, *inFlightFences[frameIndex]);

        vk::PresentInfoKHR presentInfo{};
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores    = &*renderFinishedSemaphores[imageIndex];
        presentInfo.swapchainCount     = 1;
        vk::SwapchainKHR rawSwapchain  = *swapChain;
        presentInfo.pSwapchains        = &rawSwapchain;
        presentInfo.pImageIndices      = &imageIndex;

        result = queue.presentKHR(presentInfo);

        if (result == vk::Result::eSuboptimalKHR ||
            result == vk::Result::eErrorOutOfDateKHR ||
            framebufferResized)
        {
            framebufferResized = false;
            recreateSwapChain();
        }
        else
        {
            if (result != vk::Result::eSuccess)
            {
                throw std::runtime_error("failed to present swap chain image!");
            }
        }

        frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    // ----------------- VERTEX BUFFER -----------------

    uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
    {
        vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
        {
            bool typeSupported = typeFilter & (1u << i);
            bool propsMatch    = (memProperties.memoryTypes[i].propertyFlags & properties) == properties;
            if (typeSupported && propsMatch)
                return i;
        }

        throw std::runtime_error("failed to find suitable memory type!");
    }

    void createVertexBuffer()
    {
        vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

        vk::BufferCreateInfo bufferInfo{};
        bufferInfo.size        = bufferSize;
        bufferInfo.usage       = vk::BufferUsageFlagBits::eVertexBuffer;
        bufferInfo.sharingMode = vk::SharingMode::eExclusive;

        vertexBuffer = vk::raii::Buffer(device, bufferInfo);

        vk::MemoryRequirements memRequirements = vertexBuffer.getMemoryRequirements();

        vk::MemoryAllocateInfo allocInfo{};
        allocInfo.allocationSize  = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(
            memRequirements.memoryTypeBits,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
        );

        vertexBufferMemory = vk::raii::DeviceMemory(device, allocInfo);

        vertexBuffer.bindMemory(*vertexBufferMemory, 0);

        void* data = vertexBufferMemory.mapMemory(0, bufferSize);
        std::memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
        vertexBufferMemory.unmapMemory();
    }

    // ----------------- SWAPCHAIN RECREATION -----------------

    void recreateSwapChain()
    {
        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        while (width == 0 || height == 0)
        {
            glfwGetFramebufferSize(window, &width, &height);
            glfwWaitEvents();
        }

        device.waitIdle();

        cleanupSwapChain();

        createSwapChain();
        createImageViews();
        createGraphicsPipeline();
        createCommandBuffers();
        createSyncObjects();
    }

    // ----------------- UTILS -----------------

    std::vector<const char*> getRequiredInstanceExtensions()
    {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
        if (enableValidationLayers)
        {
            extensions.push_back(vk::EXTDebugUtilsExtensionName);
        }

        return extensions;
    }

    static std::vector<char> readFile(const std::string& filename)
    {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file.is_open())
        {
            throw std::runtime_error("failed to open file!");
        }

        size_t fileSize = static_cast<size_t>(file.tellg());
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);
        return buffer;
    }
};

int main()
{
    try
    {
        HelloTriangleApplication app;
        app.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
