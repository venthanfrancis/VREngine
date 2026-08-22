#include "VulkanCheckerboard.hpp"

#include "AREngine/Core/Assert.hpp"

namespace AREngine::Rendering::Vulkan
{
    std::vector<std::uint8_t> GenerateCheckerboardRGBA8(
        std::uint32_t width, std::uint32_t height, std::uint32_t tileSize)
    {
        AR_ASSERT_MSG(tileSize > 0, "GenerateCheckerboardRGBA8 requires a nonzero tileSize");

        std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * height * 4);

        for (std::uint32_t y = 0; y < height; ++y)
        {
            for (std::uint32_t x = 0; x < width; ++x)
            {
                const bool isWhiteTile = ((x / tileSize) + (y / tileSize)) % 2 == 0;
                const std::uint8_t value = isWhiteTile ? 255 : 0;

                const std::size_t pixelIndex = (static_cast<std::size_t>(y) * width + x) * 4;
                pixels[pixelIndex + 0] = value; // R
                pixels[pixelIndex + 1] = value; // G
                pixels[pixelIndex + 2] = value; // B
                pixels[pixelIndex + 3] = 255;   // A - fully opaque
            }
        }

        return pixels;
    }
}
