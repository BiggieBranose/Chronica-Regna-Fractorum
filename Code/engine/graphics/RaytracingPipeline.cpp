#include "RaytracingPipeline.hpp"
#include "core/Log.hpp"
#include "core/Assert.hpp"

#include <array>
#include <cstring>
#include <vulkan/vulkan.h>

namespace crf {

RaytracingPipeline::RaytracingPipeline(VulkanContext& context, AccelerationStructure& accelStruct)
    : m_context(context), m_accelStruct(accelStruct) {
}

RaytracingPipeline::~RaytracingPipeline() {
    VkDevice device = m_context.getDevice();

    if (m_pipeline) vkDestroyPipeline(device, m_pipeline, nullptr);
    if (m_pipelineLayout) vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);
    if (m_descriptorSetLayout) vkDestroyDescriptorSetLayout(device, m_descriptorSetLayout, nullptr);
    if (m_descriptorPool) vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);

    if (m_shaderBindingTableBuffer) {
        vkDestroyBuffer(device, m_shaderBindingTableBuffer, nullptr);
        vkFreeMemory(device, m_shaderBindingTableMemory, nullptr);
    }
}

void RaytracingPipeline::createRaytracingDescriptorSetLayout() {
    std::array<VkDescriptorSetLayoutBinding, 4> bindings{};

    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    bindings[3].binding = 3;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<u32>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    VkResult result = vkCreateDescriptorSetLayout(m_context.getDevice(), &layoutInfo, nullptr, &m_descriptorSetLayout);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create raytracing descriptor set layout");
}

void RaytracingPipeline::createRaytracingPipeline() {
    auto rayGenCode = VulkanPipeline::readFile("shaders/raygen.spv");
    auto missCode = VulkanPipeline::readFile("shaders/miss.spv");
    auto closestHitCode = VulkanPipeline::readFile("shaders/closesthit.spv");

    VkShaderModule rayGenModule = VulkanPipeline::createShaderModule(m_context.getDevice(), rayGenCode);
    VkShaderModule missModule = VulkanPipeline::createShaderModule(m_context.getDevice(), missCode);
    VkShaderModule closestHitModule = VulkanPipeline::createShaderModule(m_context.getDevice(), closestHitCode);

    VkPipelineShaderStageCreateInfo rayGenStageInfo{};
    rayGenStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    rayGenStageInfo.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    rayGenStageInfo.module = rayGenModule;
    rayGenStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo missStageInfo{};
    missStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    missStageInfo.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
    missStageInfo.module = missModule;
    missStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo closestHitStageInfo{};
    closestHitStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    closestHitStageInfo.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    closestHitStageInfo.module = closestHitModule;
    closestHitStageInfo.pName = "main";

    std::array<VkPipelineShaderStageCreateInfo, 3> shaderStages = {rayGenStageInfo, missStageInfo, closestHitStageInfo};

    VkRayTracingShaderGroupCreateInfoKHR rayGenGroup{};
    rayGenGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    rayGenGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    rayGenGroup.generalShader = 0;
    rayGenGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
    rayGenGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
    rayGenGroup.intersectionShader = VK_SHADER_UNUSED_KHR;

    VkRayTracingShaderGroupCreateInfoKHR missGroup{};
    missGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    missGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    missGroup.generalShader = 1;
    missGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
    missGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
    missGroup.intersectionShader = VK_SHADER_UNUSED_KHR;

    VkRayTracingShaderGroupCreateInfoKHR closestHitGroup{};
    closestHitGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    closestHitGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    closestHitGroup.generalShader = VK_SHADER_UNUSED_KHR;
    closestHitGroup.closestHitShader = 2;
    closestHitGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
    closestHitGroup.intersectionShader = VK_SHADER_UNUSED_KHR;

    std::array<VkRayTracingShaderGroupCreateInfoKHR, 3> shaderGroups = {rayGenGroup, missGroup, closestHitGroup};

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(RaytracingPushConstants);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    VkResult result = vkCreatePipelineLayout(m_context.getDevice(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create raytracing pipeline layout");

    VkRayTracingPipelineCreateInfoKHR rayTracingPipelineInfo{};
    rayTracingPipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    rayTracingPipelineInfo.stageCount = static_cast<u32>(shaderStages.size());
    rayTracingPipelineInfo.pStages = shaderStages.data();
    rayTracingPipelineInfo.groupCount = static_cast<u32>(shaderGroups.size());
    rayTracingPipelineInfo.pGroups = shaderGroups.data();
    rayTracingPipelineInfo.maxPipelineRayRecursionDepth = 2;
    rayTracingPipelineInfo.layout = m_pipelineLayout;

    result = m_context.vkCreateRayTracingPipelinesKHR(m_context.getDevice(), VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rayTracingPipelineInfo, nullptr, &m_pipeline);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create raytracing pipeline");

    vkDestroyShaderModule(m_context.getDevice(), closestHitModule, nullptr);
    vkDestroyShaderModule(m_context.getDevice(), missModule, nullptr);
    vkDestroyShaderModule(m_context.getDevice(), rayGenModule, nullptr);

    createShaderBindingTable();
}

void RaytracingPipeline::createShaderBindingTable() {
    u32 handleSize = 0;
    u32 handleAlignment = 0;
    u32 baseAlignment = 0;

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR properties{};
    properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

    VkPhysicalDeviceProperties2 deviceProperties2{};
    deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    deviceProperties2.pNext = &properties;

    vkGetPhysicalDeviceProperties2(m_context.getPhysicalDevice(), &deviceProperties2);

    handleSize = properties.shaderGroupHandleSize;
    handleAlignment = properties.shaderGroupHandleAlignment;
    baseAlignment = properties.shaderGroupBaseAlignment;

    m_shaderGroupHandleSize = handleSize;
    m_shaderGroupHandleAlignment = handleAlignment;
    m_shaderGroupBaseAlignment = baseAlignment;

    u32 groupCount = 3;
    u32 sbtSize = groupCount * baseAlignment;

    std::vector<u8> shaderHandleStorage(sbtSize);

    VkResult result = m_context.vkGetRayTracingShaderGroupHandlesKHR(
        m_context.getDevice(), m_pipeline, 0, groupCount,
        sbtSize, shaderHandleStorage.data()
    );
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to get raytracing shader group handles");

    VkDevice device = m_context.getDevice();

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = sbtSize;
    bufferInfo.usage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    result = vkCreateBuffer(device, &bufferInfo, nullptr, &m_shaderBindingTableBuffer);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create shader binding table buffer");

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, m_shaderBindingTableBuffer, &memRequirements);

    VkMemoryAllocateFlagsInfo allocFlagsInfo{};
    allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = VulkanBuffer::findMemoryType(
        memRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        m_context.getPhysicalDevice()
    );
    allocInfo.pNext = &allocFlagsInfo;

    result = vkAllocateMemory(device, &allocInfo, nullptr, &m_shaderBindingTableMemory);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to allocate SBT memory");

    vkBindBufferMemory(device, m_shaderBindingTableBuffer, m_shaderBindingTableMemory, 0);

    void* data;
    vkMapMemory(device, m_shaderBindingTableMemory, 0, sbtSize, 0, &data);

    for (u32 i = 0; i < groupCount; i++) {
        std::memcpy(static_cast<u8*>(data) + i * baseAlignment,
                    shaderHandleStorage.data() + i * handleSize,
                    handleSize);
    }

    vkUnmapMemory(device, m_shaderBindingTableMemory);

    Log::info("Shader binding table created ({} bytes)", sbtSize);
}

void RaytracingPipeline::createRaytracingDescriptorPool() {
    std::array<VkDescriptorPoolSize, 4> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    poolSizes[0].descriptorCount = 1;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[1].descriptorCount = 1;
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[2].descriptorCount = 1;
    poolSizes[3].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[3].descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<u32>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 1;

    VkResult result = vkCreateDescriptorPool(m_context.getDevice(), &poolInfo, nullptr, &m_descriptorPool);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create raytracing descriptor pool");
}

void RaytracingPipeline::createRaytracingDescriptorSets(VkImageView outputImageView, VkSampler outputSampler,
                                                         VkBuffer vertexBuffer, VkDeviceSize vertexBufferSize,
                                                         VkBuffer cameraBuffer, VkDeviceSize cameraBufferSize) {
    (void)outputSampler;
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_descriptorSetLayout;

    m_descriptorSets.resize(1);

    VkResult result = vkAllocateDescriptorSets(m_context.getDevice(), &allocInfo, m_descriptorSets.data());
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to allocate raytracing descriptor sets");

    VkWriteDescriptorSetAccelerationStructureKHR accelerationStructureInfo{};
    accelerationStructureInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    accelerationStructureInfo.accelerationStructureCount = 1;
    VkAccelerationStructureKHR topLevelAS = m_accelStruct.getTopLevelAS();
    accelerationStructureInfo.pAccelerationStructures = &topLevelAS;

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageView = outputImageView;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkDescriptorBufferInfo vertexBufferInfo{};
    vertexBufferInfo.buffer = vertexBuffer;
    vertexBufferInfo.offset = 0;
    vertexBufferInfo.range = vertexBufferSize;

    VkDescriptorBufferInfo cameraBufferInfo{};
    cameraBufferInfo.buffer = cameraBuffer;
    cameraBufferInfo.offset = 0;
    cameraBufferInfo.range = cameraBufferSize;

    std::array<VkWriteDescriptorSet, 4> descriptorWrites{};

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].pNext = &accelerationStructureInfo;
    descriptorWrites[0].dstSet = m_descriptorSets[0];
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    descriptorWrites[0].descriptorCount = 1;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = m_descriptorSets[0];
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pImageInfo = &imageInfo;

    descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[2].dstSet = m_descriptorSets[0];
    descriptorWrites[2].dstBinding = 2;
    descriptorWrites[2].dstArrayElement = 0;
    descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[2].descriptorCount = 1;
    descriptorWrites[2].pBufferInfo = &vertexBufferInfo;

    descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[3].dstSet = m_descriptorSets[0];
    descriptorWrites[3].dstBinding = 3;
    descriptorWrites[3].dstArrayElement = 0;
    descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[3].descriptorCount = 1;
    descriptorWrites[3].pBufferInfo = &cameraBufferInfo;

    vkUpdateDescriptorSets(m_context.getDevice(), static_cast<u32>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

void RaytracingPipeline::recordRaytracingCommands(VkCommandBuffer commandBuffer, u32 width, u32 height) {
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_pipeline);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                            m_pipelineLayout, 0, 1, m_descriptorSets.data(), 0, nullptr);

    VkStridedDeviceAddressRegionKHR raygenShaderBindingTable{};
    raygenShaderBindingTable.deviceAddress = getBufferDeviceAddress(m_shaderBindingTableBuffer);
    raygenShaderBindingTable.stride = m_shaderGroupBaseAlignment;
    raygenShaderBindingTable.size = m_shaderGroupBaseAlignment;

    VkStridedDeviceAddressRegionKHR missShaderBindingTable{};
    missShaderBindingTable.deviceAddress = getBufferDeviceAddress(m_shaderBindingTableBuffer) + m_shaderGroupBaseAlignment;
    missShaderBindingTable.stride = m_shaderGroupBaseAlignment;
    missShaderBindingTable.size = m_shaderGroupBaseAlignment;

    VkStridedDeviceAddressRegionKHR hitShaderBindingTable{};
    hitShaderBindingTable.deviceAddress = getBufferDeviceAddress(m_shaderBindingTableBuffer) + 2 * m_shaderGroupBaseAlignment;
    hitShaderBindingTable.stride = m_shaderGroupBaseAlignment;
    hitShaderBindingTable.size = m_shaderGroupBaseAlignment;

    VkStridedDeviceAddressRegionKHR callableShaderBindingTable{};

    m_context.vkCmdTraceRaysKHR(commandBuffer,
                          &raygenShaderBindingTable,
                          &missShaderBindingTable,
                          &hitShaderBindingTable,
                          &callableShaderBindingTable,
                          width, height, 1);
}

VkDeviceAddress RaytracingPipeline::getBufferDeviceAddress(VkBuffer buffer) {
    VkBufferDeviceAddressInfo addressInfo{};
    addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addressInfo.buffer = buffer;

    return m_context.vkGetBufferDeviceAddress(m_context.getDevice(), &addressInfo);
}

}
