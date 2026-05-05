#include "../../header/vulkan/TextureMapping.hpp"

using namespace vkapp;

void TexMap::initialize(VulkanDevice& device, Buffers& buffer){
    createTextureImage(device, buffer);
}

void TexMap::createTextureImage(VulkanDevice& device, Buffers& buffer){
    vk::raii::Buffer stagingBuffer({});
    vk::raii::DeviceMemory stagingBufferMemory({});

    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load("textures/texture.jpg", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    vk::DeviceSize imageSize = texWidth * texHeight * 4;

    if(!pixels){
        throw std::runtime_error("Failed to load texture image");
    }

    buffer.createBuffer(device, imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

    void* data = stagingBufferMemory.mapMemory(0, imageSize);
    memcpy(data, pixels, imageSize);
    stagingBufferMemory.unmapMemory();

    stbi_image_free(pixels);
}