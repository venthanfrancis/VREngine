#pragma once

#include <cstdint>
#include <string>

namespace AREngine::Platform
{
    // Minimal window configuration. Deliberately no fullscreen,
    // multi-monitor, DPI-scaling, or window-mode options yet — see
    // docs/ROADMAP.md.
    struct WindowDesc
    {
        std::string title = "AREngine";
        std::uint32_t width = 1280;
        std::uint32_t height = 720;
    };
}
