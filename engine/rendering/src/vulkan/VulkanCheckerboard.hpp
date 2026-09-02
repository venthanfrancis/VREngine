#pragma once

// Private Vulkan bring-up implementation — see VulkanVersion.hpp.

#include <array>
#include <cstdint>
#include <vector>

namespace AREngine::Rendering::Vulkan
{
    // Generates a tiny procedural checkerboard as tightly packed 8-bit
    // RGBA pixels (`width * height * 4` bytes, row-major, no padding) —
    // M8E's test texture. Chosen over any real image file specifically
    // to avoid pulling in an image decoder (PNG/JPEG/stb_image) this
    // milestone doesn't need — see docs/ARCHITECTURE.md, "Checkerboard
    // Generation Strategy (M8E)".
    //
    // Pure logic, no Vulkan calls — directly unit-testable. A tile is
    // `colorA` (default white, {255,255,255,255}) if
    // `(x / tileSize + y / tileSize)` is even, `colorB` (default black,
    // {0,0,0,255}) otherwise. The color parameters were added in M13
    // specifically so a second, visually distinct material's texture
    // could reuse this same generator instead of needing a separate
    // one — every pre-M13 call site keeps its exact original black/
    // white output via these defaults. See docs/ARCHITECTURE.md,
    // "M13 - Material & Render Resource Binding Foundation".
    [[nodiscard]] std::vector<std::uint8_t> GenerateCheckerboardRGBA8(
        std::uint32_t width, std::uint32_t height, std::uint32_t tileSize,
        std::array<std::uint8_t, 4> colorA = {255, 255, 255, 255},
        std::array<std::uint8_t, 4> colorB = {0, 0, 0, 255});
}
