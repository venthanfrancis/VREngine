#pragma once

#include "AREngine/Core/Event.hpp"
#include "AREngine/Core/MouseButton.hpp"

namespace AREngine::Platform
{
    // Raised on WM_LBUTTONUP/WM_RBUTTONUP/WM_MBUTTONUP.
    class MouseButtonReleasedEvent : public Core::Event
    {
    public:
        explicit MouseButtonReleasedEvent(Core::MouseButton button) : button(button) {}

        Core::MouseButton button;
    };
}
