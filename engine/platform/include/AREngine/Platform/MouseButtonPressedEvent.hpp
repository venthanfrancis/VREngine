#pragma once

#include "AREngine/Core/Event.hpp"
#include "AREngine/Core/MouseButton.hpp"

namespace AREngine::Platform
{
    // Raised on WM_LBUTTONDOWN/WM_RBUTTONDOWN/WM_MBUTTONDOWN.
    class MouseButtonPressedEvent : public Core::Event
    {
    public:
        explicit MouseButtonPressedEvent(Core::MouseButton button) : button(button) {}

        Core::MouseButton button;
    };
}
