#pragma once
#include <vulkan/vulkan_raii.hpp>
#include <glm/glm.hpp>
#include <vector>
#include <memory>

typedef struct VmaAllocator_T* VmaAllocator;
typedef struct VmaAllocation_T* VmaAllocation;

namespace crf {

class Model {
public:
    Model() = default;
    ~Model();

    bool loadOBJ(VkDevice device, VmaAllocator allocator, const std::string& filepath);
    void destroy(VkDevice device, VmaAllocator allocator);

    void draw(vk::raii::CommandBuffer& cmd) const;

private:
    struct Mesh {
        VkBuffer vertexBuffer = VK_NULL_HANDLE;
        VmaAllocation vertexAllocation = nullptr;
        VkBuffer indexBuffer = VK_NULL_HANDLE;
        VmaAllocation indexAllocation = nullptr;
        uint32_t indexCount = 0;
    };

    std::vector<Mesh> m_meshes;
};

} // namespace crf
