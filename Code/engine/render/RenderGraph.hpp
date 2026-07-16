#pragma once

#include "core/Types.hpp"
#include "graphics/VulkanContext.hpp"
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <vulkan/vulkan.h>

namespace crf {

class RenderGraph;
class RenderPass;
class Buffer;
class Image;
class DescriptorSet;
class Pipeline;

struct RenderGraphResource {
    enum class Type { Buffer, Image, External };
    Type type = Type::Buffer;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent3D extent = {1, 1, 1};
    VkImageUsageFlags usage = 0;
    VkBufferUsageFlags bufferUsage = 0;
    VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_AUTO;
    bool persistent = false;
};

class RenderGraph {
public:
    struct Pass {
        std::string name;
        std::vector<std::string> reads;
        std::vector<std::string> writes;
        std::function<void(VkCommandBuffer, const RenderGraph&)> execute;
        VkPipelineStageFlags2 stageMask = 0;
        VkAccessFlags2 accessMask = 0;
    };

    RenderGraph(VulkanContext& context);
    ~RenderGraph();

    RenderGraph(const RenderGraph&) = delete;
    RenderGraph& operator=(const RenderGraph&) = delete;

    std::string addResource(const std::string& name, const RenderGraphResource& desc);
    std::string importResource(const std::string& name, VkImage image, VkImageView view, VkFormat format, VkExtent3D extent);
    std::string importResource(const std::string& name, VkBuffer buffer, VkDeviceSize size, VkDeviceSize offset);

    void addPass(const Pass& pass);

    void compile();
    void execute(VkCommandBuffer cmd);

    VkImageView getImageView(const std::string& name) const;
    VkBuffer getBuffer(const std::string& name) const;
    VkExtent3D getExtent(const std::string& name) const;

private:
    struct Resource {
        RenderGraphResource desc;
        VkImage image = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        bool imported = false;
        std::vector<uint32_t> passReads;
        std::vector<uint32_t> passWrites;
        VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    };

    struct CompiledPass {
        Pass pass;
        std::vector<VkImageMemoryBarrier2> barriersBefore;
        std::vector<VkImageMemoryBarrier2> barriersAfter;
        std::vector<VkBufferMemoryBarrier2> bufferBarriersBefore;
        std::vector<VkBufferMemoryBarrier2> bufferBarriersAfter;
    };

    VulkanContext& m_context;
    std::unordered_map<std::string, Resource> m_resources;
    std::vector<Pass> m_passes;
    std::vector<CompiledPass> m_compiledPasses;
    bool m_compiled = false;

    void createResource(const std::string& name, Resource& res);
    void destroyResource(Resource& res);
    void computeBarriers();
    VkImageLayout getRequiredLayout(VkImageUsageFlags usage, bool isWrite) const;
    VkPipelineStageFlags2 usageToStage(VkImageUsageFlags usage) const;
    VkAccessFlags2 usageToAccess(VkImageUsageFlags usage, bool isWrite) const;
};

}