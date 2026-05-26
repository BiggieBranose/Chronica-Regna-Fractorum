#include "../../header/vulkan/SwapchainPipeline.hpp"
#include "../../header/vulkan/Instance.hpp"
#include "../../header/vulkan/Device.hpp"
#include "../../header/vulkan/Buffers.hpp"

#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <limits>
#include <cassert>

namespace vkapp
{
    // ----------------- PUBLIC -----------------

    void SwapchainPipeline::initialize(GLFWwindow* window, VulkanInstance& instance, VulkanDevice& device)
    {
        m_window = window;

        createSwapchain(instance, device);
        createImageViews(device);
        createDescriptorSetLayout(device);
        createGraphicsPipeline(device);
    }

    void SwapchainPipeline::cleanup()
    {
        m_imageViews.clear();
        m_graphicsPipeline    = nullptr;
        m_pipelineLayout      = nullptr;
        m_descriptorSetLayout = nullptr;
        m_swapchain           = nullptr;
    }

    // ----------------- SWAPCHAIN -----------------

    uint32_t SwapchainPipeline::chooseMinImageCount(const vk::SurfaceCapabilitiesKHR& caps)
    {
        uint32_t count = std::max(3u, caps.minImageCount);
        if (caps.maxImageCount > 0 && caps.maxImageCount < count)
            count = caps.maxImageCount;
        return count;
    }

    vk::SurfaceFormatKHR SwapchainPipeline::chooseSurfaceFormat(
        const std::vector<vk::SurfaceFormatKHR>& formats)
    {
        assert(!formats.empty());
        for (auto const& f : formats)
        {
            if (f.format == vk::Format::eB8G8R8A8Srgb &&
                f.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
            {
                return f;
            }
        }
        return formats[0];
    }

    vk::PresentModeKHR SwapchainPipeline::choosePresentMode(
        const std::vector<vk::PresentModeKHR>& modes)
    {
        bool hasMailbox = false;
        bool hasFifo    = false;

        for (auto pm : modes)
        {
            if (pm == vk::PresentModeKHR::eMailbox) hasMailbox = true;
            if (pm == vk::PresentModeKHR::eFifo)    hasFifo    = true;
        }

        assert(hasFifo);
        return hasMailbox ? vk::PresentModeKHR::eMailbox : vk::PresentModeKHR::eFifo;
    }

    vk::Extent2D SwapchainPipeline::chooseExtent(
        const vk::SurfaceCapabilitiesKHR& caps,
        GLFWwindow* window)
    {
        if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max())
            return caps.currentExtent;

        int w = 0, h = 0;
        glfwGetFramebufferSize(window, &w, &h);

        vk::Extent2D actual{};
        actual.width  = std::clamp<uint32_t>(w, caps.minImageExtent.width,  caps.maxImageExtent.width);
        actual.height = std::clamp<uint32_t>(h, caps.minImageExtent.height, caps.maxImageExtent.height);
        return actual;
    }

    void SwapchainPipeline::createSwapchain(VulkanInstance& instance, VulkanDevice& device)
    {
        auto& pd      = device.getPhysicalDevice();
        auto& surface = instance.getSurface();

        vk::SurfaceCapabilitiesKHR caps = pd.getSurfaceCapabilitiesKHR(*surface);
        m_extent = chooseExtent(caps, m_window);
        uint32_t minImageCount = chooseMinImageCount(caps);

        auto formats = pd.getSurfaceFormatsKHR(*surface);
        m_surfaceFormat = chooseSurfaceFormat(formats);

        auto presentModes = pd.getSurfacePresentModesKHR(*surface);
        vk::PresentModeKHR presentMode = choosePresentMode(presentModes);

        vk::SwapchainCreateInfoKHR info{};
        info.surface          = *surface;
        info.minImageCount    = minImageCount;
        info.imageFormat      = m_surfaceFormat.format;
        info.imageColorSpace  = m_surfaceFormat.colorSpace;
        info.imageExtent      = m_extent;
        info.imageArrayLayers = 1;
        info.imageUsage       = vk::ImageUsageFlagBits::eColorAttachment;
        info.imageSharingMode = vk::SharingMode::eExclusive;
        info.preTransform     = caps.currentTransform;
        info.compositeAlpha   = vk::CompositeAlphaFlagBitsKHR::eOpaque;
        info.presentMode      = presentMode;
        info.clipped          = VK_TRUE;

        m_swapchain       = vk::raii::SwapchainKHR(device.getDevice(), info);
        m_swapchainImages = m_swapchain.getImages();
    }

    void SwapchainPipeline::createImageViews(VulkanDevice& device)
    {
        m_imageViews.clear();
        m_imageViews.reserve(m_swapchainImages.size());

        for (auto& img : m_swapchainImages)
        {
            vk::ImageViewCreateInfo view{};
            view.image    = img;
            view.viewType = vk::ImageViewType::e2D;
            view.format   = m_surfaceFormat.format;
            view.subresourceRange.aspectMask     = vk::ImageAspectFlagBits::eColor;
            view.subresourceRange.baseMipLevel   = 0;
            view.subresourceRange.levelCount     = 1;
            view.subresourceRange.baseArrayLayer = 0;
            view.subresourceRange.layerCount     = 1;

            m_imageViews.emplace_back(device.getDevice(), view);
        }
    }

    // ----------------- DESCRIPTOR SET LAYOUT -----------------

    void SwapchainPipeline::createDescriptorSetLayout(VulkanDevice& device)
    {
        vk::DescriptorSetLayoutBinding ubo{};
        ubo.binding         = 0;
        ubo.descriptorType  = vk::DescriptorType::eUniformBuffer;
        ubo.descriptorCount = 1;
        ubo.stageFlags      = vk::ShaderStageFlagBits::eVertex;

        vk::DescriptorSetLayoutCreateInfo info{};
        info.bindingCount = 1;
        info.pBindings    = &ubo;

        m_descriptorSetLayout = vk::raii::DescriptorSetLayout(device.getDevice(), info);
    }

    // ----------------- PIPELINE -----------------

    static std::vector<char> readFile(const std::string& filename)
    {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file.is_open())
            throw std::runtime_error("failed to open file: " + filename);

        size_t size = (size_t)file.tellg();
        std::vector<char> buffer(size);
        file.seekg(0);
        file.read(buffer.data(), size);
        return buffer;
    }

    void SwapchainPipeline::createGraphicsPipeline(VulkanDevice& device)
    {
        auto code = readFile("shaders/slang.spv");
        vk::ShaderModuleCreateInfo smInfo{};
        smInfo.codeSize = code.size();
        smInfo.pCode    = reinterpret_cast<const uint32_t*>(code.data());

        vk::raii::ShaderModule shader(device.getDevice(), smInfo);

        vk::PipelineShaderStageCreateInfo vert{};
        vert.stage  = vk::ShaderStageFlagBits::eVertex;
        vert.module = *shader;
        vert.pName  = "vertMain";

        vk::PipelineShaderStageCreateInfo frag{};
        frag.stage  = vk::ShaderStageFlagBits::eFragment;
        frag.module = *shader;
        frag.pName  = "fragMain";

        vk::PipelineShaderStageCreateInfo stages[] = { vert, frag };

        // ---- Vertex input: use Vertex helpers ----
        auto binding    = Vertex::getBindingDescription();
        auto attributes = Vertex::getAttributeDescriptions();

        vk::PipelineVertexInputStateCreateInfo vertexInput{};
        vertexInput.vertexBindingDescriptionCount   = 1;
        vertexInput.pVertexBindingDescriptions      = &binding;
        vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
        vertexInput.pVertexAttributeDescriptions    = attributes.data();

        // ---- function states ----

        vk::PipelineInputAssemblyStateCreateInfo inputAsm{};
        inputAsm.topology = vk::PrimitiveTopology::eTriangleList;

        vk::PipelineViewportStateCreateInfo viewport{};
        viewport.viewportCount = 1;
        viewport.scissorCount  = 1;

        vk::PipelineRasterizationStateCreateInfo raster{};
        raster.polygonMode = vk::PolygonMode::eFill;
        raster.cullMode    = vk::CullModeFlagBits::eBack;
        raster.frontFace   = vk::FrontFace::eClockwise;
        raster.lineWidth   = 1.0f;

        vk::PipelineMultisampleStateCreateInfo ms{};
        ms.rasterizationSamples = vk::SampleCountFlagBits::e1;

        vk::PipelineColorBlendAttachmentState blend{};
        blend.colorWriteMask =
            vk::ColorComponentFlagBits::eR |
            vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB |
            vk::ColorComponentFlagBits::eA;

        vk::PipelineColorBlendStateCreateInfo blendState{};
        blendState.attachmentCount = 1;
        blendState.pAttachments    = &blend;

        vk::DynamicState dynStates[] = {
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor
        };

        vk::PipelineDynamicStateCreateInfo dyn{};
        dyn.dynamicStateCount = 2;
        dyn.pDynamicStates    = dynStates;

        vk::PipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts    = &*m_descriptorSetLayout;

        m_pipelineLayout = vk::raii::PipelineLayout(device.getDevice(), layoutInfo);

        vk::GraphicsPipelineCreateInfo pipe{};
        pipe.stageCount          = 2;
        pipe.pStages             = stages;
        pipe.pVertexInputState   = &vertexInput;
        pipe.pInputAssemblyState = &inputAsm;
        pipe.pViewportState      = &viewport;
        pipe.pRasterizationState = &raster;
        pipe.pMultisampleState   = &ms;
        pipe.pColorBlendState    = &blendState;
        pipe.pDynamicState       = &dyn;
        pipe.layout              = *m_pipelineLayout;
        pipe.renderPass          = nullptr;
        pipe.subpass             = 0;

        vk::PipelineRenderingCreateInfo renderInfo{};
        renderInfo.colorAttachmentCount    = 1;
        renderInfo.pColorAttachmentFormats = &m_surfaceFormat.format;

        vk::StructureChain chain(pipe, renderInfo);

        m_graphicsPipeline = vk::raii::Pipeline(
            device.getDevice(),
            nullptr,
            chain.get<vk::GraphicsPipelineCreateInfo>()
        );
    }

}
