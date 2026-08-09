#pragma once

#include "core/Types.hpp"
#include <vector>
#include <vulkan/vulkan.h>

namespace crf {

struct Vertex {
    f32 pos[3];
    f32 color[3];
    f32 texCoord[2];

    static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();

    bool operator==(const Vertex& other) const;
};

}
