#include "VulkanClipSpace.hpp"

namespace AREngine::Rendering::Vulkan
{
    AREngine::Core::Math::Mat4 ApplyVulkanYFlip(AREngine::Core::Math::Mat4 projection)
    {
        projection.Set(1, 1, -projection.At(1, 1));
        return projection;
    }
}
