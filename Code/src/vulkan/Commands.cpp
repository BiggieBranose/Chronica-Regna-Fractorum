#include "../../header/vulkan/Commands.hpp"
#include "../../header/vulkan/Instance.hpp"
#include "../../header/vulkan/Device.hpp"
#include "../../header/vulkan/SwapchainPipeline.hpp"
#include "../../header/vulkan/Buffers.hpp"

#include <chrono>
#include <cstring>
#include <stdexcept>

namespace vkapp
{
    // ----------------- PUBLIC -----------------

    void Commands::initialize(
        VulkanInstance& instance,
        VulkanDevice& device,
        SwapchainPipeline& pipeline,
        Buffers& buffers)
    {
        createCommandPool(device);
        createCommandBuffers(device);
        createSyncObjects(device, pipeline.getSwapchainImages().size());
    }

    void Commands::cleanup(VulkanDevice& device)
    {
        m_imageAvailable.clear();
        m_renderFinished.clear();
        m_inFlight.clear();

        m_commandBuffers.clear();
        m_commandPool = nullptr;
    }

    // ----------------- COMMAND POOL -----------------

    void Commands::createCommandPool(VulkanDevice& device)
    {
        vk::CommandPoolCreateInfo info{};
        info.queueFamilyIndex = device.getGraphicsQueueFamilyIndex();
        info.flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

        m_commandPool = vk::raii::CommandPool(device.getDevice(), info);
    }

    // ----------------- COMMAND BUFFERS -----------------

    void Commands::createCommandBuffers(VulkanDevice& device)
    {
        vk::CommandBufferAllocateInfo alloc{};
        alloc.commandPool        = *m_commandPool;
        alloc.level              = vk::CommandBufferLevel::ePrimary;
        alloc.commandBufferCount = 2; // MAX_FRAMES_IN_FLIGHT

        auto bufs = vk::raii::CommandBuffers(device.getDevice(), alloc);

        m_commandBuffers.clear();
        for (auto& cb : bufs)
            m_commandBuffers.emplace_back(std::move(cb));
    }

    // ----------------- SYNC OBJECTS -----------------

    void Commands::createSyncObjects(VulkanDevice& device, uint32_t swapchainImageCount)
    {
        auto& dev = device.getDevice();

        // Reserve capacity (no default construction)
        m_imageAvailable.reserve(2);
        m_inFlight.reserve(2);
        m_renderFinished.reserve(swapchainImageCount);

        vk::SemaphoreCreateInfo semInfo{};
        vk::FenceCreateInfo fenceInfo{};
        fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;

        // Create semaphores for frames in flight
        for (uint32_t i = 0; i < 2; i++)
        {
            m_imageAvailable.emplace_back(dev, semInfo);
            m_inFlight.emplace_back(dev, fenceInfo);
        }

        // Create semaphores for each swapchain image
        for (uint32_t i = 0; i < swapchainImageCount; i++)
        {
            m_renderFinished.emplace_back(dev, semInfo);
        }
    }

    // ----------------- RECORD COMMAND BUFFER -----------------

    void Commands::recordCommandBuffer(
        vk::CommandBuffer cb,
        uint32_t imageIndex,
        VulkanDevice& device,
        SwapchainPipeline& pipeline,
        Buffers& buffers)
    {
        vk::CommandBufferBeginInfo begin{};
        cb.begin(begin);

        // Dynamic rendering
        vk::RenderingAttachmentInfo colorAttachment{};
        colorAttachment.imageView   = *pipeline.getImageViews()[imageIndex];
        colorAttachment.imageLayout = vk::ImageLayout::eAttachmentOptimal;
        colorAttachment.loadOp      = vk::AttachmentLoadOp::eClear;
        colorAttachment.storeOp     = vk::AttachmentStoreOp::eStore;
        colorAttachment.clearValue  = vk::ClearValue(vk::ClearColorValue(std::array<float,4>{0.f,0.f,0.f,1.f}));

        vk::RenderingInfo renderInfo{};
        renderInfo.renderArea = vk::Rect2D({0,0}, pipeline.getExtent());
        renderInfo.layerCount = 1;
        renderInfo.colorAttachmentCount = 1;
        renderInfo.pColorAttachments    = &colorAttachment;

        cb.beginRendering(renderInfo);

        // Viewport + scissor
        vk::Viewport vp{};
        vp.x = 0;
        vp.y = 0;
        vp.width  = (float)pipeline.getExtent().width;
        vp.height = (float)pipeline.getExtent().height;
        vp.minDepth = 0.f;
        vp.maxDepth = 1.f;

        vk::Rect2D scissor({0,0}, pipeline.getExtent());

        cb.setViewport(0, vp);
        cb.setScissor(0, scissor);

        // Bind pipeline
        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline.getPipeline());

        // Bind vertex + index buffers
        vk::Buffer vb(buffers.getVertexBuffer());
        vk::DeviceSize offset = 0;
        cb.bindVertexBuffers(0, 1, &vb, &offset);

        cb.bindIndexBuffer(buffers.getIndexBuffer(), 0, vk::IndexType::eUint16);

        // Bind descriptor set
        cb.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            *pipeline.getPipelineLayout(),
            0,
            {*buffers.getDescriptorSets()[m_frameIndex]},
            {});

        // Draw
        cb.drawIndexed(6, 1, 0, 0, 0);

        cb.endRendering();
        cb.end();
    }

    // ----------------- UPDATE UNIFORM BUFFER -----------------

    void Commands::updateUniformBuffer(VulkanDevice& device, SwapchainPipeline& pipeline, Buffers& buffers)
    {
        static auto startTime = std::chrono::high_resolution_clock::now();

        auto  currentTime = std::chrono::high_resolution_clock::now();
        float time        = std::chrono::duration<float>(currentTime - startTime).count();

        UniformBufferObject ubo{};
        ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.view  = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.proj  = glm::perspective(glm::radians(45.0f), static_cast<float>(pipeline.getExtent().width) / static_cast<float>(pipeline.getExtent().height), 0.1f, 10.0f);
        ubo.proj[1][1] *= -1;

        memcpy(buffers.getUniformMapped()[m_frameIndex], &ubo, sizeof(ubo));
    }

    // ----------------- COPY BUFFER -----------------

    void Commands::copyBuffer(VulkanDevice& device, vk::raii::Buffer& src, vk::raii::Buffer& dst, vk::DeviceSize size)
    {
        vk::CommandBufferAllocateInfo allocInfo{};
        allocInfo.commandPool        = *m_commandPool;
        allocInfo.level              = vk::CommandBufferLevel::ePrimary;
        allocInfo.commandBufferCount = 1;

        vk::raii::CommandBuffer cb = std::move(vk::raii::CommandBuffers(device.getDevice(), allocInfo).front());

        vk::CommandBufferBeginInfo beginInfo{};
        beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
        cb.begin(beginInfo);

        vk::BufferCopy copyRegion(0, 0, size);
        cb.copyBuffer(*src, *dst, copyRegion);

        cb.end();

        vk::SubmitInfo submitInfo{};
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &*cb;

        device.getGraphicsQueue().submit(submitInfo, nullptr);
        device.getGraphicsQueue().waitIdle();
    }

    // ----------------- TRANSITION IMAGE LAYOUT -----------------

    void Commands::transitionImageLayout(VulkanDevice& device, vk::Image image, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout)
    {
        vk::CommandBufferAllocateInfo allocInfo{};
        allocInfo.commandPool        = *m_commandPool;
        allocInfo.level              = vk::CommandBufferLevel::ePrimary;
        allocInfo.commandBufferCount = 1;

        vk::raii::CommandBuffer cb = std::move(vk::raii::CommandBuffers(device.getDevice(), allocInfo).front());

        vk::CommandBufferBeginInfo beginInfo{};
        beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
        cb.begin(beginInfo);

        vk::ImageMemoryBarrier2 barrier{};
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image               = image;
        barrier.subresourceRange.aspectMask     = vk::ImageAspectFlagBits::eColor;
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.subresourceRange.levelCount     = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = 1;

        if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal)
        {
            barrier.srcStageMask  = vk::PipelineStageFlagBits2::eTopOfPipe;
            barrier.srcAccessMask = {};
            barrier.dstStageMask  = vk::PipelineStageFlagBits2::eTransfer;
            barrier.dstAccessMask = vk::AccessFlagBits2::eTransferWrite;
        }
        else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
        {
            barrier.srcStageMask  = vk::PipelineStageFlagBits2::eTransfer;
            barrier.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
            barrier.dstStageMask  = vk::PipelineStageFlagBits2::eFragmentShader;
            barrier.dstAccessMask = vk::AccessFlagBits2::eShaderRead;
        }
        else if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eColorAttachmentOptimal)
        {
            barrier.srcStageMask  = vk::PipelineStageFlagBits2::eTopOfPipe;
            barrier.srcAccessMask = {};
            barrier.dstStageMask  = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
            barrier.dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
        }

        vk::DependencyInfo depInfo{};
        depInfo.imageMemoryBarrierCount = 1;
        depInfo.pImageMemoryBarriers    = &barrier;

        cb.pipelineBarrier2(depInfo);
        cb.end();

        vk::SubmitInfo submitInfo{};
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &*cb;

        device.getGraphicsQueue().submit(submitInfo, nullptr);
        device.getGraphicsQueue().waitIdle();
    }

    // ----------------- DRAW FRAME -----------------

    void Commands::drawFrame(
        VulkanInstance& instance,
        VulkanDevice& device,
        SwapchainPipeline& pipeline,
        Buffers& buffers,
        bool& framebufferResized)
    {
        auto& dev = device.getDevice();

        vk::Result fencesResult =
            dev.waitForFences(*m_inFlight[m_frameIndex], VK_TRUE, UINT64_MAX);
        if (fencesResult != vk::Result::eSuccess)
            throw std::runtime_error("Failed to wait for fences");

        auto [acquireResult, imageIndex] =
            pipeline.getSwapchain().acquireNextImage(
                UINT64_MAX,
                *m_imageAvailable[m_frameIndex],
                nullptr);

        if (acquireResult == vk::Result::eErrorOutOfDateKHR)
        {
            framebufferResized = false;
            return;
        }
        if (acquireResult != vk::Result::eSuccess && acquireResult != vk::Result::eSuboptimalKHR)
            throw std::runtime_error("Failed to acquire swapchain image");

        updateUniformBuffer(device, pipeline, buffers);

        dev.resetFences(*m_inFlight[m_frameIndex]);

        vk::CommandBuffer cb = *m_commandBuffers[m_frameIndex];
        cb.reset();

        recordCommandBuffer(cb, imageIndex, device, pipeline, buffers);

        vk::Semaphore waitSemaphores[]   = { *m_imageAvailable[m_frameIndex] };
        vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;

        vk::Semaphore signalSemaphores[] = { *m_renderFinished[imageIndex] };

        vk::SubmitInfo submit{};
        submit.waitSemaphoreCount   = 1;
        submit.pWaitSemaphores      = waitSemaphores;
        submit.pWaitDstStageMask    = &waitStage;
        submit.commandBufferCount   = 1;
        submit.pCommandBuffers      = &cb;
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores    = signalSemaphores;

        device.getGraphicsQueue().submit(submit, *m_inFlight[m_frameIndex]);

        vk::PresentInfoKHR present{};
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores    = signalSemaphores;
        present.swapchainCount     = 1;
        present.pSwapchains        = &*pipeline.getSwapchain();
        present.pImageIndices      = &imageIndex;

        vk::Result presentResult = device.getGraphicsQueue().presentKHR(present);

        if (presentResult == vk::Result::eSuboptimalKHR || presentResult == vk::Result::eErrorOutOfDateKHR || framebufferResized)
        {
            framebufferResized = false;
            return;
        }
        if (presentResult != vk::Result::eSuccess)
            throw std::runtime_error("Failed to present swap chain image");

        m_frameIndex = (m_frameIndex + 1) % 2;
    }

}
