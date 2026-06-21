#include "Texture.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "../../external/stb/stb_image.h"
#include "../../external/VMA/vk_mem_alloc.h"
#include "../core/Log.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace crf {

static VmaAllocator toVma(void* p) { return static_cast<VmaAllocator>(p); }

Texture::Texture(Texture&& other) noexcept
    : m_image(other.m_image), m_allocation(other.m_allocation),
      m_imageView(std::move(other.m_imageView)), m_sampler(std::move(other.m_sampler)),
      m_mipLevels(other.m_mipLevels), m_width(other.m_width), m_height(other.m_height)
{
    other.m_image = VK_NULL_HANDLE;
    other.m_allocation = nullptr;
}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        m_image = other.m_image; other.m_image = VK_NULL_HANDLE;
        m_allocation = other.m_allocation; other.m_allocation = nullptr;
        m_imageView = std::move(other.m_imageView);
        m_sampler = std::move(other.m_sampler);
        m_mipLevels = other.m_mipLevels;
        m_width = other.m_width;
        m_height = other.m_height;
    }
    return *this;
}

Texture::~Texture() {}

void Texture::destroy(VkDevice device, VmaAllocator allocator) {
    (void)device;
    m_sampler = nullptr;
    m_imageView = nullptr;
    if (m_image != VK_NULL_HANDLE) {
        if (m_allocation)
            vmaDestroyImage(toVma(allocator), m_image, m_allocation);
        m_image = VK_NULL_HANDLE;
        m_allocation = nullptr;
    }
}

bool Texture::loadFromFile(const std::string& filename, vk::raii::Device& device, VmaAllocator allocator,
                           vk::raii::CommandPool& pool, vk::raii::Queue& queue)
{
    int texW, texH, texCh;
    stbi_uc* pixels = stbi_load(filename.c_str(), &texW, &texH, &texCh, STBI_rgb_alpha);
    if (!pixels) {
        Log::error("Failed to load texture: {}", filename);
        return false;
    }

    vk::DeviceSize imageSize = texW * texH * 4;
    m_mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texW, texH)))) + 1;
    m_width = texW;
    m_height = texH;

    auto usage = vk::ImageUsageFlagBits::eTransferSrc |
                 vk::ImageUsageFlagBits::eTransferDst |
                 vk::ImageUsageFlagBits::eSampled;

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = texW;
    imageInfo.extent.height = texH;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = m_mipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = static_cast<VkImageUsageFlags>(usage);
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

    VkImage image;
    VmaAllocation allocation;
    if (vmaCreateImage(toVma(allocator), &imageInfo, &allocInfo, &image, &allocation, nullptr) != VK_SUCCESS) {
        Log::error("Failed to create texture image via VMA");
        stbi_image_free(pixels);
        return false;
    }
    m_image = image;
    m_allocation = allocation;

    auto beginSingleTime = [&]() {
        vk::CommandBufferAllocateInfo alloc{};
        alloc.commandPool = *pool;
        alloc.level = vk::CommandBufferLevel::ePrimary;
        alloc.commandBufferCount = 1;
        auto cmdBufs = vk::raii::CommandBuffers(device, alloc);
        vk::raii::CommandBuffer cb = std::move(cmdBufs.front());
        vk::CommandBufferBeginInfo begin{};
        begin.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
        cb.begin(begin);
        return cb;
    };

    auto endSingleTime = [&](vk::raii::CommandBuffer& cb) {
        cb.end();
        vk::SubmitInfo submit{};
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &*cb;
        queue.submit(submit, nullptr);
        queue.waitIdle();
    };

    auto transitionLayout = [&](vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
                                vk::PipelineStageFlags2 srcStage, vk::PipelineStageFlags2 dstStage,
                                vk::AccessFlags2 srcAccess, vk::AccessFlags2 dstAccess)
    {
        auto cb = beginSingleTime();
        vk::ImageMemoryBarrier2 barrier{};
        barrier.image = m_image;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = m_mipLevels;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcStageMask = srcStage;
        barrier.srcAccessMask = srcAccess;
        barrier.dstStageMask = dstStage;
        barrier.dstAccessMask = dstAccess;
        vk::DependencyInfo dep{};
        dep.imageMemoryBarrierCount = 1;
        dep.pImageMemoryBarriers = &barrier;
        cb.pipelineBarrier2(dep);
        endSingleTime(cb);
    };

    transitionLayout(vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
                     vk::PipelineStageFlagBits2::eTopOfPipe, vk::PipelineStageFlagBits2::eTransfer,
                     {}, vk::AccessFlagBits2::eTransferWrite);

    {
        VkBufferCreateInfo bufInfo{};
        bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size = imageSize;
        bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo stagingAlloc{};
        stagingAlloc.usage = VMA_MEMORY_USAGE_AUTO;
        stagingAlloc.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                             VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VkBuffer stagingBuf;
        VmaAllocation stagingAllocation;
        VmaAllocationInfo stagingInfo;
        vmaCreateBuffer(toVma(allocator), &bufInfo, &stagingAlloc,
                        &stagingBuf, &stagingAllocation, &stagingInfo);
        memcpy(stagingInfo.pMappedData, pixels, (size_t)imageSize);

        auto cb = beginSingleTime();
        vk::BufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageExtent.width = texW;
        region.imageExtent.height = texH;
        region.imageExtent.depth = 1;
        cb.copyBufferToImage(vk::Buffer(stagingBuf), m_image,
                             vk::ImageLayout::eTransferDstOptimal, {region});
        endSingleTime(cb);
        vmaDestroyBuffer(toVma(allocator), stagingBuf, stagingAllocation);
    }

    if (m_mipLevels > 1) {
        auto cb = beginSingleTime();
        vk::ImageMemoryBarrier2 barrier{};
        barrier.image = m_image;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        int32_t mipW = texW, mipH = texH;
        for (uint32_t i = 1; i < m_mipLevels; i++) {
            barrier.subresourceRange.baseMipLevel = i - 1;
            barrier.subresourceRange.levelCount = 1;
            barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
            barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
            barrier.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
            barrier.dstStageMask = vk::PipelineStageFlagBits2::eTransfer;
            barrier.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
            barrier.dstAccessMask = vk::AccessFlagBits2::eTransferRead;
            vk::DependencyInfo dep; dep.imageMemoryBarrierCount = 1; dep.pImageMemoryBarriers = &barrier;
            cb.pipelineBarrier2(dep);

            std::array<vk::Offset3D, 2> srcOffsets = {
                vk::Offset3D(0, 0, 0), vk::Offset3D(mipW, mipH, 1)
            };
            std::array<vk::Offset3D, 2> dstOffsets = {
                vk::Offset3D(0, 0, 0),
                vk::Offset3D(mipW > 1 ? mipW / 2 : 1, mipH > 1 ? mipH / 2 : 1, 1)
            };
            vk::ImageBlit2 blit{};
            blit.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = 1;
            blit.srcOffsets = srcOffsets;
            blit.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
            blit.dstSubresource.mipLevel = i;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = 1;
            blit.dstOffsets = dstOffsets;
            vk::BlitImageInfo2 blitInfo{};
            blitInfo.srcImage = m_image;
            blitInfo.srcImageLayout = vk::ImageLayout::eTransferSrcOptimal;
            blitInfo.dstImage = m_image;
            blitInfo.dstImageLayout = vk::ImageLayout::eTransferDstOptimal;
            blitInfo.filter = vk::Filter::eLinear;
            blitInfo.regionCount = 1;
            blitInfo.pRegions = &blit;
            cb.blitImage2(blitInfo);

            barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
            barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            barrier.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
            barrier.dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader;
            barrier.srcAccessMask = vk::AccessFlagBits2::eTransferRead;
            barrier.dstAccessMask = vk::AccessFlagBits2::eShaderRead;
            vk::DependencyInfo dep2; dep2.imageMemoryBarrierCount = 1; dep2.pImageMemoryBarriers = &barrier;
            cb.pipelineBarrier2(dep2);

            if (mipW > 1) mipW /= 2;
            if (mipH > 1) mipH /= 2;
        }
        barrier.subresourceRange.baseMipLevel = m_mipLevels - 1;
        barrier.subresourceRange.levelCount = 1;
        barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        barrier.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
        barrier.dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader;
        barrier.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits2::eShaderRead;
        vk::DependencyInfo dep3; dep3.imageMemoryBarrierCount = 1; dep3.pImageMemoryBarriers = &barrier;
        cb.pipelineBarrier2(dep3);
        endSingleTime(cb);
    } else {
        transitionLayout(vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
                         vk::PipelineStageFlagBits2::eTransfer, vk::PipelineStageFlagBits2::eFragmentShader,
                         vk::AccessFlagBits2::eTransferWrite, vk::AccessFlagBits2::eShaderRead);
    }

    stbi_image_free(pixels);

    {
        vk::ImageViewCreateInfo viewInfo{};
        viewInfo.image = m_image;
        viewInfo.viewType = vk::ImageViewType::e2D;
        viewInfo.format = vk::Format::eR8G8B8A8Srgb;
        viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = m_mipLevels;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        m_imageView = vk::raii::ImageView(device, viewInfo);
    }

    {
        vk::SamplerCreateInfo samplerInfo{};
        samplerInfo.magFilter = vk::Filter::eLinear;
        samplerInfo.minFilter = vk::Filter::eLinear;
        samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
        samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
        samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = 16.0f;
        samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = vk::CompareOp::eAlways;
        samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = (float)m_mipLevels;
        m_sampler = vk::raii::Sampler(device, samplerInfo);
    }

    Log::info("Texture loaded: {} ({}x{} mips={})", filename, m_width, m_height, m_mipLevels);
    return true;
}

} // namespace crf
