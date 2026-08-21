#pragma once

#include <cstdint>
#include <string>

namespace AREngine::Runtime
{
    // Minimal application configuration. Deliberately no graphics, XR,
    // fullscreen, or asset-path settings yet — see docs/ROADMAP.md.
    struct RuntimeConfig
    {
        std::string windowTitle = "AREngine";
        std::uint32_t windowWidth = 1280;
        std::uint32_t windowHeight = 720;
    };
}
