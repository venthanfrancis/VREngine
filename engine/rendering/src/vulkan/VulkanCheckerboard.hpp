#pragma once

// Private Vulkan bring-up implementation — see VulkanVersion.hpp.

#include <cstdint>
#include <vector>

namespace AREngine::Rendering::Vulkan
{
    // Generates a tiny procedural black/white checkerboard as tightly
    // packed 8-bit RGBA pixels (`width * height * 4` bytes, row-major,
    // no padding) — M8E's test texture. Chosen over any real image
    // file specifically to avoid pulling in an image decoder (PNG/JPEG/
    // stb_image) this milestone doesn't need — see
    // docs/ARCHITECTURE.md, "Checkerboard Generation Strategy (M8E)".
    //
    // Pure logic, no Vulkan calls — directly unit-testable. A tile is
    // white ({255,255,255,255}) if `(x / tileSize + y / tileSize)` is
    // even, black ({0,0,0,255}) otherwise (opaque alpha either way -
    // this texture demonstrates color sampling, not transparency).
    [[nodiscard]] std::vector<std::uint8_t> GenerateCheckerboardRGBA8(
        std::uint32_t width, std::uint32_t height, std::uint32_t tileSize);
}
