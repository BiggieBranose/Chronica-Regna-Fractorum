#include "../header/SpriteSystem.hpp"
#include <graphics/VulkanPipeline.hpp>
#include <core/Log.hpp>
#include <core/Assert.hpp>
#include <cstring>
#include <array>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace game {

static const std::vector<SpriteVertex> quadVerts = {
    {{-0.5f, -0.5f}, {0.0f, 1.0f}},
    {{ 0.5f, -0.5f}, {1.0f, 1.0f}},
    {{ 0.5f,  0.5f}, {1.0f, 0.0f}},
    {{-0.5f,  0.5f}, {0.0f, 0.0f}},
};

static const std::vector<uint32_t> quadIndices = {
    0, 1, 2, 2, 3, 0
};

static std::vector<uint8_t> generateSpriteTexture(int width, int height) {
    std::vector<uint8_t> pixels(width * height * 4, 0);

    auto setPixel = [&](int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        if (x < 0 || x >= width || y < 0 || y >= height) return;
        int i = (y * width + x) * 4;
        pixels[i + 0] = r;
        pixels[i + 1] = g;
        pixels[i + 2] = b;
        pixels[i + 3] = a;
    };

    auto fillRect = [&](int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++)
                setPixel(x, y, r, g, b, a);
    };

    // Head (skin color)
    fillRect(5, 1, 10, 4, 230, 180, 130, 255);
    // Eyes
    setPixel(6, 3, 30, 30, 30, 255);
    setPixel(9, 3, 30, 30, 30, 255);
    // Hair (dark brown)
    fillRect(5, 1, 10, 1, 80, 50, 30, 255);
    fillRect(4, 1, 4, 2, 80, 50, 30, 255);
    fillRect(11, 1, 11, 2, 80, 50, 30, 255);
    // Body (blue tunic)
    fillRect(5, 5, 10, 9, 40, 80, 180, 255);
    // Belt
    fillRect(5, 8, 10, 8, 120, 70, 30, 255);
    // Belt buckle
    setPixel(7, 8, 220, 190, 50, 255);
    setPixel(8, 8, 220, 190, 50, 255);
    // Arms (skin)
    fillRect(3, 5, 4, 8, 230, 180, 130, 255);
    fillRect(11, 5, 12, 8, 230, 180, 130, 255);
    // Sword in right hand
    fillRect(13, 3, 13, 7, 200, 200, 210, 255);
    fillRect(12, 7, 14, 7, 160, 120, 40, 255);
    // Legs (brown pants)
    fillRect(5, 10, 7, 13, 100, 70, 40, 255);
    fillRect(8, 10, 10, 13, 100, 70, 40, 255);
    // Boots (dark brown)
    fillRect(4, 14, 7, 15, 60, 40, 20, 255);
    fillRect(8, 14, 11, 15, 60, 40, 20, 255);

    return pixels;
}

SpriteSystem::SpriteSystem(crf::VulkanContext& context, crf::VulkanRenderPass& renderPass)
    : m_context(context), m_renderPass(renderPass) {
}

SpriteSystem::~SpriteSystem() {
    VkDevice device = m_context.getDevice();

    vkDestroyPipeline(device, m_pipeline, nullptr);
    vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);
    vkDestroyDescriptorPool(device, m_descPool, nullptr);
    vkDestroyDescriptorSetLayout(device, m_descSetLayout, nullptr);

    for (size_t i = 0; i < m_uniformBuffers.size(); i++) {
        vkDestroyBuffer(device, m_uniformBuffers[i], nullptr);
        vkFreeMemory(device, m_uniformBuffersMemory[i], nullptr);
    }

    vkDestroyBuffer(device, m_quadVB, nullptr);
    vkFreeMemory(device, m_quadVBMemory, nullptr);

    vkDestroyImageView(device, m_textureView, nullptr);
    vkDestroyImage(device, m_textureImage, nullptr);
    vkFreeMemory(device, m_textureMemory, nullptr);
    vkDestroySampler(device, m_sampler, nullptr);
}

void SpriteSystem::init() {
    crf::Log::info("SpriteSystem: initializing");
    createQuadBuffer();
    createTexture();
    createUniformBuffers();
    createDescriptorSetLayout();
    createPipelineLayout();
    createDescriptorPool();
    createDescriptorSets();
    createPipeline();
    crf::Log::info("SpriteSystem: ready");
}

void SpriteSystem::render(VkCommandBuffer cmd, crf::u32 imageIndex,
                           const float* modelPtr, const float* viewPtr, const float* projPtr) {
    crf::UniformBufferObject ubo{};
    std::memcpy(ubo.model, modelPtr, sizeof(float) * 16);
    std::memcpy(ubo.view, viewPtr, sizeof(float) * 16);
    std::memcpy(ubo.proj, projPtr, sizeof(float) * 16);

    crf::u32 frame = m_renderPass.getCurrentFrame();
    std::memcpy(m_uniformBuffersMapped[frame], &ubo, sizeof(crf::UniformBufferObject));

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_context.getSwapChainExtent().width);
    viewport.height = static_cast<float>(m_context.getSwapChainExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = m_context.getSwapChainExtent();
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    VkBuffer vbs[] = {m_quadVB};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offsets);
    vkCmdBindIndexBuffer(cmd, m_quadVB, sizeof(SpriteVertex) * 4, VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_pipelineLayout, 0, 1, &m_descSets[frame], 0, nullptr);

    vkCmdDrawIndexed(cmd, static_cast<uint32_t>(quadIndices.size()), 1, 0, 0, 0);
}

void SpriteSystem::createQuadBuffer() {
    VkDeviceSize vbSize = sizeof(SpriteVertex) * quadVerts.size();
    VkDeviceSize ibSize = sizeof(uint32_t) * quadIndices.size();
    VkDeviceSize totalSize = vbSize + ibSize;

    crf::VulkanBuffer tmp(m_context, m_renderPass.getCommandPool());

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    tmp.createBuffer(totalSize,
                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingBuffer, stagingMemory);

    void* data;
    vkMapMemory(m_context.getDevice(), stagingMemory, 0, totalSize, 0, &data);
    std::memcpy(data, quadVerts.data(), vbSize);
    std::memcpy(static_cast<uint8_t*>(data) + vbSize, quadIndices.data(), ibSize);
    vkUnmapMemory(m_context.getDevice(), stagingMemory);

    tmp.createBuffer(totalSize,
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                     m_quadVB, m_quadVBMemory);

    VkCommandBuffer cmd = m_renderPass.beginSingleTimeCommands();
    tmp.copyBuffer(stagingBuffer, m_quadVB, totalSize);
    m_renderPass.endSingleTimeCommands(cmd);

    vkDestroyBuffer(m_context.getDevice(), stagingBuffer, nullptr);
    vkFreeMemory(m_context.getDevice(), stagingMemory, nullptr);
}

void SpriteSystem::createTexture() {
    const int w = 16, h = 16;
    auto pixels = generateSpriteTexture(w, h);
    VkDeviceSize imageSize = w * h * 4;

    crf::VulkanBuffer tmp(m_context, m_renderPass.getCommandPool());

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    tmp.createBuffer(imageSize,
                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingBuffer, stagingMemory);

    void* data;
    vkMapMemory(m_context.getDevice(), stagingMemory, 0, imageSize, 0, &data);
    std::memcpy(data, pixels.data(), imageSize);
    vkUnmapMemory(m_context.getDevice(), stagingMemory);

    VkImageCreateInfo imgInfo{};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.extent.width = w;
    imgInfo.extent.height = h;
    imgInfo.extent.depth = 1;
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCreateImage(m_context.getDevice(), &imgInfo, nullptr, &m_textureImage);

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(m_context.getDevice(), m_textureImage, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = crf::VulkanBuffer::findMemoryType(
        memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_context.getPhysicalDevice());
    vkAllocateMemory(m_context.getDevice(), &allocInfo, nullptr, &m_textureMemory);
    vkBindImageMemory(m_context.getDevice(), m_textureImage, m_textureMemory, 0);

    VkCommandBuffer cmd = m_renderPass.beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_textureImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy copyRegion{};
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageExtent = {w, h, 1};
    vkCmdCopyBufferToImage(cmd, stagingBuffer, m_textureImage,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    m_renderPass.endSingleTimeCommands(cmd);

    vkDestroyBuffer(m_context.getDevice(), stagingBuffer, nullptr);
    vkFreeMemory(m_context.getDevice(), stagingMemory, nullptr);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_textureImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    vkCreateImageView(m_context.getDevice(), &viewInfo, nullptr, &m_textureView);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.minLod = 0;
    samplerInfo.maxLod = 0;
    vkCreateSampler(m_context.getDevice(), &samplerInfo, nullptr, &m_sampler);
}

void SpriteSystem::createUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(crf::UniformBufferObject);
    m_uniformBuffers.resize(crf::VulkanContext::MAX_FRAMES_IN_FLIGHT);
    m_uniformBuffersMemory.resize(crf::VulkanContext::MAX_FRAMES_IN_FLIGHT);
    m_uniformBuffersMapped.resize(crf::VulkanContext::MAX_FRAMES_IN_FLIGHT);

    crf::VulkanBuffer tmp(m_context, m_renderPass.getCommandPool());

    for (size_t i = 0; i < crf::VulkanContext::MAX_FRAMES_IN_FLIGHT; i++) {
        tmp.createBuffer(bufferSize,
                         VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         m_uniformBuffers[i], m_uniformBuffersMemory[i]);
        vkMapMemory(m_context.getDevice(), m_uniformBuffersMemory[i], 0, bufferSize, 0, &m_uniformBuffersMapped[i]);
    }
}

void SpriteSystem::createDescriptorSetLayout() {
    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};

    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    VkResult result = vkCreateDescriptorSetLayout(m_context.getDevice(), &layoutInfo, nullptr, &m_descSetLayout);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create sprite descriptor set layout");
}

void SpriteSystem::createDescriptorPool() {
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = crf::VulkanContext::MAX_FRAMES_IN_FLIGHT;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = crf::VulkanContext::MAX_FRAMES_IN_FLIGHT;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = crf::VulkanContext::MAX_FRAMES_IN_FLIGHT;

    VkResult result = vkCreateDescriptorPool(m_context.getDevice(), &poolInfo, nullptr, &m_descPool);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create sprite descriptor pool");
}

void SpriteSystem::createDescriptorSets() {
    std::vector<VkDescriptorSetLayout> layouts(crf::VulkanContext::MAX_FRAMES_IN_FLIGHT, m_descSetLayout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descPool;
    allocInfo.descriptorSetCount = crf::VulkanContext::MAX_FRAMES_IN_FLIGHT;
    allocInfo.pSetLayouts = layouts.data();

    m_descSets.resize(crf::VulkanContext::MAX_FRAMES_IN_FLIGHT);
    VkResult result = vkAllocateDescriptorSets(m_context.getDevice(), &allocInfo, m_descSets.data());
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to allocate sprite descriptor sets");

    for (size_t i = 0; i < crf::VulkanContext::MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_uniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(crf::UniformBufferObject);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = m_textureView;
        imageInfo.sampler = m_sampler;

        std::array<VkWriteDescriptorSet, 2> writes{};

        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = m_descSets[i];
        writes[0].dstBinding = 0;
        writes[0].dstArrayElement = 0;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[0].descriptorCount = 1;
        writes[0].pBufferInfo = &bufferInfo;

        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = m_descSets[i];
        writes[1].dstBinding = 1;
        writes[1].dstArrayElement = 0;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[1].descriptorCount = 1;
        writes[1].pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(m_context.getDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
}

void SpriteSystem::createPipelineLayout() {
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_descSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 0;

    VkResult result = vkCreatePipelineLayout(m_context.getDevice(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create sprite pipeline layout");
}

void SpriteSystem::createPipeline() {
    auto vertCode = crf::VulkanPipeline::readFile("shaders/billboard.vert.spv");
    auto fragCode = crf::VulkanPipeline::readFile("shaders/billboard.frag.spv");

    VkShaderModule vertModule = crf::VulkanPipeline::createShaderModule(m_context.getDevice(), vertCode);
    VkShaderModule fragModule = crf::VulkanPipeline::createShaderModule(m_context.getDevice(), fragCode);

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertModule;
    vertStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragModule;
    fragStage.pName = "main";

    std::array<VkPipelineShaderStageCreateInfo, 2> stages = {vertStage, fragStage};

    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding = 0;
    bindingDesc.stride = sizeof(SpriteVertex);
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 2> attrDescs{};
    attrDescs[0].binding = 0;
    attrDescs[0].location = 0;
    attrDescs[0].format = VK_FORMAT_R32G32_SFLOAT;
    attrDescs[0].offset = offsetof(SpriteVertex, pos);
    attrDescs[1].binding = 0;
    attrDescs[1].location = 1;
    attrDescs[1].format = VK_FORMAT_R32G32_SFLOAT;
    attrDescs[1].offset = offsetof(SpriteVertex, uv);

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &bindingDesc;
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrDescs.size());
    vertexInput.pVertexAttributeDescriptions = attrDescs.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_TRUE;
    multisampling.rasterizationSamples = m_renderPass.getMsaaSamples();
    multisampling.minSampleShading = .2f;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
    pipelineInfo.pStages = stages.data();
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = m_renderPass.getRenderPass();
    pipelineInfo.subpass = 0;

    VkResult result = vkCreateGraphicsPipelines(m_context.getDevice(), VK_NULL_HANDLE, 1,
                                            &pipelineInfo, nullptr, &m_pipeline);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create sprite graphics pipeline");

    vkDestroyShaderModule(m_context.getDevice(), vertModule, nullptr);
    vkDestroyShaderModule(m_context.getDevice(), fragModule, nullptr);
}

}
