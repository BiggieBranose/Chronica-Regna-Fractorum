#define STB_IMAGE_IMPLEMENTATION
#include "../../header/vulkan/TextureMapping.hpp"

using namespace vkapp;

void TexMap::initialize(VulkanDevice& device, Buffers& buffer){
    createTextureImage(device, buffer);
}

void TexMap::createTextureImage(VulkanDevice& device, Buffers& buffer){
    int            texWidth, texHeight, texChannels;
    stbi_uc       *pixels    = stbi_load("textures/texture.jpg", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    vk::DeviceSize imageSize = texWidth * texHeight * 4;

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
    memcpy(data, pixels, imageSize);
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