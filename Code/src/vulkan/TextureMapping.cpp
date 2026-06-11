#define STB_IMAGE_IMPLEMENTATION
#include "../../header/vulkan/TextureMapping.hpp"
#include "../../header/vulkan/Buffers.hpp"
#include <cstring>

using namespace vkapp;

void TexMap::initialize(VulkanDevice& device, vk::raii::CommandPool& commandPool){
    createTextureImage(device, commandPool);
    createTextureImageView(device);
    createTextureSampler(device);
}

void TexMap::createTextureImage(VulkanDevice& device, vk::raii::CommandPool& commandPool){
    int            texWidth, texHeight, texChannels;
    stbi_uc       *pixels    = stbi_load("textures/texture.jpg", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    vk::DeviceSize imageSize = static_cast<vk::DeviceSize>(texWidth) * texHeight * 4;

    if (!pixels)
    {
    throw std::runtime_error("failed to load texture image!");
    }

    vk::BufferCreateInfo stagingBufferInfo({}, imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::SharingMode::eExclusive);
    vk::raii::Buffer stagingBuffer(device.getDevice(), stagingBufferInfo);
    vk::MemoryRequirements stagingMemReq = stagingBuffer.getMemoryRequirements();
    vk::MemoryAllocateInfo stagingAllocInfo(stagingMemReq.size, findMemoryType(device, stagingMemReq.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent));
    vk::raii::DeviceMemory stagingBufferMemory(device.getDevice(), stagingAllocInfo);
    stagingBuffer.bindMemory(stagingBufferMemory, 0);

    void *data = stagingBufferMemory.mapMemory(0, imageSize);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    stagingBufferMemory.unmapMemory();

    stbi_image_free(pixels);

    std::tie(textureImage, textureImageMemory) = createImage(
        device,
        texWidth,
        texHeight,
        vk::Format::eR8G8B8A8Srgb,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
        vk::MemoryPropertyFlagBits::eDeviceLocal
    );

    transitionImageLayout(device, commandPool, *textureImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
    copyBufferToImage(device, commandPool, stagingBuffer, textureImage, texWidth, texHeight);
    transitionImageLayout(device, commandPool, *textureImage, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
}

std::pair<vk::raii::Image, vk::raii::DeviceMemory> TexMap::createImage(
        VulkanDevice& device, uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties
){
    vk::ImageCreateInfo imageInfo(
        {}, vk::ImageType::e2D, format, vk::Extent3D{width, height, 1},
        1, 1, vk::SampleCountFlagBits::e1, tiling, usage,
        vk::SharingMode::eExclusive
    );

    vk::raii::Image image(device.getDevice(), imageInfo);

    vk::MemoryRequirements memRequirements = image.getMemoryRequirements();
    vk::MemoryAllocateInfo allocInfo(memRequirements.size, findMemoryType(device, memRequirements.memoryTypeBits, properties), nullptr);
    vk::raii::DeviceMemory imageMemory(device.getDevice(), allocInfo);
    image.bindMemory(imageMemory, 0);

    return {std::move(image), std::move(imageMemory)};
}

vk::raii::ImageView TexMap::createImageView(VulkanDevice& device, vk::Image image, vk::Format format)
{
    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.image    = image;
    viewInfo.viewType = vk::ImageViewType::e2D;
    viewInfo.format   = format;
    viewInfo.subresourceRange.aspectMask     = vk::ImageAspectFlagBits::eColor;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;
    return vk::raii::ImageView(device.getDevice(), viewInfo);
}

void TexMap::createTextureImageView(VulkanDevice& device)
{
    textureImageView = createImageView(device, *textureImage, vk::Format::eR8G8B8A8Srgb);
}

void TexMap::createTextureSampler(VulkanDevice& device)
{
    vk::PhysicalDeviceProperties props = device.getPhysicalDevice().getProperties();

    vk::SamplerCreateInfo info{};
    info.magFilter        = vk::Filter::eLinear;
    info.minFilter        = vk::Filter::eLinear;
    info.mipmapMode       = vk::SamplerMipmapMode::eLinear;
    info.addressModeU     = vk::SamplerAddressMode::eRepeat;
    info.addressModeV     = vk::SamplerAddressMode::eRepeat;
    info.addressModeW     = vk::SamplerAddressMode::eRepeat;
    info.mipLodBias       = 0.0f;
    info.anisotropyEnable = vk::True;
    info.maxAnisotropy    = props.limits.maxSamplerAnisotropy;
    info.compareEnable    = vk::False;
    info.compareOp        = vk::CompareOp::eAlways;
    info.minLod           = 0.0f;
    info.maxLod           = 0.0f;

    textureSampler = vk::raii::Sampler(device.getDevice(), info);
}

void TexMap::copyBufferToImage(VulkanDevice& device, vk::raii::CommandPool& commandPool, vk::raii::Buffer& src, vk::raii::Image& dst, uint32_t width, uint32_t height)
{
    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.commandPool        = *commandPool;
    allocInfo.level              = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandBufferCount = 1;

    vk::raii::CommandBuffer cb = std::move(vk::raii::CommandBuffers(device.getDevice(), allocInfo).front());

    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    cb.begin(beginInfo);

    vk::BufferImageCopy region{};
    region.bufferOffset      = 0;
    region.bufferRowLength   = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask     = vk::ImageAspectFlagBits::eColor;
    region.imageSubresource.mipLevel       = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount     = 1;
    region.imageOffset                     = vk::Offset3D{0, 0, 0};
    region.imageExtent                     = vk::Extent3D{width, height, 1};

    cb.copyBufferToImage(*src, *dst, vk::ImageLayout::eTransferDstOptimal, region);
    cb.end();

    vk::SubmitInfo submitInfo{};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &*cb;

    device.getGraphicsQueue().submit(submitInfo, nullptr);
    device.getGraphicsQueue().waitIdle();
}

void TexMap::transitionImageLayout(VulkanDevice& device, vk::raii::CommandPool& commandPool, vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout)
{
    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.commandPool        = *commandPool;
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