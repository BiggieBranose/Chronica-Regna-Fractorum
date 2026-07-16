#include "RenderGraph.hpp"
#include "core/Log.hpp"
#include "core/Assert.hpp"
#include <vulkan/vulkan.h>

namespace crf {

RenderGraph::RenderGraph(VulkanContext& context) : m_context(context) {}

RenderGraph::~RenderGraph() {
    for (auto& [name, res] : m_resources) {
        destroyResource(res);
    }
}

std::string RenderGraph::addResource(const std::string& name, const RenderGraphResource& desc) {
    CRF_ASSERT_MSG(m_resources.find(name) == m_resources.end(), "Resource already exists: " + name);
    Resource res;
    res.desc = desc;
    m_resources[name] = std::move(res);
    return name;
}

std::string RenderGraph::importResource(const std::string& name, VkImage image, VkImageView view, VkFormat format, VkExtent3D extent) {
    CRF_ASSERT_MSG(m_resources.find(name) == m_resources.end(), "Resource already exists: " + name);
    Resource res;
    res.desc.type = RenderGraphResource::Type::Image;
    res.desc.format = format;
    res.desc.extent = extent;
    res.image = image;
    res.view = view;
    res.imported = true;
    m_resources[name] = std::move(res);
    return name;
}

std::string RenderGraph::importResource(const std::string& name, VkBuffer buffer, VkDeviceSize size, VkDeviceSize offset) {
    CRF_ASSERT_MSG(m_resources.find(name) == m_resources.end(), "Resource already exists: " + name);
    Resource res;
    res.desc.type = RenderGraphResource::Type::Buffer;
    res.desc.bufferUsage = 0;
    res.buffer = buffer;
    res.imported = true;
    m_resources[name] = std::move(res);
    return name;
}

void RenderGraph::addPass(const Pass& pass) {
    CRF_ASSERT_MSG(!m_compiled, "Cannot add pass after compile");
    m_passes.push_back(pass);
}

void RenderGraph::compile() {
    CRF_ASSERT_MSG(!m_compiled, "Already compiled");
    
    for (uint32_t i = 0; i < m_passes.size(); ++i) {
        for (const auto& read : m_passes[i].reads) {
            auto it = m_resources.find(read);
            CRF_ASSERT_MSG(it != m_resources.end(), "Read resource not found: " + read);
            it->second.passReads.push_back(i);
        }
        for (const auto& write : m_passes[i].writes) {
            auto it = m_resources.find(write);
            CRF_ASSERT_MSG(it != m_resources.end(), "Write resource not found: " + write);
            it->second.passWrites.push_back(i);
        }
    }

    computeBarriers();

    for (auto& [name, res] : m_resources) {
        if (!res.imported) {
            createResource(name, res);
        }
    }

    m_compiled = true;
    Log::info("RenderGraph compiled with {} passes, {} resources", m_passes.size(), m_resources.size());
}

void RenderGraph::execute(VkCommandBuffer cmd) {
    CRF_ASSERT_MSG(m_compiled, "RenderGraph not compiled");

    for (size_t i = 0; i < m_compiledPasses.size(); ++i) {
        const auto& pass = m_compiledPasses[i];

        if (!pass.barriersBefore.empty() || !pass.bufferBarriersBefore.empty()) {
            VkDependencyInfo depInfo{};
            depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            depInfo.imageMemoryBarrierCount = static_cast<uint32_t>(pass.barriersBefore.size());
            depInfo.pImageMemoryBarriers = pass.barriersBefore.data();
            depInfo.bufferMemoryBarrierCount = static_cast<uint32_t>(pass.bufferBarriersBefore.size());
            depInfo.pBufferMemoryBarriers = pass.bufferBarriersBefore.data();
            vkCmdPipelineBarrier2(cmd, &depInfo);
        }

        if (pass.pass.execute) {
            pass.pass.execute(cmd, *this);
        }

        if (!pass.barriersAfter.empty() || !pass.bufferBarriersAfter.empty()) {
            VkDependencyInfo depInfo{};
            depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            depInfo.imageMemoryBarrierCount = static_cast<uint32_t>(pass.barriersAfter.size());
            depInfo.pImageMemoryBarriers = pass.barriersAfter.data();
            depInfo.bufferMemoryBarrierCount = static_cast<uint32_t>(pass.bufferBarriersAfter.size());
            depInfo.pBufferMemoryBarriers = pass.bufferBarriersAfter.data();
            vkCmdPipelineBarrier2(cmd, &depInfo);
        }
    }
}

VkImageView RenderGraph::getImageView(const std::string& name) const {
    auto it = m_resources.find(name);
    CRF_ASSERT_MSG(it != m_resources.end(), "Resource not found: " + name);
    return it->second.view;
}

VkBuffer RenderGraph::getBuffer(const std::string& name) const {
    auto it = m_resources.find(name);
    CRF_ASSERT_MSG(it != m_resources.end(), "Resource not found: " + name);
    return it->second.buffer;
}

VkExtent3D RenderGraph::getExtent(const std::string& name) const {
    auto it = m_resources.find(name);
    CRF_ASSERT_MSG(it != m_resources.end(), "Resource not found: " + name);
    return it->second.desc.extent;
}

void RenderGraph::createResource(const std::string& name, Resource& res) {
    VkDevice device = m_context.getDevice();
    VmaAllocator allocator = m_context.getAllocator();

    if (res.desc.type == RenderGraphResource::Type::Image) {
        VkImageCreateInfo imgInfo{};
        imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgInfo.imageType = VK_IMAGE_TYPE_2D;
        imgInfo.format = res.desc.format;
        imgInfo.extent = res.desc.extent;
        imgInfo.mipLevels = 1;
        imgInfo.arrayLayers = 1;
        imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imgInfo.usage = res.desc.usage;
        imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = res.desc.memoryUsage;
        allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

        VkResult result = vmaCreateImage(allocator, &imgInfo, &allocInfo, &res.image, &res.allocation, nullptr);
        CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create image: " + name);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = res.image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = res.desc.format;
        viewInfo.subresourceRange.aspectMask = (res.desc.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) 
            ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;

        VkResult result = vkCreateImageView(device, &viewInfo, nullptr, &res.view);
        CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create image view: " + name);
    } else if (res.desc.type == RenderGraphResource::Type::Buffer) {
        VkBufferCreateInfo bufInfo{};
        bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size = res.desc.extent.width;
        bufInfo.usage = res.desc.bufferUsage;
        bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = res.desc.memoryUsage;

        VkResult result = vmaCreateBuffer(allocator, &bufInfo, &allocInfo, &res.buffer, &res.allocation, nullptr);
        CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create buffer: " + name);
    }
}

void RenderGraph::destroyResource(Resource& res) {
    VkDevice device = m_context.getDevice();
    VmaAllocator allocator = m_context.getAllocator();

    if (res.view) {
        vkDestroyImageView(device, res.view, nullptr);
        res.view = VK_NULL_HANDLE;
    }
    if (res.image && !res.imported) {
        vmaDestroyImage(m_context.getAllocator(), res.image, res.allocation);
        res.image = VK_NULL_HANDLE;
        res.allocation = VK_NULL_HANDLE;
    }
    if (res.buffer && !res.imported) {
        vmaDestroyBuffer(allocator, res.buffer, res.allocation);
        res.buffer = VK_NULL_HANDLE;
        res.allocation = VK_NULL_HANDLE;
    }
}

void RenderGraph::computeBarriers() {
    m_compiledPasses.resize(m_passes.size());

    for (size_t i = 0; i < m_passes.size(); ++i) {
        const auto& pass = m_passes[i];
        CompiledPass& cp = m_compiledPasses[i];
        cp.pass = pass;

        for (const auto& read : pass.reads) {
            auto& res = m_resources[read];
            VkImageLayout requiredLayout = getRequiredLayout(res.desc.usage, false);
            if (res.currentLayout != requiredLayout) {
                VkImageMemoryBarrier2 barrier{};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
                barrier.srcStageMask = res.currentLayout == VK_IMAGE_LAYOUT_UNDEFINED 
                    ? VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT 
                    : usageToStage(res.desc.usage);
                barrier.srcAccessMask = usageToAccess(res.desc.usage, false);
                barrier.dstStageMask = pass.stageMask ? pass.stageMask : usageToStage(res.desc.usage);
                barrier.dstAccessMask = usageToAccess(res.desc.usage, false);
                barrier.oldLayout = res.currentLayout;
                barrier.newLayout = requiredLayout;
                barrier.image = res.image;
                barrier.subresourceRange.aspectMask = (res.desc.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
                    ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
                barrier.subresourceRange.levelCount = 1;
                barrier.subresourceRange.layerCount = 1;
                cp.barriersBefore.push_back(barrier);
                res.currentLayout = requiredLayout;
            }
        }

        for (const auto& write : pass.writes) {
            auto& res = m_resources[write];
            VkImageLayout requiredLayout = getRequiredLayout(res.desc.usage, true);
            if (res.currentLayout != requiredLayout) {
                VkImageMemoryBarrier2 barrier{};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
                barrier.srcStageMask = res.currentLayout == VK_IMAGE_LAYOUT_UNDEFINED
                    ? VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT
                    : usageToStage(res.desc.usage);
                barrier.srcAccessMask = usageToAccess(res.desc.usage, false);
                barrier.dstStageMask = pass.stageMask ? pass.stageMask : usageToStage(res.desc.usage);
                barrier.dstAccessMask = usageToAccess(res.desc.usage, true);
                barrier.oldLayout = res.currentLayout;
                barrier.newLayout = requiredLayout;
                barrier.image = res.image;
                barrier.subresourceRange.aspectMask = (res.desc.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
                    ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
                barrier.subresourceRange.levelCount = 1;
                barrier.subresourceRange.layerCount = 1;
                cp.barriersBefore.push_back(barrier);
                res.currentLayout = requiredLayout;
            }
        }

        for (const auto& write : pass.writes) {
            auto& res = m_resources[write];
            VkImageLayout finalLayout = getRequiredLayout(res.desc.usage, true);
            VkImageMemoryBarrier2 barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            barrier.srcStageMask = pass.stageMask ? pass.stageMask : usageToStage(res.desc.usage);
            barrier.srcAccessMask = usageToAccess(res.desc.usage, true);
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
            barrier.dstAccessMask = 0;
            barrier.oldLayout = finalLayout;
            barrier.newLayout = finalLayout;
            barrier.image = res.image;
            barrier.subresourceRange.aspectMask = (res.desc.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
                ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.layerCount = 1;
            cp.barriersAfter.push_back(barrier);
        }
    }
}

VkImageLayout RenderGraph::getRequiredLayout(VkImageUsageFlags usage, bool isWrite) const {
    if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
        return isWrite ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL 
                       : VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    }
    if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
        return isWrite ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                       : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    if (usage & VK_IMAGE_USAGE_STORAGE_BIT) {
        return VK_IMAGE_LAYOUT_GENERAL;
    }
    if (usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT) {
        return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    }
    if (usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
        return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    }
    return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

VkPipelineStageFlags2 RenderGraph::usageToStage(VkImageUsageFlags usage) const {
    if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) return VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) return VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    if (usage & VK_IMAGE_USAGE_STORAGE_BIT) return VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    if (usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT) return VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    if (usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) return VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    return VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
}

VkAccessFlags2 RenderGraph::usageToAccess(VkImageUsageFlags usage, bool isWrite) const {
    VkAccessFlags2 flags = 0;
    if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
        flags |= isWrite ? VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT : VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
    }
    if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
        flags |= isWrite ? (VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT) 
                         : (VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT);
    }
    if (usage & VK_IMAGE_USAGE_STORAGE_BIT) {
        flags |= isWrite ? VK_ACCESS_2_SHADER_WRITE_BIT : VK_ACCESS_2_SHADER_READ_BIT;
    }
    if (usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT) flags |= VK_ACCESS_2_TRANSFER_WRITE_BIT;
    if (usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) flags |= VK_ACCESS_2_TRANSFER_READ_BIT;
    if (usage & VK_IMAGE_USAGE_SAMPLED_BIT) flags |= VK_ACCESS_2_SHADER_READ_BIT;
    return flags;
}

}