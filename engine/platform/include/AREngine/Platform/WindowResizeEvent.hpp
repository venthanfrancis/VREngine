#pragma once

#include "AREngine/Core/Event.hpp"

#include <cstdint>

namespace AREngine::Platform
{
    // Raised when a Window's client area changes size.
    class WindowResizeEvent : public Core::Event
    {
    public:
        WindowResizeEvent(std::uint32_t width, std::uint32_t height)
            : width(width), height(height)
        {
        }

        std::uint32_t width;
        std::uint32_t height;
    };
}
