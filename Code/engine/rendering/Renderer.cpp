#include "Renderer.hpp"
#include "Camera.hpp"
#include "Texture.hpp"
#include "SpriteRenderer.hpp"
#include "../core/Log.hpp"
#include "../core/Assert.hpp"
#define VMA_IMPLEMENTATION
#include "../../external/VMA/vk_mem_alloc.h"
#include <stdexcept>
#include <cstring>
#include <set>
#include <algorithm>
#include <limits>

namespace crf {

static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
    vk::DebugUtilsMessageTypeFlagsEXT,
    const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void*)
{
    if (severity >= vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
        Log::warn("Vulkan: {}", pCallbackData->pMessage);
    else
        Log::debug("Vulkan: {}", pCallbackData->pMessage);
    return vk::False;
}

Renderer::Renderer() {}

Renderer::~Renderer() { shutdown(); }

void Renderer::framebufferResizeCallback(GLFWwindow* window, int, int) {
    auto app = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
    if (app) app->m_framebufferResized = true;
}

bool Renderer::initialize(GLFWwindow* window) {
    m_window = window;
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);

    if (!initInstanceAndSurface()) return false;
    if (!initDevice()) return false;
    if (!initSwapchain()) return false;
    if (!initCommands()) return false;
    if (!initSpriteRenderer()) return false;

    {
        vk::DescriptorPoolSize poolSizes[] = {
            {vk::DescriptorType::eSampler, 100},
            {vk::DescriptorType::eCombinedImageSampler, 100},
            {vk::DescriptorType::eSampledImage, 100},
            {vk::DescriptorType::eStorageImage, 100},
            {vk::DescriptorType::eUniformTexelBuffer, 100},
            {vk::DescriptorType::eStorageTexelBuffer, 100},
            {vk::DescriptorType::eUniformBuffer, 100},
            {vk::DescriptorType::eStorageBuffer, 100},
            {vk::DescriptorType::eUniformBufferDynamic, 100},
            {vk::DescriptorType::eStorageBufferDynamic, 100},
            {vk::DescriptorType::eInputAttachment, 100},
        };
        vk::DescriptorPoolCreateInfo poolInfo{};
        poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
        poolInfo.maxSets = 100 * (sizeof(poolSizes) / sizeof(poolSizes[0]));
        poolInfo.poolSizeCount = static_cast<uint32_t>(sizeof(poolSizes) / sizeof(poolSizes[0]));
        poolInfo.pPoolSizes = poolSizes;
        m_imguiDescriptorPool = vk::raii::DescriptorPool(m_device, poolInfo);
    }

    Log::info("Renderer initialized successfully");
    return true;
}

bool Renderer::initInstanceAndSurface() {
    vk::ApplicationInfo appInfo{};
    appInfo.pApplicationName = "CRF Engine";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "ChronicaRegnaFractorum";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = vk::ApiVersion14;

    uint32_t glfwExtCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtCount);
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtCount);
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    const std::vector<const char*> layers = {"VK_LAYER_KHRONOS_validation"};

    vk::DebugUtilsMessengerCreateInfoEXT debugInfo{};
    debugInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
    debugInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                            vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                            vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;
    debugInfo.pfnUserCallback = debugCallback;

    vk::InstanceCreateInfo createInfo{};
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
    createInfo.ppEnabledLayerNames = layers.data();
    createInfo.pNext = &debugInfo;

    try {
        m_instance = vk::raii::Instance(m_context, createInfo);
        m_debugMessenger = m_instance.createDebugUtilsMessengerEXT(debugInfo);
    } catch (const std::exception& e) {
        Log::error("Failed to create Vulkan instance: {}", e.what());
        return false;
    }

    VkSurfaceKHR rawSurface;
    if (glfwCreateWindowSurface(*m_instance, m_window, nullptr, &rawSurface) != VK_SUCCESS) {
        Log::error("Failed to create window surface");
        return false;
    }
    m_surface = vk::raii::SurfaceKHR(m_instance, rawSurface);
    Log::info("Vulkan instance and surface created");
    return true;
}

bool Renderer::initDevice() {
    auto devices = m_instance.enumeratePhysicalDevices();
    for (auto& pd : devices) {
        bool supportsVulkan13 = pd.getProperties().apiVersion >= VK_API_VERSION_1_3;
        auto queueFamilies = pd.getQueueFamilyProperties();
        bool supportsGraphics = false, supportsPresent = false;
        uint32_t index = 0;
        for (auto& qfp : queueFamilies) {
            if (qfp.queueFlags & vk::QueueFlagBits::eGraphics) supportsGraphics = true;
            if (pd.getSurfaceSupportKHR(index, *m_surface)) supportsPresent = true;
            if (supportsGraphics && supportsPresent) break;
            index++;
        }

        auto availableExtensions = pd.enumerateDeviceExtensionProperties();
        bool hasSwapchain = false;
        for (auto& ext : availableExtensions) {
            if (strcmp(ext.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
                hasSwapchain = true; break;
            }
        }

        auto features = pd.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features>();
        bool hasDynamicRendering = features.get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering;
        bool hasSync2 = features.get<vk::PhysicalDeviceVulkan13Features>().synchronization2;

        if (supportsVulkan13 && supportsGraphics && supportsPresent && hasSwapchain &&
            hasDynamicRendering && hasSync2) {
            m_physicalDevice = pd;
            break;
        }
    }

    if (!*m_physicalDevice) {
        Log::error("No suitable GPU found");
        return false;
    }

    auto queueFamilyProps = m_physicalDevice.getQueueFamilyProperties();
    for (uint32_t i = 0; i < queueFamilyProps.size(); i++) {
        if ((queueFamilyProps[i].queueFlags & vk::QueueFlagBits::eGraphics) &&
            m_physicalDevice.getSurfaceSupportKHR(i, *m_surface)) {
            m_queueIndex = i;
            break;
        }
    }

    if (m_queueIndex == ~0u) {
        Log::error("No suitable queue family found");
        return false;
    }

    const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    vk::PhysicalDeviceFeatures2 baseFeatures{};
    vk::PhysicalDeviceVulkan13Features f13{};
    f13.dynamicRendering = VK_TRUE;
    f13.synchronization2 = VK_TRUE;
    baseFeatures.features.samplerAnisotropy = VK_TRUE;
    baseFeatures.pNext = &f13;

    float queuePriority = 1.0f;
    vk::DeviceQueueCreateInfo queueInfo{};
    queueInfo.queueFamilyIndex = m_queueIndex;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &queuePriority;

    vk::DeviceCreateInfo deviceInfo{};
    deviceInfo.pNext = &baseFeatures;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    deviceInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();

    try {
        m_device = vk::raii::Device(m_physicalDevice, deviceInfo);
        m_queue = vk::raii::Queue(m_device, m_queueIndex, 0);
    } catch (const std::exception& e) {
        Log::error("Failed to create logical device: {}", e.what());
        return false;
    }

    VmaVulkanFunctions funcs{};
    funcs.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
    funcs.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocInfo{};
    allocInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    allocInfo.physicalDevice = *m_physicalDevice;
    allocInfo.device = *m_device;
    allocInfo.instance = *m_instance;
    allocInfo.pVulkanFunctions = &funcs;

    VmaAllocator allocator;
    if (vmaCreateAllocator(&allocInfo, &allocator) != VK_SUCCESS) {
        Log::error("Failed to create VMA allocator");
        return false;
    }
    m_vmaAllocator = allocator;

    Log::info("Vulkan device initialized");
    return true;
}

bool Renderer::initSwapchain() {
    auto caps = m_physicalDevice.getSurfaceCapabilitiesKHR(*m_surface);
    auto formats = m_physicalDevice.getSurfaceFormatsKHR(*m_surface);
    auto presentModes = m_physicalDevice.getSurfacePresentModesKHR(*m_surface);

    m_surfaceFormat = formats[0];
    for (auto& f : formats) {
        if (f.format == vk::Format::eB8G8R8A8Srgb && f.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            m_surfaceFormat = f; break;
        }
    }

    vk::PresentModeKHR presentMode = vk::PresentModeKHR::eFifo;
    for (auto pm : presentModes) {
        if (pm == vk::PresentModeKHR::eMailbox) { presentMode = pm; break; }
    }

    int w, h;
    glfwGetFramebufferSize(m_window, &w, &h);
    m_extent.width = std::clamp(static_cast<uint32_t>(w), caps.minImageExtent.width, caps.maxImageExtent.width);
    m_extent.height = std::clamp(static_cast<uint32_t>(h), caps.minImageExtent.height, caps.maxImageExtent.height);

    uint32_t imageCount = std::max(3u, caps.minImageCount);
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
        imageCount = caps.maxImageCount;

    vk::SwapchainCreateInfoKHR info{};
    info.surface = *m_surface;
    info.minImageCount = imageCount;
    info.imageFormat = m_surfaceFormat.format;
    info.imageColorSpace = m_surfaceFormat.colorSpace;
    info.imageExtent = m_extent;
    info.imageArrayLayers = 1;
    info.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
    info.imageSharingMode = vk::SharingMode::eExclusive;
    info.preTransform = caps.currentTransform;
    info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    info.presentMode = presentMode;
    info.clipped = VK_TRUE;

    m_swapchain = vk::raii::SwapchainKHR(m_device, info);
    m_swapchainImages = m_swapchain.getImages();

    m_imageViews.clear();
    for (auto& img : m_swapchainImages) {
        vk::ImageViewCreateInfo view{};
        view.image = img;
        view.viewType = vk::ImageViewType::e2D;
        view.format = m_surfaceFormat.format;
        view.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        view.subresourceRange.baseMipLevel = 0;
        view.subresourceRange.levelCount = 1;
        view.subresourceRange.baseArrayLayer = 0;
        view.subresourceRange.layerCount = 1;
        m_imageViews.emplace_back(m_device, view);
    }

    Log::info("Swapchain created: {}x{}", m_extent.width, m_extent.height);
    return true;
}

bool Renderer::initCommands() {
    vk::CommandPoolCreateInfo poolInfo{};
    poolInfo.queueFamilyIndex = m_queueIndex;
    poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    m_commandPool = vk::raii::CommandPool(m_device, poolInfo);

    vk::CommandBufferAllocateInfo alloc{};
    alloc.commandPool = *m_commandPool;
    alloc.level = vk::CommandBufferLevel::ePrimary;
    alloc.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
    auto bufs = vk::raii::CommandBuffers(m_device, alloc);
    for (auto& cb : bufs)
        m_graphicsCommandBuffers.push_back(std::move(cb));

    createSyncObjects();
    return true;
}

void Renderer::createSyncObjects() {
    vk::SemaphoreCreateInfo semInfo{};
    vk::FenceCreateInfo fenceInfo{};
    fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;

    m_imageAvailableSemaphores.reserve(MAX_FRAMES_IN_FLIGHT);
    m_inFlightFences.reserve(MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        m_imageAvailableSemaphores.emplace_back(m_device, semInfo);
        m_inFlightFences.emplace_back(m_device, fenceInfo);
    }
}

bool Renderer::initSpriteRenderer() {
    m_spriteRenderer = std::make_unique<SpriteRenderer>();
    return m_spriteRenderer->initialize(
        m_device, toVma(m_vmaAllocator),
        m_commandPool, m_queue, m_surfaceFormat.format);
}

bool Renderer::beginFrame() {
    auto& dev = m_device;

    auto fencesResult = dev.waitForFences(*m_inFlightFences[m_frameIndex], VK_TRUE, UINT64_MAX);
    if (fencesResult != vk::Result::eSuccess) return false;

    auto [acquireResult, imageIndex] = m_swapchain.acquireNextImage(
        UINT64_MAX, *m_imageAvailableSemaphores[m_frameIndex], nullptr);

    if (acquireResult == vk::Result::eErrorOutOfDateKHR) {
        onResize();
        return false;
    }
    if (acquireResult != vk::Result::eSuccess && acquireResult != vk::Result::eSuboptimalKHR)
        return false;

    m_currentImageIndex = imageIndex;
    dev.resetFences(*m_inFlightFences[m_frameIndex]);

    auto& cb = m_graphicsCommandBuffers[m_frameIndex];
    cb.reset();

    vk::CommandBufferBeginInfo begin{};
    cb.begin(begin);

    transitionImageLayout(imageIndex,
        vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
        {}, vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        m_swapchainImages[imageIndex]);

    vk::RenderingAttachmentInfo colorAttachment{};
    colorAttachment.imageView = *m_imageViews[imageIndex];
    colorAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    colorAttachment.clearValue = vk::ClearValue(vk::ClearColorValue(std::array<float,4>{0.1f,0.1f,0.15f,1.0f}));

    vk::RenderingInfo renderInfo{};
    renderInfo.renderArea = vk::Rect2D({0,0}, m_extent);
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = &colorAttachment;

    cb.beginRendering(renderInfo);

    vk::Viewport vp{};
    vp.x = 0; vp.y = 0;
    vp.width = (float)m_extent.width;
    vp.height = (float)m_extent.height;
    vp.minDepth = 0; vp.maxDepth = 1;
    cb.setViewport(0, vp);

    vk::Rect2D scissor({0,0}, m_extent);
    cb.setScissor(0, scissor);

    return true;
}

void Renderer::recordCommandBuffer(uint32_t imageIndex) {
    (void)imageIndex;
}

void Renderer::endFrame() {
    auto& dev = m_device;
    auto& cb = m_graphicsCommandBuffers[m_frameIndex];

    cb.endRendering();

    transitionImageLayout(m_currentImageIndex,
        vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR,
        vk::AccessFlagBits2::eColorAttachmentWrite, {},
        vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eBottomOfPipe,
        m_swapchainImages[m_currentImageIndex]);

    cb.end();

    vk::Semaphore waitSem = *m_imageAvailableSemaphores[m_frameIndex];
    vk::Semaphore signalSem;
    vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;

    vk::SubmitInfo submit{};
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &waitSem;
    submit.pWaitDstStageMask = &waitStage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &*cb;
    submit.signalSemaphoreCount = 0;

    m_queue.submit(submit, *m_inFlightFences[m_frameIndex]);

    uint32_t imageIndex = m_currentImageIndex;
    vk::PresentInfoKHR present{};
    present.waitSemaphoreCount = 0;
    present.swapchainCount = 1;
    present.pSwapchains = &*m_swapchain;
    present.pImageIndices = &imageIndex;

    try {
        auto result = m_queue.presentKHR(present);
        if (result == vk::Result::eSuboptimalKHR || result == vk::Result::eErrorOutOfDateKHR)
            m_framebufferResized = true;
    } catch (const vk::OutOfDateKHRError&) {
        m_framebufferResized = true;
    }

    m_frameIndex = (m_frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::transitionImageLayout(uint32_t imageIndex, vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
                                     vk::AccessFlags2 srcAccess, vk::AccessFlags2 dstAccess,
                                     vk::PipelineStageFlags2 srcStage, vk::PipelineStageFlags2 dstStage,
                                     vk::Image image)
{
    auto& cb = m_graphicsCommandBuffers[m_frameIndex];
    vk::ImageMemoryBarrier2 barrier{};
    barrier.srcStageMask = srcStage;
    barrier.srcAccessMask = srcAccess;
    barrier.dstStageMask = dstStage;
    barrier.dstAccessMask = dstAccess;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    vk::DependencyInfo dep{};
    dep.imageMemoryBarrierCount = 1;
    dep.pImageMemoryBarriers = &barrier;
    cb.pipelineBarrier2(dep);
}

void Renderer::onResize() {
    auto& dev = m_device;
    dev.waitIdle();

    m_imageViews.clear();
    m_swapchain = nullptr;

    initSwapchain();

    for (auto& cb : m_graphicsCommandBuffers)
        cb.reset();
}

void Renderer::destroySwapchain() {
    m_imageViews.clear();
    m_swapchain = nullptr;
}

void Renderer::destroyAll() {
    if (*m_device)
        m_device.waitIdle();

    m_spriteRenderer.reset();
    m_imguiDescriptorPool = nullptr;

    m_graphicsCommandBuffers.clear();

    m_inFlightFences.clear();
    m_imageAvailableSemaphores.clear();

    destroySwapchain();
    m_commandPool = nullptr;

    if (m_vmaAllocator) {
        vmaDestroyAllocator(toVma(m_vmaAllocator));
        m_vmaAllocator = nullptr;
    }
    m_queue = nullptr;
    m_device = nullptr;
    m_physicalDevice = nullptr;
    m_surface = nullptr;
    m_debugMessenger = nullptr;
    m_instance = nullptr;
}

void Renderer::shutdown() {
    destroyAll();
    Log::info("Renderer shutdown");
}

} // namespace crf
