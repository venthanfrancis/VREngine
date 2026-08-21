#pragma once

#include "AREngine/Core/Event.hpp"

namespace AREngine::Platform
{
    // Raised when the window loses keyboard focus (WM_KILLFOCUS).
    // Carries no data — its purpose is purely as a signal that anyone
    // tracking held keys/buttons (Input::InputSystem) should treat all
    // of them as released, since further key-up messages for keys held
    // when focus was lost may never arrive. See docs/ARCHITECTURE.md,
    // "Focus-Loss Behavior".
    class WindowFocusLostEvent : public Core::Event
    {
    };
}
