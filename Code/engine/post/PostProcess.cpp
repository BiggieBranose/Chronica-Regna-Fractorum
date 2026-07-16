#include "PostProcess.hpp"
#include "core/Log.hpp"
#include "core/Assert.hpp"
#include <vulkan/vulkan.h>
#include <array>

namespace crf {

PostProcess::PostProcess(VulkanContext& context) : m_context(context) {}

PostProcess::~PostProcess() {
    cleanup();
}

void PostProcess::init(RenderGraph& graph, const std::string& colorInput, const std::string& finalOutput) {
    m_graph = &graph;
    m_colorInput = colorInput;
    m_finalOutput = finalOutput;
    createResources();
    createDescriptorSet();
    createPipelines();
    createSampler();
    Log::info("PostProcess initialized");
}

void PostProcess::cleanup() {
    destroyResources();
}

void PostProcess::createResources() {
    VkDevice device = m_context.getDevice();
    VmaAllocator allocator = m_context.getAllocator();

    VkImageCreateInfo imgInfo{};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    imgInfo.extent = {1280, 720, 1};
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VkResult result = vmaCreateImage(m_context.getAllocator(), &imgInfo, &allocInfo, &m_bloomImage, &m_bloomAlloc, nullptr);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create bloom image");

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_bloomImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;

    VkResult result = vkCreateImageView(m_context.getDevice(), &viewInfo, nullptr, &m_bloomView);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create bloom image view");
}

void PostProcess::createDescriptorSet() {
    VkDescriptorSetLayoutBinding bindings[] = {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
    };

    VkDescriptorSetLayoutCreateInfo dslInfo{};
    dslInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dslInfo.bindingCount = 3;
    dslInfo.pBindings = bindings;

    VkResult result = vkCreateDescriptorSetLayout(m_context.getDevice(), &dslInfo, nullptr, &m_descSetLayout);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create post-process descriptor set layout");

    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = 1;

    VkResult result = vkCreateDescriptorPool(m_context.getDevice(), &poolInfo, nullptr, &m_descPool);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create post-process descriptor pool");

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_descSetLayout;

    result = vkAllocateDescriptorSets;
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to allocate post-process descriptor set");
}

void PostProcess::createPipelines() {
    m_pipelines[0] = createPipeline("shaders/fullscreen.vert.spv", "shaders/tonemap.frag.spv");
    m_pipelines[1] = createPipeline("shaders/fullscreen.vert.spv", "shaders/bloom_extract.frag.spv");
    m_pipelines[2] = createPipeline("shaders/fullscreen.vert.spv", "shaders/bloom_blur.frag.spv");
    m_pipelines[3] = createPipeline("shaders/fullscreen.vert.spv", "shaders/composite.frag.spv");
}

VkPipeline PostProcess::createPipeline(const std::string& vertPath, const std::string& fragPath) {
    auto vertCode = VulkanPipeline::readFile(vertPath);
    auto fragCode = VulkanPipeline::readFile(fragPath);

    VkShaderModule vertModule = VulkanPipeline::createShaderModule(m_context.getDevice(), vertCode);
    VkShaderModule fragModule = VulkanPipeline::createShaderModule(m_context.getDevice(), fragCode);

    VkPipelineShaderStageCreateInfo stages[] = {
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertModule, "main", nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragModule, "main", nullptr}
    };

    VkPipelineVertexInputStateCreateInfo viInfo{};
    viInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

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
    rastInfo.cullMode = VK_CULL_MODE_NONE;
    rastInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rastInfo.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo msInfo{};
    msInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo dsInfo{};
    dsInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    dsInfo.depthTestEnable = VK_FALSE;
    dsInfo.depthWriteEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState cbAttach{};
    cbAttach.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    cbInfo.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo cbInfo{};
    cbInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cbInfo.attachmentCount = 1;
    cbInfo.pAttachments = &cbAttach;

    VkDynamicState dynStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynInfo{};
    dynInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynInfo.dynamicStateCount = 2;
    dynInfo.pDynamicStates = dynStates;

    VkPipelineLayoutCreateInfo plInfo{};
    plInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plInfo.setLayoutCount = 1;
    plInfo.pSetLayouts = &m_descSetLayout;
    VkPushConstantRange pcRange{};
    pcRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pcRange.offset = 0;
    pcRange.size = sizeof(m_pushConstants);
    plInfo.pushConstantRangeCount = 1;
    plInfo.pPushConstantRanges = &pcRange;

    VkPipelineLayout layout;
    VkResult result = vkCreatePipelineLayout(m_context.getDevice(), &plInfo, nullptr, &layout);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create pipeline layout");

    VkPipelineRenderingCreateInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachmentFormats = &VK_FORMAT_R16G16B16A16_SFLOAT;

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
    pipeInfo.layout = layout;
    pipeInfo.pNext = &renderingInfo;

    VkPipeline pipeline;
    VkResult result = vkCreateGraphicsPipelines(m_context.getDevice(), VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &pipeline);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create post-process pipeline");

    vkDestroyShaderModule(m_context.getDevice(), fragModule, nullptr);
    vkDestroyShaderModule(m_context.getDevice(), vertModule, nullptr);
    vkDestroyPipelineLayout(m_context.getDevice(), layout, nullptr);

    return pipeline;
}

void PostProcess::createSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.maxLod = 1.0f;

    VkResult result = vkCreateSampler(m_context.getDevice(), &samplerInfo, nullptr, &m_sampler);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create post-process sampler");
}

void PostProcess::destroyResources() {
    VkDevice device = m_context.getDevice();

    for (auto pipeline : m_pipelines) {
        if (pipeline) vkDestroyPipeline(m_context.getDevice(), pipeline, nullptr);
    }
    if (m_descSetLayout) vkDestroyDescriptorSetLayout(m_context.getDevice(), m_descSetLayout, nullptr);
    if (m_descPool) vkDestroyDescriptorPool(m_context.getDevice(), m_descPool, nullptr);
    if (m_sampler) vkDestroySampler(m_context.getDevice(), m_sampler, nullptr);
    if (m_bloomView) vkDestroyImageView(m_context.getDevice(), m_bloomView, nullptr);
    if (m_bloomImage) vmaDestroyImage(m_context.getAllocator(), m_bloomImage, m_bloomAlloc);
}

}