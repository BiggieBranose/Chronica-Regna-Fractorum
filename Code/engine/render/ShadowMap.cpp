#include "ShadowMap.hpp"
#include "core/Log.hpp"
#include "core/Assert.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.h>
#include <algorithm>

namespace crf {

ShadowMap::ShadowMap(VulkanContext& context, uint32_t resolution) : m_context(context), m_resolution(resolution, resolution) {
    m_cascadeSplits[0] = 0.1f;
    m_cascadeSplits[1] = 0.3f;
    m_cascadeSplits[2] = 0.6f;
    m_cascadeSplits[3] = 1.0f;
}

ShadowMap::~ShadowMap() {
    cleanup();
}

void ShadowMap::init() {
    createResources();
    createRenderPass();
    createFramebuffer();
    createPipeline();
    createDescriptorSet();
    createSampler();
    Log::info("ShadowMap initialized: {}x{}", m_resolution.width, m_resolution.height);
}

void ShadowMap::cleanup() {
    destroyResources();
    Log::info("ShadowMap cleaned up");
}

void ShadowMap::createResources() {
    VkDevice device = m_context.getDevice();

    VkImageCreateInfo imgInfo{};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = DEPTH_FORMAT;
    imgInfo.extent.width = m_resolution.width;
    imgInfo.extent.height = m_resolution.height;
    imgInfo.extent.depth = 1;
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = CASCADE_COUNT;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VkResult result = vmaCreateImage(m_context.getAllocator(), &imgInfo, &VmaAllocationCreateInfo{}, &m_depthArray, &m_depthArrayAlloc, nullptr);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create shadow map depth array");

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_depthArray;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    viewInfo.format = DEPTH_FORMAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = CASCADE_COUNT;

    VkResult result = vkCreateImageView(m_context.getDevice(), &viewInfo, nullptr, &m_depthArrayView);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create shadow map array view");

    for (uint32_t i = 0; i < CASCADE_COUNT; ++i) {
        VkImageViewCreateInfo cascadeViewInfo = viewInfo;
        cascadeViewInfo.subresourceRange.baseArrayLayer = i;
        cascadeViewInfo.subresourceRange.layerCount = 1;
        VkResult res = vkCreateImageView(m_context.getDevice(), &cascadeViewInfo, nullptr, &m_cascadeViews[i]);
        CRF_ASSERT_MSG(res == VK_SUCCESS, "Failed to create cascade view");
        m_cascades[i].view = m_cascadeViews[i];
    }
}

void ShadowMap::createRenderPass() {
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = DEPTH_FORMAT;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 0;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 0;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &depthAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VkResult result = vkCreateRenderPass(m_context.getDevice(), &depthAttachment, nullptr, &m_renderPass);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create shadow map render pass");
}

void ShadowMap::createFramebuffer() {
    VkFramebufferCreateInfo fbInfo{};
    fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass = m_renderPass;
    fbInfo.attachmentCount = 1;
    fbInfo.pAttachments = &m_depthArrayView;
    fbInfo.width = m_resolution.width;
    fbInfo.height = m_resolution.height;
    fbInfo.layers = CASCADE_COUNT;

    VkResult result = vkCreateFramebuffer(m_context.getDevice(), &fbInfo, nullptr, &m_framebuffer);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create shadow map framebuffer");
}

void ShadowMap::createPipeline() {
    VkDevice device = m_context.getDevice();

    VkDescriptorSetLayoutBinding dslBinding{};
    dslBinding.binding = 0;
    dslBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    dslBinding.descriptorCount = 1;
    dslBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo dslInfo{};
    dslInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dslInfo.bindingCount = 1;
    dslInfo.pBindings = &dslInfo;

    VkResult result = vkCreateDescriptorSetLayout(m_context.getDevice(), &dslInfo, nullptr, &m_descSetLayout);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create shadow descriptor set layout");

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo plInfo{};
    plInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plInfo.setLayoutCount = 1;
    plInfo.pSetLayouts = &m_descSetLayout;
    plInfo.pushConstantRangeCount = 1;
    plInfo.pPushConstantRanges = &pushConstantRange;

    VkResult result = vkCreatePipelineLayout(m_context.getDevice(), &plInfo, nullptr, &m_pipelineLayout);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create shadow pipeline layout");

    auto vertCode = VulkanPipeline::readFile("shaders/shadow.vert.spv");
    auto fragCode = VulkanPipeline::readFile("shaders/shadow.frag.spv");

    VkShaderModule vertModule = VulkanPipeline::createShaderModule(m_context.getDevice(), vertCode);
    VkShaderModule fragModule = VulkanPipeline::createShaderModule(m_context.getDevice(), fragCode);

    VkPipelineShaderStageCreateInfo stages[2] = {
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertModule, "main", nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragModule, "main", nullptr}
    };

    VkPipelineVertexInputStateCreateInfo viInfo{};
    viInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    viInfo.vertexBindingDescriptionCount = 0;
    viInfo.vertexAttributeDescriptionCount = 0;

    VkPipelineInputAssemblyStateCreateInfo iaInfo{};
    iaInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    iaInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vpInfo{};
    vpInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vpInfo.viewportCount = 1;
    vpInfo.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rastInfo{};
    rastInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rastInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rastInfo.cullMode = VK_CULL_MODE_BACK_BIT;
    rastInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rastInfo.lineWidth = 1.0f;
    rastInfo.depthBiasEnable = VK_TRUE;
    rastInfo.depthBiasConstantFactor = 4.0f;
    rastInfo.depthBiasSlopeFactor = 1.5f;

    VkPipelineMultisampleStateCreateInfo msInfo{};
    msInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo dsInfo{};
    dsInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    dsInfo.depthTestEnable = VK_TRUE;
    dsInfo.depthWriteEnable = VK_TRUE;
    dsInfo.depthCompareOp = VK_COMPARE_OP_LESS;
    dsInfo.depthBoundsTestEnable = VK_FALSE;
    dsInfo.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo cbInfo{};
    cbInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cbInfo.logicOpEnable = VK_FALSE;
    cbInfo.attachmentCount = 0;

    VkDynamicState dynStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynInfo{};
    dynInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynInfo.dynamicStateCount = 2;
    dynInfo.pDynamicStates = dynStates;

    VkGraphicsPipelineCreateInfo pipeInfo{};
    pipeInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeInfo.stageCount = 2;
    pipeInfo.pStages = stages;
    pipeInfo.pVertexInputState = &viInfo;
    pipeInfo.pInputAssemblyState = &iaInfo;
    pipeInfo.pViewportState = &vpInfo;
    pipeInfo.pRasterizationState = &rastInfo;
    pipeInfo.pMultisampleState = &msInfo;
    pipeInfo.pDepthStencilState = &dsInfo;
    pipeInfo.pColorBlendState = &cbInfo;
    pipeInfo.pDynamicState = &dynInfo;
    pipeInfo.layout = m_pipelineLayout;
    pipeInfo.renderPass = m_renderPass;
    pipeInfo.subpass = 0;

    VkResult result = vkCreateGraphicsPipelines(m_context.getDevice(), VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &m_pipeline);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create shadow pipeline");

    vkDestroyShaderModule(m_context.getDevice(), vertModule, nullptr);
    vkDestroyShaderModule(m_context.getDevice(), fragModule, nullptr);
}

void ShadowMap::createDescriptorSet() {
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo dslInfo{};
    dslInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dslInfo.bindingCount = 1;
    dslInfo.pBindings = &dslInfo;

    VkResult result = vkCreateDescriptorSetLayout(m_context.getDevice(), &dslInfo, nullptr, &m_descSetLayout);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create shadow desc set layout");

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;

    VkResult result = vkCreateDescriptorPool(m_context.getDevice(), &poolInfo, nullptr, &m_descPool);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create shadow descriptor pool");

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_descSetLayout;

    VkResult result = vkAllocateDescriptorSets(m_context.getDevice(), &allocInfo, &m_descSet);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to allocate shadow descriptor set");
}

void ShadowMap::createSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_TRUE;
    samplerInfo.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    VkResult result = vkCreateSampler(m_context.getDevice(), &samplerInfo, nullptr, &m_shadowSampler);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create shadow sampler");
}

void ShadowMap::destroyResources() {
    VkDevice device = m_context.getDevice();

    if (m_shadowSampler) vkDestroySampler(device, m_shadowSampler, nullptr);
    if (m_descPool) vkDestroyDescriptorPool(m_context.getDevice(), m_descPool, nullptr);
    if (m_descSetLayout) vkDestroyDescriptorSetLayout(device, m_descSetLayout, nullptr);
    if (m_pipelineLayout) vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);
    if (m_pipeline) vkDestroyPipeline(device, m_pipeline, nullptr);
    if (m_framebuffer) vkDestroyFramebuffer(device, m_framebuffer, nullptr);
    if (m_renderPass) vkDestroyRenderPass(device, m_renderPass, nullptr);
    if (m_depthArrayView) vkDestroyImageView(device, m_depthArrayView, nullptr);
    for (auto view : m_cascadeViews) {
        if (view) vkDestroyImageView(device, view, nullptr);
    }
    if (m_depthArray) vmaDestroyImage(m_context.getAllocator(), m_depthArray, m_depthArrayAlloc);
}

void ShadowMap::updateCascades(const glm::vec3& lightDir, const glm::mat4& view, const glm::mat4& proj, float nearPlane, float farPlane) {
    float clipRange = farPlane - nearPlane;
    float minZ = nearPlane;
    float maxZ = nearPlane + clipRange;
    float range = maxZ - minZ;
    float ratio = maxZ / minZ;

    for (uint32_t i = 0; i < CASCADE_COUNT; ++i) {
        float p = (i + 1) / static_cast<float>(CASCADE_COUNT);
        float log = minZ * std::pow(ratio, p);
        float uniform = minZ + range * p;
        float d = m_cascadeSplits[i] * (log - uniform) + uniform;

        float near = (i == 0) ? minZ : m_pushConstants.cascadeSplits[i - 1];
        float far = d;
        m_pushConstants.cascadeSplits[i] = (far - nearPlane) / (farPlane - nearPlane);

        float tanHalfFOVY = std::tan(glm::radians(45.0f) * 0.5f);
        float tanHalfFOVX = tanHalfFOVY * 16.0f / 9.0f;

        float xn = near * tanHalfFOVX;
        float xf = far * tanHalfFOVX;
        float yn = near * tanHalfFOVY;
        float yf = far * tanHalfFOVY;

        glm::vec3 corners[8] = {
            {-xn, -yn, -near}, {xn, -yn, -near}, {xn, yn, -near}, {-xn, yn, -near},
            {-xf, -yf, -far}, {xf, -yf, -far}, {xf, yf, -far}, {-xf, yf, -far}
        };

        glm::mat4 viewProj = proj * view;
        for (int j = 0; j < 8; ++j) {
            corners[j] = viewProj * glm::vec4(corners[j], 1.0f);
            corners[j] /= corners[j].w;
        }

        float minX = corners[0].x, maxX = corners[0].x;
        float minY = corners[0].y, maxY = corners[0].y;
        float minZ = corners[0].z, maxZ = corners[0].z;
        for (int j = 1; j < 8; ++j) {
            minX = std::min(minX, corners[j].x);
            maxX = std::max(maxX, corners[j].x);
            minY = std::min(minY, corners[j].y);
            maxY = std::max(maxY, corners[j].y);
            minZ = std::min(minZ, corners[j].z);
            maxZ = std::max(maxZ, corners[j].z);
        }

        glm::vec3 center = (glm::vec3(minX, minY, minZ) + glm::vec3(maxX, maxY, maxZ)) * 0.5f;
        float radius = std::max(maxX - minX, maxY - minY) * 0.5f;
        float aspect = 1.0f;

        center.x = std::round(center.x / 0.001f) * 0.001f;
        center.y = std::round(center.y / 0.001f) * 0.001f;
        center.z = std::round(center.z / 0.001f) * 0.001f;
        radius = std::ceil(radius * 1000.0f) / 1000.0f;

        m_cascades[i].viewProj = glm::ortho(-radius * aspect, radius * aspect, -radius, radius, 0.1f, maxZ - minZ + radius * 2.0f) *
                                glm::lookAt(center + lightDir * radius, center, glm::vec3(0, 1, 0));
    }

    m_pushConstants.cascadeSplits[0] = m_cascadeSplits[0];
    m_pushConstants.cascadeSplits[1] = m_cascadeSplits[1];
    m_pushConstants.cascadeSplits[2] = m_cascadeSplits[2];
    m_pushConstants.cascadeSplits[3] = m_cascadeSplits[3];
}

void ShadowMap::render(VkCommandBuffer cmd, const std::function<void(VkCommandBuffer)>& drawOpaque) {
    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass = m_renderPass;
    rpInfo.framebuffer = m_framebuffer;
    rpInfo.renderArea.offset = {0, 0};
    rpInfo.renderArea.extent = m_resolution;
    VkClearValue clearValue{};
    clearValue.depthStencil = {1.0f, 0};
    rpInfo.clearValueCount = 1;
    rpInfo.pClearValues = &clearValue;

    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = static_cast<float>(m_resolution.width);
    viewport.height = static_cast<float>(m_resolution.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = m_resolution;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

    for (uint32_t i = 0; i < CASCADE_COUNT; ++i) {
        m_pushConstants.cascadeViewProj[i] = m_cascades[i].viewProj;
        vkCmdPushConstants(cmd, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &m_pushConstants);

        VkViewport cascadeViewport = viewport;
        cascadeViewport.width = static_cast<float>(m_resolution.width);
        cascadeViewport.height = static_cast<float>(m_resolution.height);
        vkCmdSetViewport(cmd, 0, 1, &cascadeViewport);

        VkRect2D cascadeScissor = scissor;
        vkCmdSetScissor(cmd, 0, 1, &cascadeScissor);

        drawOpaque(cmd);
    }

    vkCmdEndRenderPass(cmd);
}

void ShadowMap::cleanup() {
    destroyResources();
}

void ShadowMap::destroyResources() {
    VkDevice device = m_context.getDevice();

    if (m_shadowSampler) vkDestroySampler(m_context.getDevice(), m_shadowSampler, nullptr);
    if (m_descPool) vkDestroyDescriptorPool(m_context.getDevice(), m_descPool, nullptr);
    if (m_descSetLayout) vkDestroyDescriptorSetLayout(m_context.getDevice(), m_descSetLayout, nullptr);
    if (m_pipelineLayout) vkDestroyPipelineLayout(m_context.getDevice(), m_pipelineLayout, nullptr);
    if (m_pipeline) vkDestroyPipeline(m_context.getDevice(), m_pipeline, nullptr);
    if (m_framebuffer) vkDestroyFramebuffer(m_context.getDevice(), m_framebuffer, nullptr);
    if (m_renderPass) vkDestroyRenderPass(m_context.getDevice(), m_renderPass, nullptr);
    if (m_depthArrayView) vkDestroyImageView(m_context.getDevice(), m_depthArrayView, nullptr);
    for (auto view : m_cascadeViews) {
        if (view) vkDestroyImageView(m_context.getDevice(), view, nullptr);
    }
    if (m_depthArray) vmaDestroyImage(m_context.getAllocator(), m_depthArray, m_depthArrayAlloc);
}

}